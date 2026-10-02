#pragma once

#include <string>
#include <vector>

namespace wilfred {

// DNS via getaddrinfo (no extra deps). Empty on failure.
std::vector<std::string> dns_lookup(const std::string& host);
// Ping via the OS ping CLI with a short timeout. Returns a one-line summary.
std::string ping_summary(const std::string& host);
// Public IP via https://api.ipify.org (short timeout, empty on failure/offline).
std::string public_ip();
// Local helpers shared with minis.
bool looks_like_email(const std::string& s);
bool looks_like_url_text(const std::string& s);
bool looks_like_path_text(const std::string& s);
bool looks_like_ip_text(const std::string& s);
bool looks_like_code_text(const std::string& s);
std::string clip_type_of(const std::string& s);

}  // namespace wilfred
