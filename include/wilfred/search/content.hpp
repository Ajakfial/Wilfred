#pragma once

#include "wilfred/index/record.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

bool content_indexable(FileKind kind, std::string_view path);
std::vector<std::string> extract_content_tokens(const std::string& text, int max_tokens);
bool looks_like_text_extension(std::string_view ext);

}  // namespace wilfred
