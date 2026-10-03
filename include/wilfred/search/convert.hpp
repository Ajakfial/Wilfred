#pragma once

// File conversion (audio + image) and image background removal.
//
// Cross-platform by design: every OS backend in this repo (Windows, macOS,
// Linux, BSDs, Android, iOS) shares this single implementation, which uses
// only the C++ standard library plus the in-tree inflate/CRC helpers.
// No link-time dependency on ffmpeg, libpng, or an ML model:
//
// - Audio: WAV <-> WAV (resample / remix / bit-depth) and WAV <-> RAW are
//   fully native. Compressed formats (mp3, ogg, opus, flac, m4a/aac, wma,
//   aiff, au, webm/mp4 audio, ...) transcode through an optional `ffmpeg`
//   CLI when present, with a per-OS install hint otherwise (same pattern
//   as transcription / OCR).
// - Images: BMP / PNG / PPM-PGM / TGA decode + encode are fully native
//   (PNG via the in-tree inflate). JPEG / GIF / WebP / HEIC / TIFF go
//   through `ffmpeg` when present, otherwise report the missing tool.
// - Background removal is pure image processing (corner-sampled background
//   color + tolerance + border flood-fill with feathered alpha) producing a
//   transparent PNG. No network, no model download, works offline on every
//   OS including the mobile sandboxes.
//
// The overlay never converts at query time: the `convert` / `bgremove`
// minis resolve the target file and offer `convert_run` / `bgremove_run`
// actions, which write the output next to the source when Enter is pressed.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wilfred {

struct Config;
class IndexEngine;

// ---------------------------------------------------------------------------
// Format helpers (pure, unit-testable)
// ---------------------------------------------------------------------------

// "MP3" / ".mp3" / "mp3 " -> "mp3". Empty when blank.
std::string normalize_format_token(std::string s);
bool is_audio_format(const std::string& fmt);
bool is_image_format(const std::string& fmt);
bool is_known_convert_format(const std::string& fmt);
// Extension (with dot) for a format token, e.g. "mp3" -> ".mp3".
std::string default_ext_for_format(const std::string& fmt);

bool is_audio_convertible(const std::string& path);
bool is_image_convertible(const std::string& path);
bool is_convertible(const std::string& path);

// ffmpeg probe / hints (per-OS install text, portable shell-out).
bool ffmpeg_available_convert();
std::string convert_install_hint(const std::string& what);  // "ffmpeg"
std::string build_convert_ffmpeg_command(const std::string& src, const std::string& dst,
                                         int sample_rate = 0, int channels = 0);

// Destination resolution: `fmt_or_dst` may be a bare format ("mp3", ".png")
// or an explicit output path ("out.mp3", "/tmp/out.png").
// Returns the full output path (sibling of src when bare) or empty on error.
std::string resolve_convert_output(const std::string& src, const std::string& fmt_or_dst,
                                   std::string& error);

// ---------------------------------------------------------------------------
// Audio (native WAV <-> WAV / RAW, ffmpeg for the rest)
// ---------------------------------------------------------------------------

struct WavData {
  int sample_rate{44100};
  int channels{1};
  int bits_per_sample{16};
  // Normalized mono-interleaved float samples in [-1, 1].
  std::vector<float> samples;
};

bool decode_wav_bytes(const std::vector<std::uint8_t>& bytes, WavData& out,
                       std::string& error);
bool encode_wav_bytes(const WavData& wav, int bits_per_sample,
                      std::vector<std::uint8_t>& out, std::string& error);
// Resample (linear) + remix (mono<->stereo) a WAV in memory.
WavData resample_remix_wav(const WavData& in, int target_rate, int target_channels);

// High-level audio conversion. `dst` is a full output path.
// `sample_rate`/`channels`/`bits` are 0 to keep the source values
// (bits defaults to 16 for WAV output). `out_path` receives the file
// actually written. Never throws; failures report through `error`.
bool convert_audio_file(const std::string& src, const std::string& dst, int sample_rate,
                        int channels, int bits, std::string& out_path, std::string& error);

// ---------------------------------------------------------------------------
// Images (native BMP / PNG / PPM / TGA, ffmpeg for the rest)
// ---------------------------------------------------------------------------

struct ImageRgba {
  int w{0};
  int h{0};
  std::vector<std::uint8_t> rgba;  // row-major RGBA, size w*h*4
};

bool decode_png_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                       std::string& error);
bool decode_bmp_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                       std::string& error);
bool decode_ppm_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                       std::string& error);
bool decode_tga_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                       std::string& error);
// Tries PNG, then BMP, then PPM/PGM, then TGA. Pure, no filesystem.
bool decode_image_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                        std::string& error);

bool encode_png_rgba(const ImageRgba& img, std::vector<std::uint8_t>& out,
                     std::string& error);
bool encode_png_rgb(const ImageRgba& img, std::vector<std::uint8_t>& out,
                    std::string& error);
bool encode_bmp_bytes(const ImageRgba& img, std::vector<std::uint8_t>& out);
bool encode_ppm_bytes(const ImageRgba& img, std::vector<std::uint8_t>& out);
bool encode_tga_bytes(const ImageRgba& img, std::vector<std::uint8_t>& out,
                      std::string& error);
// Native encode by format token ("png", "bmp", "ppm", "tga", ...).
bool encode_image_native(const ImageRgba& img, const std::string& fmt,
                         std::vector<std::uint8_t>& out, std::string& error);

// Decode any supported image file (native first, ffmpeg temp PNG fallback
// for jpg/gif/webp/... when available). Never throws.
bool decode_image_file(const std::string& path, ImageRgba& out, std::string& error);
// Convert one image file to another. Native when both ends are
// bmp/png/ppm/pgm/tga, otherwise via ffmpeg. `out_path` gets the file.
bool convert_image_file(const std::string& src, const std::string& dst,
                        std::string& out_path, std::string& error);

// Dispatch audio vs image based on extensions (ffmpeg fallback inside).
bool convert_media_file(const std::string& src, const std::string& dst, int sample_rate,
                        int channels, int bits, std::string& out_path, std::string& error);

// ---------------------------------------------------------------------------
// Background removal (pure, native, transparent PNG output)
// ---------------------------------------------------------------------------

struct BgRemoveOptions {
  std::uint8_t r{255};
  std::uint8_t g{255};
  std::uint8_t b{255};
  bool has_color{false};  // false = auto-sample corners
  int tolerance{32};      // 0..100, default 32
  bool contiguous{true};   // true = border flood-fill, false = global chroma-key
  int feather{2};         // 0..8 feather pixels for smooth edges
};

bool parse_hex_color(const std::string& s, std::uint8_t& r, std::uint8_t& g,
                     std::uint8_t& b);
bool remove_background(const ImageRgba& src, const BgRemoveOptions& opts, ImageRgba& dst);
// `dst` empty means "<stem>.transparent.png" next to the source.
bool bgremove_file(const std::string& src, const std::string& dst,
                   const BgRemoveOptions& opts, std::string& out_path, std::string& error);

// ---------------------------------------------------------------------------
// Query parsing + index lookup (pure except index scan)
// ---------------------------------------------------------------------------

struct ConvertRequest {
  std::string src;
  std::string fmt;  // normalized, no dot ("mp3")
  std::string dst;  // explicit output path when given
  int sample_rate{0};
  int channels{0};  // 1 = mono, 2 = stereo
  int bits{0};      // 8 / 16 / 24 / 32
};

// `remainder` is the text after the `convert` keyword.
// Accepts "song.wav to mp3", "song.wav to out.mp3", "song.wav mp3",
// "song.wav -> ogg", "song.wav as wav 44100 stereo", ...
bool parse_convert_query(const std::string& remainder, ConvertRequest& out);

struct BgRemoveRequest {
  std::string src;
  std::string dst;
  BgRemoveOptions opts;
};

// Accepts "photo.png", "photo.png 40", "photo.png #00ff00 25 global",
// "photo.png white 30", ...
bool parse_bgremove_query(const std::string& remainder, BgRemoveRequest& out);

// Payloads carried in SearchResult::payload (newline-separated, no newlines
// in paths in practice; encode rejects them).
std::string encode_convert_payload(const std::string& src, const std::string& fmt,
                                   const std::string& dst, int rate, int channels);
bool decode_convert_payload(const std::string& payload, std::string& src, std::string& fmt,
                            std::string& dst, int& rate, int& channels);
std::string encode_bgremove_payload(const std::string& src, const BgRemoveOptions& opts,
                                    const std::string& dst);
bool decode_bgremove_payload(const std::string& payload, std::string& src,
                             BgRemoveOptions& opts, std::string& dst);

// Index scans for convertible candidates (path substring, case-insensitive).
std::vector<std::string> find_convertible_in_index(IndexEngine& index,
                                                   const std::string& needle, int limit = 8);
std::vector<std::string> find_image_in_index(IndexEngine& index, const std::string& needle,
                                             int limit = 8);

}  // namespace wilfred
