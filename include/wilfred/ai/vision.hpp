#pragma once

// Screen-aware AI helpers: turn a screenshot file into provider-ready
// vision input without any image library.
//
// - PNG/JPEG screenshots (macOS/Linux capture PNG) pass through untouched
//   when small enough for inline API payloads.
// - BMP screenshots (Windows capture) are decoded, box-downscaled to a sane
//   longest side, and re-encoded as PNG with stored (uncompressed) DEFLATE
//   blocks, using only core/crc32.hpp plus a tiny Adler-32.
// - Per-provider multimodal request bodies are pure string builders so unit
//   tests cover them without network access.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wilfred {

// Base64-encode raw bytes (standard alphabet with padding).
std::string base64_encode_bytes(const std::uint8_t* data, std::size_t size);

// Decode a 24-bit uncompressed BMP into row-major RGB. Handles bottom-up
// (positive height) and top-down (negative height) bitmaps.
bool decode_bmp_24(const std::uint8_t* data, std::size_t size, int& w, int& h,
                   std::vector<std::uint8_t>& rgb_out);

// Box-downscale row-major RGB in place so the longest side <= max_dim.
// No-op when already small enough or on degenerate input.
void downscale_box(std::vector<std::uint8_t>& rgb, int& w, int& h, int max_dim);

// Encode row-major RGB as a PNG (8-bit truecolor, stored DEFLATE blocks).
bool encode_png_rgb(int w, int h, const std::vector<std::uint8_t>& rgb,
                    std::vector<std::uint8_t>& png_out);

// Screenshot file -> PNG bytes ready for base64 + inline upload. BMP goes
// through decode/downscale/encode; PNG/JPEG pass through when under the
// byte cap. Returns false with a human error otherwise.
bool prepare_vision_image(const std::string& path, std::vector<std::uint8_t>& png_out,
                          std::string& mime_out, int max_dim, std::string& error);

// Multimodal request bodies mirroring AiAssistant::ask shapes.
std::string build_openai_vision_body(const std::string& model, const std::string& prompt,
                                     const std::string& image_b64, const std::string& mime,
                                     int max_tokens, double temperature);
std::string build_anthropic_vision_body(const std::string& model, const std::string& prompt,
                                        const std::string& image_b64, const std::string& mime,
                                        int max_tokens);
std::string build_gemini_vision_body(const std::string& prompt, const std::string& image_b64,
                                     const std::string& mime);

}  // namespace wilfred
