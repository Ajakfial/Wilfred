#include "wilfred/index/record.hpp"

#include "wilfred/core/utf8.hpp"

namespace wilfred {

std::string_view kind_name(FileKind k) {
  switch (k) {
    case FileKind::File:
      return "file";
    case FileKind::Directory:
      return "directory";
    case FileKind::Application:
      return "application";
    case FileKind::Executable:
      return "executable";
    case FileKind::Document:
      return "document";
    case FileKind::Image:
      return "image";
    case FileKind::Video:
      return "video";
    case FileKind::Audio:
      return "audio";
    case FileKind::Archive:
      return "archive";
    case FileKind::Source:
      return "source";
    case FileKind::Config:
      return "config";
    case FileKind::Shortcut:
      return "shortcut";
    case FileKind::BrowserData:
      return "browser";
    default:
      return "unknown";
  }
}

FileKind kind_from_name(std::string_view n) {
  auto s = to_lower_utf8(n);
  if (s == "file" || s == "files") return FileKind::File;
  if (s == "dir" || s == "directory" || s == "folder" || s == "folders")
    return FileKind::Directory;
  if (s == "app" || s == "application" || s == "applications") return FileKind::Application;
  if (s == "exe" || s == "executable" || s == "bin") return FileKind::Executable;
  if (s == "doc" || s == "document" || s == "documents") return FileKind::Document;
  if (s == "image" || s == "images" || s == "img" || s == "photo") return FileKind::Image;
  if (s == "video" || s == "videos" || s == "movie") return FileKind::Video;
  if (s == "audio" || s == "music" || s == "sound") return FileKind::Audio;
  if (s == "archive" || s == "zip") return FileKind::Archive;
  if (s == "source" || s == "code") return FileKind::Source;
  if (s == "config" || s == "cfg") return FileKind::Config;
  if (s == "shortcut" || s == "link") return FileKind::Shortcut;
  if (s == "browser") return FileKind::BrowserData;
  return FileKind::Unknown;
}

}  // namespace wilfred
