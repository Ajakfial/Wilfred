#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace wilfred {
#ifdef _WIN32

bool native_launch(const std::string& path) {
  auto w = utf8_to_wide(path);
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

bool native_reveal(const std::string& path) {
  auto w = utf8_to_wide(path);
  std::wstring args = L"/select,\"" + w + L"\"";
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

bool native_open_url(const std::string& url) {
  auto w = utf8_to_wide(url);
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

#else
bool native_launch(const std::string&);
#endif
}  // namespace wilfred
