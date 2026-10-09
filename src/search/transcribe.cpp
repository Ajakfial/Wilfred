#include "wilfred/search/transcribe.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/platform/platform.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
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

// system() is unavailable in the iOS SDK (no process spawning in the
// sandbox); every caller already treats nonzero as "tool missing/failed".
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

bool ffmpeg_available() {
  return tool_available("ffmpeg");
}

std::string resolve_whisper_binary(const Config& cfg) {
  if (!cfg.transcription.binary.empty()) return cfg.transcription.binary;
  return probe_whisper_binary();
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
           "and set transcription.model to its path";
  }
#ifdef _WIN32
  return "Install whisper.cpp (a whisper-cli build) and put it on PATH, or set "
         "transcription.binary to its location";
#elif defined(__APPLE__)
  return "Install with: brew install whisper-cpp  (or set transcription.binary)";
#else
  return "Build whisper.cpp from github.com/ggerganov/whisper.cpp (or set transcription.binary)";
#endif
}

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

  std::string wav = audio_path;
  std::string tmp_wav;
  auto ext = to_lower_utf8(path_extension(audio_path));
  if (ext != ".wav") {
    if (!ffmpeg_available()) {
      if (ext != ".mp3") {
        error = "Container audio needs ffmpeg to extract. " + transcribe_install_hint("ffmpeg");
        return false;
      }
      // No ffmpeg: newer whisper-cli builds read MP3 directly; try as-is and
      // report the failure plainly if this build cannot.
    } else {
      create_directories(data_directory());
      tmp_wav = path_join(data_directory(), "transcribe-tmp.wav");
      if (run_shell(build_ffmpeg_command(tmp_wav, audio_path).c_str()) != 0 ||
          !file_exists(tmp_wav)) {
        error = "Could not extract audio with ffmpeg";
        remove_file(tmp_wav);
        return false;
      }
      wav = tmp_wav;
    }
  }

  create_directories(data_directory());
  auto out_base = path_join(data_directory(), "transcribe-out");
  remove_file(out_base + ".txt");
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
  int alpha = 0;
  for (unsigned char c : text) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      if (++alpha >= 3) break;
  }
  if (alpha < 3) {
    error = "No speech detected in this audio";
    return false;
  }
  out_text = text;
  return true;
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
