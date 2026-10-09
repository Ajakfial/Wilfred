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
bool unpack_backup_archive(const std::string& blob, std::vector<BackupEntry>& files,
                           std::string& err);

// Encrypted variant (dependency-free SHA-256 KDF + keystream XOR).
// Encrypted blobs use magic "WILFEK1" and embed salt/iterations/crc.
bool pack_backup_archive_encrypted(const std::vector<BackupEntry>& files,
                                   const std::string& password, std::string& out, std::string& err);
bool unpack_backup_archive_auto(const std::string& blob, const std::string& password,
                                std::vector<BackupEntry>& files, std::string& err);
bool restore_backup_with_password(const std::string& src_path, const std::string& password,
                                  std::string& err);
bool resolve_sync_password(const Config& cfg, std::string& out, std::string& err);

bool create_backup(const Config& cfg, const std::string& dest_path, bool include_index,
                   std::string& err);
bool restore_backup(const std::string& src_path, std::string& err);

std::string default_backup_path();

bool sync_push(const Config& cfg, std::string& err);
bool sync_pull(const Config& cfg, std::string& err);

}  // namespace wilfred
