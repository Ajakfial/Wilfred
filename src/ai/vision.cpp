#include "wilfred/ai/vision.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/json.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace wilfred {
namespace {

std::uint32_t rd_u32le(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t rd_i32le(const std::uint8_t* p) {
  return static_cast<std::int32_t>(rd_u32le(p));
}

std::uint16_t rd_u16le(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

void put_u32be(std::vector<std::uint8_t>& o, std::uint32_t v) {
  o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
  o.push_back(static_cast<std::uint8_t>(v & 0xff));
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t size) {
  const std::uint32_t kMod = 65521;
  std::uint32_t a = 1, b = 0;
  for (std::size_t i = 0; i < size; ++i) {
    a = (a + data[i]) % kMod;
    b = (b + a) % kMod;
  }
  return (b << 16) | a;
}

void put_chunk(std::vector<std::uint8_t>& o, const char type[4], const std::uint8_t* data,
               std::size_t size) {
  put_u32be(o, static_cast<std::uint32_t>(size));
  std::uint32_t crc = crc32(type, 4);
  if (size) crc = crc32(data, size, crc);
  for (int i = 0; i < 4; ++i)
    o.push_back(type[i]);
  o.insert(o.end(), data, data + size);
  put_u32be(o, crc);
}

}  // namespace

std::string base64_encode_bytes(const std::uint8_t* data, std::size_t size) {
  static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve(((size + 2) / 3) * 4);
  std::size_t i = 0;
  // Main loop over complete triples.
  while (i + 3 <= size) {
    unsigned n = (static_cast<unsigned>(data[i]) << 16) |
                 (static_cast<unsigned>(data[i + 1]) << 8) | static_cast<unsigned>(data[i + 2]);
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(tbl[(n >> 6) & 63]);
    o.push_back(tbl[n & 63]);
    i += 3;
  }
  std::size_t rem = size - i;
  if (rem == 1) {
    unsigned n = static_cast<unsigned>(data[i]) << 16;
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back('=');
    o.push_back('=');
  } else if (rem == 2) {
    unsigned n = (static_cast<unsigned>(data[i]) << 16) | (static_cast<unsigned>(data[i + 1]) << 8);
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(tbl[(n >> 6) & 63]);
    o.push_back('=');
  }
  return o;
}

bool decode_bmp_24(const std::uint8_t* data, std::size_t size, int& w, int& h,
                   std::vector<std::uint8_t>& rgb_out) {
  rgb_out.clear();
  if (size < 54) return false;
  if (data[0] != 'B' || data[1] != 'M') return false;
  std::uint32_t off = rd_u32le(data + 10);
  std::uint32_t dib = rd_u32le(data + 14);
  if (dib < 40) return false;
  std::int32_t wi = rd_i32le(data + 18);
  std::int32_t hi = rd_i32le(data + 22);
  if (wi <= 0 || hi == 0 || wi > 32768 || hi > 32768 || hi < -32768) return false;
  if (rd_u16le(data + 26) != 1 || rd_u16le(data + 28) != 24) return false;
  if (rd_u32le(data + 30) != 0) return false;  // BI_RGB only
  int height = hi < 0 ? -hi : hi;
  bool top_down = hi < 0;
  std::size_t stride = ((static_cast<std::size_t>(wi) * 3 + 3) / 4) * 4;
  if (off + stride * static_cast<std::size_t>(height) > size) return false;
  rgb_out.resize(static_cast<std::size_t>(wi) * height * 3);
  for (int y = 0; y < height; ++y) {
    int src_y = top_down ? y : (height - 1 - y);
    const std::uint8_t* row = data + off + static_cast<std::size_t>(src_y) * stride;
    for (int x = 0; x < wi; ++x) {
      // BMP stores BGR; emit RGB.
      rgb_out[(static_cast<std::size_t>(y) * wi + x) * 3 + 0] = row[x * 3 + 2];
      rgb_out[(static_cast<std::size_t>(y) * wi + x) * 3 + 1] = row[x * 3 + 1];
      rgb_out[(static_cast<std::size_t>(y) * wi + x) * 3 + 2] = row[x * 3 + 0];
    }
  }
  w = wi;
  h = height;
  return true;
}

void downscale_box(std::vector<std::uint8_t>& rgb, int& w, int& h, int max_dim) {
  if (max_dim <= 0 || w <= 0 || h <= 0) return;
  int longest = w > h ? w : h;
  if (longest <= max_dim) return;
  int nw = std::max(1, (w * max_dim) / longest);
  int nh = std::max(1, (h * max_dim) / longest);
  std::vector<std::uint8_t> small(static_cast<std::size_t>(nw) * nh * 3);
  for (int y = 0; y < nh; ++y) {
    int y0 = (y * h) / nh;
    int y1 = ((y + 1) * h) / nh;
    if (y1 <= y0) y1 = y0 + 1;
    for (int x = 0; x < nw; ++x) {
      int x0 = (x * w) / nw;
      int x1 = ((x + 1) * w) / nw;
      if (x1 <= x0) x1 = x0 + 1;
      long r = 0, g = 0, b = 0;
      long n = 0;
      for (int sy = y0; sy < y1; ++sy)
        for (int sx = x0; sx < x1; ++sx) {
          auto* p = &rgb[(static_cast<std::size_t>(sy) * w + sx) * 3];
          r += p[0];
          g += p[1];
          b += p[2];
          ++n;
        }
      auto* d = &small[(static_cast<std::size_t>(y) * nw + x) * 3];
      d[0] = static_cast<std::uint8_t>(r / n);
      d[1] = static_cast<std::uint8_t>(g / n);
      d[2] = static_cast<std::uint8_t>(b / n);
    }
  }
  rgb.swap(small);
  w = nw;
  h = nh;
}

bool encode_png_rgb(int w, int h, const std::vector<std::uint8_t>& rgb,
                    std::vector<std::uint8_t>& png_out) {
  png_out.clear();
  if (w <= 0 || h <= 0 || w > 16383 || h > 16383) return false;
  if (rgb.size() < static_cast<std::size_t>(w) * h * 3) return false;
  static const std::uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  png_out.insert(png_out.end(), sig, sig + 8);
  std::uint8_t ihdr[13]{};
  ihdr[0] = static_cast<std::uint8_t>((w >> 24) & 0xff);
  ihdr[1] = static_cast<std::uint8_t>((w >> 16) & 0xff);
  ihdr[2] = static_cast<std::uint8_t>((w >> 8) & 0xff);
  ihdr[3] = static_cast<std::uint8_t>(w & 0xff);
  ihdr[4] = static_cast<std::uint8_t>((h >> 24) & 0xff);
  ihdr[5] = static_cast<std::uint8_t>((h >> 16) & 0xff);
  ihdr[6] = static_cast<std::uint8_t>((h >> 8) & 0xff);
  ihdr[7] = static_cast<std::uint8_t>(h & 0xff);
  ihdr[8] = 8;  // bit depth
  ihdr[9] = 2;  // truecolor
  put_chunk(png_out, "IHDR", ihdr, 13);
  // Raw scanlines with filter byte 0, wrapped in zlib (stored blocks).
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(w) * h * 3 + h);
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);
    raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(y) * w * 3,
               rgb.begin() + static_cast<std::ptrdiff_t>(y + 1) * w * 3);
  }
  std::vector<std::uint8_t> zlib;
  zlib.push_back(0x78);
  zlib.push_back(0x01);  // no compression, low window
  std::size_t pos = 0;
  while (pos < raw.size()) {
    std::size_t chunk = std::min<std::size_t>(raw.size() - pos, 65535);
    bool last = pos + chunk >= raw.size();
    zlib.push_back(last ? 0x01 : 0x00);
    auto len = static_cast<std::uint16_t>(chunk);
    zlib.push_back(static_cast<std::uint8_t>(len & 0xff));
    zlib.push_back(static_cast<std::uint8_t>((len >> 8) & 0xff));
    zlib.push_back(static_cast<std::uint8_t>((~len) & 0xff));
    zlib.push_back(static_cast<std::uint8_t>(((~len) >> 8) & 0xff));
    zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                raw.begin() + static_cast<std::ptrdiff_t>(pos + chunk));
    pos += chunk;
  }
  std::uint32_t ad = adler32(raw.data(), raw.size());
  zlib.push_back(static_cast<std::uint8_t>((ad >> 24) & 0xff));
  zlib.push_back(static_cast<std::uint8_t>((ad >> 16) & 0xff));
  zlib.push_back(static_cast<std::uint8_t>((ad >> 8) & 0xff));
  zlib.push_back(static_cast<std::uint8_t>(ad & 0xff));
  put_chunk(png_out, "IDAT", zlib.data(), zlib.size());
  put_chunk(png_out, "IEND", nullptr, 0);
  return true;
}

bool prepare_vision_image(const std::string& path, std::vector<std::uint8_t>& png_out,
                          std::string& mime_out, int max_dim, std::string& error) {
  png_out.clear();
  mime_out.clear();
  if (path.empty() || !file_exists(path)) {
    error = "screenshot file not found";
    return false;
  }
  std::string blob;
  if (!read_file_all(path, blob) || blob.empty()) {
    error = "could not read screenshot";
    return false;
  }
  auto ext = to_lower_utf8(path_extension(path));
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
    constexpr std::size_t kCap = 12ull * 1024 * 1024;
    if (blob.size() > kCap) {
      error = "screenshot too large for vision (use window capture)";
      return false;
    }
    png_out.assign(blob.begin(), blob.end());
    mime_out = ext == ".png" ? "image/png" : "image/jpeg";
    return true;
  }
  if (ext == ".bmp") {
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgb;
    if (!decode_bmp_24(reinterpret_cast<const std::uint8_t*>(blob.data()), blob.size(), w, h,
                       rgb)) {
      error = "could not decode screenshot";
      return false;
    }
    downscale_box(rgb, w, h, max_dim > 0 ? max_dim : 1568);
    if (!encode_png_rgb(w, h, rgb, png_out)) {
      error = "could not encode screenshot";
      return false;
    }
    mime_out = "image/png";
    return true;
  }
  error = "unsupported screenshot format (need png, jpg, or bmp)";
  return false;
}

std::string build_openai_vision_body(const std::string& model, const std::string& prompt,
                                     const std::string& image_b64, const std::string& mime,
                                     int max_tokens, double temperature) {
  char tmp[64];
  std::snprintf(tmp, sizeof(tmp), "%.2f", temperature);
  std::string m = json_escape(model);
  std::string p = json_escape(prompt);
  return "{\"model\":\"" + m + "\",\"max_tokens\":" + std::to_string(max_tokens) +
         ",\"temperature\":" + tmp +
         ",\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":"
         "\"text\",\"text\":\"" +
         p + "\"},{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:" + mime + ";base64," +
         image_b64 + "\"}}]}]}";
}

std::string build_anthropic_vision_body(const std::string& model, const std::string& prompt,
                                        const std::string& image_b64, const std::string& mime,
                                        int max_tokens) {
  std::string m = json_escape(model);
  std::string p = json_escape(prompt);
  return "{\"model\":\"" + m + "\",\"max_tokens\":" + std::to_string(max_tokens) +
         ",\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"" + p +
         "\"},{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":\"" + mime +
         "\",\"data\":\"" + image_b64 + "\"}}]}]}";
}

std::string build_gemini_vision_body(const std::string& prompt, const std::string& image_b64,
                                     const std::string& mime) {
  std::string p = json_escape(prompt);
  return "{\"contents\":[{\"parts\":[{\"text\":\"" + p + "\"},{\"inline_data\":{\"mime_type\":\"" +
         mime + "\",\"data\":\"" + image_b64 + "\"}}]}]}";
}

}  // namespace wilfred
