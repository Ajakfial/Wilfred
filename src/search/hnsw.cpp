#include "wilfred/search/hnsw.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/mmap.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
#include <unordered_map>

namespace wilfred {
namespace {

void w32(std::string& o, std::uint32_t v) {
  o.push_back(static_cast<char>(v & 0xFF));
  o.push_back(static_cast<char>((v >> 8) & 0xFF));
  o.push_back(static_cast<char>((v >> 16) & 0xFF));
  o.push_back(static_cast<char>((v >> 24) & 0xFF));
}

std::uint32_t r32(const std::uint8_t*& p, const std::uint8_t* end, bool& ok) {
  if (p + 4 > end) {
    ok = false;
    return 0;
  }
  std::uint32_t v = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
                    (static_cast<std::uint32_t>(p[2]) << 16) |
                    (static_cast<std::uint32_t>(p[3]) << 24);
  p += 4;
  return v;
}

}  // namespace

HnswIndex::HnswIndex(int dim, int m, int ef_construction)
    : dim_(dim), m_(m), ef_construction_(ef_construction) {}

void HnswIndex::configure(int dim, int m, int ef_construction) {
  if (dim != dim_) clear();
  dim_ = dim;
  m_ = m > 4 ? m : 4;
  ef_construction_ = ef_construction > 8 ? ef_construction : 8;
}

void HnswIndex::clear() {
  ids_.clear();
  vecs_.clear();
  levels_.clear();
  links_.clear();
  counter_ = 0;
  entry_ = 0;
}

int HnswIndex::level_of(std::uint64_t counter) const {
  // Deterministic geometric levels (ml ≈ 1/ln(M)) without RNG state.
  std::uint64_t x = counter * 2654435761ull + 0x9e3779b97f4a7c15ull;
  int lvl = 0;
  while ((x & 0xFFFFull) < (0xFFFFull / (std::uint64_t)(m_ > 0 ? m_ : 16)) && lvl < 5) {
    ++lvl;
    x >>= 7;
    x ^= x * 2862933555777941757ull;
  }
  return lvl;
}

float HnswIndex::dot(const std::vector<float>& a, const std::vector<float>& b) {
  double d = 0;
  for (std::size_t i = 0; i < a.size(); ++i)
    d += (double)a[i] * b[i];
  return (float)d;
}

float HnswIndex::dist2(std::size_t a, std::size_t b) const {
  // 1 - cosine for normalized vectors; monotonic with cosine.
  return 1.0f - dot(vecs_[a], vecs_[b]);
}

bool HnswIndex::has(std::uint32_t id) const {
  for (auto v : ids_)
    if (v == id) return true;
  return false;
}

const std::vector<float>* HnswIndex::get(std::uint32_t id) const {
  for (std::size_t i = 0; i < ids_.size(); ++i)
    if (ids_[i] == id) return &vecs_[i];
  return nullptr;
}

bool HnswIndex::remove(std::uint32_t id) {
  for (std::size_t i = 0; i < ids_.size(); ++i) {
    if (ids_[i] != id) continue;
    ids_.erase(ids_.begin() + (long)i);
    vecs_.erase(vecs_.begin() + (long)i);
    levels_.erase(levels_.begin() + (long)i);
    links_.erase(links_.begin() + (long)i);
    // Remap neighbor positions > i.
    for (auto& per : links_)
      for (auto& lvl : per)
        for (auto& n : lvl)
          if (n > i) --n;
    // Scrub dangling refs to i.
    for (auto& per : links_)
      for (auto& lvl : per) {
        lvl.erase(std::remove(lvl.begin(), lvl.end(), (std::uint32_t)i), lvl.end());
        for (auto& n : lvl)
          if (n >= links_.size()) n = 0;
      }
    if (entry_ >= links_.size()) entry_ = links_.empty() ? 0 : 0;
    return true;
  }
  return false;
}

bool HnswIndex::add(std::uint32_t id, const std::vector<float>& vec) {
  if (dim_ <= 0 || (int)vec.size() != dim_) return false;
  // Replace in place when the id already exists (keeps graph stable).
  for (std::size_t i = 0; i < ids_.size(); ++i) {
    if (ids_[i] == id) {
      vecs_[i] = vec;
      return true;
    }
  }
  int lvl = level_of(counter_++);
  std::size_t pos = ids_.size();
  ids_.push_back(id);
  vecs_.push_back(vec);
  levels_.push_back(lvl);
  links_.emplace_back((std::size_t)lvl + 1);

  if (pos == 0) {
    entry_ = 0;
    return true;
  }

  // Greedy descent from the entry point on upper layers.
  std::uint32_t cur = entry_;
  int cur_max = levels_[entry_];
  for (int l = cur_max; l > lvl; --l) {
    bool improved = true;
    while (improved) {
      improved = false;
      float best = 0;
      // distance query<->cur via temp dot
      {
        double d = 0;
        for (int d_i = 0; d_i < dim_; ++d_i)
          d += (double)vec[d_i] * vecs_[cur][d_i];
        best = 1.0f - (float)d;
      }
      if (l < (int)links_[cur].size()) {
        for (auto nb : links_[cur][l]) {
          if (nb >= vecs_.size()) continue;
          double d = 0;
          for (int d_i = 0; d_i < dim_; ++d_i)
            d += (double)vec[d_i] * vecs_[nb][d_i];
          float dd = 1.0f - (float)d;
          if (dd < best) {
            best = dd;
            cur = nb;
            improved = true;
          }
        }
      }
    }
  }
  // Connect on each level <= lvl via brute-force nearest among existing.
  for (int l = 0; l <= lvl; ++l) {
    // Candidate pool: ef_construction nearest by linear scan (indexes are
    // small enough that this stays fast; graph edges make query fast).
    std::vector<std::pair<float, std::uint32_t>> scored;
    scored.reserve(vecs_.size() - 1);
    for (std::size_t i = 0; i + 1 < vecs_.size(); ++i) {
      double d = 0;
      for (int d_i = 0; d_i < dim_; ++d_i)
        d += (double)vec[d_i] * vecs_[i][d_i];
      scored.emplace_back(1.0f - (float)d, (std::uint32_t)i);
    }
    std::size_t want = (std::size_t)std::min<int>(m_, (int)scored.size());
    std::nth_element(scored.begin(), scored.begin() + (long)want, scored.end(),
                     [](auto& a, auto& b) { return a.first < b.first; });
    auto& mine = links_[pos][l];
    for (std::size_t i = 0; i < want; ++i) {
      std::uint32_t nb = scored[i].second;
      mine.push_back(nb);
      if (l < (int)links_[nb].size()) {
        links_[nb][l].push_back((std::uint32_t)pos);
        // Cap degree at 2*M with simple pruning (keep nearest).
        if (links_[nb][l].size() > (std::size_t)(m_ * 2)) {
          auto& lst = links_[nb][l];
          std::vector<std::pair<float, std::uint32_t>> s2;
          for (auto n : lst) {
            if (n >= vecs_.size()) continue;
            s2.emplace_back(dist2(nb, n), n);
          }
          std::sort(s2.begin(), s2.end(), [](auto& a, auto& b) { return a.first < b.first; });
          lst.clear();
          for (std::size_t k = 0; k < s2.size() && k < (std::size_t)m_; ++k)
            lst.push_back(s2[k].second);
        }
      }
    }
  }
  if (lvl > levels_[entry_]) entry_ = (std::uint32_t)pos;
  return true;
}

std::vector<HnswHit> HnswIndex::search(const std::vector<float>& query, int k, int ef) const {
  std::vector<HnswHit> out;
  if (vecs_.empty() || (int)query.size() != dim_ || k <= 0) return out;
  if (ef < k) ef = k;
  if (ef < 8) ef = 8;

  // Greedy descent to base layer.
  std::uint32_t cur = entry_;
  if (cur >= vecs_.size()) cur = 0;
  auto dist_q = [&](std::uint32_t p) {
    double d = 0;
    for (int i = 0; i < dim_; ++i)
      d += (double)query[i] * vecs_[p][i];
    return 1.0f - (float)d;
  };
  int top = levels_[cur];
  for (int l = top; l > 0; --l) {
    bool improved = true;
    while (improved) {
      improved = false;
      float best = dist_q(cur);
      if (l < (int)links_[cur].size()) {
        for (auto nb : links_[cur][l]) {
          if (nb >= vecs_.size()) continue;
          float dd = dist_q(nb);
          if (dd < best) {
            best = dd;
            cur = nb;
            improved = true;
          }
        }
      }
    }
  }
  // Best-first beam search on layer 0 with ef width.
  using Item = std::pair<float, std::uint32_t>;  // (dist, pos)
  struct Cmp {
    bool operator()(const Item& a, const Item& b) const { return a.first < b.first; }
  };
  std::priority_queue<Item, std::vector<Item>, Cmp> topk;  // max-heap
  std::priority_queue<Item, std::vector<Item>, Cmp> cand;  // max-heap used as min via negation
  // Use a simple visited set.
  std::vector<char> visited(vecs_.size(), 0);
  // Seed with entry + a few neighbors for recall.
  std::vector<std::uint32_t> seeds{cur};
  if (!links_[cur].empty())
    for (auto n : links_[cur][0]) {
      if (seeds.size() >= 8) break;
      seeds.push_back(n);
    }
  // To keep it a min-heap via std::priority_queue we push negative dist.
  using MinItem = std::pair<float, std::uint32_t>;
  struct MinCmp {
    bool operator()(const MinItem& a, const MinItem& b) const { return a.first > b.first; }
  };
  std::priority_queue<MinItem, std::vector<MinItem>, MinCmp> frontier;
  for (auto s : seeds) {
    if (s >= vecs_.size() || visited[s]) continue;
    visited[s] = 1;
    frontier.emplace(dist_q(s), s);
  }
  while (!frontier.empty()) {
    auto [d, p] = frontier.top();
    frontier.pop();
    if ((int)topk.size() >= ef && d > topk.top().first) break;
    topk.emplace(d, p);
    if ((int)topk.size() > ef) topk.pop();
    if (!links_[p].empty()) {
      for (auto nb : links_[p][0]) {
        if (nb >= vecs_.size() || visited[nb]) continue;
        visited[nb] = 1;
        float dd = dist_q(nb);
        if ((int)topk.size() < ef || dd < topk.top().first) frontier.emplace(dd, nb);
      }
    }
    // Linear fallback for tiny indexes: scan everything when ef is large
    // relative to size so recall stays exact on small libraries.
    if (vecs_.size() < 400 && frontier.empty()) {
      for (std::uint32_t i = 0; i < vecs_.size(); ++i) {
        if (visited[i]) continue;
        visited[i] = 1;
        frontier.emplace(dist_q(i), i);
      }
    }
  }
  std::vector<Item> all;
  while (!topk.empty()) {
    all.push_back(topk.top());
    topk.pop();
  }
  std::sort(all.begin(), all.end(), [](auto& a, auto& b) { return a.first < b.first; });
  for (std::size_t i = 0; i < all.size() && (int)out.size() < k; ++i) {
    HnswHit h;
    h.id = ids_[all[i].second];
    h.score = 1.0f - all[i].first;
    if (h.score > 1) h.score = 1;
    if (h.score < -1) h.score = -1;
    out.push_back(h);
  }
  (void)cand;
  return out;
}

bool HnswIndex::save(const std::string& path) const {
  std::string o;
  o.reserve(16 + ids_.size() * (8 + (std::size_t)dim_ * 4));
  o.push_back('W');
  o.push_back('V');
  o.push_back('E');
  o.push_back('C');
  w32(o, 1);  // version
  w32(o, (std::uint32_t)dim_);
  w32(o, (std::uint32_t)ids_.size());
  for (std::size_t i = 0; i < ids_.size(); ++i) {
    w32(o, ids_[i]);
    for (int d = 0; d < dim_; ++d) {
      float f = d < (int)vecs_[i].size() ? vecs_[i][d] : 0.0f;
      std::uint32_t u;
      std::memcpy(&u, &f, 4);
      w32(o, u);
    }
  }
  std::uint32_t crc = crc32(o.data(), o.size());
  w32(o, crc);
  return write_file_atomic(path, o.data(), o.size());
}

bool HnswIndex::load(const std::string& path) {
  clear();
  MappedFile mf;
  if (!mf.open_read(path)) return false;
  const std::uint8_t* p = mf.data();
  const std::uint8_t* end = p + mf.size();
  bool ok = true;
  if (mf.size() < 16) return false;
  if (p[0] != 'W' || p[1] != 'V' || p[2] != 'E' || p[3] != 'C') return false;
  p += 4;
  std::uint32_t ver = r32(p, end, ok);
  std::uint32_t dim = r32(p, end, ok);
  std::uint32_t n = r32(p, end, ok);
  if (!ok || ver != 1 || dim < 32 || dim > 4096 || n > 4000000) return false;
  dim_ = (int)dim;
  // Verify CRC over everything except trailing 4 bytes.
  if (mf.size() < 4) return false;
  std::uint32_t want = 0;
  {
    const std::uint8_t* q = end - 4;
    want = (std::uint32_t)q[0] | ((std::uint32_t)q[1] << 8) | ((std::uint32_t)q[2] << 16) |
           ((std::uint32_t)q[3] << 24);
  }
  std::uint32_t got = crc32(reinterpret_cast<const char*>(mf.data()), mf.size() - 4);
  if (got != want) {
    clear();
    return false;
  }
  for (std::uint32_t i = 0; i < n; ++i) {
    std::uint32_t id = r32(p, end, ok);
    if (!ok) {
      clear();
      return false;
    }
    std::vector<float> v;
    v.reserve(dim);
    for (std::uint32_t d = 0; d < dim; ++d) {
      std::uint32_t u = r32(p, end, ok);
      if (!ok) {
        clear();
        return false;
      }
      float f;
      std::memcpy(&f, &u, 4);
      v.push_back(f);
    }
    // Rebuild graph incrementally (levels deterministic via counter_).
    add(id, v);
    if (p > end) {
      clear();
      return false;
    }
  }
  return true;
}

}  // namespace wilfred
