#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/index/store.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wilfred {

struct RankContext {
  std::string query;
  std::string folded;
  std::vector<std::string> tokens;
  std::unordered_map<std::string, int> frequency;
  std::unordered_map<std::string, std::int64_t> last_selected;
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, int> learned_paths;
  int hour{12};
  int weekday{0};
  std::string clipboard_folded;
  std::vector<std::string> clipboard_tokens;
  std::vector<std::string> clipboard_paths;
  std::vector<std::string> recent_parents;
  std::vector<std::string> recent_exts;
  std::vector<std::string> recent_names;
  std::vector<std::string> session_tokens;
  bool context_aware{true};
  bool allow_content{true};
};

struct ScoredHit {
  std::uint32_t id{0};
  int score{0};
  const IndexRecord* rec{nullptr};
};

int rank_record(const RankContext& ctx, const IndexStore& store, const IndexRecord& rec,
                const RankingWeights& w);
std::vector<ScoredHit> take_top(std::vector<ScoredHit> hits, std::size_t n);

// Extra scoring components registered at runtime; each is independently tunable
// via RankingWeights plus its own return value.
class RankPipeline {
public:
  using Signal = std::function<int(const RankContext&, const IndexStore&, const IndexRecord&,
                                   const RankingWeights&)>;
  void add(std::string name, Signal signal);
  int extras(const RankContext& ctx, const IndexStore& store, const IndexRecord& rec,
             const RankingWeights& w) const;
  void clear();
  const std::vector<std::string>& names() const { return names_; }

private:
  std::vector<std::string> names_;
  std::vector<Signal> signals_;
};

RankPipeline& default_rank_pipeline();

}  // namespace wilfred
