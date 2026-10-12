#include "spoke/platform.h"

namespace spoke {

std::string platformName() {
#if defined(__APPLE__) && defined(__MACH__)
  return "macos";
#elif defined(__linux__)
  return "linux";
#elif defined(_WIN32)
  return "windows";
#else
  return "unknown";
#endif
}

std::string archName() {
#if defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#else
  return "unknown";
#endif
}

std::string compilerName() {
#if defined(_MSC_VER)
  return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
  return "Clang " + std::to_string(__clang_major__) + "." +
         std::to_string(__clang_minor__) +
#if defined(__apple_build_version__)
         " (Apple)";
#else
         "";
#endif
#elif defined(__GNUC__)
  return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
  return "unknown compiler";
#endif
}

std::string platformPretty() { return platformName() + " (" + archName() + ", " + compilerName() + ")"; }

std::string nativeLibraryName() {
#if defined(_WIN32)
  return "spoke.dll";
#elif defined(__APPLE__) && defined(__MACH__)
  return "libspoke.dylib";
#else
  return "libspoke.so";
#endif
}

char pathSeparator() {
#if defined(_WIN32)
  return '\\';
#else
  return '/';
#endif
}

}  // namespace spoke
