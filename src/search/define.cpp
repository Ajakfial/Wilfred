#include "wilfred/search/define.hpp"

#include "wilfred/core/utf8.hpp"

#include <algorithm>

namespace wilfred {
namespace {

inline std::string trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  std::size_t e = s.size();
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
    --e;
  return s.substr(b, e - b);
}

struct Row {
  const char* w;
  const char* pos;
  const char* def;
  const char* syn;
};

// Compact offline wordlist: common words users actually look up.
// Definitions are one line; synonyms comma-joined for the thesaurus view.
constexpr Row kWords[] = {
    {"abandon", "verb", "Give up completely; leave behind.", "leave, forsake, relinquish"},
    {"ability", "noun", "Skill or power to do something.", "skill, talent, capability"},
    {"abstract", "adjective", "Theoretical rather than concrete; hard to picture.", "theoretical, conceptual"},
    {"achieve", "verb", "Reach a goal through effort.", "accomplish, attain, reach"},
    {"adapt", "verb", "Adjust to new conditions.", "adjust, conform, acclimate"},
    {"adventure", "noun", "An exciting or unusual experience.", "escapade, journey, quest"},
    {"aesthetic", "adjective", "Concerned with beauty or appearance.", "artistic, stylish, tasteful"},
    {"agile", "adjective", "Able to move quickly and adapt.", "nimble, flexible, spry"},
    {"ambiguous", "adjective", "Open to more than one meaning.", "vague, unclear, equivocal"},
    {"ambition", "noun", "Strong desire to succeed.", "drive, aspiration, goal"},
    {"analyze", "verb", "Examine in detail to understand.", "inspect, study, dissect"},
    {"ancient", "adjective", "Very old; from long ago.", "antique, old, archaic"},
    {"anxiety", "noun", "Worry or unease about the future.", "worry, stress, apprehension"},
    {"archive", "noun", "Stored collection of records or files.", "repository, records, vault"},
    {"articulate", "adjective", "Clear in speech or expression.", "eloquent, fluent, expressive"},
    {"audit", "noun", "Formal review of records or accounts.", "review, inspection, check"},
    {"authentic", "adjective", "Genuine; true to origin.", "genuine, real, legitimate"},
    {"automate", "verb", "Make work run without manual effort.", "mechanize, streamline"},
    {"benchmark", "noun", "Standard used to measure performance.", "standard, yardstick, baseline"},
    {"brave", "adjective", "Ready to face danger or difficulty.", "courageous, bold, fearless"},
    {"brief", "adjective", "Short in time or length.", "short, concise, succinct"},
    {"brilliant", "adjective", "Very bright or exceptionally smart.", "bright, clever, superb"},
    {"cache", "noun", "Stored copy for faster access.", "store, buffer, reserve"},
    {"calm", "adjective", "Peaceful; free from agitation.", "peaceful, serene, tranquil"},
    {"candid", "adjective", "Frank and honest in speech.", "frank, honest, direct"},
    {"chaos", "noun", "Complete disorder and confusion.", "disorder, turmoil, mayhem"},
    {"clarity", "noun", "Clearness of thought or expression.", "lucidity, precision"},
    {"collaborate", "verb", "Work jointly on an activity.", "cooperate, team up"},
    {"compile", "verb", "Gather and assemble; turn code into a program.", "assemble, build"},
    {"complex", "adjective", "Made of many connected parts.", "intricate, involved, elaborate"},
    {"concise", "adjective", "Brief and to the point.", "succinct, terse, pithy"},
    {"configure", "verb", "Set up options for use.", "set up, arrange, customize"},
    {"contemplate", "verb", "Think deeply about something.", "consider, ponder, reflect"},
    {"context", "noun", "Situation surrounding an event or word.", "background, setting"},
    {"courage", "noun", "Strength to face fear or difficulty.", "bravery, nerve, valor"},
    {"creative", "adjective", "Producing original ideas.", "imaginative, inventive, original"},
    {"criteria", "noun", "Standards used to judge something.", "standards, benchmarks"},
    {"curious", "adjective", "Eager to learn or know.", "inquisitive, interested"},
    {"deadline", "noun", "Date by which work must be done.", "due date, cutoff"},
    {"debug", "verb", "Find and fix errors in code.", "fix, troubleshoot, patch"},
    {"decisive", "adjective", "Quick and firm in deciding.", "resolute, determined"},
    {"delegate", "verb", "Assign work to another person.", "assign, entrust, hand off"},
    {"deploy", "verb", "Put into use or position.", "launch, release, install"},
    {"design", "noun", "Plan for how something looks or works.", "plan, blueprint, layout"},
    {"diligent", "adjective", "Careful and steady in work.", "industrious, thorough"},
    {"distribute", "verb", "Share out among many.", "spread, deliver, dispense"},
    {"document", "noun", "Written record or file with information.", "record, file, paper"},
    {"efficient", "adjective", "Working with least waste.", "productive, streamlined, lean"},
    {"eloquent", "adjective", "Fluent and persuasive in speech.", "articulate, expressive"},
    {"empathy", "noun", "Understanding another's feelings.", "compassion, sympathy"},
    {"ephemeral", "adjective", "Lasting a very short time.", "fleeting, brief, transient"},
    {"ergonomic", "adjective", "Designed for comfort and efficiency.", "comfortable, practical"},
    {"essential", "adjective", "Absolutely necessary.", "vital, crucial, key"},
    {"evolve", "verb", "Develop gradually over time.", "develop, progress, adapt"},
    {"exquisite", "adjective", "Extremely beautiful or delicate.", "lovely, elegant, fine"},
    {"facilitate", "verb", "Make an action easier.", "ease, enable, assist"},
    {"fastidious", "adjective", "Very attentive to detail.", "meticulous, picky, exacting"},
    {"flexible", "adjective", "Able to bend or adapt easily.", "adaptable, pliable, versatile"},
    {"focus", "noun", "Center of attention or effort.", "concentration, attention"},
    {"fragile", "adjective", "Easily broken or harmed.", "delicate, brittle, frail"},
    {"frequent", "adjective", "Happening often.", "common, regular, repeated"},
    {"frugal", "adjective", "Careful with money or resources.", "thrifty, economical"},
    {"generic", "adjective", "General, not specific or branded.", "general, common, standard"},
    {"genuine", "adjective", "Truly what it claims to be.", "authentic, real, sincere"},
    {"graceful", "adjective", "Smooth and elegant in motion.", "elegant, fluid, poised"},
    {"habit", "noun", "Regular repeated behavior.", "routine, custom, practice"},
    {"harmony", "noun", "Agreement or pleasant combination.", "accord, balance, unity"},
    {"heuristic", "noun", "Practical rule for solving problems.", "rule of thumb, guide"},
    {"humble", "adjective", "Modest; not proud.", "modest, unassuming"},
    {"hypothesis", "noun", "Proposed explanation to test.", "theory, supposition"},
    {"idle", "adjective", "Not active; doing nothing.", "inactive, dormant, still"},
    {"illuminate", "verb", "Light up; make clear.", "light, clarify, explain"},
    {"imagine", "verb", "Form a picture in the mind.", "envision, picture, conceive"},
    {"implement", "verb", "Put a plan into effect.", "execute, apply, enforce"},
    {"implicit", "adjective", "Implied though not stated.", "implied, unspoken, tacit"},
    {"improve", "verb", "Make better.", "enhance, refine, upgrade"},
    {"index", "noun", "Ordered list for lookup; indicator.", "catalog, list, directory"},
    {"inevitable", "adjective", "Certain to happen.", "unavoidable, certain, fated"},
    {"ingenious", "adjective", "Clever and original.", "clever, inventive, brilliant"},
    {"innovate", "verb", "Introduce new ideas or methods.", "invent, pioneer, create"},
    {"insight", "noun", "Deep clear understanding.", "perception, grasp, wisdom"},
    {"integrity", "noun", "Honesty and strong principles.", "honesty, honor, principle"},
    {"intuitive", "adjective", "Easy to understand without study.", "natural, instinctive"},
    {"iterate", "verb", "Repeat to improve step by step.", "repeat, refine, cycle"},
    {"jargon", "noun", "Special words of a field.", "terminology, slang, lingo"},
    {"jubilant", "adjective", "Full of joy.", "joyful, elated, triumphant"},
    {"kernel", "noun", "Core part; seed.", "core, nucleus, essence"},
    {"laconic", "adjective", "Using few words.", "concise, terse, brief"},
    {"latency", "noun", "Delay before action starts.", "delay, lag"},
    {"legacy", "noun", "Something handed down; old system.", "heritage, inheritance"},
    {"lucid", "adjective", "Clear and easy to understand.", "clear, plain, intelligible"},
    {"malleable", "adjective", "Easily shaped or influenced.", "flexible, pliable"},
    {"manual", "noun", "Book of instructions.", "guide, handbook, instructions"},
    {"meticulous", "adjective", "Very careful about detail.", "thorough, precise, careful"},
    {"minimal", "adjective", "Least possible; very small.", "least, smallest, bare"},
    {"momentum", "noun", "Force of forward motion.", "drive, impetus, push"},
    {"monitor", "verb", "Watch over time.", "observe, track, supervise"},
    {"myriad", "noun", "A great number.", "multitude, host, lots"},
    {"nimble", "adjective", "Quick and light in movement.", "agile, spry, quick"},
    {"nostalgia", "noun", "Longing for the past.", "longing, reminiscence"},
    {"novel", "adjective", "New and original.", "new, fresh, innovative"},
    {"nuance", "noun", "Small subtle difference.", "shade, subtlety, gradation"},
    {"obsolete", "adjective", "No longer used; out of date.", "outdated, old, defunct"},
    {"opaque", "adjective", "Not see-through; hard to understand.", "cloudy, obscure, unclear"},
    {"optimize", "verb", "Make as good as possible.", "improve, refine, tune"},
    {"patience", "noun", "Calm waiting without complaint.", "tolerance, endurance"},
    {"persistent", "adjective", "Continuing firmly; not giving up.", "tenacious, steady"},
    {"pragmatic", "adjective", "Practical rather than idealistic.", "practical, realistic"},
    {"precise", "adjective", "Exact and accurate.", "exact, accurate, specific"},
    {"procrastinate", "verb", "Delay doing work.", "dawdle, stall, postpone"},
    {"prolific", "adjective", "Producing a lot.", "productive, fertile, fruitful"},
    {"query", "noun", "Question or search request.", "question, inquiry, search"},
    {"rapid", "adjective", "Very fast.", "quick, swift, speedy"},
    {"redundant", "adjective", "Extra; no longer needed.", "extra, surplus, spare"},
    {"refactor", "verb", "Restructure code without changing behavior.", "restructure, clean up"},
    {"relentless", "adjective", "Never stopping; persistent.", "unrelenting, tireless"},
    {"resilient", "adjective", "Quick to recover.", "tough, hardy, flexible"},
    {"robust", "adjective", "Strong and reliable.", "sturdy, solid, reliable"},
    {"routine", "noun", "Regular way of doing things.", "habit, procedure, pattern"},
    {"sapient", "adjective", "Wise; having insight.", "wise, sagacious"},
    {"scalable", "adjective", "Able to grow to larger size.", "expandable, extensible"},
    {"serene", "adjective", "Calm and peaceful.", "tranquil, placid, calm"},
    {"snapshot", "noun", "Captured state at one moment.", "capture, image, record"},
    {"stoic", "adjective", "Unmoved by emotion; enduring.", "impassive, calm, resigned"},
    {"streamline", "verb", "Simplify to be efficient.", "simplify, optimize"},
    {"subtle", "adjective", "Delicate; hard to notice.", "faint, slight, understated"},
    {"succinct", "adjective", "Brief and clear.", "concise, terse, pithy"},
    {"synchronize", "verb", "Make occur at the same time.", "sync, align, coordinate"},
    {"taciturn", "adjective", "Quiet; saying little.", "silent, reserved, reticent"},
    {"template", "noun", "Pattern to copy from.", "pattern, model, blueprint"},
    {"tenacious", "adjective", "Holding firm; persistent.", "persistent, dogged, firm"},
    {"threshold", "noun", "Point where something begins.", "limit, brink, edge"},
    {"triage", "verb", "Sort by priority.", "prioritize, sort, rank"},
    {"ubiquitous", "adjective", "Found everywhere.", "omnipresent, pervasive"},
    {"urgent", "adjective", "Needing immediate action.", "pressing, critical, immediate"},
    {"verbose", "adjective", "Using too many words.", "wordy, long-winded"},
    {"versatile", "adjective", "Able to do many things.", "flexible, adaptable, handy"},
    {"vibrant", "adjective", "Full of energy and color.", "lively, vivid, bright"},
    {"vigilant", "adjective", "Watchful for danger.", "alert, watchful, attentive"},
    {"workflow", "noun", "Sequence of steps to finish work.", "process, pipeline, routine"},
    {"zeal", "noun", "Great enthusiasm.", "passion, fervor, eagerness"},
};

}  // namespace

bool define_lookup(const std::string& word, DefineHit& out) {
  auto w = to_lower_utf8(trim(word));
  if (w.empty() || w.size() > 48) return false;
  // Strip trailing punctuation.
  while (!w.empty() && (w.back() == '.' || w.back() == ',' || w.back() == '!' || w.back() == '?'))
    w.pop_back();
  for (auto& r : kWords) {
    if (w == r.w) {
      out.word = r.w;
      out.pos = r.pos;
      out.definition = r.def;
      out.synonyms = r.syn;
      return true;
    }
  }
  return false;
}

std::vector<std::string> define_suggest(const std::string& prefix, std::size_t limit) {
  auto p = to_lower_utf8(trim(prefix));
  std::vector<std::string> out;
  if (p.empty()) return out;
  for (auto& r : kWords) {
    std::string w = r.w;
    if (w.rfind(p, 0) == 0) {
      out.emplace_back(w);
      if (out.size() >= limit) break;
    }
  }
  return out;
}

}  // namespace wilfred
