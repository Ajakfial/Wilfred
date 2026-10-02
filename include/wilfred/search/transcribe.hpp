#pragma once

// On-demand audio transcription for MP3s and MP4 (and other containers).
//
// Uses the `whisper.cpp` CLI (`whisper-cli` or `whisper`) when
// `transcription.enabled` is set, with `ffmpeg` for container audio
// extraction when needed. Cross-platform: shells out to those CLIs with a
// per-OS probe (`where` on Windows, `command -v` elsewhere) and returns
// false with a helpful error when a binary or model is missing. No link-time
// dependency on whisper/ffmpeg.
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
bool ffmpeg_available();

// Resolve the whisper binary: explicit `transcription.binary` first,
// otherwise the on-PATH probe. Empty when unavailable.
std::string resolve_whisper_binary(const Config& cfg);

// Resolve the model file: explicit `transcription.model` when it exists,
// otherwise conventional spots under the data directory. Empty when none.
std::string resolve_whisper_model(const Config& cfg);

// Build the whisper CLI invocation (pure, unit-testable).
// Produces `<out_base>.txt` next to the temp dir when run.
std::string build_whisper_command(const std::string& binary, const std::string& model,
                                  const std::string& wav_path, const std::string& out_base,
                                  const std::string& language);

// Build the ffmpeg container-audio extraction command (pure, unit-testable).
std::string build_ffmpeg_command(const std::string& wav_out, const std::string& src);

// Run one transcription. Returns true with collapsed transcript text.
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
std::string transcribe_install_hint(const std::string& missing);

}  // namespace wilfred
