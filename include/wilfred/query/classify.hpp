#pragma once

#include <string>
#include <string_view>

namespace wilfred {

enum class QueryKind {
  Empty,
  FileSearch,
  AppLaunch,
  FolderSearch,
  Math,
  Url,
  WebSearch,
  SystemAction,
  FilteredSearch,
  Alias,
  Command,
};

struct QueryClass {
  QueryKind kind{QueryKind::FileSearch};
  std::string text;
  std::string remainder;
  bool confident{false};
};

QueryClass classify_query(std::string_view raw);
bool looks_like_url(std::string_view s);
bool looks_like_math(std::string_view s);

}  // namespace wilfred
