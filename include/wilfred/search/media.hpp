#pragma once

#include <cstdint>
#include <string>

namespace wilfred {

// Terminates a process by PID. Returns true on success; error holds a message.
bool native_kill_process(std::uint32_t pid, std::string& error);
// Media keys: play | pause | playpause | next | prev | stop | mute | volup | voldn
bool native_media_action(const std::string& id, std::string& error);

}  // namespace wilfred
