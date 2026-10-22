#include "wilfred/ai/chat.hpp"

#include "wilfred/core/time_util.hpp"

#include <algorithm>

namespace wilfred {

namespace {

// ~20 turns: keeps request payloads small on every provider while still
// covering long threads. Oldest messages drop first, user + assistant
// together.
constexpr std::size_t kMaxMessages = 40;

}  // namespace

ChatStore& ChatStore::instance() {
  static ChatStore inst;
  return inst;
}

std::vector<ChatMessage> ChatStore::history() const {
  std::lock_guard<std::mutex> lock(mu_);
  return messages_;
}

void ChatStore::record_user(const std::string& text) {
  if (text.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  ChatMessage m;
  m.role = "user";
  m.content = text;
  m.when = unix_seconds();
  messages_.push_back(std::move(m));
  if (messages_.size() > kMaxMessages)
    messages_.erase(messages_.begin(), messages_.end() - kMaxMessages);
}

void ChatStore::record_assistant(const std::string& text) {
  if (text.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  ChatMessage m;
  m.role = "assistant";
  m.content = text;
  m.when = unix_seconds();
  messages_.push_back(std::move(m));
  if (messages_.size() > kMaxMessages)
    messages_.erase(messages_.begin(), messages_.end() - kMaxMessages);
}

bool ChatStore::active() const {
  std::lock_guard<std::mutex> lock(mu_);
  return !messages_.empty();
}

int ChatStore::turns() const {
  std::lock_guard<std::mutex> lock(mu_);
  return static_cast<int>(
      std::count_if(messages_.begin(), messages_.end(),
                    [](const ChatMessage& m) { return m.role == "assistant"; }));
}

std::string ChatStore::topic() const {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& m : messages_) {
    if (m.role == "user") {
      auto t = m.content;
      for (char& c : t)
        if (c == '\n' || c == '\r') c = ' ';
      if (t.size() > 60) t = t.substr(0, 57) + "…";
      return t;
    }
  }
  return {};
}

std::string ChatStore::last_answer() const {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto it = messages_.rbegin(); it != messages_.rend(); ++it) {
    if (it->role == "assistant") {
      auto t = it->content;
      for (char& c : t)
        if (c == '\n' || c == '\r') c = ' ';
      return t;
    }
  }
  return {};
}

void ChatStore::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  messages_.clear();
}

}  // namespace wilfred
