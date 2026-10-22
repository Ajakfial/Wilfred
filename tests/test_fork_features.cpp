#include "test.hpp"

#include "wilfred/ai/assistant.hpp"
#include "wilfred/ai/chat.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/reminders.hpp"

#include <string>
#include <vector>

#include <cstdio>
#include <cstring>

// Fork additions: multiturn AI chat, clipboard privacy, reminders.
void test_fork_features() {
  using namespace wilfred;
  set_mini_network_enabled(false);

  // --- ChatStore: multiturn session bookkeeping ---
  {
    auto& chat = ChatStore::instance();
    chat.reset();
    CHECK(!chat.active());
    chat.record_user("what is rust");
    chat.record_assistant("a systems language");
    chat.record_user("who made it");
    CHECK(chat.active());
    CHECK_EQ(chat.turns(), 1);
    CHECK_EQ(chat.topic(), std::string("what is rust"));
    CHECK(chat.last_answer().find("systems language") != std::string::npos);
    auto h = chat.history();
    CHECK_EQ(h.size(), 3u);
    CHECK_EQ(h.front().role, std::string("user"));
    CHECK_EQ(h.back().role, std::string("user"));
    chat.reset();
    CHECK(!chat.active());
    CHECK_EQ(chat.turns(), 0);
  }

  // --- ai_chat_is_request prefix parsing ---
  {
    std::string prompt;
    bool reset = false;
    CHECK(ai_chat_is_request("ai chat", prompt, reset) && prompt.empty() && !reset);
    CHECK(ai_chat_is_request("ai chat hello", prompt, reset) && prompt == "hello");
    CHECK(ai_chat_is_request("ai  chat:  reset my stuff?", prompt, reset) &&
          prompt == "reset my stuff?" && !reset);
    CHECK(ai_chat_is_request("ask chat reset", prompt, reset) && prompt.empty() && reset);
    CHECK(ai_chat_is_request("gpt chat clear", prompt, reset) && reset);
    CHECK(!ai_chat_is_request("ai chats are fun", prompt, reset));
    CHECK(!ai_chat_is_request("aichat", prompt, reset));
    CHECK(!ai_chat_is_request("hello ai chat", prompt, reset));
  }

  // --- ai_query_is_request still recognizes plain prompts ---
  {
    std::string prompt;
    CHECK(ai_query_is_request("ai what is 2+2", prompt) && prompt == "what is 2+2");
    CHECK(ai_query_is_request("ask", prompt) && prompt.empty());
    CHECK(!ai_query_is_request("paint", prompt));
  }

  // --- Reminder time parsing ---
  {
    std::int64_t now = 1758510000000LL;  // arbitrary fixed "now"
    std::int64_t due = 0;
    std::size_t used = 0;
    CHECK(parse_reminder_due("30m stretch", now, due, used));
    CHECK_EQ(used, 3u);
    CHECK_EQ(due, now + 30 * 60 * 1000);
    CHECK(parse_reminder_due("1h30m walk the dog", now, due, used));
    CHECK_EQ(used, 5u);
    CHECK_EQ(due, now + 90 * 60 * 1000);
    // Clock times parse; a passing slot rolls forward.
    CHECK(parse_reminder_due("at 14:30 pickup kid", now, due, used));
    CHECK_EQ(used, 8u);
    CHECK(parse_reminder_due("tomorrow 9am standup", now, due, used));
    CHECK_EQ(used, 12u);  // the "tomorrow 9am" prefix
    CHECK(parse_reminder_due("9:30pm meds", now, due, used));
    CHECK_EQ(used, 6u);
    CHECK(!parse_reminder_due("stretch at lunch", now, due, used));
    CHECK(!parse_reminder_due("", now, due, used));
    CHECK(!parse_reminder_due("buy milk 2b", now, due, used));
  }

  // --- ReminderStore: add, list, due, remove---
  {
    auto& store = ReminderStore::instance();
    store.configure("");  // no persistence during tests
    store.clear_all();
    std::int64_t now = 1758510000000LL;
    store.add("call bob later", now + 60 * 1000);
    store.add("email sue", now - 60 * 1000);
    CHECK_EQ(store.size(), 2u);
    auto fired = store.due(now);
    CHECK_EQ(fired.size(), 1u);
    CHECK_EQ(fired.front().label, std::string("email sue"));
    // Firing marks the entry, and it does not fire twice.
    CHECK_EQ(store.due(now).size(), 0u);
    CHECK(!store.list().empty());
    CHECK(store.clear_all().find("cleared") != std::string::npos);
    CHECK_EQ(store.size(), 0u);
    store.add("lunch soon", now + 5 * 60 * 1000);
    CHECK_EQ(store.size(), 1u);
    // Id match: next id is not 1 after clear (monotonic).
    auto items = store.list();
    CHECK_EQ(items.front().label, std::string("lunch soon"));
    CHECK(store.remove(std::to_string(items.front().id)).find("Removed") != std::string::npos);
    CHECK_EQ(store.size(), 0u);
    CHECK(store.remove("ghost").find("No reminder") != std::string::npos);
  }

  // --- Reminder mini intents & cards ---
  {
    CHECK(parse_mini_intent("remind 10m test").kind == MiniKind::Reminders);
    CHECK(parse_mini_intent("reminders").kind == MiniKind::Reminders);
    auto& store = ReminderStore::instance();
    store.clear_all();
    Config cfg;
    auto create = mini_results("remind 45m water the plants", cfg, "");
    CHECK(!create.empty());
    CHECK_EQ(store.size(), 1u);
    auto list = mini_results("remind", cfg, "");
    CHECK(!list.empty());
    bool has_label = false;
    for (auto& r : list)
      if (r.payload.find("water the plants") != std::string::npos) has_label = true;
    CHECK(has_label);
    auto its = store.list();
    CHECK(!its.empty());
    auto rm = mini_results("remind rm " + std::to_string(its.front().id), cfg, "");
    CHECK(!rm.empty());
    CHECK_EQ(store.size(), 0u);
    auto bad = mini_results("remind someday maybe", cfg, "");
    CHECK(!bad.empty());
    CHECK_EQ(store.size(), 0u);
    store.clear_all();
  }

  // --- Clipboard privacy: deny-list patterns ---
  {
    auto& store = ClipStore::instance();
    store.clear_all();
    std::vector<std::string> patterns = {"secret", "OTP:"};
    store.set_privacy(patterns);
    store.record("my password secret123");
    store.record("code OTP: 4242");
    store.record("harmless text");
    CHECK_EQ(store.size(), 1u);
    CHECK_EQ(store.texts().front(), std::string("harmless text"));
    // Entering copy + paste text outside the list is case-insensitive.
    store.record("this is SECRET stuff");
    CHECK_EQ(store.size(), 1u);
    store.set_privacy({});
    store.record("visible again");
    CHECK_EQ(store.size(), 2u);
    store.clear_all();
  }

  // --- Clipboard privacy: runtime pause ---
  {
    auto& store = ClipStore::instance();
    store.clear_all();
    store.set_paused(true);
    store.record("never stored");
    CHECK_EQ(store.size(), 0u);
    CHECK(store.paused());
    store.set_paused(false);
    store.record("stored now");
    CHECK_EQ(store.size(), 1u);
    CHECK(!store.paused());
    store.clear_all();
    store.set_privacy({});
  }

  // --- Clips pause/resume/status intents ---
  {
    Config cfg;
    auto paused = mini_results("clips pause", cfg, "");
    CHECK(!paused.empty());
    CHECK(ClipStore::instance().paused());
    auto resumed = mini_results("clips resume", cfg, "");
    CHECK(!resumed.empty());
    CHECK(!ClipStore::instance().paused());
    auto status = mini_results("clips status", cfg, "");
    CHECK(!status.empty());
  }

  // --- Clipboard config keys parse ---
  {
    ConfigError err;
    Config cfg;
    std::string yml =
        "clipboard:\n"
        "  manager: true\n"
        "  max_entries: 40\n"
        "  persist: false\n"
        "  ignore_patterns:\n"
        "    - \"otp:\"\n"
        "    - card\n"
        "  clear_on_exit: true\n";
    CHECK(load_config_text(yml, cfg, err));
    CHECK_EQ(cfg.clipboard.max_entries, 40);
    CHECK(!cfg.clipboard.persist);
    CHECK(cfg.clipboard.clear_on_exit);
    CHECK_EQ(cfg.clipboard.ignore_patterns.size(), 2u);
    CHECK_EQ(cfg.clipboard.ignore_patterns[0], std::string("otp:"));
    CHECK_EQ(cfg.clipboard.ignore_patterns[1], std::string("card"));
  }
}
