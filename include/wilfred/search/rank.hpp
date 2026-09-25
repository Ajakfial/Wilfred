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
