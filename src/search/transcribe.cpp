#include "wilfred/search/transcribe.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/platform/platform.hpp"

#ifdef WILFRED_WITH_SPOKE
#include "spoke/config.h"
#include "spoke/transcriber.h"
#endif

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace wilfred {
namespace {

std::string shell_quote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"')
      o += "\\\"";
    else
      o.push_back(c);
  }
  o.push_back('"');
  return o;
}

// system()/popen() are unavailable in the iOS SDK (no process spawning in
// the sandbox); every caller already treats nonzero as "tool missing/failed".
int run_shell(const std::string& cmd) {
#if defined(WILFRED_IOS)
  (void)cmd;
  return 1;
#else
  return std::system(cmd.c_str());
#endif
}

bool tool_available(const std::string& name) {
  if (name.empty()) return false;
#ifdef _WIN32
  std::string cmd = "where " + name + " >NUL 2>NUL";
#else
  std::string cmd = "command -v " + name + " >/dev/null 2>&1";
#endif
  return run_shell(cmd.c_str()) == 0;
}

#ifdef _WIN32
const char* kNullRedir = " 2>NUL";
#else
const char* kNullRedir = " 2>/dev/null";
#endif

// Capture a command's stdout (stderr discarded by the caller via kNullRedir
// when needed). Returns {output, exit_code}; on iOS always {"", 1}.
std::pair<std::string, int> capture_stdout(const std::string& cmd) {
#if defined(WILFRED_IOS)
  (void)cmd;
  return {"", 1};
#else
  std::string out;
  FILE* f = POPEN(cmd.c_str(), "r");
  if (!f) return {"", 1};
  char buf[4096];
  while (std::fgets(buf, sizeof(buf), f)) {
    out += buf;
    if (out.size() > 256 * 1024) break;
  }
  int rc = PCLOSE(f);
  return {out, rc};
#endif
}

std::string collapse_ws(const std::string& s, std::size_t cap) {
  std::string o;
  o.reserve(s.size() < cap ? s.size() : cap);
  bool ws = true;
  for (char c : s) {
    bool is_ws = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
    if (is_ws) {
      if (!ws) {
        o.push_back(' ');
        ws = true;
        if (o.size() >= cap) break;
      }
    } else {
      o.push_back(c);
      ws = false;
      if (o.size() >= cap) break;
    }
  }
  while (!o.empty() && o.back() == ' ')
    o.pop_back();
  return o;
}

std::string safe_lang(const std::string& language) {
  std::string o;
  for (char c : language) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-') o.push_back(c);
  }
  return o;
}

bool has_speech_letters(const std::string& text) {
  int alpha = 0;
  for (unsigned char c : text) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      if (++alpha >= 3) return true;
  }
  return false;
}

// Shared container -> 16 kHz mono WAV step. Returns the WAV to feed the
// engine (the original path when already WAV, else a data-dir temp file) and
// sets `tmp_wav` when the caller must delete it. On failure returns empty
// with `error` set. Spoke only reads WAV, so unlike whisper it always
// requires ffmpeg for non-WAV input.
std::string prepare_wav_input(const std::string& audio_path, bool spoke, std::string& tmp_wav,
                              std::string& error) {
  tmp_wav.clear();
  auto ext = to_lower_utf8(path_extension(audio_path));
  if (ext == ".wav") return audio_path;
  if (!ffmpeg_available()) {
    if (!spoke && ext == ".mp3") return audio_path;  // newer whisper-cli reads MP3 directly
    if (spoke)
      error = "Spoke reads WAV input; container audio needs ffmpeg to extract. " +
              transcribe_install_hint("ffmpeg");
    else
      error = "Container audio needs ffmpeg to extract. " + transcribe_install_hint("ffmpeg");
    return {};
  }
  create_directories(data_directory());
  tmp_wav = path_join(data_directory(), spoke ? "spoke-tmp.wav" : "transcribe-tmp.wav");
  if (run_shell(build_ffmpeg_command(tmp_wav, audio_path).c_str()) != 0 ||
      !file_exists(tmp_wav)) {
    error = "Could not extract audio with ffmpeg";
    remove_file(tmp_wav);
    tmp_wav.clear();
    return {};
  }
  return tmp_wav;
}

}  // namespace

bool is_audio_extension(const std::string& path) {
  auto e = to_lower_utf8(path_extension(path));
  return e == ".mp3" || e == ".wav" || e == ".m4a" || e == ".mp4" || e == ".ogg" || e == ".oga" ||
         e == ".flac" || e == ".opus" || e == ".webm" || e == ".aac" || e == ".wma" ||
         e == ".aiff" || e == ".aif";
}

bool is_transcribe_candidate(const std::string& path) {
  return is_audio_extension(path);
}

std::string probe_whisper_binary() {
  if (tool_available("whisper-cli")) return "whisper-cli";
  if (tool_available("whisper")) return "whisper";
  return {};
}

std::string probe_spoke_binary() {
  if (tool_available("spoke-cli")) return "spoke-cli";
  if (tool_available("spoke_cli")) return "spoke_cli";
  if (tool_available("spoke")) return "spoke";
  return {};
}

bool spoke_builtin_available() {
#ifdef WILFRED_WITH_SPOKE
  return true;
#else
  return false;
#endif
}

bool ffmpeg_available() {
  return tool_available("ffmpeg");
}

std::string normalize_transcription_engine(const std::string& engine) {
  auto e = to_lower_utf8(engine);
  // Trim surrounding whitespace.
  while (!e.empty() && (e.front() == ' ' || e.front() == '\t')) e.erase(e.begin());
  while (!e.empty() && (e.back() == ' ' || e.back() == '\t' || e.back() == '\r')) e.pop_back();
  if (e == "spoke" || e == "spoke.cpp" || e == "spokecpp" || e == "spoke-cpp" ||
      e == "spoke_cpp")
    return "spoke";
  if (e == "whisper" || e == "whisper.cpp" || e == "whispercpp" || e == "whisper-cpp" ||
      e == "whisper_cpp")
    return "whisper";
  if (e == "auto") return "auto";
  return "auto";
}

bool spoke_available(const Config& cfg) {
  if (spoke_builtin_available()) return true;
  return !resolve_spoke_binary(cfg).empty();
}

std::string resolve_transcription_engine(const Config& cfg) {
  auto norm = normalize_transcription_engine(cfg.transcription.engine);
  if (norm == "spoke") return "spoke";
  if (norm == "whisper") return "whisper";
  // auto: spoke when it can run without further setup (built-in or CLI on
  // PATH), else whisper (the historical default).
  if (spoke_builtin_available()) return "spoke";
  if (!probe_spoke_binary().empty()) return "spoke";
  return "whisper";
}

std::string resolve_whisper_binary(const Config& cfg) {
  if (!cfg.transcription.binary.empty()) return cfg.transcription.binary;
  return probe_whisper_binary();
}

std::string resolve_spoke_binary(const Config& cfg) {
  if (!cfg.transcription.spoke_binary.empty()) return cfg.transcription.spoke_binary;
  return probe_spoke_binary();
}

std::string resolve_whisper_model(const Config& cfg) {
  if (!cfg.transcription.model.empty() && file_exists(cfg.transcription.model))
    return cfg.transcription.model;
  static const char* kNames[] = {"ggml-base.en.bin", "ggml-small.en.bin", "ggml-base.bin",
                                 "ggml-small.bin", nullptr};
  auto dir = path_join(data_directory(), "models");
  for (auto** p = kNames; *p; ++p) {
    auto cand = path_join(dir, *p);
    if (file_exists(cand)) return cand;
  }
  return {};
}

std::string build_whisper_command(const std::string& binary, const std::string& model,
                                  const std::string& wav_path, const std::string& out_base,
                                  const std::string& language) {
  std::string cmd = shell_quote(binary) + " -m " + shell_quote(model) + " -f " +
                    shell_quote(wav_path) + " -otxt -of " + shell_quote(out_base);
  auto lang = safe_lang(language);
  if (!lang.empty() && to_lower_utf8(lang) != "auto") cmd += " -l " + lang;
  cmd += kNullRedir;
  return cmd;
}

std::string build_spoke_command(const std::string& binary, const std::string& wav_path,
                                int threads, int beam_width) {
  std::string cmd = shell_quote(binary) + " transcribe " + shell_quote(wav_path);
  if (threads > 0) {
    if (threads > 64) threads = 64;
    cmd += " --threads " + std::to_string(threads);
  }
  if (beam_width > 0) {
    if (beam_width < 1) beam_width = 1;
    if (beam_width > 64) beam_width = 64;
    cmd += " --beam " + std::to_string(beam_width);
  }
  cmd += " --json";
  cmd += kNullRedir;
  return cmd;
}

std::string parse_spoke_json_text(const std::string& json_output) {
  // Minimal extractor for {"text":"..."} with standard JSON string escapes.
  auto key = json_output.find("\"text\"");
  if (key == std::string::npos) return {};
  auto colon = json_output.find(':', key + 6);
  if (colon == std::string::npos) return {};
  auto q = json_output.find('"', colon + 1);
  if (q == std::string::npos) return {};
  std::string o;
  for (std::size_t i = q + 1; i < json_output.size(); ++i) {
    char c = json_output[i];
    if (c == '"') break;
    if (c != '\\') {
      o.push_back(c);
      continue;
    }
    if (++i >= json_output.size()) break;
    char e = json_output[i];
    switch (e) {
      case '"': o.push_back('"'); break;
      case '\\': o.push_back('\\'); break;
      case '/': o.push_back('/'); break;
      case 'b': o.push_back('\b'); break;
      case 'f': o.push_back('\f'); break;
      case 'n': o.push_back('\n'); break;
      case 'r': o.push_back('\r'); break;
      case 't': o.push_back('\t'); break;
      case 'u': {
        // Best-effort \uXXXX -> UTF-8 (BMP only; lone surrogates dropped).
        if (i + 4 >= json_output.size()) break;
        unsigned code = 0;
        bool ok = true;
        for (int k = 1; k <= 4; ++k) {
          char h = json_output[i + k];
          code <<= 4;
          if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
          else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
          else ok = false;
        }
        if (ok) {
          if (code < 0x80) {
            o.push_back(static_cast<char>(code));
          } else if (code < 0x800) {
            o.push_back(static_cast<char>(0xC0 | (code >> 6)));
            o.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else if (code < 0xD800 || code >= 0xE000) {
            o.push_back(static_cast<char>(0xE0 | (code >> 12)));
            o.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            o.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          }
          i += 4;
        }
        break;
      }
      default: o.push_back(e); break;
    }
    if (o.size() > 256 * 1024) break;
  }
  return o;
}

std::string build_ffmpeg_command(const std::string& wav_out, const std::string& src) {
  return "ffmpeg -y -v error -i " + shell_quote(src) + " -vn -ar 16000 -ac 1 -c:a pcm_s16le " +
         shell_quote(wav_out) + kNullRedir;
}

std::string build_mic_command(const std::string& wav_out, int seconds, const std::string& mic) {
  if (seconds < 1) seconds = 1;
  if (seconds > 120) seconds = 120;
  std::string tail = " -t " + std::to_string(seconds) + " -ar 16000 -ac 1 -c:a pcm_s16le " +
                     shell_quote(wav_out) + kNullRedir;
#ifdef _WIN32
  std::string dev = mic.empty() ? "Microphone" : mic;
  return "ffmpeg -y -v error -f dshow -i " + shell_quote("audio=" + dev) + tail;
#elif defined(__APPLE__)
  std::string dev = mic.empty() ? ":0" : mic;
  if (dev.find(':') == std::string::npos) dev = ":" + dev;
  return "ffmpeg -y -v error -f avfoundation -i " + shell_quote(dev) + tail;
#else
  std::string dev = mic.empty() ? "default" : mic;
  return "ffmpeg -y -v error -f alsa -i " + shell_quote(dev) + tail;
#endif
}

bool record_microphone(const std::string& wav_out, int seconds, const std::string& mic,
                       std::string& error) {
  error.clear();
  if (wav_out.empty()) {
    error = "no output path for recording";
    return false;
  }
  if (!ffmpeg_available()) {
    error = "Microphone recording needs ffmpeg. " + transcribe_install_hint("ffmpeg");
    return false;
  }
  create_directories(path_parent(wav_out));
  remove_file(wav_out);
  if (run_shell(build_mic_command(wav_out, seconds, mic).c_str()) != 0 || !file_exists(wav_out)) {
#ifdef _WIN32
    error =
        "Could not open the microphone. Set transcription.mic to your input device name "
        "(see it with: ffmpeg -list_devices true -f dshow -i dummy)";
#else
    error = "Could not open the microphone. Set transcription.mic to your input device";
#endif
    remove_file(wav_out);
    return false;
  }
  return true;
}

std::string transcribe_install_hint(const std::string& missing) {
  if (missing == "ffmpeg") {
#ifdef _WIN32
    return "Install with: winget install ffmpeg  (or choco install ffmpeg)";
#elif defined(__APPLE__)
    return "Install with: brew install ffmpeg";
#else
    return "Install with: sudo apt install ffmpeg  (or your distro equivalent)";
#endif
  }
  if (missing == "model") {
    return "Download a model, e.g. ggml-base.en.bin from huggingface.co/ggerganov/whisper.cpp, "
           "and set transcription.model to its path (or set transcription.engine: spoke for "
           "model-free transcription)";
  }
  if (missing == "spoke") {
#ifdef WILFRED_WITH_SPOKE
    (void)0;  // built-in present; this hint only fires for explicit CLI configs.
#endif
    return "Build spoke.cpp (cmake --preset <platform> && cmake --build --preset <platform>) "
           "and put spoke_cli on PATH, or set transcription.spoke_binary to its location "
           "(or rebuild Wilfred with spoke.cpp/ present for the built-in engine)";
  }
#ifdef _WIN32
  return "Install whisper.cpp (a whisper-cli build) and put it on PATH, or set "
         "transcription.binary to its location (or set transcription.engine: spoke)";
#elif defined(__APPLE__)
  return "Install with: brew install whisper-cpp  (or set transcription.binary; or set "
         "transcription.engine: spoke)";
#else
  return "Build whisper.cpp from github.com/ggerganov/whisper.cpp (or set transcription.binary; "
         "or set transcription.engine: spoke)";
#endif
}

namespace {

// Whisper path: ffmpeg extract (mp3-direct fallback without ffmpeg), then
// whisper-cli to a sidecar txt, then collapse + speech gate.
bool transcribe_via_whisper(const std::string& audio_path, const Config& cfg, std::string& out_text,
                            std::string& error, std::size_t max_chars) {
  auto binary = resolve_whisper_binary(cfg);
  if (binary.empty()) {
    error = "No whisper CLI found. " + transcribe_install_hint("whisper");
    return false;
  }
  auto model = resolve_whisper_model(cfg);
  if (model.empty()) {
    if (!cfg.transcription.model.empty())
      error = "Whisper model not found: " + cfg.transcription.model;
    else
      error = "No whisper model configured. " + transcribe_install_hint("model");
    return false;
  }

  std::string tmp_wav, wav;
  {
    std::string prep_err;
    wav = prepare_wav_input(audio_path, false, tmp_wav, prep_err);
    if (wav.empty()) {
      error = prep_err;
      return false;
    }
  }

  create_directories(data_directory());
  auto out_base = path_join(data_directory(), "transcribe-out");
  remove_file(out_base + ".txt");
  auto ext = to_lower_utf8(path_extension(audio_path));
  int rc = run_shell(
      build_whisper_command(binary, model, wav, out_base, cfg.transcription.language).c_str());
  if (!tmp_wav.empty()) remove_file(tmp_wav);
  if (rc != 0) {
    error = "Whisper failed (exit " + std::to_string(rc) + "). The build may not read this format" +
            (tmp_wav.empty() && ext != ".wav" ? "; installing ffmpeg usually fixes this" : "");
    remove_file(out_base + ".txt");
    return false;
  }
  std::string raw;
  if (!read_file_all(out_base + ".txt", raw)) {
    error = "Whisper produced no output";
    return false;
  }
  remove_file(out_base + ".txt");
  auto text = collapse_ws(raw, max_chars == 0 ? 65536 : max_chars);
  if (!has_speech_letters(text)) {
    error = "No speech detected in this audio";
    return false;
  }
  out_text = text;
  return true;
}

// Spoke CLI path: ffmpeg extract to WAV (spoke reads WAV only), then
// `spoke_cli transcribe <wav> --json`, then parse + speech gate.
bool transcribe_via_spoke_cli(const std::string& audio_path, const std::string& spoke_binary,
                              const Config& cfg, std::string& out_text, std::string& error,
                              std::size_t max_chars) {
  std::string tmp_wav, wav;
  {
    std::string prep_err;
    wav = prepare_wav_input(audio_path, true, tmp_wav, prep_err);
    if (wav.empty()) {
      error = prep_err;
      return false;
    }
  }
  auto cmd = build_spoke_command(spoke_binary, wav, cfg.transcription.threads,
                                 cfg.transcription.beam_width);
  auto [captured, rc] = capture_stdout(cmd);
  if (!tmp_wav.empty()) remove_file(tmp_wav);
  auto text_raw = parse_spoke_json_text(captured);
  if (text_raw.empty() && !captured.empty() && captured.find("\"text\"") == std::string::npos) {
    // Not JSON (older CLI?): treat stdout as plain transcript text.
    text_raw = captured;
  }
  auto text = collapse_ws(text_raw, max_chars == 0 ? 65536 : max_chars);
  if (text.empty()) {
    if (rc != 0)
      error = "Spoke failed (exit " + std::to_string(rc) + ")";
    else
      error = "Spoke produced no output";
    return false;
  }
  if (!has_speech_letters(text)) {
    error = "No speech detected in this audio";
    return false;
  }
  out_text = text;
  return true;
}

// In-process spoke path (compiled in via WILFRED_WITH_SPOKE). Same WAV
// prep as the CLI, then straight into spoke::Transcriber: no binary, no
// model files, fully offline.
bool transcribe_via_spoke_builtin_inner(const std::string& audio_path, const Config& cfg,
                                        std::string& out_text, std::string& error,
                                        std::size_t max_chars) {
#ifdef WILFRED_WITH_SPOKE
  std::string tmp_wav, wav;
  {
    std::string prep_err;
    wav = prepare_wav_input(audio_path, true, tmp_wav, prep_err);
    if (wav.empty()) {
      error = prep_err;
      return false;
    }
  }
  spoke::SpokeConfig scfg = spoke::SpokeConfig::tiny();
  if (cfg.transcription.threads > 0) scfg.num_threads = cfg.transcription.threads;
  if (cfg.transcription.beam_width >= 1 && cfg.transcription.beam_width <= 64)
    scfg.beam_width = cfg.transcription.beam_width;
  spoke::Transcriber engine(scfg);
  spoke::TranscriptionResult result;
  auto st = engine.transcribeFile(wav, result);
  if (!tmp_wav.empty()) remove_file(tmp_wav);
  if (!st.ok()) {
    error = "Spoke failed: " + st.message;
    return false;
  }
  auto text = collapse_ws(result.text, max_chars == 0 ? 65536 : max_chars);
  if (!has_speech_letters(text)) {
    error = "No speech detected in this audio";
    return false;
  }
  out_text = text;
  return true;
#else
  (void)audio_path;
  (void)cfg;
  (void)out_text;
  error = "Spoke engine not built in. " + transcribe_install_hint("spoke");
  return false;
#endif
}

// Exception firewall around the engine: spoke may throw (bad_alloc on huge
// inputs, thread/resource failures) and Wilfred promises never to throw out
// of transcribe_audio_file -- an uncaught throw would terminate the daemon.
// Any fault becomes a plain error string instead of a crash.
bool transcribe_via_spoke_builtin(const std::string& audio_path, const Config& cfg,
                                  std::string& out_text, std::string& error,
                                  std::size_t max_chars) {
#ifdef WILFRED_WITH_SPOKE
  try {
    return transcribe_via_spoke_builtin_inner(audio_path, cfg, out_text, error, max_chars);
  } catch (const std::exception& e) {
    // Best-effort temp cleanup (path is deterministic, see prepare_wav_input).
    remove_file(path_join(data_directory(), "spoke-tmp.wav"));
    error = std::string("Spoke failed: ") + e.what();
    return false;
  } catch (...) {
    remove_file(path_join(data_directory(), "spoke-tmp.wav"));
    error = "Spoke failed with an unexpected error";
    return false;
  }
#else
  (void)audio_path;
  (void)cfg;
  (void)out_text;
  error = "Spoke engine not built in. " + transcribe_install_hint("spoke");
  return false;
#endif
}

bool transcribe_via_spoke(const std::string& audio_path, const Config& cfg, std::string& out_text,
                          std::string& error, std::size_t max_chars) {
  if (spoke_builtin_available()) return transcribe_via_spoke_builtin(audio_path, cfg, out_text,
                                                                      error, max_chars);
  auto binary = resolve_spoke_binary(cfg);
  if (binary.empty()) {
    error = "No spoke CLI found. " + transcribe_install_hint("spoke");
    return false;
  }
  return transcribe_via_spoke_cli(audio_path, binary, cfg, out_text, error, max_chars);
}

}  // namespace

bool transcribe_audio_file(const std::string& audio_path, const Config& cfg, std::string& out_text,
                           std::string& error, std::size_t max_chars) {
  out_text.clear();
  error.clear();
  if (!cfg.transcription.enabled) {
    error = "Transcription is disabled (transcription.enabled: false)";
    return false;
  }
  if (audio_path.empty() || !is_transcribe_candidate(audio_path)) {
    error = "Not an audio file (mp3, wav, m4a, mp4, ogg, flac, opus, webm, aac, wma)";
    return false;
  }
  if (!file_exists(audio_path)) {
    error = "Audio file not found: " + audio_path;
    return false;
  }
  if (resolve_transcription_engine(cfg) == "spoke")
    return transcribe_via_spoke(audio_path, cfg, out_text, error, max_chars);
  return transcribe_via_whisper(audio_path, cfg, out_text, error, max_chars);
}

bool write_transcript_sidecar(const std::string& audio_path, const std::string& text,
                              std::string& error) {
  error.clear();
  if (audio_path.empty() || text.empty()) {
    error = "Nothing to save";
    return false;
  }
  auto dest = audio_path + ".txt";
  if (!write_file_atomic(dest.data(), text.data(), text.size())) {
    error = "Could not write " + dest;
    return false;
  }
  return true;
}

std::vector<std::string> find_audio_in_index(IndexEngine& index, const std::string& needle,
                                             int limit) {
  std::vector<std::string> out;
  if (limit <= 0) limit = 8;
  auto q = to_lower_utf8(needle);
  auto& store = index.store();
  std::lock_guard<std::recursive_mutex> lock(store.mutex());
  for (auto& rec : store.records()) {
    if (static_cast<int>(out.size()) >= limit) break;
    if (has_flag(rec.flags, RecordFlags::Directory)) continue;
    std::string_view pv = store.pool().get(rec.path_id);
    if (pv.empty()) continue;
    std::string path(pv);
    if (!is_transcribe_candidate(path)) continue;
    if (!q.empty() && to_lower_utf8(path).find(q) == std::string::npos) continue;
    out.push_back(std::move(path));
  }
  return out;
}

}  // namespace wilfred
