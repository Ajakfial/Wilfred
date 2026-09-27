#include "wilfred/plugin/host.hpp"
#include "wilfred/plugin/abi.hpp"

#include "wilfred/config/yaml.hpp"
#include "wilfred/core/json.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace wilfred {
namespace fs = std::filesystem;

std::string plugin_query_json(const std::string& q, std::size_t limit) {
  return "{\"op\":\"query\",\"q\":\"" + json_escape(q) + "\",\"limit\":" + std::to_string(limit) + "}";
}

std::vector<SearchResult> parse_plugin_results_json(const std::string& json,
                                                    const std::string& plugin_id) {
  std::vector<SearchResult> out;
  auto arr = json_extract_array(json, "results");
  if (arr.empty()) return out;
  for (auto& obj : json_object_array(arr)) {
    SearchResult r;
    r.title = json_get_string(obj, "title");
    r.subtitle = json_get_string(obj, "subtitle");
    r.path = json_get_string(obj, "path");
    r.payload = json_get_string(obj, "payload");
    if (r.payload.empty()) r.payload = r.path;
    r.score = json_get_int(obj, "score", 500);
    r.kind_label = json_get_string(obj, "kind");
    if (r.kind_label.empty()) r.kind_label = "plugin";
    r.category = "plugin";
    auto act = to_lower_utf8(json_get_string(obj, "action"));
    if (act == "copy")
      r.action = ResultAction::Copy;
    else if (act == "web" || act == "websearch")
      r.action = ResultAction::WebSearch;
    else if (act == "reveal")
      r.action = ResultAction::Reveal;
    else if (act == "none")
      r.action = ResultAction::None;
    else
      r.action = ResultAction::Plugin;
    r.plugin_id = plugin_id;
    auto acts = json_extract_array(obj, "actions");
    for (auto& a : json_object_array(acts)) {
      ResultActionItem it;
      it.id = json_get_string(a, "id");
      if (it.id.empty()) it.id = json_get_string(a, "action");
      it.label = json_get_string(a, "label");
      if (it.label.empty()) it.label = it.id;
      if (!it.id.empty()) r.actions.push_back(std::move(it));
    }
    if (r.title.empty() && r.path.empty()) continue;
    if (r.title.empty()) r.title = path_filename(r.path);
    out.push_back(std::move(r));
  }
  return out;
}

std::vector<std::string> default_plugin_directories() {
  std::vector<std::string> d;
  d.push_back(path_join(config_directory(), "plugins"));
  d.push_back(path_join(data_directory(), "plugins"));
  return d;
}

static bool load_manifest_file(const std::string& path, PluginManifest& m) {
  std::string text;
  if (!read_file_all(path, text)) return false;
  YamlValue root;
  YamlError ye;
  if (!parse_yaml(text, root, ye) || !root.is_map()) {
    // JSON-ish fallback
    m.id = json_get_string(text, "id");
    m.kind = json_get_string(text, "kind");
    m.path = json_get_string(text, "path");
    m.command = json_get_string(text, "command");
    m.timeout_ms = json_get_int(text, "timeout_ms", 400);
    if (m.id.empty()) return false;
    if (m.kind.empty()) m.kind = m.command.empty() ? "native" : "stdio";
    return true;
  }
  m.id = root.str("id", "");
  m.kind = root.str("kind", "");
  m.path = root.str("path", "");
  m.command = root.str("command", "");
  m.timeout_ms = static_cast<int>(root.integer("timeout_ms", 400));
  m.enabled = root.boolean("enabled", true);
  m.args = root.string_list("args");
  if (m.id.empty()) m.id = path_stem(path);
  if (m.kind.empty()) m.kind = m.command.empty() ? "native" : "stdio";
  return true;
}

static std::vector<PluginManifest> discover_plugins(const Config& cfg) {
  std::vector<PluginManifest> out;
  auto dirs = cfg.plugins.directories;
  if (dirs.empty()) dirs = default_plugin_directories();
  std::error_code ec;
  for (auto& dir : dirs) {
    fs::path root = fs::u8path(dir);
    if (!fs::exists(root, ec)) continue;
    for (auto it = fs::directory_iterator(root, ec); it != fs::directory_iterator(); it.increment(ec)) {
      if (ec) break;
      auto p = it->path();
      PluginManifest m;
      m.directory = p.parent_path().u8string().empty()
                        ? dir
                        : std::string(p.parent_path().u8string().begin(), p.parent_path().u8string().end());
      auto name = p.filename().u8string();
      std::string fname(name.begin(), name.end());
      auto ext = to_lower_utf8(path_extension(fname));
      if (it->is_directory(ec)) {
        auto yml = path_join(p.u8string().empty() ? dir : std::string(p.u8string().begin(), p.u8string().end()),
                             "plugin.yml");
        auto json = path_join(path_parent(yml), "plugin.json");
        std::string folder(p.u8string().begin(), p.u8string().end());
        if (file_exists(yml) && load_manifest_file(yml, m)) {
          m.directory = folder;
          if (m.path.empty() && m.command.empty()) m.path = folder;
          out.push_back(m);
        } else if (file_exists(json) && load_manifest_file(json, m)) {
          m.directory = folder;
          out.push_back(m);
        }
        continue;
      }
      if (fname == "plugin.yml" || fname == "plugin.json") {
        if (load_manifest_file(p.u8string().empty() ? fname : std::string(p.u8string().begin(), p.u8string().end()),
                               m)) {
          m.directory = dir;
          out.push_back(m);
        }
        continue;
      }
#ifdef _WIN32
      if (ext == ".dll")
#else
#ifdef __APPLE__
      if (ext == ".dylib" || ext == ".so")
#else
      if (ext == ".so")
#endif
#endif
      {
        m.id = path_stem(fname);
        m.kind = "native";
        m.path = std::string(p.u8string().begin(), p.u8string().end());
        m.directory = dir;
        m.timeout_ms = cfg.plugins.timeout_ms;
        out.push_back(m);
      }
    }
  }
  return out;
}

struct NativePlugin {
  PluginManifest manifest;
#ifdef _WIN32
  HMODULE handle{nullptr};
#else
  void* handle{nullptr};
#endif
  wilfred_plugin_query_fn query{nullptr};
  wilfred_plugin_exec_fn exec{nullptr};
  wilfred_plugin_id_fn idfn{nullptr};
};

#ifdef _WIN32
static std::wstring quote_win_arg(const std::string& a) {
  auto w = utf8_to_wide(a);
  if (w.find_first_of(L" \t\"") == std::wstring::npos) return w;
  std::wstring o = L"\"";
  for (wchar_t c : w) {
    if (c == L'"') o += L"\\\"";
    else
      o.push_back(c);
  }
  o.push_back(L'"');
  return o;
}
#endif

static bool looks_like_native_lib(const std::string& p) {
  auto e = to_lower_utf8(path_extension(p));
#ifdef _WIN32
  return e == ".dll";
#elif defined(__APPLE__)
  return e == ".dylib" || e == ".so";
#else
  return e == ".so";
#endif
}

static std::string resolve_native_lib(const PluginManifest& m) {
  std::error_code ec;
  auto as_lib = [&](const std::string& p) -> std::string {
    if (p.empty()) return {};
    fs::path fp = fs::u8path(p);
    if (fs::is_regular_file(fp, ec) && looks_like_native_lib(p)) return p;
    return {};
  };
  if (auto hit = as_lib(m.path); !hit.empty()) return hit;
  if (!m.directory.empty() && !m.path.empty()) {
    if (auto hit = as_lib(path_join(m.directory, m.path)); !hit.empty()) return hit;
  }
  fs::path dir;
  if (!m.path.empty() && fs::is_directory(fs::u8path(m.path), ec))
    dir = fs::u8path(m.path);
  else if (!m.directory.empty())
    dir = fs::u8path(m.directory);
  if (!dir.empty() && fs::is_directory(dir, ec)) {
    for (auto it = fs::directory_iterator(dir, ec); it != fs::directory_iterator(); it.increment(ec)) {
      if (ec || !it->is_regular_file(ec)) continue;
      auto p = std::string(it->path().u8string().begin(), it->path().u8string().end());
      if (looks_like_native_lib(p)) return p;
    }
  }
  return m.path;
}

static std::string resolve_stdio_exe(const PluginManifest& m) {
  std::error_code ec;
  auto exe = m.command.empty() ? m.path : m.command;
  if (exe.empty()) return {};
  if (fs::is_regular_file(fs::u8path(exe), ec)) return exe;
  if (!m.directory.empty()) {
    auto joined = path_join(m.directory, exe);
    if (fs::is_regular_file(fs::u8path(joined), ec)) return joined;
  }
  return exe;
}

static std::string run_stdio_once(const PluginManifest& m, const std::string& request, int timeout_ms) {
  std::string line = request;
  if (line.empty() || line.back() != '\n') line.push_back('\n');

#ifdef _WIN32
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE in_r = nullptr, in_w = nullptr, out_r = nullptr, out_w = nullptr;
  if (!CreatePipe(&in_r, &in_w, &sa, 0)) return {};
  if (!CreatePipe(&out_r, &out_w, &sa, 0)) {
    CloseHandle(in_r);
    CloseHandle(in_w);
    return {};
  }
  SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = in_r;
  si.hStdOutput = out_w;
  si.hStdError = out_w;
  PROCESS_INFORMATION pi{};
  std::wstring cmd = quote_win_arg(resolve_stdio_exe(m));
  for (auto& a : m.args) {
    cmd.push_back(L' ');
    cmd += quote_win_arg(a);
  }
  std::wstring cwd = m.directory.empty() ? std::wstring() : utf8_to_wide(m.directory);
  BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                           cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
  CloseHandle(in_r);
  CloseHandle(out_w);
  if (!ok) {
    CloseHandle(in_w);
    CloseHandle(out_r);
    return {};
  }
  DWORD wr = 0;
  WriteFile(in_w, line.data(), static_cast<DWORD>(line.size()), &wr, nullptr);
  CloseHandle(in_w);
  std::string resp;
  auto deadline = GetTickCount64() + static_cast<ULONGLONG>(std::max(50, timeout_ms));
  char buf[4096];
  for (;;) {
    DWORD avail = 0;
    if (!PeekNamedPipe(out_r, nullptr, 0, nullptr, &avail, nullptr)) break;
    if (avail) {
      DWORD n = 0;
      if (!ReadFile(out_r, buf, sizeof(buf), &n, nullptr) || n == 0) break;
      resp.append(buf, n);
      if (resp.find('\n') != std::string::npos) break;
    } else {
      DWORD st = WaitForSingleObject(pi.hProcess, 10);
      if (st == WAIT_OBJECT_0) {
        DWORD n = 0;
        if (ReadFile(out_r, buf, sizeof(buf), &n, nullptr) && n) resp.append(buf, n);
        break;
      }
      if (GetTickCount64() > deadline) {
        TerminateProcess(pi.hProcess, 1);
        break;
      }
    }
  }
  CloseHandle(out_r);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r')) resp.pop_back();
  return resp;
#else
  int in_pipe[2];
  int out_pipe[2];
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) return {};
  pid_t pid = fork();
  if (pid < 0) {
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    return {};
  }
  if (pid == 0) {
    dup2(in_pipe[0], STDIN_FILENO);
    dup2(out_pipe[1], STDOUT_FILENO);
    dup2(out_pipe[1], STDERR_FILENO);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    if (!m.directory.empty()) chdir(m.directory.c_str());
    std::vector<std::string> argv_store;
    argv_store.push_back(resolve_stdio_exe(m));
    argv_store.insert(argv_store.end(), m.args.begin(), m.args.end());
    std::vector<char*> argv;
    for (auto& a : argv_store) argv.push_back(a.data());
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(in_pipe[0]);
  close(out_pipe[1]);
  auto n = write(in_pipe[1], line.data(), line.size());
  (void)n;
  close(in_pipe[1]);
  std::string resp;
  char buf[4096];
  int flags = fcntl(out_pipe[0], F_GETFL, 0);
  fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);
  auto start = std::chrono::steady_clock::now();
  for (;;) {
    auto got = read(out_pipe[0], buf, sizeof(buf));
    if (got > 0) {
      resp.append(buf, static_cast<std::size_t>(got));
      if (resp.find('\n') != std::string::npos) break;
    } else {
      int st = 0;
      pid_t w = waitpid(pid, &st, WNOHANG);
      if (w == pid) break;
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
                    .count();
      if (ms > timeout_ms) {
        kill(pid, SIGKILL);
        waitpid(pid, &st, 0);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
  }
  close(out_pipe[0]);
  int st = 0;
  waitpid(pid, &st, WNOHANG);
  while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r')) resp.pop_back();
  return resp;
#endif
}

struct PluginHost::Impl {
  std::mutex mu;
  std::vector<PluginManifest> manifests;
  std::vector<NativePlugin> native;
  int timeout_ms{400};

  void unload() {
    for (auto& n : native) {
      if (n.handle) {
#ifdef _WIN32
        FreeLibrary(n.handle);
#else
        dlclose(n.handle);
#endif
      }
    }
    native.clear();
    manifests.clear();
  }
};

PluginHost::PluginHost() : impl_(std::make_unique<Impl>()) {}
PluginHost::~PluginHost() { unload(); }

void PluginHost::unload() {
  if (impl_) impl_->unload();
}

const std::vector<PluginManifest>& PluginHost::manifests() const { return impl_->manifests; }

void PluginHost::load(const Config& cfg) {
  unload();
  if (!cfg.plugins.enabled) return;
  impl_->timeout_ms = cfg.plugins.timeout_ms > 0 ? cfg.plugins.timeout_ms : 400;
  impl_->manifests = discover_plugins(cfg);
  for (auto& m : impl_->manifests) {
    if (!m.enabled) continue;
    if (m.timeout_ms <= 0) m.timeout_ms = impl_->timeout_ms;
    if (to_lower_utf8(m.kind) != "native") continue;
    NativePlugin np;
    np.manifest = m;
    auto lib = resolve_native_lib(m);
    if (!looks_like_native_lib(lib)) {
      log_warn("plugin", "no native library for " + m.id);
      continue;
    }
    np.manifest.path = lib;
#ifdef _WIN32
    np.handle = LoadLibraryW(utf8_to_wide(lib).c_str());
    if (!np.handle) {
      log_warn("plugin", "failed to load " + lib);
      continue;
    }
    auto abi = reinterpret_cast<wilfred_plugin_abi_fn>(GetProcAddress(np.handle, "wilfred_plugin_abi"));
    np.query = reinterpret_cast<wilfred_plugin_query_fn>(GetProcAddress(np.handle, "wilfred_plugin_query"));
    np.exec = reinterpret_cast<wilfred_plugin_exec_fn>(GetProcAddress(np.handle, "wilfred_plugin_exec"));
    np.idfn = reinterpret_cast<wilfred_plugin_id_fn>(GetProcAddress(np.handle, "wilfred_plugin_id"));
#else
    np.handle = dlopen(lib.c_str(), RTLD_NOW);
    if (!np.handle) {
      log_warn("plugin", "failed to load " + lib);
      continue;
    }
    auto abi = reinterpret_cast<wilfred_plugin_abi_fn>(dlsym(np.handle, "wilfred_plugin_abi"));
    np.query = reinterpret_cast<wilfred_plugin_query_fn>(dlsym(np.handle, "wilfred_plugin_query"));
    np.exec = reinterpret_cast<wilfred_plugin_exec_fn>(dlsym(np.handle, "wilfred_plugin_exec"));
    np.idfn = reinterpret_cast<wilfred_plugin_id_fn>(dlsym(np.handle, "wilfred_plugin_id"));
#endif
    if (abi && abi() != WILFRED_PLUGIN_ABI) {
      log_warn("plugin", "incompatible ABI: " + lib);
#ifdef _WIN32
      FreeLibrary(np.handle);
#else
      dlclose(np.handle);
#endif
      continue;
    }
    if (!np.query) {
      log_warn("plugin", "missing wilfred_plugin_query: " + lib);
#ifdef _WIN32
      FreeLibrary(np.handle);
#else
      dlclose(np.handle);
#endif
      continue;
    }
    if (np.idfn) {
      if (const char* id = np.idfn()) np.manifest.id = id;
    }
    impl_->native.push_back(np);
    log_info("plugin", "loaded native " + np.manifest.id);
  }
}

std::vector<SearchResult> PluginHost::query(const std::string& text, const Config& cfg, std::size_t limit) {
  (void)cfg;
  std::lock_guard<std::mutex> lock(impl_->mu);
  std::vector<SearchResult> out;
  if (text.empty()) return out;
  auto req = plugin_query_json(text, limit);
  for (auto& n : impl_->native) {
    try {
      const char* resp = n.query ? n.query(req.c_str()) : nullptr;
      if (!resp) continue;
      auto part = parse_plugin_results_json(resp, n.manifest.id);
      out.insert(out.end(), part.begin(), part.end());
    } catch (...) {
      log_warn("plugin", "native query failed: " + n.manifest.id);
    }
  }
  for (auto& m : impl_->manifests) {
    if (!m.enabled) continue;
    if (to_lower_utf8(m.kind) != "stdio") continue;
    try {
      auto resp = run_stdio_once(m, req, m.timeout_ms);
      if (resp.empty()) continue;
      auto part = parse_plugin_results_json(resp, m.id);
      out.insert(out.end(), part.begin(), part.end());
    } catch (...) {
      log_warn("plugin", "stdio query failed: " + m.id);
    }
  }
  if (out.size() > limit) out.resize(limit);
  return out;
}

bool PluginHost::execute(const SearchResult& r, const std::string& action_id) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto req = std::string("{\"op\":\"exec\",\"action\":\"") + json_escape(action_id) + "\",\"path\":\"" +
             json_escape(r.path) + "\",\"payload\":\"" + json_escape(r.payload) + "\",\"title\":\"" +
             json_escape(r.title) + "\"}";
  for (auto& n : impl_->native) {
    if (n.manifest.id != r.plugin_id) continue;
    if (!n.exec) return false;
    try {
      n.exec(req.c_str());
      return true;
    } catch (...) {
      return false;
    }
  }
  for (auto& m : impl_->manifests) {
    if (m.id != r.plugin_id) continue;
    if (to_lower_utf8(m.kind) != "stdio") continue;
    run_stdio_once(m, req, m.timeout_ms);
    return true;
  }
  return false;
}

}  // namespace wilfred
