#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/index/store.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

struct SearchFilter {
  std::vector<std::string> extensions;
  std::vector<FileKind> kinds;
  std::vector<std::string> in_dirs;
  std::vector<std::string> scope_names;
  std::vector<std::string> name_contains;
  std::vector<std::string> phrases;
  std::optional<std::uint64_t> min_size;
  std::optional<std::uint64_t> max_size;
  std::optional<std::int64_t> min_mtime;
  std::optional<std::int64_t> max_mtime;
  std::optional<bool> hidden;
  std::optional<bool> system;
  std::optional<bool> directories_only;
  std::optional<bool> files_only;
  std::optional<bool> apps_only;
  std::vector<std::string> content_contains;
  std::optional<bool> content_only;
};

bool record_matches_filter(const SearchFilter& f, const IndexStore& store,
                           const IndexRecord& rec);
SearchFilter parse_filter_clauses(std::string& query_inout);
void apply_named_scopes(SearchFilter& f, const Config& cfg);
std::uint64_t parse_size_token(std::string_view t, bool& ok);
std::int64_t parse_time_token(std::string_view t, bool& ok);

}  // namespace wilfred
