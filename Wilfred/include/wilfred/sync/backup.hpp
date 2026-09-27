#pragma once

#include "wilfred/config/config.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct BackupEntry {
  std::string name;
  std::string data;
};

bool pack_backup_archive(const std::vector<BackupEntry>& files, std::string& out, std::string& err);
bool unpack_backup_archive(const std::string& blob, std::vector<BackupEntry>& files, std::string& err);

bool create_backup(const Config& cfg, const std::string& dest_path, bool include_index,
                   std::string& err);
bool restore_backup(const std::string& src_path, std::string& err);

std::string default_backup_path();

bool sync_push(const Config& cfg, std::string& err);
bool sync_pull(const Config& cfg, std::string& err);

}  // namespace wilfred
