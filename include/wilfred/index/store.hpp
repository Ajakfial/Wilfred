#pragma once

#include "wilfred/index/record.hpp"
#include "wilfred/index/string_pool.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {

class IndexStore {
public:
  bool load(const std::string& snapshot_path);
  bool save(const std::string& snapshot_path) const;

  std::uint32_t upsert(IndexRecord rec, std::string_view path);
  void add_content_tokens(std::uint32_t id, const std::vector<std::string>& tokens);
  bool has_content_tokens(std::uint32_t id) const;
  int content_token_hits(std::uint32_t id, const std::vector<std::string>& tokens) const;
  bool content_covers_tokens(std::uint32_t id, const std::vector<std::string>& tokens) const;
  bool remove_path(std::string_view path);
  bool rename_path(std::string_view from, std::string_view to);
  const IndexRecord* get(std::uint32_t id) const;
  const IndexRecord* by_path(std::string_view path) const;
  std::uint32_t path_id(std::string_view path) const;

  StringPool& pool() { return pool_; }
  const StringPool& pool() const { return pool_; }

  const std::vector<IndexRecord>& records() const { return records_; }
  std::size_t live_count() const { return live_count_; }
  std::uint32_t next_id() const { return next_id_; }

  const std::vector<std::uint32_t>& posting(std::uint32_t token_id) const;
  const std::vector<std::uint32_t>& trigram(std::uint32_t tri_id) const;
  const std::vector<std::uint32_t>& prefix_index() const { return sorted_by_name_; }
  const std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>& ext_index() const {
    return ext_postings_;
  }

  void rebuild_secondary();
  void clear();

  std::recursive_mutex& mutex() { return mu_; }

private:
  void clear_unlocked();
  void rebuild_secondary_unlocked();
  void index_record_locked(const IndexRecord& rec);
  void unindex_record_locked(const IndexRecord& rec);
  void add_posting(std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>& m,
                   std::uint32_t key, std::uint32_t id);
  void remove_posting(std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>& m,
                      std::uint32_t key, std::uint32_t id);

  mutable std::recursive_mutex mu_;
  StringPool pool_;
  std::vector<IndexRecord> records_;
  std::vector<std::uint8_t> live_;
  std::unordered_map<std::uint32_t, std::uint32_t> path_to_id_;
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> token_postings_;
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> tri_postings_;
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> ext_postings_;
  std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> extra_tokens_;
  std::vector<std::uint32_t> sorted_by_name_;
  std::uint32_t next_id_{1};
  std::size_t live_count_{0};
  static const std::vector<std::uint32_t> kEmpty;
};

}  // namespace wilfred
