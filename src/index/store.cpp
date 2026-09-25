#include "wilfred/index/store.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cstring>

namespace wilfred {

const std::vector<std::uint32_t> IndexStore::kEmpty{};

static void write_u32(std::vector<std::uint8_t>& o, std::uint32_t v) {
  o.push_back(static_cast<std::uint8_t>(v));
  o.push_back(static_cast<std::uint8_t>(v >> 8));
  o.push_back(static_cast<std::uint8_t>(v >> 16));
  o.push_back(static_cast<std::uint8_t>(v >> 24));
}
static void write_u64(std::vector<std::uint8_t>& o, std::uint64_t v) {
  write_u32(o, static_cast<std::uint32_t>(v));
  write_u32(o, static_cast<std::uint32_t>(v >> 32));
}
static std::uint32_t rd32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
static std::uint64_t rd64(const std::uint8_t* p) {
  return rd32(p) | (static_cast<std::uint64_t>(rd32(p + 4)) << 32);
}

void IndexStore::add_posting(std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>& m,
                             std::uint32_t key, std::uint32_t id) {
  auto& v = m[key];
  if (v.empty() || v.back() != id) v.push_back(id);
}

void IndexStore::remove_posting(std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>& m,
                                std::uint32_t key, std::uint32_t id) {
  auto it = m.find(key);
  if (it == m.end()) return;
  auto& v = it->second;
  v.erase(std::remove(v.begin(), v.end(), id), v.end());
  if (v.empty()) m.erase(it);
}

void IndexStore::index_record_locked(const IndexRecord& rec) {
  auto name = std::string(pool_.get(rec.name_id));
  auto folded = fold_search(name);
  for (auto& tok : tokenize_name(name)) {
    auto tid = pool_.intern(tok);
    add_posting(token_postings_, tid, rec.id);
  }
  auto ac = acronym_of(name);
  if (ac.size() >= 2) add_posting(token_postings_, pool_.intern(ac), rec.id);
  for (auto& tri : trigrams(folded)) add_posting(tri_postings_, pool_.intern(tri), rec.id);
  if (rec.ext_id != StringPool::kInvalid) add_posting(ext_postings_, rec.ext_id, rec.id);
  auto path = std::string(pool_.get(rec.path_id));
  std::string comp;
  auto flush_comp = [&] {
    if (comp.empty()) return;
    if (!(comp.size() == 2 && comp[1] == ':')) {
      auto cf = fold_search(comp);
      add_posting(token_postings_, pool_.intern(cf), rec.id);
      for (auto& tok : tokenize_name(comp)) add_posting(token_postings_, pool_.intern(tok), rec.id);
    }
    comp.clear();
  };
  for (char c : path) {
    if (c == '/' || c == '\\')
      flush_comp();
    else
      comp.push_back(c);
  }
  flush_comp();
  auto extra = extra_tokens_.find(rec.id);
  if (extra != extra_tokens_.end()) {
    for (auto tid : extra->second) add_posting(token_postings_, tid, rec.id);
  }
  path_to_id_[rec.path_id] = rec.id;
}

void IndexStore::unindex_record_locked(const IndexRecord& rec) {
  auto name = std::string(pool_.get(rec.name_id));
  auto folded = fold_search(name);
  for (auto& tok : tokenize_name(name)) {
    auto tid = pool_.find(tok);
    if (tid != StringPool::kInvalid) remove_posting(token_postings_, tid, rec.id);
  }
  auto ac = acronym_of(name);
  if (ac.size() >= 2) {
    auto tid = pool_.find(ac);
    if (tid != StringPool::kInvalid) remove_posting(token_postings_, tid, rec.id);
  }
  for (auto& tri : trigrams(folded)) {
    auto tid = pool_.find(tri);
    if (tid != StringPool::kInvalid) remove_posting(tri_postings_, tid, rec.id);
  }
  if (rec.ext_id != StringPool::kInvalid) remove_posting(ext_postings_, rec.ext_id, rec.id);
  auto path = std::string(pool_.get(rec.path_id));
  std::string comp;
  auto flush_comp = [&] {
    if (comp.empty()) return;
    if (!(comp.size() == 2 && comp[1] == ':')) {
      auto cf = fold_search(comp);
      auto tid = pool_.find(cf);
      if (tid != StringPool::kInvalid) remove_posting(token_postings_, tid, rec.id);
      for (auto& tok : tokenize_name(comp)) {
        auto id = pool_.find(tok);
        if (id != StringPool::kInvalid) remove_posting(token_postings_, id, rec.id);
      }
    }
    comp.clear();
  };
  for (char c : path) {
    if (c == '/' || c == '\\')
      flush_comp();
    else
      comp.push_back(c);
  }
  flush_comp();
  auto extra = extra_tokens_.find(rec.id);
  if (extra != extra_tokens_.end()) {
    for (auto tid : extra->second) remove_posting(token_postings_, tid, rec.id);
  }
  path_to_id_.erase(rec.path_id);
}

std::uint32_t IndexStore::upsert(IndexRecord rec, std::string_view path) {
  std::lock_guard<std::mutex> lock(mu_);
  auto path_id = pool_.intern(path);
  auto name = std::string(path);
  auto slash = name.find_last_of("/\\");
  std::string fname = slash == std::string::npos ? name : name.substr(slash + 1);
  rec.path_id = path_id;
  rec.name_id = pool_.intern(fname);
  auto parent = slash == std::string::npos ? std::string() : name.substr(0, slash);
  rec.parent_id = parent.empty() ? StringPool::kInvalid : pool_.intern(parent);
  auto dot = fname.find_last_of('.');
  std::string ext;
  if (dot != std::string::npos && dot != 0) {
    ext = fold_search(fname.substr(dot));
    rec.ext_id = pool_.intern(ext);
  } else {
    rec.ext_id = pool_.intern("");
  }

  auto it = path_to_id_.find(path_id);
  if (it != path_to_id_.end()) {
    rec.id = it->second;
    if (rec.id < records_.size()) {
      unindex_record_locked(records_[rec.id]);
      extra_tokens_.erase(rec.id);
      records_[rec.id] = rec;
      live_[rec.id] = 1;
      index_record_locked(rec);
      return rec.id;
    }
  }
  rec.id = next_id_++;
  if (records_.size() <= rec.id) {
    records_.resize(rec.id + 1);
    live_.resize(rec.id + 1);
  }
  records_[rec.id] = rec;
  live_[rec.id] = 1;
  ++live_count_;
  index_record_locked(rec);
  return rec.id;
}

void IndexStore::add_content_tokens(std::uint32_t id, const std::vector<std::string>& tokens) {
  std::lock_guard<std::mutex> lock(mu_);
  if (id >= records_.size() || id >= live_.size() || !live_[id]) return;
  auto& rec = records_[id];
  unindex_record_locked(rec);
  auto& extra = extra_tokens_[id];
  extra.clear();
  for (auto& tok : tokens) {
    if (tok.size() < 3) continue;
    extra.push_back(pool_.intern(tok));
  }
  index_record_locked(rec);
}

bool IndexStore::remove_path(std::string_view path) {
  std::lock_guard<std::mutex> lock(mu_);
  auto pid = pool_.find(std::string(path));
  if (pid == StringPool::kInvalid) return false;
  auto it = path_to_id_.find(pid);
  if (it == path_to_id_.end()) return false;
  auto id = it->second;
  if (id >= live_.size() || !live_[id]) return false;
  unindex_record_locked(records_[id]);
  extra_tokens_.erase(id);
  live_[id] = 0;
  if (live_count_) --live_count_;
  return true;
}

bool IndexStore::rename_path(std::string_view from, std::string_view to) {
  IndexRecord copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto pid = pool_.find(std::string(from));
    if (pid == StringPool::kInvalid) return false;
    auto it = path_to_id_.find(pid);
    if (it == path_to_id_.end()) return false;
    copy = records_[it->second];
  }
  remove_path(from);
  upsert(copy, to);
  return true;
}

const IndexRecord* IndexStore::get(std::uint32_t id) const {
  if (id >= records_.size() || id >= live_.size() || !live_[id]) return nullptr;
  return &records_[id];
}

const IndexRecord* IndexStore::by_path(std::string_view path) const {
  auto pid = pool_.find(std::string(path));
  if (pid == StringPool::kInvalid) return nullptr;
  auto it = path_to_id_.find(pid);
  if (it == path_to_id_.end()) return nullptr;
  return get(it->second);
}

std::uint32_t IndexStore::path_id(std::string_view path) const { return pool_.find(std::string(path)); }

const std::vector<std::uint32_t>& IndexStore::posting(std::uint32_t token_id) const {
  auto it = token_postings_.find(token_id);
  return it == token_postings_.end() ? kEmpty : it->second;
}

const std::vector<std::uint32_t>& IndexStore::trigram(std::uint32_t tri_id) const {
  auto it = tri_postings_.find(tri_id);
  return it == tri_postings_.end() ? kEmpty : it->second;
}

void IndexStore::rebuild_secondary_unlocked() {
  token_postings_.clear();
  tri_postings_.clear();
  ext_postings_.clear();
  path_to_id_.clear();
  sorted_by_name_.clear();
  live_count_ = 0;
  for (std::uint32_t id = 1; id < records_.size(); ++id) {
    if (id >= live_.size() || !live_[id]) continue;
    ++live_count_;
    index_record_locked(records_[id]);
    sorted_by_name_.push_back(id);
  }
  std::sort(sorted_by_name_.begin(), sorted_by_name_.end(), [&](std::uint32_t a, std::uint32_t b) {
    return pool_.get(records_[a].name_id) < pool_.get(records_[b].name_id);
  });
  next_id_ = std::max<std::uint32_t>(next_id_, static_cast<std::uint32_t>(records_.size()));
}

void IndexStore::rebuild_secondary() {
  std::lock_guard<std::mutex> lock(mu_);
  rebuild_secondary_unlocked();
}

void IndexStore::clear_unlocked() {
  pool_.clear();
  records_.clear();
  live_.clear();
  path_to_id_.clear();
  token_postings_.clear();
  tri_postings_.clear();
  ext_postings_.clear();
  extra_tokens_.clear();
  sorted_by_name_.clear();
  next_id_ = 1;
  live_count_ = 0;
}

void IndexStore::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  clear_unlocked();
}

bool IndexStore::save(const std::string& snapshot_path) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::uint8_t> buf;
  buf.reserve(64 + pool_.bytes() + records_.size() * 80);
  buf.insert(buf.end(), {'W', 'I', 'L', 'F'});
  write_u32(buf, 1);  // version
  write_u32(buf, next_id_);
  write_u32(buf, static_cast<std::uint32_t>(records_.size()));

  std::vector<std::uint8_t> poolb;
  pool_.serialize(poolb);
  write_u32(buf, static_cast<std::uint32_t>(poolb.size()));
  buf.insert(buf.end(), poolb.begin(), poolb.end());

  write_u32(buf, static_cast<std::uint32_t>(live_.size()));
  buf.insert(buf.end(), live_.begin(), live_.end());

  for (std::size_t i = 0; i < records_.size(); ++i) {
    const auto& r = records_[i];
    write_u32(buf, r.id);
    write_u32(buf, r.name_id);
    write_u32(buf, r.path_id);
    write_u32(buf, r.parent_id);
    write_u32(buf, r.ext_id);
    write_u64(buf, r.size);
    write_u64(buf, static_cast<std::uint64_t>(r.ctime));
    write_u64(buf, static_cast<std::uint64_t>(r.mtime));
    write_u64(buf, static_cast<std::uint64_t>(r.atime));
    write_u32(buf, static_cast<std::uint32_t>(r.flags));
    write_u32(buf, r.volume_id);
    write_u32(buf, static_cast<std::uint32_t>(r.kind));
    write_u32(buf, r.mode);
  }
  auto c = crc32(buf.data(), buf.size());
  write_u32(buf, c);
  return write_file_atomic(snapshot_path, buf.data(), buf.size());
}

bool IndexStore::load(const std::string& snapshot_path) {
  MappedFile mf;
  if (!mf.open_read(snapshot_path) || mf.size() < 24) return false;
  auto* p = mf.data();
  if (p[0] != 'W' || p[1] != 'I' || p[2] != 'L' || p[3] != 'F') return false;
  auto crc_stored = rd32(p + mf.size() - 4);
  auto crc_calc = crc32(p, mf.size() - 4);
  if (crc_stored != crc_calc) return false;
  std::uint32_t ver = rd32(p + 4);
  if (ver != 1) return false;
  std::lock_guard<std::mutex> lock(mu_);
  clear_unlocked();
  next_id_ = rd32(p + 8);
  std::uint32_t nrec = rd32(p + 12);
  std::uint32_t pooln = rd32(p + 16);
  std::size_t off = 20;
  if (off + pooln + 4 > mf.size()) return false;
  if (!pool_.deserialize(p + off, pooln)) return false;
  off += pooln;
  std::uint32_t nlive = rd32(p + off);
  off += 4;
  if (off + nlive > mf.size() - 4) return false;
  live_.assign(p + off, p + off + nlive);
  off += nlive;
  records_.assign(nrec, {});
  constexpr std::size_t recsz = 4 * 10 + 8 * 4;  // 40 + 32 = 72? Let's count:
  // 5 u32 ids = 20, size u64=8, 3 times i64=24, flags u32, vol u32, kind u32, mode u32 = 16; total 68
  constexpr std::size_t rec_bytes = 20 + 8 + 24 + 16;
  if (off + static_cast<std::size_t>(nrec) * rec_bytes + 4 > mf.size()) return false;
  for (std::uint32_t i = 0; i < nrec; ++i) {
    auto* r = p + off;
    IndexRecord rec;
    rec.id = rd32(r + 0);
    rec.name_id = rd32(r + 4);
    rec.path_id = rd32(r + 8);
    rec.parent_id = rd32(r + 12);
    rec.ext_id = rd32(r + 16);
    rec.size = rd64(r + 20);
    rec.ctime = static_cast<std::int64_t>(rd64(r + 28));
    rec.mtime = static_cast<std::int64_t>(rd64(r + 36));
    rec.atime = static_cast<std::int64_t>(rd64(r + 44));
    rec.flags = static_cast<RecordFlags>(rd32(r + 52));
    rec.volume_id = static_cast<std::uint16_t>(rd32(r + 56));
    rec.kind = static_cast<FileKind>(rd32(r + 60));
    rec.mode = rd32(r + 64);
    off += rec_bytes;
    if (rec.id < records_.size()) records_[rec.id] = rec;
  }
  rebuild_secondary_unlocked();
  return true;
}

}  // namespace wilfred
