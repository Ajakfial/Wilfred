#include "wilfred/search/suggest.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/fuzzy.hpp"
#include "wilfred/search/macros.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_set>

namespace wilfred {

std::vector<std::string> command_vocabulary(const Config& cfg) {
  std::vector<std::string> v = {
      "weather",
      "time",
      "disk",
      "disku",
      "ram",
      "cpu",
      "process",
      "top",
      "proc",
      "ps",
      "battery",
      "host",
      "hostname",
      "ip",
      "uptime",
      "user",
      "whoami",
      "clip",
      "clips",
      "os",
      "cores",
      "screen",
      "swap",
      "help",
      "speedtest",
      "macros",
      "windows",
      "window",
      "switch",
      "emoji",
      "symbol",
      "fx",
      "currency",
      "tz",
      "color",
      "colour",
      "uuid",
      "guid",
      "base64",
      "sha256",
      "hash",
      "sha",
      "lorem",
      "json",
      "pretty",
      "lock",
      "sleep",
      "shutdown",
      "restart",
      "reboot",
      "logout",
      "logoff",
      "signout",
      "empty trash",
      "screenshot",
      "screencap",
      "screencapture",
      "printscreen",
      "bookmarks",
      "history",
      "tabs",
      "calendar",
      "contacts",
      "notes",
      "ai",
      "ask",
      "content:",
      "type:",
      "size:",
      "intext",
      "scope:",
      "clips clear",
      "speedtest again",
      "screenshot window",
      "screenshot region",
      "screen capture",
      "transcribe",
      "transcription",
      "stt",
      "minimize",
      "maximize",
      "minimise",
      "dictate",
      "dictation",
      "layout",
      "layouts",
  };
  // Built-in + custom macros.
  for (auto& [k, _] : builtin_macros()) {
    v.push_back("!" + k);
    v.push_back(k);
  }
  for (auto& [k, _] : cfg.macros) {
    auto l = to_lower_utf8(k);
    v.push_back("!" + l);
    v.push_back(l);
  }
  for (auto& [k, _] : cfg.aliases)
    v.push_back(to_lower_utf8(k));
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
  return v;
}

static std::string first_token_lower(const std::string& query, std::string& rest_out) {
  std::string s = normalize_query(query);
  auto sp = s.find(' ');
  if (sp == std::string::npos) {
    rest_out.clear();
    return s;
  }
  rest_out = s.substr(sp + 1);
  return s.substr(0, sp);
}

std::string correct_command_key(std::string_view key, const Config& cfg) {
  if (key.empty()) return {};
  std::string k = to_lower_utf8(key);
  auto vocab = command_vocabulary(cfg);
  // Exact (case-insensitive) hit: no correction needed.
  for (auto& w : vocab)
    if (w == k) return {};
  int best_d = 99;
  std::string best;
  int thr = typo_threshold(k.size());
  // Short keys are typo-prone ("timm", "weahter"); allow slightly looser
  // matching for the single-token command case.
  if (k.size() <= 6) thr = std::max(thr, 2);
  for (auto& w : vocab) {
    // Only compare against single-token vocab for single-token keys
    // (multi-word entries handled at query level).
    if (w.find(' ') != std::string::npos && k.find(' ') == std::string::npos) continue;
    if (w.size() < 2) continue;
    int diff = static_cast<int>(w.size() > k.size() ? w.size() - k.size() : k.size() - w.size());
    if (diff > thr) continue;
    int d = damerau_bounded(k, w, thr);
    if (d <= thr && d < best_d) {
      best_d = d;
      best = w;
    }
  }
  // Require high confidence: distance 1 always ok, distance 2 only when the
  // key is long enough or shares a prefix.
  if (!best.empty()) {
    if (best_d == 1) return best;
    if (best_d == 2 && k.size() >= 4 && !k.empty() && !best.empty() && k[0] == best[0]) return best;
  }
  return {};
}

std::string correct_query_command(std::string_view query, const Config& cfg) {
  std::string s(query);
  // Trim.
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  if (s.empty()) return {};
  // Split off leading bang/slash for macros.
  bool bang = (!s.empty() && (s[0] == '!' || s[0] == '/'));
  std::string body = bang ? s.substr(1) : s;
  std::string rest;
  std::string key = first_token_lower(body, rest);
  // Strip trailing colon ("screenshot:window" -> key "screenshot:window" handled below).
  std::string colon_key = key;
  std::string colon_rest;
  auto colon = key.find(':');
  std::string base_key = key;
  if (colon != std::string::npos) {
    base_key = key.substr(0, colon);
    colon_rest = key.substr(colon);  // includes ':...'
  }
  std::string fix = correct_command_key(base_key, cfg);
  if (fix.empty()) {
    // Try the whole first word including colon-arg ("screeshot:window").
    if (colon == std::string::npos) return {};
    return {};
  }
  std::string out;
  if (bang) out.push_back(s[0]);
  out += fix;
  out += colon_rest;
  if (!rest.empty()) {
    out.push_back(' ');
    // Preserve original casing of the remainder by slicing the raw query.
    // Find remainder offset in the original string (best effort).
    auto pos = s.find(rest);
    (void)pos;
    out += rest;
  }
  // Avoid correcting when the remainder looks like a file search that
  // happens to start with a command-like word (e.g. "time tracker.docx").
  // Only correct single-token queries or known multi-word commands.
  if (!rest.empty()) {
    static const char* multi[] = {"empty trash",       "log out",           "log off",
                                  "sign out",          "screen capture",    "screen shot",
                                  "capture screen",    "print screen",      "speedtest again",
                                  "screenshot window", "screenshot region", nullptr};
    std::string lowered = to_lower_utf8(out);
    bool is_multi = false;
    for (auto** p = multi; *p; ++p) {
      if (lowered == *p || lowered.rfind(std::string(*p) + " ", 0) == 0) {
        is_multi = true;
        break;
      }
    }
    // Single-word mini commands with arguments (weather London, tz tokyo,
    // emoji smile, fx ..., screenshot ...) are safe to correct.
    static const char* with_args[] = {
        "weather", "tz",         "emoji",     "symbol",    "fx",     "currency", "color",
        "colour",  "screenshot", "screencap", "process",   "proc",   "window",   "windows",
        "switch",  "clip",       "clips",     "json",      "base64", "sha256",   "hash",
        "sha",     "lorem",      "time",      "speedtest", nullptr};
    bool takes_arg = false;
    for (auto** p = with_args; *p; ++p)
      if (fix == *p) {
        takes_arg = true;
        break;
      }
    if (!is_multi && !takes_arg) return {};
  }
  if (to_lower_utf8(out) == to_lower_utf8(s)) return {};
  return out;
}

std::string suggest_correction(const std::string& query, const Config& cfg, HistoryStore* history) {
  std::string s = normalize_query(query);
  if (s.size() < 3) return {};
  // 1) Command typo ("weahter" -> "weather").
  if (auto fix = correct_query_command(query, cfg); !fix.empty()) return fix;
  // 2) History typo ("firefoz" typed often as "firefox").
  if (history && history->enabled()) {
    auto folded = fold_search(s);
    int best_d = 99;
    std::string best;
    // Consider frequent queries only (cheap + relevant).
    for (auto& q : history->recent_queries()) {
      if (q.size() > 64) continue;
      auto fq = fold_search(q);
      if (fq == folded) return {};
      int diff = static_cast<int>(fq.size() > folded.size() ? fq.size() - folded.size()
                                                            : folded.size() - fq.size());
      int thr = typo_threshold(std::min(fq.size(), folded.size()));
      if (diff > thr) continue;
      // Require shared prefix to avoid wild corrections.
      if (!folded.empty() && !fq.empty() && folded[0] != fq[0]) continue;
      int d = damerau_bounded(folded, fq, thr);
      if (d <= thr && d < best_d) {
        best_d = d;
        best = q;
      }
    }
    if (!best.empty() && best_d <= 2) return best;
  }
  return {};
}

std::string autocomplete_ghost(const std::string& query, const Config& cfg, HistoryStore* history) {
  if (query.empty()) return {};
  // Trailing space means the token is complete; don't ghost.
  if (!query.empty() && (query.back() == ' ' || query.back() == '\t')) return {};
  auto cands = autocomplete_candidates(query, cfg, history, 1);
  if (cands.empty()) return {};
  return cands.front();
}

std::vector<std::string> autocomplete_candidates(const std::string& query, const Config& cfg,
                                                 HistoryStore* history, int n) {
  std::vector<std::string> out;
  if (query.empty() || n <= 0) return out;
  std::string q = normalize_query(query);
  if (q.empty()) return out;
  std::unordered_set<std::string> seen;
  auto push = [&](const std::string& c) {
    if (c.empty() || (int)out.size() >= n) return;
    if (to_lower_utf8(c) == q) return;
    auto k = to_lower_utf8(c);
    if (seen.count(k)) return;
    seen.insert(k);
    out.push_back(c);
  };
  // 1) History prefix completions (most relevant first).
  if (history && history->enabled()) {
    for (auto& h : history->suggest_queries(q, n * 2)) {
      auto lh = to_lower_utf8(h);
      if (lh.rfind(q, 0) == 0 && lh.size() > q.size()) push(h);
      if ((int)out.size() >= n) break;
    }
    // Typo-prefix: "firefo" -> "firefox" from history.
    if ((int)out.size() < n) {
      for (auto& h : history->recent_queries()) {
        if ((int)out.size() >= n) break;
        auto lh = to_lower_utf8(h);
        if (lh.size() < q.size()) continue;
        auto head = lh.substr(0, q.size());
        if (head == q) continue;
        if (is_typo_match(q, head)) push(h);
      }
    }
  }
  // 2) Vocabulary prefix completions ("wea" -> "weather").
  if ((int)out.size() < n) {
    for (auto& w : command_vocabulary(cfg)) {
      if ((int)out.size() >= n) break;
      if (w.rfind(q, 0) == 0 && w.size() > q.size()) push(w);
    }
  }
  // 3) Typo-prefix over vocabulary ("weah" -> "weather").
  if ((int)out.size() < n && q.size() >= 3) {
    for (auto& w : command_vocabulary(cfg)) {
      if ((int)out.size() >= n) break;
      if (w.size() < q.size()) continue;
      auto head = w.substr(0, q.size());
      if (head == q) continue;
      if (is_typo_match(q, head)) push(w);
    }
  }
  return out;
}

std::vector<std::string> index_autocomplete(const std::string& query, IndexEngine& index, int n) {
  std::vector<std::string> out;
  if (query.empty() || n <= 0) return out;
  std::string q = normalize_query(query);
  if (q.empty()) return out;
  // Only complete the last path segment so "Docu" completes "Documents" and
  // "my-pro" completes "my-project", without leaking full absolute paths.
  std::string seg = q;
  auto slash = seg.find_last_of("/\\ ");
  if (slash != std::string::npos) seg = seg.substr(slash + 1);
  if (seg.empty()) return out;
  auto& store = index.store();
  std::lock_guard<std::recursive_mutex> lock(store.mutex());
  std::unordered_set<std::string> seen;
  auto push_display = [&](const std::string& disp) {
    if (disp.empty() || (int)out.size() >= n) return;
    auto k = to_lower_utf8(disp);
    if (k == q || k == seg) return;
    if (seen.count(k)) return;
    seen.insert(k);
    out.push_back(disp);
  };
  // Pass 1: exact prefix over basenames (fast, bounded scan from most recent).
  const auto& recs = store.records();
  std::size_t scanned = 0;
  const std::size_t cap = 20000;
  for (std::size_t i = recs.size(); i-- > 0 && scanned < cap && (int)out.size() < n;) {
    ++scanned;
    const auto& r = recs[i];
    if (r.name_id == 0 && r.path_id == 0) continue;
    std::string name(store.pool().get(r.name_id));
    if (name.empty()) continue;
    auto folded = fold_search(name);
    // Strip extension for display matching ("firefo" -> "firefox.exe").
    std::string stem = folded;
    auto dot = stem.rfind('.');
    if (dot != std::string::npos && dot > 1) stem = stem.substr(0, dot);
    if (folded.rfind(seg, 0) == 0 || stem.rfind(seg, 0) == 0) {
      // Reconstruct ghost: preserve typed prefix casing, append remainder.
      std::string remainder;
      const std::string& src = (stem.rfind(seg, 0) == 0 ? stem : folded);
      // Map back to original casing for display.
      std::string orig = name;
      if (stem.rfind(seg, 0) == 0 && dot != std::string::npos) orig = name.substr(0, dot);
      if (orig.size() > seg.size())
        remainder = orig.substr(seg.size());
      else if (src.size() > seg.size())
        remainder = src.substr(seg.size());
      std::string prefix = query.substr(0, query.size() - seg.size());
      push_display(prefix + seg + remainder);
    }
    if (i == 0) break;
  }
  // Pass 2: typo-prefix over basenames ("firefoz" -> "firefox").
  if ((int)out.size() < n && seg.size() >= 3) {
    scanned = 0;
    for (std::size_t i = recs.size(); i-- > 0 && scanned < cap && (int)out.size() < n;) {
      ++scanned;
      const auto& r = recs[i];
      if (r.name_id == 0 && r.path_id == 0) continue;
      std::string name(store.pool().get(r.name_id));
      if (name.empty()) continue;
      auto folded = fold_search(name);
      std::string stem = folded;
      auto dot = stem.rfind('.');
      if (dot != std::string::npos && dot > 1) stem = stem.substr(0, dot);
      for (auto cand : {folded, stem}) {
        if (cand.size() < seg.size()) continue;
        auto head = cand.substr(0, seg.size());
        if (head == seg) continue;
        if (is_typo_match(seg, head)) {
          std::string orig = name;
          if (cand == stem && dot != std::string::npos) orig = name.substr(0, dot);
          std::string prefix = query.substr(0, query.size() - seg.size());
          // Full corrected basename as candidate.
          push_display(prefix + orig);
          break;
        }
      }
      if (i == 0) break;
    }
  }
  return out;
}

std::string correct_index_name(const std::string& query, IndexEngine& index) {
  std::string q = normalize_query(query);
  if (q.size() < 3) return {};
  std::string seg = q;
  auto slash = seg.find_last_of("/\\ ");
  if (slash != std::string::npos) seg = seg.substr(slash + 1);
  if (seg.size() < 3) return {};
  auto& store = index.store();
  std::lock_guard<std::recursive_mutex> lock(store.mutex());
  int thr = typo_threshold(seg.size());
  if (seg.size() <= 6) thr = std::max(thr, 2);
  int best_d = thr + 1;
  std::string best;
  const auto& recs = store.records();
  std::size_t scanned = 0;
  const std::size_t cap = 20000;
  for (std::size_t i = recs.size(); i-- > 0 && scanned < cap;) {
    ++scanned;
    const auto& r = recs[i];
    if (r.name_id == 0) {
      if (i == 0) break;
      continue;
    }
    std::string name(store.pool().get(r.name_id));
    if (name.empty()) {
      if (i == 0) break;
      continue;
    }
    auto folded = fold_search(name);
    std::string stem = folded;
    auto dot = stem.rfind('.');
    if (dot != std::string::npos && dot > 1) stem = stem.substr(0, dot);
    for (auto cand : {folded, stem}) {
      if (cand.empty()) continue;
      if (cand == seg) {
        if (i == 0) break;
        continue;
      }
      int diff = static_cast<int>(cand.size() > seg.size() ? cand.size() - seg.size()
                                                           : seg.size() - cand.size());
      if (diff > thr) continue;
      if (!seg.empty() && !cand.empty() && seg[0] != cand[0]) continue;
      int d = damerau_bounded(seg, cand, thr);
      if (d <= thr && d < best_d) {
        best_d = d;
        // Return display form (original casing, without extension noise kept).
        best = (cand == stem && dot != std::string::npos) ? name.substr(0, dot) : name;
      }
    }
    if (best_d == 1 && best.size() >= 4) break;
    if (i == 0) break;
  }
  if (best.empty()) return {};
  if (best_d == 1) {
    std::string prefix = query.substr(0, query.size() - seg.size());
    return prefix + best;
  }
  if (best_d == 2 && seg.size() >= 4 && !seg.empty() && !best.empty() &&
      std::tolower((unsigned char)seg[0]) == std::tolower((unsigned char)best[0])) {
    std::string prefix = query.substr(0, query.size() - seg.size());
    return prefix + best;
  }
  return {};
}

AssistResult build_assist(const std::string& query, const Config& cfg, HistoryStore* history,
                          IndexEngine* index) {
  AssistResult a;
  if (query.empty()) return a;
  // Trailing space = token complete; still offer correction but no ghost.
  bool trailing_space = !query.empty() && (query.back() == ' ' || query.back() == '\t');
  std::string trimmed = query;
  while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
    trimmed.erase(trimmed.begin());
  while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
    trimmed.pop_back();
  if (trimmed.empty()) return a;
  // 1) Correction: command/history first (cheap, high precision), then index.
  a.correction = suggest_correction(trimmed, cfg, history);
  if (a.correction.empty() && index) {
    // Only suggest file corrections when the query looks like a filename
    // (no spaces, or last segment typo). Avoids "time tracker" -> file noise.
    bool single = trimmed.find(' ') == std::string::npos;
    if (single) a.correction = correct_index_name(trimmed, *index);
  }
  // 2) Candidates: history+vocab+index merged, deduped, capped.
  auto cands = autocomplete_candidates(query, cfg, history, 6);
  if (index && (int)cands.size() < 6 && !trailing_space) {
    auto idx = index_autocomplete(query, *index, 6 - (int)cands.size());
    std::unordered_set<std::string> seen;
    for (auto& c : cands)
      seen.insert(to_lower_utf8(c));
    for (auto& c : idx) {
      auto k = to_lower_utf8(c);
      if (seen.count(k)) continue;
      seen.insert(k);
      cands.push_back(c);
      if ((int)cands.size() >= 6) break;
    }
  }
  a.candidates = std::move(cands);
  // 3) Ghost: first candidate that extends the query (prefix or typo-prefix).
  if (!trailing_space && !a.candidates.empty()) {
    std::string ql = to_lower_utf8(query);
    for (auto& c : a.candidates) {
      auto cl = to_lower_utf8(c);
      if (cl.size() > ql.size() && cl.rfind(ql, 0) == 0) {
        a.ghost = c;
        break;
      }
    }
    if (a.ghost.empty()) {
      // Typo-prefix ghost: show the corrected completion.
      a.ghost = a.candidates.front();
    }
  }
  // If ghost equals correction target, keep both (UI shows ghost inline +
  // correction bar). If correction equals query (case-insensitive), drop it.
  if (!a.correction.empty() && to_lower_utf8(a.correction) == to_lower_utf8(trimmed))
    a.correction.clear();
  return a;
}

}  // namespace wilfred
