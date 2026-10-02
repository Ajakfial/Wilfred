#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

enum class MiniKind {
  None,
  Weather,
  Time,
  Disk,
  DiskUsage,
  Ram,
  Cpu,
  Process,
  Battery,
  Hostname,
  Ip,
  Uptime,
  User,
  Clipboard,
  Clips,
  Os,
  Cores,
  Screen,
  Swap,
  Help,
  Speedtest,
  MacrosList,
  Windows,
  Emoji,
  Symbol,
  Fx,
  Tz,
  Color,
  Uuid,
  Base64,
  Sha256,
  Lorem,
  Json,
  System,
  Screenshot,
  Browser,
  Timer,
  Stopwatch,
  Note,
  Todo,
  Kill,
  Media,
  Ping,
  Dns,
  MyIp,
  Base,
  Bits,
  Regex,
  UrlCodec,
  Jwt,
  Dupes,
  Large,
  Workflow,
  Quicklink,
};

struct MiniIntent {
  MiniKind kind{MiniKind::None};
  std::string remainder;
  bool exact{false};
};

MiniIntent parse_mini_intent(std::string_view query);
std::vector<SearchResult> mini_results(const std::string& query, const Config& cfg,
                                       const std::string& clipboard, class IndexEngine* index = nullptr);
void set_mini_network_enabled(bool enabled);

}  // namespace wilfred
