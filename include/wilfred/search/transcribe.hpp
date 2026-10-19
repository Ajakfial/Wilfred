#pragma once

// On-demand audio transcription for MP3s and MP4 (and other containers).
//
// Two interchangeable backends, selected by `transcription.engine`
// ("auto" | "whisper" | "spoke", with "whisper.cpp"/"spoke.cpp" accepted as
// aliases):
//
// * whisper -- the `whisper.cpp` CLI (`whisper-cli` or `whisper`) plus a ggml
//   model file, with `ffmpeg` for container audio extraction when needed.
// * spoke -- the in-tree spoke.cpp tiny English engine. When Wilfred is built
//   with WILFRED_WITH_SPOKE it runs in-process (no model files, no extra
//   binary); otherwise it shells out to a `spoke_cli` binary
//   (`spoke-cli` / `spoke_cli` / `spoke` on PATH, or
//   `transcription.spoke_binary`). Container audio still goes through ffmpeg
//   to a 16 kHz WAV first, exactly like the whisper path.
//
// Cross-platform: shells out to those CLIs with a per-OS probe (`where` on
// Windows, `command -v` elsewhere) and returns false with a helpful error
// when a binary or model is missing. No link-time dependency on
// whisper/ffmpeg; the spoke link is compile-time optional.
//
// The overlay never runs transcription at query time: the `transcribe` mini
// resolves the target file and offers a `transcribe_run` action, which
// delivers the transcript to the clipboard (plus an optional `<audio>.txt`
// sidecar) when the user presses Enter.

#include <cstddef>
#include <string>
#include <vector>

namespace wilfred {

struct Config;
class IndexEngine;

bool is_audio_extension(const std::string& path);
bool is_transcribe_candidate(const std::string& path);

// Probe results. Empty string when the binary is not on PATH.
std::string probe_whisper_binary();
std::string probe_spoke_binary();
bool ffmpeg_available();

// True when the in-process spoke engine was compiled in
// (WILFRED_WITH_SPOKE). CLI fallback works without it.
bool spoke_builtin_available();

// Normalize `transcription.engine` to "auto" | "whisper" | "spoke".
// Accepts "whisper.cpp"/"spoke.cpp" spellings; unknown values map to "auto".
std::string normalize_transcription_engine(const std::string& engine);

// Effective engine for this config: "spoke" or "whisper". "auto" prefers
// spoke when the built-in engine or a spoke CLI is available, else whisper.
std::string resolve_transcription_engine(const Config& cfg);

// True when spoke can run right now (built-in or CLI binary present).
bool spoke_available(const Config& cfg);

// Resolve the whisper binary: explicit `transcription.binary` first,
// otherwise the on-PATH probe. Empty when unavailable.
std::string resolve_whisper_binary(const Config& cfg);

// Resolve the spoke CLI binary: explicit `transcription.spoke_binary`
// first, otherwise the on-PATH probe (`spoke-cli`, `spoke_cli`, `spoke`).
// Empty when unavailable. The in-process built-in needs no binary.
std::string resolve_spoke_binary(const Config& cfg);

// Resolve the model file: explicit `transcription.model` when it exists,
// otherwise conventional spots under the data directory. Empty when none.
// (Whisper-only; spoke ships no weight files.)
std::string resolve_whisper_model(const Config& cfg);

// Build the whisper CLI invocation (pure, unit-testable).
// Produces `<out_base>.txt` next to the temp dir when run.
std::string build_whisper_command(const std::string& binary, const std::string& model,
                                  const std::string& wav_path, const std::string& out_base,
                                  const std::string& language);

// Build the spoke CLI invocation (pure, unit-testable):
// `<binary> transcribe <wav> --threads N --beam N --json`.
// `threads` <= 0 means auto (flag omitted); `beam` <= 0 means default 8.
std::string build_spoke_command(const std::string& binary, const std::string& wav_path,
                                int threads, int beam_width);

// Extract the "text" field from `spoke_cli ... --json` stdout:
// `{"text":"...","rtf":...,"segments":N}`. Returns "" when unparsable.
std::string parse_spoke_json_text(const std::string& json_output);

// Build the ffmpeg container-audio extraction command (pure, unit-testable).
std::string build_ffmpeg_command(const std::string& wav_out, const std::string& src);

// Build the ffmpeg microphone capture command for this OS (pure,
// unit-testable). `seconds` is clamped to 1..120. `mic` selects the device:
// Windows dshow audio device name, macOS avfoundation audio index (or full
// ":idx" spec), Linux ALSA device. Empty `mic` means the OS default.
std::string build_mic_command(const std::string& wav_out, int seconds, const std::string& mic);

// Record `seconds` of microphone audio to a 16 kHz mono WAV. Never throws;
// failures (no ffmpeg, bad device) report through `error`.
bool record_microphone(const std::string& wav_out, int seconds, const std::string& mic,
                       std::string& error);

// Run one transcription. Returns true with collapsed transcript text.
// Dispatches to the spoke or whisper backend per `transcription.engine`.
// Never throws; failures report through `error`.
bool transcribe_audio_file(const std::string& audio_path, const Config& cfg, std::string& out_text,
                           std::string& error, std::size_t max_chars = 65536);

// Write `<audio_path>.txt` next to the source file.
bool write_transcript_sidecar(const std::string& audio_path, const std::string& text,
                              std::string& error);

// Find indexed audio files whose path contains `needle` (case-insensitive).
std::vector<std::string> find_audio_in_index(IndexEngine& index, const std::string& needle,
                                             int limit = 8);

// Human install hint naming the missing piece for this OS.
// `missing` is "whisper" | "spoke" | "model" | "ffmpeg".
std::string transcribe_install_hint(const std::string& missing);

}  // namespace wilfred
