#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

std::string path_join(std::string_view a, std::string_view b);
std::string path_normalize(std::string_view p);
std::string path_filename(std::string_view p);
std::string path_stem(std::string_view p);
std::string path_extension(std::string_view p);
std::string path_parent(std::string_view p);
std::string path_lower(std::string_view p);
bool path_is_absolute(std::string_view p);
char path_separator();

std::string home_directory();
std::string config_directory();
std::string data_directory();
std::string default_config_path();
std::string default_index_path();
std::string default_history_path();
std::string default_log_path();
std::string ipc_endpoint();

std::vector<std::string> default_index_roots();
std::vector<std::string> default_system_directories();

bool path_equals(std::string_view a, std::string_view b);
bool path_is_under(std::string_view path, std::string_view root);
std::string native_path(std::string_view utf8);

}  // namespace wilfred
