#include "wilfred/updater/updater.hpp"

#include "wilfred/platform/platform.hpp"

#include <cctype>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <unistd.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace wilfred {

std::string Version::to_string() const {
  std::ostringstream oss;
  oss << major << '.' << minor << '.' << patch;
  return oss.str();
}

Version parse_version(const std::string& tag) {
  std::string s = tag;
  // Strip leading 'v' or 'V'
  if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);

  // Strip any pre-release suffix (e.g. "-beta")
  auto dash = s.find('-');
  if (dash != std::string::npos) s = s.substr(0, dash);

  Version v;
  std::istringstream iss(s);
  std::string part;
  int* fields[3] = {&v.major, &v.minor, &v.patch};
  int idx = 0;

  while (std::getline(iss, part, '.') && idx < 3) {
    // Trim whitespace
    while (!part.empty() && std::isspace(static_cast<unsigned char>(part.back())))
      part.pop_back();
    if (part.empty()) return Version();
    // Must be all digits
    for (char c : part) {
      if (!std::isdigit(static_cast<unsigned char>(c))) return Version();
    }
    try {
      *fields[idx] = std::stoi(part);
    } catch (...) {
      return Version();
    }
    ++idx;
  }

  if (idx == 0) return Version();
  return v;
}

PlatformInfo detect_platform() {
  PlatformInfo info;
  info.os = platform_name();

#if defined(_WIN32)
  SYSTEM_INFO si;
  GetNativeSystemInfo(&si);
  switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      info.arch = "x64";
      break;
    case PROCESSOR_ARCHITECTURE_ARM64:
      info.arch = "arm64";
      break;
    case PROCESSOR_ARCHITECTURE_INTEL:
      info.arch = "x86";
      break;
    default:
      info.arch = "x64";
      break;
  }
  info.asset_ext = ".zip";
#elif defined(__APPLE__)
  // Detect ARM64 vs x86_64 on Apple Silicon vs Intel
  int is_arm = 0;
  size_t sz = sizeof(is_arm);
  sysctlbyname("hw.optional.arm64", &is_arm, &sz, nullptr, 0);
  if (is_arm) {
    info.arch = "arm64";
  } else {
    // Check if running under Rosetta
    int translated = 0;
    size_t tsz = sizeof(translated);
    sysctlbyname("sysctl.proc_translated", &translated, &tsz, nullptr, 0);
    if (translated) {
      info.arch = "arm64";
    } else {
      info.arch = "x64";
    }
  }
  info.asset_ext = ".tar.gz";
#else
  struct utsname buf;
  if (uname(&buf) == 0) {
    std::string machine(buf.machine);
    if (machine == "x86_64" || machine == "amd64") {
      info.arch = "x64";
    } else if (machine == "aarch64" || machine == "arm64") {
      info.arch = "arm64";
    } else if (machine == "i386" || machine == "i686") {
      info.arch = "x86";
    } else {
      info.arch = "x64";
    }
  } else {
    info.arch = "x64";
  }
  info.asset_ext = ".tar.gz";
#endif

  return info;
}

std::string wilfred_version() {
#ifdef WILFRED_VERSION_STRING
  return WILFRED_VERSION_STRING;
#else
  return "0.0.0";
#endif
}

}  // namespace wilfred
