#pragma once
#include <string>

namespace spoke {

/// Compile-time OS / architecture / toolchain identification.
/// Used by the CLI (`spoke_cli info`), packaging scripts and docs so every
/// platform reports itself honestly. No OS-specific APIs live here -- only
/// preprocessor detection, so this header compiles everywhere.

std::string platformName();       ///< "windows" | "linux" | "macos" | "unknown"
std::string platformPretty();     ///< "Windows (x86_64, MSVC)" etc.
std::string archName();           ///< "x86_64" | "arm64" | "unknown"
std::string compilerName();       ///< "MSVC 19.xx" | "GCC 13.x" | "Clang 17.x" ...
std::string nativeLibraryName();  ///< "spoke.dll" | "libspoke.so" | "libspoke.dylib"
char pathSeparator();             ///< '\\' on Windows, '/' elsewhere

}  // namespace spoke
