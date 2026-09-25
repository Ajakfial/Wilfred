#include "wilfred/fs/classify.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace wilfred {
namespace fs = std::filesystem;

FileKind classify_extension(std::string_view ext) {
  auto e = to_lower_utf8(ext);
  if (e.empty()) return FileKind::File;
  static const char* docs[] = {".pdf", ".doc",  ".docx", ".odt", ".rtf", ".txt", ".md",
                               ".xls", ".xlsx", ".ppt",  ".pptx", ".csv", nullptr};
  static const char* imgs[] = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp",
                               ".tif", ".tiff", ".ico",  ".svg",  ".heic", nullptr};
  static const char* vids[] = {".mp4", ".mkv", ".avi", ".mov", ".webm", ".wmv", ".m4v", nullptr};
  static const char* aud[] = {".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac", ".wma", nullptr};
  static const char* arc[] = {".zip", ".7z",  ".rar", ".tar", ".gz",  ".bz2",
                              ".xz",  ".iso", ".cab", nullptr};
  static const char* src[] = {".c",    ".h",   ".cpp", ".hpp", ".cc",  ".cxx", ".rs",  ".py",
                              ".js",   ".ts",  ".jsx", ".tsx", ".go",  ".java", ".kt", ".cs",
                              ".rb",   ".php", ".swift", ".m",  ".mm", ".sh",  ".ps1", ".lua",
                              ".r",    ".sql", ".html", ".css", ".scss", ".vue", ".json", nullptr};
  static const char* cfg[] = {".yml", ".yaml", ".toml", ".ini", ".cfg", ".conf",
                              ".xml", ".plist", ".env", nullptr};
  static const char* exe[] = {".exe", ".bat", ".cmd", ".com", ".msi", ".appimage", nullptr};
  auto has = [&](const char** a) {
    for (auto* p = a; *p; ++p)
      if (e == *p) return true;
    return false;
  };
  if (has(docs)) return FileKind::Document;
  if (has(imgs)) return FileKind::Image;
  if (has(vids)) return FileKind::Video;
  if (has(aud)) return FileKind::Audio;
  if (has(arc)) return FileKind::Archive;
  if (has(src)) return FileKind::Source;
  if (has(cfg)) return FileKind::Config;
  if (has(exe)) return FileKind::Executable;
  if (e == ".lnk" || e == ".desktop" || e == ".url") return FileKind::Shortcut;
  if (e == ".app") return FileKind::Application;
  return FileKind::File;
}

FileKind classify_path(std::string_view path, bool is_dir, bool is_exec) {
  auto name = path_filename(path);
  auto ext = path_extension(path);
#ifdef __APPLE__
  if (is_dir && ext == ".app") return FileKind::Application;
#endif
  if (is_dir) return FileKind::Directory;
  auto k = classify_extension(ext);
  if (k == FileKind::File && is_exec) return FileKind::Executable;
  if (name == "chrome" || name == "firefox" || name == "msedge" || name == "safari")
    return FileKind::Application;
  return k;
}

bool looks_hidden(std::string_view name, std::string_view path) {
  if (!name.empty() && name[0] == '.') return true;
#ifdef _WIN32
  (void)path;
#else
  (void)path;
#endif
  return false;
}

bool looks_system_name(std::string_view name) {
  auto n = to_lower_utf8(name);
  return n == "system32" || n == "syswow64" || n == "windows" || n == "system" || n == "proc" ||
         n == "sys";
}

FileStat stat_path(const std::string& path) {
  FileStat st;
  std::error_code ec;
  auto p = fs::u8path(path);
  auto s = fs::symlink_status(p, ec);
  if (ec) return st;
  st.exists = true;
  st.is_symlink = fs::is_symlink(s);
  auto follow = fs::status(p, ec);
  auto use = ec ? s : follow;
  st.is_dir = fs::is_directory(use);
  st.is_file = fs::is_regular_file(use);
  if (st.is_file) {
    auto sz = fs::file_size(p, ec);
    if (!ec) st.size = sz;
  }
  auto ftime = fs::last_write_time(p, ec);
  if (!ec) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
    st.mtime = static_cast<std::int64_t>(secs);
    st.ctime = st.mtime;
    st.atime = st.mtime;
  }
  auto name = path_filename(path);
  st.is_hidden = looks_hidden(name, path);

#ifdef _WIN32
  auto w = utf8_to_wide(path);
  WIN32_FILE_ATTRIBUTE_DATA fad{};
  if (GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &fad)) {
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) st.is_hidden = true;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) st.is_system = true;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
    }
    ULARGE_INTEGER c, m, a;
    c.LowPart = fad.ftCreationTime.dwLowDateTime;
    c.HighPart = fad.ftCreationTime.dwHighDateTime;
    m.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    m.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    a.LowPart = fad.ftLastAccessTime.dwLowDateTime;
    a.HighPart = fad.ftLastAccessTime.dwHighDateTime;
    st.ctime = file_time_to_unix(c.QuadPart);
    st.mtime = file_time_to_unix(m.QuadPart);
    st.atime = file_time_to_unix(a.QuadPart);
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) st.is_dir = true;
  }
  auto ext = path_extension(path);
  st.is_exec = ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com";
#else
  struct stat buf {};
  if (::lstat(path.c_str(), &buf) == 0) {
    st.mode = static_cast<std::uint32_t>(buf.st_mode);
    st.ctime = static_cast<std::int64_t>(buf.st_ctime);
    st.mtime = static_cast<std::int64_t>(buf.st_mtime);
    st.atime = static_cast<std::int64_t>(buf.st_atime);
    st.is_exec = (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0 && !st.is_dir;
    st.size = static_cast<std::uint64_t>(buf.st_size);
  }
#endif
  return st;
}

}  // namespace wilfred
