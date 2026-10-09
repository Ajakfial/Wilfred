#include "wilfred/updater/updater.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/ipc/http.hpp"
#include "wilfred/platform/platform.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(WILFRED_BSD)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#endif

namespace wilfred {

namespace {

// Background check state
std::atomic<bool> g_check_started{false};
std::atomic<bool> g_check_done{false};

// Last background-check outcome, surfaced to the overlay `update` mini.
std::mutex g_pending_mu;
PendingUpdate g_pending;

// The GitHub repo API base
const char* kRepoApiBase = "https://api.github.com/repos/Ajakfial/Wilfred";

// Staging directory for downloaded updates
std::string staging_dir() {
  return path_join(data_directory(), "update-staging");
}

// Helper to create directories
void ensure_dir(const std::string& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
}

// Get the path to the current executable
std::string current_executable_path() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (len == 0) return "";
  return wide_to_utf8(std::wstring(buf, len));
#else
  char buf[4096];
#if defined(__FreeBSD__) || defined(__DragonFly__)
  // No /proc by default: ask the kernel directly (cf. web_ui exe dir).
  {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    std::size_t len = sizeof(buf);
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == 0 && len > 1) {
      buf[sizeof(buf) - 1] = '\0';
      return std::string(buf);
    }
  }
#elif defined(__NetBSD__)
  // NetBSD keeps the executable pathname under KERN_PROC_ARGS
  // (cf. libuv's uv_exepath); documented in sysctl(7).
  {
    int mib[4] = {CTL_KERN, KERN_PROC_ARGS, static_cast<int>(getpid()), KERN_PROC_PATHNAME};
    std::size_t len = sizeof(buf);
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == 0 && len > 1) {
      buf[sizeof(buf) - 1] = '\0';
      return std::string(buf);
    }
  }
#endif
  // OpenBSD has no sysctl for this; all BSDs fall through to procfs here
  // (present only when mounted — otherwise "" and callers degrade).
  ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len < 0) {
    // NetBSD procfs layout.
    len = readlink("/proc/curproc/exe", buf, sizeof(buf) - 1);
  }
  if (len < 0) return "";
  buf[len] = '\0';
  return std::string(buf);
#endif
}

// Get the directory containing the current executable
std::string current_executable_dir() {
  std::string exe = current_executable_path();
  auto slash = exe.find_last_of("/\\");
  if (slash == std::string::npos) return ".";
  return exe.substr(0, slash);
}

// Build the asset filename for a given tag and platform
std::string asset_filename(const std::string& tag, const PlatformInfo& platform) {
  return "wilfred-" + tag + "-" + platform.id() + platform.asset_ext;
}

// Build the download URL for a given asset
std::string download_url(const std::string& tag, const PlatformInfo& platform) {
  return "https://github.com/Ajakfial/Wilfred/releases/download/" + tag + "/" +
         asset_filename(tag, platform);
}

// Parse the GitHub releases API response to find the latest release info
bool parse_latest_release(const std::string& json_resp, std::string& tag_out,
                          std::string& url_out) {
  // Look for "tag_name":"v1.2.3"
  tag_out = json_get_string(json_resp, "tag_name");
  if (tag_out.empty()) return false;

  // Look for the browser_download_url in assets array
  std::string assets = json_extract_array(json_resp, "assets");
  if (assets.empty()) return false;

  auto objects = json_object_array(assets);
  for (const auto& obj : objects) {
    std::string url = json_get_string(obj, "browser_download_url");
    if (!url.empty()) {
      url_out = url;
      return true;
    }
  }

  return false;
}

// Platform-specific: launch a detached helper process that will wait for
// the current process to exit, replace files, and restart.
bool launch_update_helper(const std::string& staging, const std::string& exe_path,
                          const std::string& exe_dir);

#ifdef _WIN32
// Windows: write a PowerShell script that waits, copies, and restarts
bool launch_update_helper(const std::string& staging, const std::string& exe_path,
                          const std::string& exe_dir) {
  std::string script_path = path_join(staging, "update-helper.ps1");

  std::ofstream f(script_path);
  if (!f) return false;

  f << "$ErrorActionPreference = 'Stop'\n";
  f << "$pid_to_wait = " << GetCurrentProcessId() << "\n";
  f << "Wait-Process -Id $pid_to_wait -Timeout 30 -ErrorAction SilentlyContinue\n";
  f << "Start-Sleep -Milliseconds 500\n";
  f << "$staging = '" << staging << "'\n";
  f << "$dest = '" << exe_dir << "'\n";
  f << "Get-ChildItem -Path $staging -Recurse | ForEach-Object {\n";
  f << "  $rel = $_.FullName.Substring($staging.Length)\n";
  f << "  $target = Join-Path $dest $rel\n";
  f << "  if ($_.PSIsContainer) {\n";
  f << "    New-Item -ItemType Directory -Force -Path $target | Out-Null\n";
  f << "  } else {\n";
  f << "    $targetDir = Split-Path $target -Parent\n";
  f << "    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null\n";
  f << "    Copy-Item $_.FullName $target -Force\n";
  f << "  }\n";
  f << "}\n";
  f << "Start-Process -FilePath '" << exe_path << "'\n";
  f << "Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue\n";
  f.close();

  // Launch PowerShell detached
  std::string cmd =
      "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" +
      script_path + "\"";

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring wcmd(cmd.begin(), cmd.end());

  BOOL ok = CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
  if (ok) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
  return ok != 0;
}
#else
// macOS/Linux: write a shell script that waits, copies, and restarts
bool launch_update_helper(const std::string& staging, const std::string& exe_path,
                          const std::string& exe_dir) {
  std::string script_path = path_join(staging, "update-helper.sh");

  std::ofstream f(script_path);
  if (!f) return false;

  f << "#!/bin/bash\n";
  f << "set -e\n";
  f << "PID_TO_WAIT=" << getpid() << "\n";
  f << "while kill -0 $PID_TO_WAIT 2>/dev/null; do\n";
  f << "  sleep 0.2\n";
  f << "done\n";
  f << "sleep 0.5\n";
  f << "STAGING='" << staging << "'\n";
  f << "DEST='" << exe_dir << "'\n";
  f << "cd \"$STAGING\"\n";
  f << "find . -type f | while read -r file; do\n";
  f << "  target=\"$DEST/$file\"\n";
  f << "  mkdir -p \"$(dirname \"$target\")\"\n";
  f << "  cp -f \"$file\" \"$target\"\n";
  f << "done\n";
  f << "chmod +x '" << exe_path << "'\n";
  f << "nohup '" << exe_path << "' >/dev/null 2>&1 &\n";
  f << "rm -rf \"$STAGING\"\n";
  f.close();

  // Make script executable
  chmod(script_path.c_str(), 0755);

  // Launch detached
  pid_t pid = fork();
  if (pid == 0) {
    // Child
    setsid();
    execl("/bin/bash", "bash", script_path.c_str(), static_cast<char*>(nullptr));
    _exit(1);
  } else if (pid > 0) {
    return true;
  }
  return false;
}
#endif

}  // namespace

// Public API implementation

UpdateCheckResult check_for_update(const std::string& repo_url, const std::string& current_ver,
                                   int timeout_ms) {
  UpdateCheckResult result;
  result.current_version = current_ver;

  std::string api_url = std::string(kRepoApiBase) + "/releases/latest";
  std::string response = http_get(api_url, "", timeout_ms);

  if (response.empty()) {
    result.error = "failed to reach GitHub API";
    return result;
  }

  std::string tag, asset_url;
  if (!parse_latest_release(response, tag, asset_url)) {
    result.error = "failed to parse release info";
    return result;
  }

  result.latest_version = tag;

  Version current = parse_version(current_ver);
  Version latest = parse_version(tag);

  if (!latest.valid()) {
    result.error = "invalid version tag: " + tag;
    return result;
  }

  if (latest.compare(current) > 0) {
    result.update_available = true;
    result.download_url = asset_url;
  }

  return result;
}

bool download_file(const std::string& url, const std::string& dest_path, ProgressCallback progress,
                   void* user_data, int timeout_ms, std::string* error) {
  // Use http_get for simplicity (downloads entire file to memory)
  // For large files this could be streamed, but Wilfred binaries are small
  std::string data = http_get(url, "", timeout_ms);

  if (data.empty()) {
    if (error) *error = "download failed or empty response";
    return false;
  }

  // Check if it's an HTML error page
  if (data.find("404") != std::string::npos && data.find("Not Found") != std::string::npos) {
    if (error) *error = "asset not found (404)";
    return false;
  }

  ensure_dir(path_parent(dest_path));

  std::ofstream f(dest_path, std::ios::binary | std::ios::trunc);
  if (!f) {
    if (error) *error = "cannot write to: " + dest_path;
    return false;
  }

  f.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!f.good()) {
    if (error) *error = "write failed";
    return false;
  }

  if (progress) {
    progress(static_cast<long long>(data.size()), static_cast<long long>(data.size()), user_data);
  }

  return true;
}

bool extract_archive(const std::string& archive_path, const std::string& dest_dir,
                     std::string* error) {
  ensure_dir(dest_dir);

  std::string ext = path_extension(archive_path);
  if (ext == ".zip") {
    return extract_zip(archive_path, dest_dir, error);
  } else if (ext == ".gz" || ext == ".tgz") {
    return extract_tarball(archive_path, dest_dir, error);
  }

  if (error) *error = "unknown archive format: " + ext;
  return false;
}

bool perform_update(bool dry_run, std::string* error) {
  PlatformInfo platform = detect_platform();
  std::string current = wilfred_version();

  log_info("updater",
           "checking for updates (current: " + current + ", platform: " + platform.id() + ")");

  UpdateCheckResult check = check_for_update(kRepoApiBase, current);
  if (!check.error.empty()) {
    if (error) *error = check.error;
    return false;
  }

  if (!check.update_available) {
    log_info("updater", "already up to date (" + current + ")");
    return false;
  }

  log_info("updater", "update available: " + current + " -> " + check.latest_version);

  if (dry_run) {
    return true;
  }

  // Download
  std::string staging = staging_dir();
  ensure_dir(staging);

  std::string asset = asset_filename(check.latest_version, platform);
  std::string archive_path = path_join(staging, asset);

  log_info("updater", "downloading " + check.download_url);
  if (!download_file(check.download_url, archive_path, nullptr, nullptr, 60000, error)) {
    return false;
  }

  // Extract
  std::string extract_dir = path_join(staging, "extracted");
  log_info("updater", "extracting " + archive_path);
  if (!extract_archive(archive_path, extract_dir, error)) {
    return false;
  }

  // Find the executable in the extracted directory
  std::string new_exe;
  std::string exe_name = is_windows() ? "wilfred.exe" : "wilfred";

  // Search for the binary in extracted content
  std::error_code ec;
  for (auto& entry : std::filesystem::recursive_directory_iterator(extract_dir, ec)) {
    if (entry.is_regular_file() && path_filename(entry.path().string()) == exe_name) {
      new_exe = entry.path().string();
      break;
    }
  }

  if (new_exe.empty()) {
    if (error) *error = "could not find " + exe_name + " in extracted archive";
    return false;
  }

  // Get current executable info
  std::string exe_path = current_executable_path();
  std::string exe_dir = current_executable_dir();

  if (exe_path.empty()) {
    if (error) *error = "cannot determine current executable path";
    return false;
  }

  // Launch helper to replace and restart
  log_info("updater", "staging update, will restart...");
  if (!launch_update_helper(extract_dir, exe_path, exe_dir)) {
    if (error) *error = "failed to launch update helper";
    return false;
  }

  return true;
}

void startup_update_check() {
  if (g_check_started.exchange(true)) return;

  std::thread([]() {
    UpdateCheckResult r = check_for_update(kRepoApiBase, wilfred_version());
    {
      std::lock_guard<std::mutex> lk(g_pending_mu);
      g_pending.checked = r.error.empty();
      g_pending.available = r.update_available;
      g_pending.current_version = r.current_version;
      g_pending.latest_version = r.latest_version;
      g_pending.download_url = r.download_url;
    }
    g_check_done = true;
  }).detach();
}

PendingUpdate pending_update() {
  std::lock_guard<std::mutex> lk(g_pending_mu);
  return g_pending;
}

int run_update_command(bool check_only, bool auto_yes) {
  std::string error;

  if (check_only) {
    PlatformInfo platform = detect_platform();
    std::string current = wilfred_version();
    UpdateCheckResult check = check_for_update(kRepoApiBase, current);

    if (!check.error.empty()) {
      std::cerr << "Update check failed: " << check.error << "\n";
      return 1;
    }

    if (check.update_available) {
      std::cout << "Update available: " << current << " -> " << check.latest_version << "\n";
      std::cout << "Run 'wilfred update' to install.\n";
    } else {
      std::cout << "Wilfred is up to date (" << current << ").\n";
    }
    return 0;
  }

  // Full update
  if (!perform_update(false, &error)) {
    if (!error.empty()) {
      std::cerr << "Update failed: " << error << "\n";
      return 1;
    }
    std::cout << "Wilfred is already up to date.\n";
    return 0;
  }

  std::cout << "Update staged. Wilfred will restart to complete the update.\n";
#ifdef _WIN32
  // On Windows, exit so the helper can replace files
  ExitProcess(0);
#else
  // On Unix, exit so the helper can replace files
  _exit(0);
#endif
  return 0;
}

}  // namespace wilfred
