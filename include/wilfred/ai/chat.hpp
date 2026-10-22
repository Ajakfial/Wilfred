#pragma once

// Multiturn chat session for the AI assistant. Holds the conversation in
// memory only (never persisted to disk, so restarting covers it) and lets
// plain `ai <question>` queries continue an active thread.
//
// Design notes:
//  - One process-wide session, like TimerStore. Thread-safe.
//  - Messages cap keeps provider payloads bounded; oldest turns drop first.
//  - Failed answers are never recorded, so a retry starts from the last
//    good turn.

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

// One turn in a chat. role is "user" or "assistant" (provider naming).
struct ChatMessage {
  std::string role;
  std::string content;
  std::int64_t when{0};
};

class ChatStore {
 public:
  static ChatStore& instance();

  // Snapshot of the stored conversation (oldest first, capped).
  std::vector<ChatMessage> history() const;
  void record_user(const std::string& text);
  void record_assistant(const std::string& text);

  // A session is "active" while it holds turns; plain `ai <q>` follows up
  // in it until `ai chat reset`.
  bool active() const;
  int turns() const;  // completed exchanges
  // First user message, trimmed for a card title/subtitle.
  std::string topic() const;
  // Most recent assistant answer (empty when none).
  std::string last_answer() const;

  void reset();

 private:
  ChatStore() = default;
  mutable std::mutex mu_;
  std::vector<ChatMessage> messages_;
};

}  // namespace wilfred
