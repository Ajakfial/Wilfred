#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace wilfred {

// Portable file helpers behind the rich per-file result actions
// (see search/actions.hpp). OS-specific pieces (open-with lookup,
// launching apps/terminals/editors) live in platform/native.hpp.

// Copy-path flavors. Empty string when the flavor does not apply.
std::string path_to_posix(const std::string& path);
std::string path_to_file_uri(const std::string& path);
std::string path_to_wsl(const std::string& path);

// SHA-256 of a file's bytes, streamed in chunks. False on missing
// file, directory, or read error.
bool sha256_file(const std::string& path, std::string& out_hex, std::string& error);

// Create a stored (uncompressed) .zip of the given files/directories.
// False when nothing could be written.
bool zip_paths_to(const std::vector<std::string>& sources, const std::string& zip_path,
                  std::string& error);

// Unique sibling output names ("Report.zip", "Report 2.zip", ...).
std::string unique_sibling_path(const std::string& dir, const std::string& stem,
                                const std::string& ext);

// "New file.txt" / "New folder" style creation inside dir.
bool create_new_file_here(const std::string& dir, std::string& out_path, std::string& error);
bool create_new_folder_here(const std::string& dir, std::string& out_path, std::string& error);

// Bulk rename every regular file directly inside dir using pattern with
// {n} (1-based counter), {name} (original stem), {ext} (original extension
// incl. dot). Returns renamed paths in renamed_out.
bool bulk_rename_in_dir(const std::string& dir, const std::string& pattern,
                        std::vector<std::string>& renamed_out, std::string& error);
// Move files/dirs into dest_dir (created when missing). Returns moved paths.
bool move_paths_to(const std::vector<std::string>& sources, const std::string& dest_dir,
                   std::vector<std::string>& moved_out, std::string& error);
// Create a file from a built-in template: empty|txt|md|python|py|cpp|h|
// html|json|gitignore|todo. new_name may omit the extension (added).
bool create_from_template(const std::string& dir, const std::string& template_name,
                          const std::string& new_name, std::string& out_path, std::string& error);

bool fs_is_directory(const std::string& path);
bool fs_exists(const std::string& path);

}  // namespace wilfred
