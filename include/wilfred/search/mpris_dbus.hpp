#pragma once

#include <string>
#include <vector>

// Minimal dependency-free D-Bus MPRIS client for Linux.
// Uses only POSIX sockets — no libdbus / playerctl required.
// All functions are safe to call when no session bus exists; they fail
// fast with an error string instead of hanging.
namespace wilfred {

// Map a Wilfred media id to an MPRIS Player method name.
// play|pause|playpause|next|prev|stop -> Play|Pause|PlayPause|Next|Previous|Stop.
// Empty when the id has no MPRIS equivalent (mute/volup/voldn).
std::string mpris_method_for(const std::string& id);

// List session-bus names starting with "org.mpris.MediaPlayer2.".
// Empty when the bus is unreachable.
std::vector<std::string> mpris_list_players(std::string& error);

// Send one MPRIS method to every known player. True when at least one
// player replies with METHOD_RETURN.
bool mpris_media_action(const std::string& id, std::string& error);

// Low-level helpers exposed for unit tests (message framing).
std::vector<unsigned char> mpris_build_method_call(std::uint32_t serial,
                                                   const std::string& destination,
                                                   const std::string& path,
                                                   const std::string& interface,
                                                   const std::string& member);
bool mpris_parse_names_reply(const unsigned char* data, std::size_t size,
                             std::vector<std::string>& out_names);

}  // namespace wilfred
