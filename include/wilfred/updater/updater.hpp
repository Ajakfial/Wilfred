#pragma once

#include <string>
#include <functional>

namespace wilfred {

// Semantic version parsed from a tag like "v1.2.3". Accepts 1-4 numeric
// components of any width ("v1", "v10.20.30", "v123.4.5.6"), an optional
// leading 'v'/'V', surrounding whitespace, and pre-release/build suffixes
// ("-beta", "+build") which are ignored for comparison.
struct Version {
  int major{0};
  int minor{0};
  int patch{0};
  int tweak{0};

  Version() = default;
  Version(int maj, int min, int pat, int twk = 0) : major(maj), minor(min), patch(pat), tweak(twk) {}

  bool valid() const { return major > 0 || minor > 0 || patch > 0 || tweak > 0; }

  int compare(const Version& o) const {
    if (major != o.major) return major < o.major ? -1 : 1;
    if (minor != o.minor) return minor < o.minor ? -1 : 1;
    if (patch != o.patch) return patch < o.patch ? -1 : 1;
    if (tweak != o.tweak) return tweak < o.tweak ? -1 : 1;
    return 0;
  }

  std::string to_string() const;
};

// Parse "v1.2.3", "v10.20.30", "v1.2.3.4", "1.2", "v1.2.3-beta+build", ...
// into a Version. Returns invalid on failure.
Version parse_version(const std::string& tag);

// Runtime platform descriptor, e.g. "windows-x64", "linux-x64", "macos-arm64".
struct PlatformInfo {
  std::string os;       // "windows", "linux", "macos"
  std::string arch;     // "x64", "arm64", "x86"
  std::string asset_ext; // ".zip" or ".tar.gz"

  std::string id() const { return os + "-" + arch; }
};

PlatformInfo detect_platform();

// The compiled-in version of this binary.
std::string wilfred_version();

// Result of an update check.
struct UpdateCheckResult {
  bool update_available{false};
  std::string current_version;
  std::string latest_version;
  std::string download_url;
  std::string error;
};

// Query GitHub for the latest release and compare against current.
// Returns result with update_available=true if a newer version exists.
UpdateCheckResult check_for_update(const std::string& repo_url,
                                   const std::string& current_ver,
                                   int timeout_ms = 10000);

// Progress callback: (bytes_downloaded, total_bytes, user_data)
using ProgressCallback = std::function<void(long long, long long, void*)>;

// Download a URL to a local file. Returns true on success.
bool download_file(const std::string& url, const std::string& dest_path,
                   ProgressCallback progress = nullptr, void* user_data = nullptr,
                   int timeout_ms = 30000, std::string* error = nullptr);

// Extract a .zip or .tar.gz archive into dest_dir. Returns true on success.
bool extract_archive(const std::string& archive_path, const std::string& dest_dir,
                     std::string* error = nullptr);

// Platform-specific extractors
bool extract_zip(const std::string& archive_path, const std::string& dest_dir,
                 std::string* error = nullptr);
bool extract_tarball(const std::string& archive_path, const std::string& dest_dir,
                     std::string* error = nullptr);

// Full update flow: check, download, extract, and schedule restart.
// If dry_run is true, only checks and reports without downloading.
// Returns true if an update was successfully staged (or available in dry_run).
bool perform_update(bool dry_run, std::string* error);

// Called on daemon startup to check for updates in the background.
// Safe to call multiple times; subsequent calls are no-ops if already checked.
void startup_update_check();

// CLI command handler: "wilfred update [--check] [--yes]"
int run_update_command(bool check_only, bool auto_yes);

}  // namespace wilfred
