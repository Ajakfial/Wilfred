#include "wilfred/search/convert.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/updater/inflate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <queue>
#include <sstream>

namespace wilfred {
namespace fs = std::filesystem;

namespace {

std::string trim_c(const std::string& s) {
  std::string o = s;
  while (!o.empty() && (o.front() == ' ' || o.front() == '\t' || o.front() == '\n' ||
                        o.front() == '\r'))
    o.erase(o.begin());
  while (!o.empty() && (o.back() == ' ' || o.back() == '\t' || o.back() == '\n' ||
                        o.back() == '\r'))
    o.pop_back();
  return o;
}

std::string strip_quotes(const std::string& s) {
  auto t = trim_c(s);
  if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') ||
                        (t.front() == '\'' && t.back() == '\'')))
    return t.substr(1, t.size() - 2);
  return t;
}

std::string lower_c(const std::string& s) { return to_lower_utf8(s); }

std::string shell_quote_c(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"') o += "\\\"";
    else o.push_back(c);
  }
  o.push_back('"');
  return o;
}

int run_shell_c(const std::string& cmd) {
#if defined(WILFRED_IOS)
  (void)cmd;
  return 1;
#else
  return std::system(cmd.c_str());
#endif
}

bool tool_available_c(const std::string& name) {
  if (name.empty()) return false;
#ifdef _WIN32
  return run_shell_c("where " + name + " >NUL 2>NUL") == 0;
#else
  return run_shell_c("command -v " + name + " >/dev/null 2>&1") == 0;
#endif
}

bool read_bytes_file(const std::string& path, std::vector<std::uint8_t>& out) {
  out.clear();
  std::string text;
  if (!read_file_all(path, text)) return false;
  out.assign(text.begin(), text.end());
  return true;
}

bool write_bytes_file(const std::string& path, const std::uint8_t* data, std::size_t n) {
  auto parent = path_parent(path);
  if (!parent.empty()) create_directories(parent);
  return write_file_atomic(path, data, n);
}

bool write_bytes_file(const std::string& path, const std::vector<std::uint8_t>& v) {
  if (v.empty()) {
    auto parent = path_parent(path);
    if (!parent.empty()) create_directories(parent);
    return write_file_atomic(path, "", 0);
  }
  return write_bytes_file(path, v.data(), v.size());
}

std::uint16_t rd_u16le_c(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}
std::uint32_t rd_u32le_c(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::uint32_t rd_u32be_c(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}
void wr_u16le_c(std::vector<std::uint8_t>& o, std::uint16_t v) {
  o.push_back(static_cast<std::uint8_t>(v & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
}
void wr_u32le_c(std::vector<std::uint8_t>& o, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) o.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xff));
}
void wr_u32be_c(std::vector<std::uint8_t>& o, std::uint32_t v) {
  o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
  o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
  o.push_back(static_cast<std::uint8_t>(v & 0xff));
}

std::uint32_t adler32_c(const std::uint8_t* data, std::size_t n) {
  const std::uint32_t kMod = 65521;
  std::uint32_t a = 1, b = 0;
  for (std::size_t i = 0; i < n; ++i) {
    a = (a + data[i]) % kMod;
    b = (b + a) % kMod;
  }
  return (b << 16) | a;
}

void png_put_chunk(std::vector<std::uint8_t>& o, const char type[4], const std::uint8_t* data,
                   std::size_t n) {
  wr_u32be_c(o, static_cast<std::uint32_t>(n));
  std::uint32_t crc = crc32(type, 4);
  if (n) crc = crc32(data, n, crc);
  for (int i = 0; i < 4; ++i) o.push_back(static_cast<std::uint8_t>(type[i]));
  if (n) o.insert(o.end(), data, data + n);
  wr_u32be_c(o, crc);
}

void zlib_wrap_stored(const std::vector<std::uint8_t>& raw, std::vector<std::uint8_t>& zlib) {
  zlib.clear();
  zlib.push_back(0x78);
  zlib.push_back(0x01);
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
  std::uint32_t ad = raw.empty() ? 1 : adler32_c(raw.data(), raw.size());
  zlib.push_back(static_cast<std::uint8_t>((ad >> 24) & 0xff));
  zlib.push_back(static_cast<std::uint8_t>((ad >> 16) & 0xff));
  zlib.push_back(static_cast<std::uint8_t>((ad >> 8) & 0xff));
  zlib.push_back(static_cast<std::uint8_t>(ad & 0xff));
}

bool zlib_unwrap_to_deflate(const std::vector<std::uint8_t>& zlib, std::vector<std::uint8_t>& raw,
                            std::string& error) {
  raw.clear();
  if (zlib.size() < 6) {
    error = "bad png data";
    return false;
  }
  // Validate zlib header when it looks like one; otherwise assume raw deflate.
  unsigned cmf = zlib[0], flg = zlib[1];
  std::size_t start = 0;
  std::size_t end = zlib.size();
  if ((cmf & 0x0f) == 8 && ((cmf * 256u + flg) % 31u) == 0) {
    start = 2;
    end = zlib.size() - 4;  // strip adler32
    if (end <= start) {
      error = "bad png data";
      return false;
    }
  }
  if (!inflate_decompress(zlib.data() + start, end - start, raw)) {
    error = "could not decompress png";
    return false;
  }
  return true;
}

std::uint8_t paeth_c(int a, int b, int c) {
  int p = a + b - c;
  int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
  if (pb <= pc) return static_cast<std::uint8_t>(b);
  return static_cast<std::uint8_t>(c);
}

}  // namespace

// ---------------------------------------------------------------------------
// Formats
// ---------------------------------------------------------------------------

std::string normalize_format_token(std::string s) {
  s = trim_c(s);
  while (!s.empty() && s.front() == '.') s.erase(s.begin());
  s = trim_c(s);
  std::string o;
  for (char c : s) {
    if ((c >= 'A' && c <= 'Z')) o.push_back(static_cast<char>(c + 32));
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-')
      o.push_back(c);
    // drop other punctuation / whitespace
  }
  if (o == "jpeg") return "jpg";
  if (o == "tif") return "tiff";
  if (o == "aif") return "aiff";
  if (o == "mpeg3") return "mp3";
  if (o == "wave") return "wav";
  return o;
}

bool is_audio_format(const std::string& fmt) {
  auto f = normalize_format_token(fmt);
  return f == "mp3" || f == "wav" || f == "ogg" || f == "oga" || f == "opus" ||
         f == "flac" || f == "m4a" || f == "aac" || f == "wma" || f == "aiff" ||
         f == "aif" || f == "au" || f == "snd" || f == "raw" || f == "pcm" ||
         f == "webm" || f == "mp4" || f == "mkv" || f == "caf";
}

bool is_image_format(const std::string& fmt) {
  auto f = normalize_format_token(fmt);
  return f == "png" || f == "jpg" || f == "bmp" || f == "ppm" || f == "pgm" ||
         f == "tga" || f == "gif" || f == "webp" || f == "tiff" || f == "heic" ||
         f == "heif" || f == "avif" || f == "ico" || f == "qoi";
}

bool is_known_convert_format(const std::string& fmt) {
  auto f = normalize_format_token(fmt);
  return is_audio_format(f) || is_image_format(f);
}

std::string default_ext_for_format(const std::string& fmt) {
  auto f = normalize_format_token(fmt);
  if (f.empty() || !is_known_convert_format(f)) return {};
  if (f == "pcm") return ".raw";
  if (f == "aif") return ".aiff";
  if (f == "snd") return ".au";
  return "." + f;
}

bool is_audio_convertible(const std::string& path) {
  return is_audio_format(lower_c(path_extension(path)));
}

bool is_image_convertible(const std::string& path) {
  auto e = lower_c(path_extension(path));
  if (e.empty()) return false;
  if (!e.empty() && e[0] == '.') e = e.substr(1);
  return is_image_format(e);
}

bool is_convertible(const std::string& path) {
  return is_audio_convertible(path) || is_image_convertible(path);
}

bool ffmpeg_available_convert() { return tool_available_c("ffmpeg"); }

std::string convert_install_hint(const std::string& what) {
  (void)what;
#ifdef _WIN32
  return "Install with: winget install ffmpeg  (or choco install ffmpeg)";
#elif defined(__APPLE__)
  return "Install with: brew install ffmpeg";
#else
  return "Install with: sudo apt install ffmpeg  (or your distro equivalent)";
#endif
}

std::string build_convert_ffmpeg_command(const std::string& src, const std::string& dst,
                                         int sample_rate, int channels) {
  std::string cmd = "ffmpeg -y -v error -i " + shell_quote_c(src);
  if (sample_rate > 0) cmd += " -ar " + std::to_string(sample_rate);
  if (channels == 1 || channels == 2) cmd += " -ac " + std::to_string(channels);
  cmd += " " + shell_quote_c(dst);
#ifdef _WIN32
  cmd += " 2>NUL";
#else
  cmd += " 2>/dev/null";
#endif
  return cmd;
}

std::string resolve_convert_output(const std::string& src, const std::string& fmt_or_dst,
                                   std::string& error) {
  error.clear();
  if (src.empty()) {
    error = "No source file";
    return {};
  }
  auto want = trim_c(fmt_or_dst);
  if (want.empty()) {
    error = "No output format (try convert <file> to mp3)";
    return {};
  }
  auto parent = path_parent(src);
  auto stem = path_stem(src);
  if (stem.empty()) stem = "converted";
  auto looks_like_path = want.find('/') != std::string::npos || want.find('\\') != std::string::npos;
  if (looks_like_path) {
    auto ext = lower_c(path_extension(want));
    std::string e = ext;
    if (!e.empty() && e[0] == '.') e = e.substr(1);
    if (!is_known_convert_format(e)) {
      error = "Unknown output format in " + want;
      return {};
    }
    return want;
  }
  if (!want.empty() && want.front() == '.') {
    auto f = normalize_format_token(want);
    if (!is_known_convert_format(f)) {
      error = "Unknown format " + want;
      return {};
    }
    std::string cand = path_join(parent, stem + default_ext_for_format(f));
    if (parent.empty()) cand = stem + default_ext_for_format(f);
    // Avoid clobbering the source when converting wav->wav in place.
    std::string lc = lower_c(cand), ls = lower_c(src);
    if (lc == ls) {
      for (int i = 2; i < 10000; ++i) {
        std::string c2 = path_join(parent, stem + " " + std::to_string(i) + default_ext_for_format(f));
        if (parent.empty()) c2 = stem + " " + std::to_string(i) + default_ext_for_format(f);
        if (!file_exists(c2)) return c2;
      }
      error = "Could not pick an output name";
      return {};
    }
    if (file_exists(cand)) {
      for (int i = 2; i < 10000; ++i) {
        std::string c2 = path_join(parent, stem + " " + std::to_string(i) + default_ext_for_format(f));
        if (parent.empty()) c2 = stem + " " + std::to_string(i) + default_ext_for_format(f);
        if (!file_exists(c2)) return c2;
      }
    }
    return cand;
  }
  auto dot = want.rfind('.');
  if (dot != std::string::npos) {
    std::string after = want.substr(dot + 1);
    std::string f = normalize_format_token(after);
    if (is_known_convert_format(f) && dot > 0) {
      // Explicit filename ("out.mp3") in the source directory.
      std::string name = trim_c(want);
      std::string cand = parent.empty() ? name : path_join(parent, name);
      return cand;
    }
    error = "Unknown format " + want;
    return {};
  }
  auto f = normalize_format_token(want);
  if (!is_known_convert_format(f)) {
    error = "Unknown format " + want + " (try mp3, wav, ogg, flac, png, jpg, bmp)";
    return {};
  }
  std::string cand = path_join(parent, stem + default_ext_for_format(f));
  if (parent.empty()) cand = stem + default_ext_for_format(f);
  std::string lc = lower_c(cand), ls = lower_c(src);
  if (lc == ls || file_exists(cand)) {
    for (int i = 2; i < 10000; ++i) {
      std::string c2 = path_join(parent, stem + " " + std::to_string(i) + default_ext_for_format(f));
      if (parent.empty()) c2 = stem + " " + std::to_string(i) + default_ext_for_format(f);
      if (!file_exists(c2)) return c2;
    }
  }
  return cand;
}

// ---------------------------------------------------------------------------
// WAV
// ---------------------------------------------------------------------------

bool decode_wav_bytes(const std::vector<std::uint8_t>& bytes, WavData& out,
                      std::string& error) {
  error.clear();
  out = WavData{};
  if (bytes.size() < 44) {
    error = "not a wav file (too small)";
    return false;
  }
  if (bytes[0] != 'R' || bytes[1] != 'I' || bytes[2] != 'F' || bytes[3] != 'F' ||
      bytes[8] != 'W' || bytes[9] != 'A' || bytes[10] != 'V' || bytes[11] != 'E') {
    error = "not a wav file (bad header)";
    return false;
  }
  int audio_format = 0, channels = 0, sample_rate = 0, bits = 0;
  const std::uint8_t* data_ptr = nullptr;
  std::size_t data_len = 0;
  std::size_t pos = 12;
  int fmt_tag = 0;
  while (pos + 8 <= bytes.size()) {
    char id[4] = {static_cast<char>(bytes[pos]), static_cast<char>(bytes[pos + 1]),
                  static_cast<char>(bytes[pos + 2]), static_cast<char>(bytes[pos + 3])};
    std::uint32_t sz = rd_u32le_c(bytes.data() + pos + 4);
    std::size_t body = pos + 8;
    if (body + sz > bytes.size()) break;  // truncated; stop
    if (id[0] == 'f' && id[1] == 'm' && id[2] == 't' && id[3] == ' ') {
      if (sz < 16) {
        error = "bad wav fmt chunk";
        return false;
      }
      audio_format = rd_u16le_c(bytes.data() + body);
      channels = rd_u16le_c(bytes.data() + body + 2);
      sample_rate = static_cast<int>(rd_u32le_c(bytes.data() + body + 4));
      bits = rd_u16le_c(bytes.data() + body + 14);
      fmt_tag = audio_format;
      if (audio_format == 0xFFFE && sz >= 40) {
        // WAVE_FORMAT_EXTENSIBLE: SubFormat GUID first word is the real tag.
        fmt_tag = rd_u16le_c(bytes.data() + body + 24);
      }
    } else if (id[0] == 'd' && id[1] == 'a' && id[2] == 't' && id[3] == 'a') {
      data_ptr = bytes.data() + body;
      data_len = sz;
      // keep first data chunk
      if (data_ptr) break;
    }
    pos = body + sz + (sz & 1);
  }
  if (channels <= 0 || channels > 32) {
    error = "unsupported wav channels";
    return false;
  }
  if (sample_rate < 1000 || sample_rate > 384000) {
    error = "unsupported wav sample rate";
    return false;
  }
  if (bits != 8 && bits != 16 && bits != 24 && bits != 32) {
    error = "unsupported wav bit depth (need 8/16/24/32)";
    return false;
  }
  if (fmt_tag != 1 && fmt_tag != 3) {
    error = "unsupported wav encoding (need PCM or float)";
    return false;
  }
  if (fmt_tag == 3 && bits != 32) {
    error = "unsupported wav float depth (need 32-bit)";
    return false;
  }
  if (!data_ptr) {
    error = "wav has no audio data";
    return false;
  }
  std::size_t frame_bytes = static_cast<std::size_t>(channels) * static_cast<std::size_t>(bits / 8);
  if (frame_bytes == 0) {
    error = "bad wav format";
    return false;
  }
  std::size_t frames = data_len / frame_bytes;
  if (frames == 0) {
    error = "wav has no audio frames";
    return false;
  }
  if (frames > 100000000ull) {
    error = "wav file too large to convert";
    return false;
  }
  out.sample_rate = sample_rate;
  out.channels = channels;
  out.bits_per_sample = bits;
  out.samples.resize(frames * static_cast<std::size_t>(channels));
  for (std::size_t f = 0; f < frames; ++f) {
    const std::uint8_t* fr = data_ptr + f * frame_bytes;
    for (int c = 0; c < channels; ++c) {
      const std::uint8_t* s = fr + static_cast<std::size_t>(c) * static_cast<std::size_t>(bits / 8);
      float v = 0;
      if (fmt_tag == 3) {
        float fv = 0;
        std::memcpy(&fv, s, 4);
        v = std::max(-1.0f, std::min(1.0f, fv));
      } else if (bits == 8) {
        v = (static_cast<int>(s[0]) - 128) / 128.0f;
      } else if (bits == 16) {
        std::int16_t iv = static_cast<std::int16_t>(s[0] | (s[1] << 8));
        v = iv / 32768.0f;
      } else if (bits == 24) {
        std::int32_t iv = static_cast<std::int32_t>(s[0] | (s[1] << 8) | (s[2] << 16));
        if (iv & 0x800000) iv |= ~0xFFFFFF;
        v = iv / 8388608.0f;
      } else {
        std::int32_t iv = static_cast<std::int32_t>(rd_u32le_c(s));
        v = iv / 2147483648.0f;
      }
      out.samples[f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] = v;
    }
  }
  return true;
}

bool encode_wav_bytes(const WavData& wav, int bits_per_sample,
                      std::vector<std::uint8_t>& out, std::string& error) {
  error.clear();
  out.clear();
  if (wav.sample_rate < 1000 || wav.sample_rate > 384000) {
    error = "bad sample rate";
    return false;
  }
  if (wav.channels < 1 || wav.channels > 32) {
    error = "bad channel count";
    return false;
  }
  if (bits_per_sample != 8 && bits_per_sample != 16 && bits_per_sample != 24 &&
      bits_per_sample != 32) {
    error = "bad bit depth (need 8/16/24/32)";
    return false;
  }
  std::size_t frames = wav.channels ? wav.samples.size() / static_cast<std::size_t>(wav.channels) : 0;
  if (wav.samples.size() % static_cast<std::size_t>(wav.channels) != 0) {
    error = "bad sample layout";
    return false;
  }
  if (frames > 100000000ull) {
    error = "audio too large to encode";
    return false;
  }
  std::size_t frame_bytes = static_cast<std::size_t>(wav.channels) * static_cast<std::size_t>(bits_per_sample / 8);
  std::size_t data_bytes = frames * frame_bytes;
  if (data_bytes > 0xFFFFFFF0ull) {
    error = "audio too large to encode";
    return false;
  }
  out.reserve(44 + data_bytes);
  out.push_back('R');
  out.push_back('I');
  out.push_back('F');
  out.push_back('F');
  wr_u32le_c(out, static_cast<std::uint32_t>(36 + data_bytes));
  out.push_back('W');
  out.push_back('A');
  out.push_back('V');
  out.push_back('E');
  out.push_back('f');
  out.push_back('m');
  out.push_back('t');
  out.push_back(' ');
  wr_u32le_c(out, 16);
  wr_u16le_c(out, 1);
  wr_u16le_c(out, static_cast<std::uint16_t>(wav.channels));
  wr_u32le_c(out, static_cast<std::uint32_t>(wav.sample_rate));
  wr_u32le_c(out, static_cast<std::uint32_t>(wav.sample_rate) * static_cast<std::uint32_t>(frame_bytes));
  wr_u16le_c(out, static_cast<std::uint16_t>(frame_bytes));
  wr_u16le_c(out, static_cast<std::uint16_t>(bits_per_sample));
  out.push_back('d');
  out.push_back('a');
  out.push_back('t');
  out.push_back('a');
  wr_u32le_c(out, static_cast<std::uint32_t>(data_bytes));
  for (std::size_t i = 0; i < wav.samples.size(); ++i) {
    float v = std::max(-1.0f, std::min(1.0f, wav.samples[i]));
    if (bits_per_sample == 8) {
      int q = static_cast<int>(std::lround(v * 127.0f)) + 128;
      out.push_back(static_cast<std::uint8_t>(std::max(0, std::min(255, q))));
    } else if (bits_per_sample == 16) {
      int q = static_cast<int>(std::lround(v * 32767.0f));
      q = std::max(-32768, std::min(32767, q));
      out.push_back(static_cast<std::uint8_t>(q & 0xff));
      out.push_back(static_cast<std::uint8_t>((q >> 8) & 0xff));
    } else if (bits_per_sample == 24) {
      int q = static_cast<int>(std::lround(v * 8388607.0f));
      q = std::max(-8388608, std::min(8388607, q));
      std::uint32_t u = static_cast<std::uint32_t>(q) & 0xFFFFFFu;
      out.push_back(static_cast<std::uint8_t>(u & 0xff));
      out.push_back(static_cast<std::uint8_t>((u >> 8) & 0xff));
      out.push_back(static_cast<std::uint8_t>((u >> 16) & 0xff));
    } else {
      long long q = std::llround(static_cast<double>(v) * 2147483647.0);
      if (q < -2147483648ll) q = -2147483648ll;
      if (q > 2147483647ll) q = 2147483647ll;
      std::uint32_t u = static_cast<std::uint32_t>(q);
      wr_u32le_c(out, u);
    }
  }
  return true;
}

WavData resample_remix_wav(const WavData& in, int target_rate, int target_channels) {
  WavData out = in;
  int rate = target_rate > 0 ? target_rate : in.sample_rate;
  int ch = target_channels > 0 ? target_channels : in.channels;
  if (rate < 1000) rate = in.sample_rate;
  if (rate > 384000) rate = 384000;
  if (ch < 1) ch = in.channels;
  if (ch > 32) ch = 32;
  std::size_t in_frames = in.channels ? in.samples.size() / static_cast<std::size_t>(in.channels) : 0;
  std::size_t out_frames = in_frames;
  if (rate != in.sample_rate && in_frames > 0) {
    double ratio = static_cast<double>(rate) / static_cast<double>(in.sample_rate);
    out_frames = static_cast<std::size_t>(static_cast<double>(in_frames) * ratio);
    if (out_frames == 0) out_frames = 1;
    if (out_frames > 100000000ull) out_frames = 100000000ull;
  }
  std::vector<float> resampled;
  resampled.reserve(out_frames * static_cast<std::size_t>(in.channels));
  if (rate == in.sample_rate) {
    resampled = in.samples;
  } else if (in_frames > 0) {
    for (std::size_t f = 0; f < out_frames; ++f) {
      double pos = static_cast<double>(f) * static_cast<double>(in.sample_rate) / static_cast<double>(rate);
      std::size_t i0 = static_cast<std::size_t>(pos);
      double frac = pos - static_cast<double>(i0);
      if (i0 >= in_frames) i0 = in_frames - 1;
      std::size_t i1 = i0 + 1 < in_frames ? i0 + 1 : i0;
      for (int c = 0; c < in.channels; ++c) {
        float a = in.samples[i0 * static_cast<std::size_t>(in.channels) + static_cast<std::size_t>(c)];
        float b = in.samples[i1 * static_cast<std::size_t>(in.channels) + static_cast<std::size_t>(c)];
        resampled.push_back(static_cast<float>(a + (b - a) * frac));
      }
    }
  }
  std::vector<float> remixed;
  remixed.reserve(out_frames * static_cast<std::size_t>(ch));
  for (std::size_t f = 0; f < out_frames; ++f) {
    if (ch == in.channels) {
      for (int c = 0; c < ch; ++c)
        remixed.push_back(resampled[f * static_cast<std::size_t>(in.channels) + static_cast<std::size_t>(c)]);
    } else if (ch == 1) {
      double sum = 0;
      for (int c = 0; c < in.channels; ++c)
        sum += resampled[f * static_cast<std::size_t>(in.channels) + static_cast<std::size_t>(c)];
      remixed.push_back(static_cast<float>(sum / static_cast<double>(in.channels)));
    } else if (in.channels == 1) {
      float m = resampled[f];
      for (int c = 0; c < ch; ++c) remixed.push_back(m);
    } else {
      for (int c = 0; c < ch; ++c)
        remixed.push_back(resampled[f * static_cast<std::size_t>(in.channels) +
                                    static_cast<std::size_t>(c % in.channels)]);
    }
  }
  out.sample_rate = rate;
  out.channels = ch;
  out.samples.swap(remixed);
  return out;
}

bool convert_audio_file(const std::string& src, const std::string& dst, int sample_rate,
                        int channels, int bits, std::string& out_path, std::string& error) {
  error.clear();
  out_path.clear();
  if (src.empty() || !file_exists(src)) {
    error = "Audio file not found: " + src;
    return false;
  }
  if (dst.empty()) {
    error = "No output path";
    return false;
  }
  if (channels != 0 && channels != 1 && channels != 2) {
    if (channels < 1 || channels > 32) {
      error = "Bad channel count (use 1 for mono, 2 for stereo)";
      return false;
    }
  }
  if (bits != 0 && bits != 8 && bits != 16 && bits != 24 && bits != 32) {
    error = "Bad bit depth (use 8, 16, 24, or 32)";
    return false;
  }
  auto src_ext = lower_c(path_extension(src));
  auto dst_ext = lower_c(path_extension(dst));
  auto norm = [](std::string e) {
    if (!e.empty() && e[0] == '.') e = e.substr(1);
    return normalize_format_token(e);
  };
  std::string sf = norm(src_ext), df = norm(dst_ext);
  bool src_wav = sf == "wav";
  bool dst_wav = df == "wav";
  bool src_raw = sf == "raw" || sf == "pcm";
  bool dst_raw = df == "raw" || df == "pcm";

  if ((src_wav && dst_wav) || (src_wav && dst_raw) || (src_raw && dst_wav)) {
    WavData wav;
    if (src_wav) {
      std::vector<std::uint8_t> bytes;
      if (!read_bytes_file(src, bytes)) {
        error = "Could not read " + src;
        return false;
      }
      if (!decode_wav_bytes(bytes, wav, error)) return false;
    } else {
      // RAW s16le assumed; rate/channels/bits select the interpretation.
      int rate = sample_rate > 0 ? sample_rate : 44100;
      int ch = channels > 0 ? channels : 1;
      int b = bits > 0 ? bits : 16;
      if (b != 8 && b != 16) {
        error = "RAW input supports 8 or 16-bit (pass bits via CLI)";
        return false;
      }
      std::vector<std::uint8_t> bytes;
      if (!read_bytes_file(src, bytes)) {
        error = "Could not read " + src;
        return false;
      }
      std::size_t per = static_cast<std::size_t>(b / 8) * static_cast<std::size_t>(ch);
      if (per == 0 || bytes.size() % per != 0) {
        error = "RAW size is not a whole number of frames";
        return false;
      }
      std::size_t frames = bytes.size() / per;
      if (frames > 100000000ull) {
        error = "Audio file too large to convert";
        return false;
      }
      wav.sample_rate = rate;
      wav.channels = ch;
      wav.bits_per_sample = b;
      wav.samples.resize(frames * static_cast<std::size_t>(ch));
      for (std::size_t f = 0; f < frames; ++f)
        for (int c = 0; c < ch; ++c) {
          const std::uint8_t* s = bytes.data() + (f * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c)) *
                                                     static_cast<std::size_t>(b / 8);
          float v = 0;
          if (b == 8) v = (static_cast<int>(s[0]) - 128) / 128.0f;
          else {
            std::int16_t iv = static_cast<std::int16_t>(s[0] | (s[1] << 8));
            v = iv / 32768.0f;
          }
          wav.samples[f * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c)] = v;
        }
      // RAW input already at the requested rate/channels; clear them so we
      // do not resample twice below.
      sample_rate = 0;
      channels = 0;
    }
    if (sample_rate > 0 || channels > 0) wav = resample_remix_wav(wav, sample_rate, channels);
    if (dst_raw) {
      int b = bits > 0 ? bits : 16;
      if (b != 8 && b != 16) {
        error = "RAW output supports 8 or 16-bit";
        return false;
      }
      std::vector<std::uint8_t> raw;
      raw.reserve(wav.samples.size() * static_cast<std::size_t>(b / 8));
      for (float v : wav.samples) {
        v = std::max(-1.0f, std::min(1.0f, v));
        if (b == 8) {
          int q = static_cast<int>(std::lround(v * 127.0f)) + 128;
          raw.push_back(static_cast<std::uint8_t>(std::max(0, std::min(255, q))));
        } else {
          int q = static_cast<int>(std::lround(v * 32767.0f));
          q = std::max(-32768, std::min(32767, q));
          raw.push_back(static_cast<std::uint8_t>(q & 0xff));
          raw.push_back(static_cast<std::uint8_t>((q >> 8) & 0xff));
        }
      }
      if (!write_bytes_file(dst, raw)) {
        error = "Could not write " + dst;
        return false;
      }
      out_path = dst;
      return true;
    }
    int b = bits > 0 ? bits : wav.bits_per_sample;
    if (b != 8 && b != 16 && b != 24 && b != 32) b = 16;
    std::vector<std::uint8_t> enc;
    if (!encode_wav_bytes(wav, b, enc, error)) return false;
    if (!write_bytes_file(dst, enc)) {
      error = "Could not write " + dst;
      return false;
    }
    out_path = dst;
    return true;
  }

  if (!ffmpeg_available_convert()) {
    error = "Converting " + sf + " to " + df + " needs ffmpeg. " + convert_install_hint("ffmpeg");
    return false;
  }
  auto parent = path_parent(dst);
  if (!parent.empty()) create_directories(parent);
  if (run_shell_c(build_convert_ffmpeg_command(src, dst, sample_rate, channels).c_str()) != 0 ||
      !file_exists(dst)) {
    error = "ffmpeg could not convert " + src + " to " + dst;
    return false;
  }
  out_path = dst;
  return true;
}

// ---------------------------------------------------------------------------
// Images: decoders
// ---------------------------------------------------------------------------

bool decode_bmp_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                      std::string& error) {
  error.clear();
  out = ImageRgba{};
  if (size < 54 || data[0] != 'B' || data[1] != 'M') {
    error = "not a bmp file";
    return false;
  }
  std::uint32_t off = rd_u32le_c(data + 10);
  std::uint32_t dib = rd_u32le_c(data + 14);
  int wi = 0, hi = 0, bpp = 0;
  std::uint32_t comp = 0;
  if (dib == 12) {
    if (size < 26) {
      error = "bad bmp header";
      return false;
    }
    wi = rd_u16le_c(data + 18);
    hi = static_cast<int>(static_cast<std::int16_t>(rd_u16le_c(data + 20)));
    if (rd_u16le_c(data + 22) != 1) {
      error = "bad bmp planes";
      return false;
    }
    bpp = rd_u16le_c(data + 24);
  } else {
    if (dib < 40 || size < 54) {
      error = "bad bmp header";
      return false;
    }
    wi = static_cast<int>(static_cast<std::int32_t>(rd_u32le_c(data + 18)));
    hi = static_cast<int>(static_cast<std::int32_t>(rd_u32le_c(data + 22)));
    if (rd_u16le_c(data + 26) != 1) {
      error = "bad bmp planes";
      return false;
    }
    bpp = rd_u16le_c(data + 28);
    comp = rd_u32le_c(data + 30);
  }
  if (wi <= 0 || hi == 0 || wi > 16383 || hi > 16383 || hi < -16383) {
    error = "bad bmp dimensions";
    return false;
  }
  if (bpp != 24 && bpp != 32) {
    error = "unsupported bmp depth (need 24 or 32-bit)";
    return false;
  }
  if (comp != 0 && !(comp == 3 && bpp == 32)) {
    error = "unsupported bmp compression";
    return false;
  }
  int h = hi < 0 ? -hi : hi;
  bool top_down = hi < 0;
  std::size_t stride = ((static_cast<std::size_t>(wi) * static_cast<std::size_t>(bpp) + 31) / 32) * 4;
  if (off + stride * static_cast<std::size_t>(h) > size) {
    error = "truncated bmp file";
    return false;
  }
  if (static_cast<std::uint64_t>(wi) * static_cast<std::uint64_t>(h) > 100000000ull) {
    error = "image too large";
    return false;
  }
  out.w = wi;
  out.h = h;
  out.rgba.resize(static_cast<std::size_t>(wi) * static_cast<std::size_t>(h) * 4);
  for (int y = 0; y < h; ++y) {
    int src_y = top_down ? y : (h - 1 - y);
    const std::uint8_t* row = data + off + static_cast<std::size_t>(src_y) * stride;
    for (int x = 0; x < wi; ++x) {
      std::uint8_t* d = &out.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(wi) +
                                   static_cast<std::size_t>(x)) * 4];
      if (bpp == 24) {
        d[0] = row[x * 3 + 2];
        d[1] = row[x * 3 + 1];
        d[2] = row[x * 3 + 0];
        d[3] = 255;
      } else {
        d[0] = row[x * 4 + 2];
        d[1] = row[x * 4 + 1];
        d[2] = row[x * 4 + 0];
        d[3] = row[x * 4 + 3];
      }
    }
  }
  return true;
}

bool decode_ppm_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                      std::string& error) {
  error.clear();
  out = ImageRgba{};
  if (size < 4 || data[0] != 'P' || (data[1] != '5' && data[1] != '6' && data[1] != '2' && data[1] != '3')) {
    error = "not a ppm/pgm file";
    return false;
  }
  bool gray = data[1] == '5' || data[1] == '2';
  bool ascii = data[1] == '2' || data[1] == '3';
  std::size_t pos = 2;
  auto skip_ws_comments = [&]() {
    while (pos < size) {
      if (data[pos] == '#') {
        while (pos < size && data[pos] != '\n') ++pos;
      } else if (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r') {
        ++pos;
      } else break;
    }
  };
  auto read_int = [&](int& v) -> bool {
    skip_ws_comments();
    if (pos >= size || data[pos] < '0' || data[pos] > '9') return false;
    long acc = 0;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') {
      acc = acc * 10 + (data[pos] - '0');
      if (acc > 100000) return false;
      ++pos;
    }
    v = static_cast<int>(acc);
    return true;
  };
  int w = 0, h = 0, maxv = 0;
  if (!read_int(w) || !read_int(h) || !read_int(maxv)) {
    error = "bad ppm header";
    return false;
  }
  if (w <= 0 || h <= 0 || w > 16383 || h > 16383) {
    error = "bad ppm dimensions";
    return false;
  }
  if (maxv != 255) {
    error = "unsupported ppm maxval (need 255)";
    return false;
  }
  if (static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h) > 100000000ull) {
    error = "image too large";
    return false;
  }
  out.w = w;
  out.h = h;
  out.rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
  if (ascii) {
    for (int i = 0; i < w * h; ++i) {
      int r = 0, g = 0, b = 0;
      if (gray) {
        if (!read_int(r)) {
          error = "truncated ppm file";
          return false;
        }
        g = b = r;
      } else {
        if (!read_int(r) || !read_int(g) || !read_int(b)) {
          error = "truncated ppm file";
          return false;
        }
      }
      if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        error = "bad ppm sample";
        return false;
      }
      std::uint8_t* d = &out.rgba[static_cast<std::size_t>(i) * 4];
      d[0] = static_cast<std::uint8_t>(r);
      d[1] = static_cast<std::uint8_t>(g);
      d[2] = static_cast<std::uint8_t>(b);
      d[3] = 255;
    }
    return true;
  }
  // Single whitespace after maxval.
  if (pos < size && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r'))
    ++pos;
  else {
    error = "bad ppm header";
    return false;
  }
  std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * (gray ? 1u : 3u);
  if (pos + need > size) {
    error = "truncated ppm file";
    return false;
  }
  for (int i = 0; i < w * h; ++i) {
    std::uint8_t* d = &out.rgba[static_cast<std::size_t>(i) * 4];
    if (gray) {
      d[0] = d[1] = d[2] = data[pos + static_cast<std::size_t>(i)];
      d[3] = 255;
    } else {
      d[0] = data[pos + static_cast<std::size_t>(i) * 3];
      d[1] = data[pos + static_cast<std::size_t>(i) * 3 + 1];
      d[2] = data[pos + static_cast<std::size_t>(i) * 3 + 2];
      d[3] = 255;
    }
  }
  return true;
}

bool decode_tga_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                      std::string& error) {
  error.clear();
  out = ImageRgba{};
  if (size < 18) {
    error = "not a tga file";
    return false;
  }
  std::uint8_t id_len = data[0];
  std::uint8_t cmap_type = data[1];
  std::uint8_t img_type = data[2];
  int w = data[12] | (data[13] << 8);
  int h = data[14] | (data[15] << 8);
  int depth = data[16];
  std::uint8_t desc = data[17];
  if (cmap_type != 0 || (img_type != 2 && img_type != 10) || (depth != 24 && depth != 32)) {
    error = "unsupported tga (need uncompressed/RLE 24/32-bit truecolor)";
    return false;
  }
  if (w <= 0 || h <= 0 || w > 16383 || h > 16383) {
    error = "bad tga dimensions";
    return false;
  }
  if (static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h) > 100000000ull) {
    error = "image too large";
    return false;
  }
  std::size_t pos = 18 + id_len;
  if (pos > size) {
    error = "truncated tga file";
    return false;
  }
  bool top_left = (desc & 0x20) != 0;
  int bpp = depth / 8;
  out.w = w;
  out.h = h;
  out.rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
  auto put_px = [&](int x, int y, std::uint8_t b, std::uint8_t g, std::uint8_t r, std::uint8_t a) {
    int yy = top_left ? y : (h - 1 - y);
    std::uint8_t* d =
        &out.rgba[(static_cast<std::size_t>(yy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 4];
    d[0] = r;
    d[1] = g;
    d[2] = b;
    d[3] = a;
  };
  if (img_type == 2) {
    std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(bpp);
    if (pos + need > size) {
      error = "truncated tga file";
      return false;
    }
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const std::uint8_t* s = data + pos + (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                              static_cast<std::size_t>(x)) * static_cast<std::size_t>(bpp);
        put_px(x, y, s[0], s[1], s[2], bpp == 4 ? s[3] : 255);
      }
    return true;
  }
  // RLE type 10.
  int x = 0, y = 0;
  while (y < h) {
    if (pos >= size) {
      error = "truncated tga file";
      return false;
    }
    std::uint8_t hdr = data[pos++];
    int count = (hdr & 0x7F) + 1;
    if (hdr & 0x80) {
      if (pos + static_cast<std::size_t>(bpp) > size) {
        error = "truncated tga file";
        return false;
      }
      std::uint8_t b = data[pos], g = data[pos + 1], r = data[pos + 2];
      std::uint8_t a = bpp == 4 ? data[pos + 3] : 255;
      pos += static_cast<std::size_t>(bpp);
      for (int i = 0; i < count; ++i) {
        if (y >= h) {
          error = "bad tga rle data";
          return false;
        }
        put_px(x, y, b, g, r, a);
        if (++x >= w) {
          x = 0;
          ++y;
        }
      }
    } else {
      if (pos + static_cast<std::size_t>(count) * static_cast<std::size_t>(bpp) > size) {
        error = "truncated tga file";
        return false;
      }
      for (int i = 0; i < count; ++i) {
        if (y >= h) {
          error = "bad tga rle data";
          return false;
        }
        put_px(x, y, data[pos], data[pos + 1], data[pos + 2], bpp == 4 ? data[pos + 3] : 255);
        pos += static_cast<std::size_t>(bpp);
        if (++x >= w) {
          x = 0;
          ++y;
        }
      }
    }
  }
  return true;
}

bool decode_png_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                      std::string& error) {
  error.clear();
  out = ImageRgba{};
  static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (size < 33 || std::memcmp(data, kSig, 8) != 0) {
    error = "not a png file";
    return false;
  }
  std::size_t pos = 8;
  int w = 0, h = 0, bit_depth = 0, color_type = -1;
  std::vector<std::uint8_t> idat;
  std::vector<std::uint8_t> plte;
  std::vector<std::uint8_t> trns;
  bool saw_ihdr = false;
  while (pos + 12 <= size) {
    std::uint32_t len = rd_u32be_c(data + pos);
    if (pos + 12 + len > size) {
      error = "truncated png file";
      return false;
    }
    const std::uint8_t* type = data + pos + 4;
    const std::uint8_t* body = data + pos + 8;
    if (type[0] == 'I' && type[1] == 'H' && type[2] == 'D' && type[3] == 'R') {
      if (saw_ihdr || len != 13) {
        error = "bad png header";
        return false;
      }
      saw_ihdr = true;
      w = static_cast<int>(rd_u32be_c(body));
      h = static_cast<int>(rd_u32be_c(body + 4));
      bit_depth = body[8];
      color_type = body[9];
      if (body[10] != 0 || body[11] != 0 || body[12] != 0) {
        error = "unsupported png (need non-interlaced)";
        return false;
      }
      if (w <= 0 || h <= 0 || w > 16383 || h > 16383) {
        error = "bad png dimensions";
        return false;
      }
      if (static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h) > 100000000ull) {
        error = "image too large";
        return false;
      }
      bool ok_type = (color_type == 0 || color_type == 2 || color_type == 3 ||
                      color_type == 4 || color_type == 6);
      bool ok_depth = false;
      if (color_type == 0) ok_depth = bit_depth == 1 || bit_depth == 2 || bit_depth == 4 ||
                                     bit_depth == 8 || bit_depth == 16;
      else if (color_type == 2) ok_depth = bit_depth == 8 || bit_depth == 16;
      else if (color_type == 3) ok_depth = bit_depth == 1 || bit_depth == 2 || bit_depth == 4 ||
                                            bit_depth == 8;
      else ok_depth = bit_depth == 8 || bit_depth == 16;
      if (!ok_type || !ok_depth) {
        error = "unsupported png color type/depth";
        return false;
      }
    } else if (type[0] == 'P' && type[1] == 'L' && type[2] == 'T' && type[3] == 'E') {
      plte.assign(body, body + len);
    } else if (type[0] == 't' && type[1] == 'R' && type[2] == 'N' && type[3] == 'S') {
      trns.assign(body, body + len);
    } else if (type[0] == 'I' && type[1] == 'D' && type[2] == 'A' && type[3] == 'T') {
      idat.insert(idat.end(), body, body + len);
    } else if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D') {
      break;
    }
    pos += 12 + len;
  }
  if (!saw_ihdr || idat.empty()) {
    error = "bad png file (missing image data)";
    return false;
  }
  std::vector<std::uint8_t> inflated;
  if (!zlib_unwrap_to_deflate(idat, inflated, error)) return false;

  int channels = 0;
  if (color_type == 0) channels = 1;
  else if (color_type == 2) channels = 3;
  else if (color_type == 3) channels = 1;
  else if (color_type == 4) channels = 2;
  else channels = 4;
  std::size_t row_bits = static_cast<std::size_t>(w) * static_cast<std::size_t>(channels) *
                         static_cast<std::size_t>(bit_depth);
  std::size_t row_bytes = (row_bits + 7) / 8;
  if (inflated.size() != static_cast<std::size_t>(h) * (row_bytes + 1)) {
    error = "bad png data size";
    return false;
  }
  // Unfilter.
  std::vector<std::uint8_t> raw(static_cast<std::size_t>(h) * row_bytes);
  std::size_t bpp = 0;  // filter bytes per pixel
  if (bit_depth >= 8)
    bpp = static_cast<std::size_t>(channels) * static_cast<std::size_t>(bit_depth / 8);
  else
    bpp = 1;
  for (int y = 0; y < h; ++y) {
    const std::uint8_t* src = inflated.data() + static_cast<std::size_t>(y) * (row_bytes + 1);
    std::uint8_t* dst = raw.data() + static_cast<std::size_t>(y) * row_bytes;
    int f = src[0];
    if (f < 0 || f > 4) {
      error = "bad png filter";
      return false;
    }
    const std::uint8_t* prior = y ? raw.data() + static_cast<std::size_t>(y - 1) * row_bytes : nullptr;
    for (std::size_t x = 0; x < row_bytes; ++x) {
      int a = x >= bpp ? dst[x - bpp] : 0;
      int b = prior ? prior[x] : 0;
      int c = (prior && x >= bpp) ? prior[x - bpp] : 0;
      int v = src[1 + x];
      if (f == 1) v += a;
      else if (f == 2) v += b;
      else if (f == 3) v += (a + b) / 2;
      else if (f == 4) v += paeth_c(a, b, c);
      dst[x] = static_cast<std::uint8_t>(v & 0xff);
    }
  }
  out.w = w;
  out.h = h;
  out.rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
  auto gray_trans = [&]() -> int {
    if (color_type == 0 && trns.size() == 2) return (trns[0] << 8) | trns[1];
    return -1;
  }();
  auto rgb_trans = [&]() -> std::uint32_t {
    if (color_type == 2 && trns.size() == 6)
      return (static_cast<std::uint32_t>(trns[0]) << 24) | (static_cast<std::uint32_t>(trns[1]) << 16) |
             (static_cast<std::uint32_t>(trns[2]) << 8) | trns[3];
    return 0xFFFFFFFFu;
  }();
  // Palette transparency lookup.
  for (int y = 0; y < h; ++y) {
    const std::uint8_t* row = raw.data() + static_cast<std::size_t>(y) * row_bytes;
    for (int x = 0; x < w; ++x) {
      std::uint8_t* d = &out.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                   static_cast<std::size_t>(x)) * 4];
      if (color_type == 0) {
        int g = 0;
        if (bit_depth == 16) {
          g = row[x * 2];
          if (gray_trans >= 0 && ((gray_trans >> 8) == g)) d[3] = 0;
          else d[3] = 255;
        } else if (bit_depth == 8) {
          g = row[x];
          if (gray_trans >= 0 && (gray_trans & 0xff) == g) d[3] = 0;
          else d[3] = 255;
        } else {
          int per = 8 / bit_depth;
          int idx = x / per;
          int shift = (per - 1 - (x % per)) * bit_depth;
          int v = (row[idx] >> shift) & ((1 << bit_depth) - 1);
          g = v * 255 / ((1 << bit_depth) - 1);
          d[3] = 255;
        }
        d[0] = d[1] = d[2] = static_cast<std::uint8_t>(g);
      } else if (color_type == 2) {
        if (bit_depth == 16) {
          d[0] = row[x * 6];
          d[1] = row[x * 6 + 2];
          d[2] = row[x * 6 + 4];
          if (rgb_trans != 0xFFFFFFFFu &&
              (static_cast<std::uint32_t>(d[0]) << 24 | static_cast<std::uint32_t>(d[1]) << 16 |
               static_cast<std::uint32_t>(d[2]) << 8) == (rgb_trans & 0xFFFFFF00u))
            d[3] = 0;
          else d[3] = 255;
        } else {
          d[0] = row[x * 3];
          d[1] = row[x * 3 + 1];
          d[2] = row[x * 3 + 2];
          if (rgb_trans != 0xFFFFFFFFu && d[0] == trns[1] && d[1] == trns[3] && d[2] == trns[5])
            d[3] = 0;
          else d[3] = 255;
        }
      } else if (color_type == 3) {
        int idx = 0;
        if (bit_depth == 8) idx = row[x];
        else {
          int per = 8 / bit_depth;
          int bi = x / per;
          int shift = (per - 1 - (x % per)) * bit_depth;
          idx = (row[bi] >> shift) & ((1 << bit_depth) - 1);
        }
        if (static_cast<std::size_t>(idx) * 3 + 2 < plte.size()) {
          d[0] = plte[static_cast<std::size_t>(idx) * 3];
          d[1] = plte[static_cast<std::size_t>(idx) * 3 + 1];
          d[2] = plte[static_cast<std::size_t>(idx) * 3 + 2];
        }
        d[3] = static_cast<std::uint8_t>(static_cast<std::size_t>(idx) < trns.size() ? trns[idx] : 255);
      } else if (color_type == 4) {
        if (bit_depth == 16) {
          d[0] = d[1] = d[2] = row[x * 4];
          d[3] = row[x * 4 + 2];
        } else {
          d[0] = d[1] = d[2] = row[x * 2];
          d[3] = row[x * 2 + 1];
        }
      } else {
        if (bit_depth == 16) {
          d[0] = row[x * 8];
          d[1] = row[x * 8 + 2];
          d[2] = row[x * 8 + 4];
          d[3] = row[x * 8 + 6];
        } else {
          d[0] = row[x * 4];
          d[1] = row[x * 4 + 1];
          d[2] = row[x * 4 + 2];
          d[3] = row[x * 4 + 3];
        }
      }
    }
  }
  return true;
}

bool decode_image_bytes(const std::uint8_t* data, std::size_t size, ImageRgba& out,
                        std::string& error) {
  if (size >= 8 && data[0] == 137 && data[1] == 'P') {
    if (decode_png_bytes(data, size, out, error)) return true;
    // PNG signature but decode failed: report, do not try other formats.
    return false;
  }
  if (size >= 2 && data[0] == 'B' && data[1] == 'M') {
    return decode_bmp_bytes(data, size, out, error);
  }
  if (size >= 2 && data[0] == 'P' && (data[1] == '5' || data[1] == '6' || data[1] == '2' || data[1] == '3')) {
    return decode_ppm_bytes(data, size, out, error);
  }
  // TGA has no magic: try it last.
  std::string tga_err;
  if (decode_tga_bytes(data, size, out, tga_err)) {
    error.clear();
    return true;
  }
  error = "unsupported image (need png, bmp, ppm, or tga)";
  return false;
}

// ---------------------------------------------------------------------------
// Images: encoders
// ---------------------------------------------------------------------------

bool encode_png_rgba(const ImageRgba& img, std::vector<std::uint8_t>& out,
                     std::string& error) {
  error.clear();
  out.clear();
  if (img.w <= 0 || img.h <= 0 || img.w > 16383 || img.h > 16383) {
    error = "bad image size";
    return false;
  }
  if (img.rgba.size() < static_cast<std::size_t>(img.w) * img.h * 4) {
    error = "bad image data";
    return false;
  }
  static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  out.insert(out.end(), kSig, kSig + 8);
  std::uint8_t ihdr[13]{};
  ihdr[0] = static_cast<std::uint8_t>((img.w >> 24) & 0xff);
  ihdr[1] = static_cast<std::uint8_t>((img.w >> 16) & 0xff);
  ihdr[2] = static_cast<std::uint8_t>((img.w >> 8) & 0xff);
  ihdr[3] = static_cast<std::uint8_t>(img.w & 0xff);
  ihdr[4] = static_cast<std::uint8_t>((img.h >> 24) & 0xff);
  ihdr[5] = static_cast<std::uint8_t>((img.h >> 16) & 0xff);
  ihdr[6] = static_cast<std::uint8_t>((img.h >> 8) & 0xff);
  ihdr[7] = static_cast<std::uint8_t>(img.h & 0xff);
  ihdr[8] = 8;
  ihdr[9] = 6;
  png_put_chunk(out, "IHDR", ihdr, 13);
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(img.w) * img.h * 4 + static_cast<std::size_t>(img.h));
  for (int y = 0; y < img.h; ++y) {
    raw.push_back(0);
    raw.insert(raw.end(),
               img.rgba.begin() + static_cast<std::ptrdiff_t>(y) * img.w * 4,
               img.rgba.begin() + static_cast<std::ptrdiff_t>(y + 1) * img.w * 4);
  }
  std::vector<std::uint8_t> zlib;
  zlib_wrap_stored(raw, zlib);
  png_put_chunk(out, "IDAT", zlib.data(), zlib.size());
  png_put_chunk(out, "IEND", nullptr, 0);
  return true;
}

bool encode_png_rgb(const ImageRgba& img, std::vector<std::uint8_t>& out,
                    std::string& error) {
  error.clear();
  out.clear();
  if (img.w <= 0 || img.h <= 0 || img.w > 16383 || img.h > 16383) {
    error = "bad image size";
    return false;
  }
  if (img.rgba.size() < static_cast<std::size_t>(img.w) * img.h * 4) {
    error = "bad image data";
    return false;
  }
  static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  out.insert(out.end(), kSig, kSig + 8);
  std::uint8_t ihdr[13]{};
  ihdr[0] = static_cast<std::uint8_t>((img.w >> 24) & 0xff);
  ihdr[1] = static_cast<std::uint8_t>((img.w >> 16) & 0xff);
  ihdr[2] = static_cast<std::uint8_t>((img.w >> 8) & 0xff);
  ihdr[3] = static_cast<std::uint8_t>(img.w & 0xff);
  ihdr[4] = static_cast<std::uint8_t>((img.h >> 24) & 0xff);
  ihdr[5] = static_cast<std::uint8_t>((img.h >> 16) & 0xff);
  ihdr[6] = static_cast<std::uint8_t>((img.h >> 8) & 0xff);
  ihdr[7] = static_cast<std::uint8_t>(img.h & 0xff);
  ihdr[8] = 8;
  ihdr[9] = 2;
  png_put_chunk(out, "IHDR", ihdr, 13);
  std::vector<std::uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(img.w) * img.h * 3 + static_cast<std::size_t>(img.h));
  for (int y = 0; y < img.h; ++y) {
    raw.push_back(0);
    for (int x = 0; x < img.w; ++x) {
      const std::uint8_t* s =
          &img.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(img.w) + static_cast<std::size_t>(x)) * 4];
      raw.push_back(s[0]);
      raw.push_back(s[1]);
      raw.push_back(s[2]);
    }
  }
  std::vector<std::uint8_t> zlib;
  zlib_wrap_stored(raw, zlib);
  png_put_chunk(out, "IDAT", zlib.data(), zlib.size());
  png_put_chunk(out, "IEND", nullptr, 0);
  return true;
}

bool encode_bmp_bytes(const ImageRgba& img, std::vector<std::uint8_t>& out) {
  out.clear();
  if (img.w <= 0 || img.h <= 0 || img.w > 16383 || img.h > 16383) return false;
  if (img.rgba.size() < static_cast<std::size_t>(img.w) * img.h * 4) return false;
  bool has_alpha = false;
  for (std::size_t i = 3; i < img.rgba.size(); i += 4)
    if (img.rgba[i] != 255) {
      has_alpha = true;
      break;
    }
  int bpp = has_alpha ? 32 : 24;
  std::size_t stride = ((static_cast<std::size_t>(img.w) * static_cast<std::size_t>(bpp) + 31) / 32) * 4;
  std::uint32_t img_size = static_cast<std::uint32_t>(stride * static_cast<std::size_t>(img.h));
  std::uint32_t file_size = 54 + img_size;
  out.reserve(file_size);
  out.push_back('B');
  out.push_back('M');
  wr_u32le_c(out, file_size);
  wr_u16le_c(out, 0);
  wr_u16le_c(out, 0);
  wr_u32le_c(out, 54);
  wr_u32le_c(out, 40);
  wr_u32le_c(out, static_cast<std::uint32_t>(img.w));
  wr_u32le_c(out, static_cast<std::uint32_t>(img.h));
  wr_u16le_c(out, 1);
  wr_u16le_c(out, static_cast<std::uint16_t>(bpp));
  wr_u32le_c(out, 0);
  wr_u32le_c(out, img_size);
  wr_u32le_c(out, 2835);
  wr_u32le_c(out, 2835);
  wr_u32le_c(out, 0);
  wr_u32le_c(out, 0);
  std::vector<std::uint8_t> pad(4, 0);
  for (int y = img.h - 1; y >= 0; --y) {
    for (int x = 0; x < img.w; ++x) {
      const std::uint8_t* s =
          &img.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(img.w) + static_cast<std::size_t>(x)) * 4];
      out.push_back(s[2]);
      out.push_back(s[1]);
      out.push_back(s[0]);
      if (has_alpha) out.push_back(s[3]);
    }
    std::size_t row = static_cast<std::size_t>(img.w) * static_cast<std::size_t>(bpp / 8);
    std::size_t rem = stride - row;
    out.insert(out.end(), pad.begin(), pad.begin() + static_cast<std::ptrdiff_t>(rem));
  }
  return true;
}

bool encode_ppm_bytes(const ImageRgba& img, std::vector<std::uint8_t>& out) {
  out.clear();
  if (img.w <= 0 || img.h <= 0 || img.w > 16383 || img.h > 16383) return false;
  if (img.rgba.size() < static_cast<std::size_t>(img.w) * img.h * 4) return false;
  std::string hdr = "P6\n# Wilfred\n" + std::to_string(img.w) + " " + std::to_string(img.h) + "\n255\n";
  out.assign(hdr.begin(), hdr.end());
  for (int i = 0; i < img.w * img.h; ++i) {
    out.push_back(img.rgba[static_cast<std::size_t>(i) * 4]);
    out.push_back(img.rgba[static_cast<std::size_t>(i) * 4 + 1]);
    out.push_back(img.rgba[static_cast<std::size_t>(i) * 4 + 2]);
  }
  return true;
}

bool encode_tga_bytes(const ImageRgba& img, std::vector<std::uint8_t>& out,
                      std::string& error) {
  error.clear();
  out.clear();
  if (img.w <= 0 || img.h <= 0 || img.w > 16383 || img.h > 16383) {
    error = "bad image size";
    return false;
  }
  if (img.rgba.size() < static_cast<std::size_t>(img.w) * img.h * 4) {
    error = "bad image data";
    return false;
  }
  bool has_alpha = false;
  for (std::size_t i = 3; i < img.rgba.size(); i += 4)
    if (img.rgba[i] != 255) {
      has_alpha = true;
      break;
    }
  std::uint8_t hdr[18]{};
  hdr[2] = 2;
  hdr[12] = static_cast<std::uint8_t>(img.w & 0xff);
  hdr[13] = static_cast<std::uint8_t>((img.w >> 8) & 0xff);
  hdr[14] = static_cast<std::uint8_t>(img.h & 0xff);
  hdr[15] = static_cast<std::uint8_t>((img.h >> 8) & 0xff);
  hdr[16] = has_alpha ? 32 : 24;
  hdr[17] = static_cast<std::uint8_t>(0x20 | (has_alpha ? 8 : 0));
  out.insert(out.end(), hdr, hdr + 18);
  for (int y = 0; y < img.h; ++y)
    for (int x = 0; x < img.w; ++x) {
      const std::uint8_t* s =
          &img.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(img.w) + static_cast<std::size_t>(x)) * 4];
      out.push_back(s[2]);
      out.push_back(s[1]);
      out.push_back(s[0]);
      if (has_alpha) out.push_back(s[3]);
    }
  return true;
}

bool encode_image_native(const ImageRgba& img, const std::string& fmt,
                         std::vector<std::uint8_t>& out, std::string& error) {
  auto f = normalize_format_token(fmt);
  if (f == "png") {
    bool has_alpha = false;
    for (std::size_t i = 3; i < img.rgba.size(); i += 4)
      if (img.rgba[i] != 255) {
        has_alpha = true;
        break;
      }
    return has_alpha ? encode_png_rgba(img, out, error) : encode_png_rgb(img, out, error);
  }
  if (f == "bmp") {
    if (!encode_bmp_bytes(img, out)) {
      error = "could not encode bmp";
      return false;
    }
    return true;
  }
  if (f == "ppm" || f == "pgm") {
    if (!encode_ppm_bytes(img, out)) {
      error = "could not encode ppm";
      return false;
    }
    return true;
  }
  if (f == "tga") return encode_tga_bytes(img, out, error);
  error = "format " + f + " needs ffmpeg (" + convert_install_hint("ffmpeg") + ")";
  return false;
}

bool decode_image_file(const std::string& path, ImageRgba& out, std::string& error) {
  error.clear();
  out = ImageRgba{};
  if (path.empty() || !file_exists(path)) {
    error = "Image file not found: " + path;
    return false;
  }
  std::vector<std::uint8_t> bytes;
  if (!read_bytes_file(path, bytes) || bytes.empty()) {
    error = "Could not read " + path;
    return false;
  }
  std::string native_err;
  if (decode_image_bytes(bytes.data(), bytes.size(), out, native_err)) return true;
  // Native failed: for broadly-used formats (jpg/gif/webp/...) try ffmpeg.
  auto ext = lower_c(path_extension(path));
  std::string e = ext;
  if (!e.empty() && e[0] == '.') e = e.substr(1);
  e = normalize_format_token(e);
  bool worth_ffmpeg = e == "jpg" || e == "gif" || e == "webp" || e == "tiff" ||
                      e == "heic" || e == "heif" || e == "avif" || e == "ico" || e == "png" ||
                      e == "bmp";
  if (!worth_ffmpeg) {
    error = native_err.empty() ? ("Unsupported image: " + path) : native_err;
    return false;
  }
  if (!ffmpeg_available_convert()) {
    if (e == "jpg" || e == "gif" || e == "webp" || e == "tiff" || e == "heic" ||
        e == "heif" || e == "avif" || e == "ico") {
      error = "Reading ." + e + " needs ffmpeg. " + convert_install_hint("ffmpeg");
      return false;
    }
    error = native_err.empty() ? ("Could not decode " + path) : native_err;
    return false;
  }
  std::string tmp = path_join(data_directory(), "wilfred-convert-tmp.png");
  create_directories(data_directory());
  remove_file(tmp);
  std::string cmd = "ffmpeg -y -v error -i " + shell_quote_c(path) + " -frames:v 1 " +
                    shell_quote_c(tmp);
#ifdef _WIN32
  cmd += " 2>NUL";
#else
  cmd += " 2>/dev/null";
#endif
  bool ok = run_shell_c(cmd.c_str()) == 0 && file_exists(tmp);
  if (!ok) {
    remove_file(tmp);
    error = native_err.empty() ? ("Could not decode " + path) : native_err;
    return false;
  }
  std::vector<std::uint8_t> tmp_bytes;
  bool read_ok = read_bytes_file(tmp, tmp_bytes);
  remove_file(tmp);
  if (!read_ok || !decode_image_bytes(tmp_bytes.data(), tmp_bytes.size(), out, error)) {
    if (error.empty()) error = "Could not decode " + path;
    return false;
  }
  return true;
}

bool convert_image_file(const std::string& src, const std::string& dst,
                        std::string& out_path, std::string& error) {
  error.clear();
  out_path.clear();
  if (src.empty() || !file_exists(src)) {
    error = "Image file not found: " + src;
    return false;
  }
  if (dst.empty()) {
    error = "No output path";
    return false;
  }
  auto dst_ext = lower_c(path_extension(dst));
  std::string de = dst_ext;
  if (!de.empty() && de[0] == '.') de = de.substr(1);
  de = normalize_format_token(de);
  bool dst_native = de == "png" || de == "bmp" || de == "ppm" || de == "pgm" || de == "tga";
  if (dst_native) {
    ImageRgba img;
    if (!decode_image_file(src, img, error)) return false;
    std::vector<std::uint8_t> enc;
    if (!encode_image_native(img, de, enc, error)) return false;
    if (!write_bytes_file(dst, enc)) {
      error = "Could not write " + dst;
      return false;
    }
    out_path = dst;
    return true;
  }
  if (!ffmpeg_available_convert()) {
    error = "Converting to ." + de + " needs ffmpeg. " + convert_install_hint("ffmpeg");
    return false;
  }
  auto parent = path_parent(dst);
  if (!parent.empty()) create_directories(parent);
  if (run_shell_c(build_convert_ffmpeg_command(src, dst).c_str()) != 0 || !file_exists(dst)) {
    error = "ffmpeg could not convert " + src + " to " + dst;
    return false;
  }
  out_path = dst;
  return true;
}

bool convert_media_file(const std::string& src, const std::string& dst, int sample_rate,
                        int channels, int bits, std::string& out_path, std::string& error) {
  auto dst_ext = lower_c(path_extension(dst));
  std::string de = dst_ext;
  if (!de.empty() && de[0] == '.') de = de.substr(1);
  de = normalize_format_token(de);
  if (is_audio_format(de)) return convert_audio_file(src, dst, sample_rate, channels, bits, out_path, error);
  if (is_image_format(de)) return convert_image_file(src, dst, out_path, error);
  error = "Unknown output format for " + dst;
  return false;
}

// ---------------------------------------------------------------------------
// Background removal
// ---------------------------------------------------------------------------

bool parse_hex_color(const std::string& s, std::uint8_t& r, std::uint8_t& g,
                     std::uint8_t& b) {
  std::string t = trim_c(s);
  if (!t.empty() && t[0] == '#') t = t.substr(1);
  if (t.size() == 3) {
    auto hex = [&](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int a = hex(t[0]), bb = hex(t[1]), c = hex(t[2]);
    if (a < 0 || bb < 0 || c < 0) return false;
    r = static_cast<std::uint8_t>(a * 17);
    g = static_cast<std::uint8_t>(bb * 17);
    b = static_cast<std::uint8_t>(c * 17);
    return true;
  }
  if (t.size() == 6) {
    auto hex2 = [&](int i) -> int {
      auto hv = [&](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int hi = hv(t[static_cast<std::size_t>(i)]), lo = hv(t[static_cast<std::size_t>(i) + 1]);
      if (hi < 0 || lo < 0) return -1;
      return hi * 16 + lo;
    };
    int a = hex2(0), bb = hex2(2), c = hex2(4);
    if (a < 0 || bb < 0 || c < 0) return false;
    r = static_cast<std::uint8_t>(a);
    g = static_cast<std::uint8_t>(bb);
    b = static_cast<std::uint8_t>(c);
    return true;
  }
  return false;
}

bool remove_background(const ImageRgba& src, const BgRemoveOptions& opts, ImageRgba& dst) {
  dst = ImageRgba{};
  if (src.w <= 0 || src.h <= 0 || src.w > 16383 || src.h > 16383) return false;
  if (src.rgba.size() < static_cast<std::size_t>(src.w) * src.h * 4) return false;
  dst.w = src.w;
  dst.h = src.h;
  dst.rgba = src.rgba;

  std::uint8_t br = opts.r, bg = opts.g, bb = opts.b;
  if (!opts.has_color) {
    // Sample 5x5 blocks at the four corners, ignoring already-transparent
    // pixels, and average them.
    long sr = 0, sg = 0, sb = 0;
    long n = 0;
    int rad = std::min(5, std::min(src.w, src.h));
    const int corners[4][2] = {{0, 0}, {src.w - rad, 0}, {0, src.h - rad}, {src.w - rad, src.h - rad}};
    for (auto& cn : corners)
      for (int y = 0; y < rad; ++y)
        for (int x = 0; x < rad; ++x) {
          int px = cn[0] + x, py = cn[1] + y;
          if (px < 0 || py < 0 || px >= src.w || py >= src.h) continue;
          const std::uint8_t* p =
              &src.rgba[(static_cast<std::size_t>(py) * static_cast<std::size_t>(src.w) +
                         static_cast<std::size_t>(px)) * 4];
          if (p[3] < 128) continue;
          sr += p[0];
          sg += p[1];
          sb += p[2];
          ++n;
        }
    if (n > 0) {
      br = static_cast<std::uint8_t>(sr / n);
      bg = static_cast<std::uint8_t>(sg / n);
      bb = static_cast<std::uint8_t>(sb / n);
    } else {
      br = bg = bb = 255;
    }
  }
  int tol = std::max(0, std::min(100, opts.tolerance));
  int feather = std::max(0, std::min(8, opts.feather));
  double thr = tol * 4.4167;  // 100 -> ~441.67 (max RGB distance)
  double band = static_cast<double>(feather) * 12.0;
  double thr2 = thr * thr;
  double far2 = (thr + band) * (thr + band);
  int w = src.w, h = src.h;

  auto dist2_at = [&](int idx) {
    const std::uint8_t* p = &src.rgba[static_cast<std::size_t>(idx) * 4];
    double dr = static_cast<double>(p[0]) - br;
    double dg = static_cast<double>(p[1]) - bg;
    double db = static_cast<double>(p[2]) - bb;
    return dr * dr + dg * dg + db * db;
  };

  if (!opts.contiguous) {
    for (int i = 0; i < w * h; ++i) {
      double d2 = dist2_at(i);
      std::uint8_t* d = &dst.rgba[static_cast<std::size_t>(i) * 4];
      double factor = 1.0;
      if (d2 <= thr2) factor = 0.0;
      else if (band > 0 && d2 <= far2) {
        double dist = std::sqrt(d2);
        factor = (dist - thr) / band;
      }
      d[3] = static_cast<std::uint8_t>(std::lround(d[3] * factor));
    }
    return true;
  }

  // Border flood-fill: transparent where connected to the edge.
  std::vector<char> transp(static_cast<std::size_t>(w) * h, 0);
  std::vector<int> stack;
  stack.reserve(1024);
  auto try_seed = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    int i = y * w + x;
    if (transp[static_cast<std::size_t>(i)]) return;
    const std::uint8_t* p = &src.rgba[static_cast<std::size_t>(i) * 4];
    if (p[3] < 8) {
      transp[static_cast<std::size_t>(i)] = 1;
      stack.push_back(i);
      return;
    }
    if (dist2_at(i) <= thr2) {
      transp[static_cast<std::size_t>(i)] = 1;
      stack.push_back(i);
    }
  };
  for (int x = 0; x < w; ++x) {
    try_seed(x, 0);
    try_seed(x, h - 1);
  }
  for (int y = 0; y < h; ++y) {
    try_seed(0, y);
    try_seed(w - 1, y);
  }
  const int dx[4] = {1, -1, 0, 0};
  const int dy[4] = {0, 0, 1, -1};
  while (!stack.empty()) {
    int cur = stack.back();
    stack.pop_back();
    int cx = cur % w, cy = cur / w;
    for (int k = 0; k < 4; ++k) {
      int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
      int ni = ny * w + nx;
      if (transp[static_cast<std::size_t>(ni)]) continue;
      const std::uint8_t* p = &src.rgba[static_cast<std::size_t>(ni) * 4];
      if (p[3] < 8 || dist2_at(ni) <= thr2) {
        transp[static_cast<std::size_t>(ni)] = 1;
        stack.push_back(ni);
      }
    }
  }
  for (int i = 0; i < w * h; ++i) {
    std::uint8_t* d = &dst.rgba[static_cast<std::size_t>(i) * 4];
    if (transp[static_cast<std::size_t>(i)]) {
      d[3] = 0;
    }
  }
  // Feather ring around the transparent region.
  if (band > 0 && feather > 0) {
    std::vector<char> is_edge(static_cast<std::size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        int i = y * w + x;
        if (transp[static_cast<std::size_t>(i)]) continue;
        bool adj = false;
        for (int k = 0; k < 4; ++k) {
          int nx = x + dx[k], ny = y + dy[k];
          if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
          if (transp[static_cast<std::size_t>(ny * w + nx)]) {
            adj = true;
            break;
          }
        }
        if (!adj) continue;
        // Walk `feather` steps outward? Single ring with distance-based
        // alpha is enough for a smooth 1-2px edge.
        double d2 = dist2_at(i);
        if (d2 <= far2) {
          double dist = std::sqrt(d2);
          double f = dist <= thr ? 0.15 : (dist - thr) / band;
          if (f < 0) f = 0;
          if (f > 1) f = 1;
          std::uint8_t* dd = &dst.rgba[static_cast<std::size_t>(i) * 4];
          double keep = static_cast<double>(src.rgba[static_cast<std::size_t>(i) * 4 + 3]) / 255.0;
          dd[3] = static_cast<std::uint8_t>(std::lround(255.0 * keep * f));
          is_edge[static_cast<std::size_t>(i)] = 1;
        }
      }
    (void)is_edge;
  }
  return true;
}

bool bgremove_file(const std::string& src, const std::string& dst,
                   const BgRemoveOptions& opts, std::string& out_path, std::string& error) {
  error.clear();
  out_path.clear();
  if (src.empty() || !file_exists(src)) {
    error = "Image file not found: " + src;
    return false;
  }
  ImageRgba img;
  if (!decode_image_file(src, img, error)) return false;
  ImageRgba cut;
  if (!remove_background(img, opts, cut)) {
    error = "Could not process " + src;
    return false;
  }
  std::string dest = trim_c(dst);
  if (dest.empty()) {
    auto parent = path_parent(src);
    auto stem = path_stem(src);
    if (stem.empty()) stem = "image";
    dest = path_join(parent, stem + ".transparent.png");
    if (parent.empty()) dest = stem + ".transparent.png";
    if (file_exists(dest)) {
      for (int i = 2; i < 10000; ++i) {
        std::string c2 = path_join(parent, stem + ".transparent " + std::to_string(i) + ".png");
        if (parent.empty()) c2 = stem + ".transparent " + std::to_string(i) + ".png";
        if (!file_exists(c2)) {
          dest = c2;
          break;
        }
      }
    }
  } else {
    // Background removal always yields alpha: force a .png extension so the
    // output never silently drops transparency into a jpeg.
    auto ext = lower_c(path_extension(dest));
    if (ext != ".png") {
      auto parent = path_parent(dest);
      auto stem = path_stem(dest);
      if (stem.empty()) stem = path_stem(src);
      if (stem.empty()) stem = "image";
      dest = path_join(parent, stem + ".png");
      if (parent.empty()) dest = stem + ".png";
    }
  }
  std::vector<std::uint8_t> enc;
  if (!encode_png_rgba(cut, enc, error)) return false;
  if (!write_bytes_file(dest, enc)) {
    error = "Could not write " + dest;
    return false;
  }
  out_path = dest;
  return true;
}

// ---------------------------------------------------------------------------
// Query parsing
// ---------------------------------------------------------------------------

namespace {

std::size_t find_ci(const std::string& hay, const std::string& needle, std::size_t from = 0) {
  auto lh = lower_c(hay), ln = lower_c(needle);
  return lh.find(ln, from);
}

std::vector<std::string> split_ws_quoted(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  char quote = 0;
  for (char c : s) {
    if (quote) {
      if (c == quote) quote = 0;
      else cur.push_back(c);
    } else if (c == '"' || c == '\'') {
      quote = c;
    } else if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

bool token_is_rate(const std::string& t, int& rate) {
  std::string l = lower_c(trim_c(t));
  std::string num = l;
  for (auto suf : {"hz", "khz"}) {
    std::string s(suf);
    if (l.size() > s.size() && l.substr(l.size() - s.size()) == s) {
      num = l.substr(0, l.size() - s.size());
      if (s == "khz") {
        try {
          double k = std::stod(trim_c(num));
          rate = static_cast<int>(k * 1000);
          return rate >= 1000 && rate <= 384000;
        } catch (...) {
          return false;
        }
      }
      break;
    }
  }
  try {
    std::size_t p = 0;
    int v = std::stoi(trim_c(num), &p);
    if (p != trim_c(num).size()) return false;
    if (v >= 1000 && v <= 384000) {
      rate = v;
      return true;
    }
  } catch (...) {
  }
  return false;
}

bool token_is_bits(const std::string& t, int& bits) {
  std::string l = lower_c(trim_c(t));
  for (auto suf : {"bit", "bits", "-bit", "-bits", "bps"}) {
    std::string s(suf);
    if (l.size() > s.size() && l.substr(l.size() - s.size()) == s) {
      auto num = trim_c(l.substr(0, l.size() - s.size()));
      if (num == "8" || num == "16" || num == "24" || num == "32") {
        bits = std::stoi(num);
        return true;
      }
      return false;
    }
  }
  return false;
}

}  // namespace

bool parse_convert_query(const std::string& remainder, ConvertRequest& out) {
  out = ConvertRequest{};
  std::string rem = trim_c(remainder);
  if (rem.empty()) return false;
  std::string src, right;
  std::size_t sep = find_ci(rem, " to ");
  std::string sep_used;
  if (sep != std::string::npos) sep_used = " to ";
  if (sep == std::string::npos) {
    sep = find_ci(rem, " -> ");
    if (sep != std::string::npos) sep_used = " -> ";
  }
  if (sep == std::string::npos) {
    sep = find_ci(rem, " as ");
    if (sep != std::string::npos) sep_used = " as ";
  }
  if (sep != std::string::npos) {
    src = trim_c(rem.substr(0, sep));
    right = trim_c(rem.substr(sep + sep_used.size()));
  } else {
    // "song.wav ->mp3" / "song.wav=>out.mp3" without spaces.
    sep = rem.find("->");
    if (sep != std::string::npos && sep > 0 && sep + 2 < rem.size()) {
      src = trim_c(rem.substr(0, sep));
      right = trim_c(rem.substr(sep + 2));
    } else {
      sep = find_ci(rem, "=>");
      if (sep != std::string::npos && sep > 0) {
        src = trim_c(rem.substr(0, sep));
        right = trim_c(rem.substr(sep + 2));
      }
    }
  }
  if (src.empty()) {
    // No separator: last whitespace token is the target, rest is the source.
    auto toks = split_ws_quoted(rem);
    if (toks.size() >= 2) {
      right = toks.back();
      std::string joined;
      for (std::size_t i = 0; i + 1 < toks.size(); ++i) {
        if (i) joined.push_back(' ');
        // Re-quote tokens with spaces so paths round-trip.
        if (toks[i].find(' ') != std::string::npos) joined += "\"" + toks[i] + "\"";
        else joined += toks[i];
      }
      src = joined;
    } else {
      // Single token: source with no target yet (mini shows format choices).
      out.src = strip_quotes(rem);
      return !out.src.empty();
    }
  }
  src = strip_quotes(src);
  right = trim_c(right);
  if (src.empty() || right.empty()) return false;

  // right = "<fmt|dst> [rate] [mono|stereo] [bits]"
  std::string first, rest_opts;
  if (!right.empty() && (right.front() == '"' || right.front() == '\'')) {
    char q = right.front();
    auto end = right.find(q, 1);
    if (end == std::string::npos) return false;
    first = right.substr(1, end - 1);
    rest_opts = trim_c(right.substr(end + 1));
  } else {
    auto sp = right.find_first_of(" \t");
    if (sp == std::string::npos) {
      first = right;
    } else {
      first = trim_c(right.substr(0, sp));
      rest_opts = trim_c(right.substr(sp + 1));
    }
  }
  first = strip_quotes(first);
  if (first.empty()) return false;
  // Decide fmt vs explicit dst path.
  bool has_sep = first.find('/') != std::string::npos || first.find('\\') != std::string::npos;
  bool has_dot = first.find('.') != std::string::npos;
  if (has_sep || (has_dot && !(first.size() > 1 && first.front() == '.'))) {
    auto ext = lower_c(path_extension(first));
    std::string e = ext;
    if (!e.empty() && e[0] == '.') e = e.substr(1);
    if (!is_known_convert_format(e)) return false;
    out.src = src;
    out.dst = first;
    out.fmt = normalize_format_token(e);
  } else {
    auto f = normalize_format_token(first);
    if (!is_known_convert_format(f)) return false;
    out.src = src;
    out.fmt = f;
  }
  // Trailing options.
  if (!rest_opts.empty()) {
    for (auto& tok : split_ws_quoted(rest_opts)) {
      std::string l = lower_c(tok);
      if (l == "mono") out.channels = 1;
      else if (l == "stereo") out.channels = 2;
      else {
        int r = 0, b = 0;
        if (token_is_rate(tok, r)) out.sample_rate = r;
        else if (token_is_bits(tok, b)) out.bits = b;
        else return false;  // unknown trailing token
      }
    }
  }
  return !out.src.empty() && (!out.fmt.empty() || !out.dst.empty());
}

bool parse_bgremove_query(const std::string& remainder, BgRemoveRequest& out) {
  out = BgRemoveRequest{};
  std::string rem = trim_c(remainder);
  if (rem.empty()) return false;
  // Explicit output: "photo.png to clean.png" / "photo.png as clean.png".
  std::string dst;
  {
    std::size_t sep = find_ci(rem, " to ");
    std::size_t used = 4;
    if (sep == std::string::npos) {
      sep = find_ci(rem, " as ");
      used = 4;
    }
    if (sep == std::string::npos) {
      sep = find_ci(rem, " -> ");
      used = 4;
    }
    if (sep != std::string::npos) {
      std::string cand = trim_c(rem.substr(sep + used));
      auto toks = split_ws_quoted(cand);
      if (!toks.empty()) {
        std::string e = lower_c(path_extension(toks[0]));
        if (!e.empty() && e[0] == '.') e = e.substr(1);
        if (is_image_format(normalize_format_token(e))) {
          dst = strip_quotes(toks[0]);
          rem = trim_c(rem.substr(0, sep) + " " + trim_c(cand.substr(toks[0].size())));
        }
      }
    }
  }
  auto toks = split_ws_quoted(rem);
  if (toks.empty()) return false;
  // Consume option tokens from the end; the rest is the source (may contain
  // spaces, e.g. an unquoted "my photo.png").
  BgRemoveOptions opts;
  opts.tolerance = 32;
  opts.contiguous = true;
  opts.feather = 2;
  bool tol_set = false;
  bool color_set = false;
  int feather_pending = -1;
  std::size_t end = toks.size();
  auto is_int = [](const std::string& t, int& v) {
    try {
      std::size_t p = 0;
      int x = std::stoi(trim_c(t), &p);
      if (p != trim_c(t).size()) return false;
      v = x;
      return true;
    } catch (...) {
      return false;
    }
  };
  while (end > 1) {
    std::string t = toks[end - 1];
    std::string l = lower_c(t);
    if (l == "feather") {
      feather_pending = 1;
      --end;
      continue;
    }
    if (feather_pending == 1) {
      int v = 0;
      if (is_int(t, v) && v >= 0 && v <= 8) {
        opts.feather = v;
        feather_pending = -1;
        --end;
        continue;
      }
      feather_pending = -1;
      // fall through: re-evaluate this token as a normal option
    }
    if (l == "global" || l == "all" || l == "chroma" || l == "chroma-key" || l == "chroma_key") {
      opts.contiguous = false;
      --end;
      continue;
    }
    if (l == "contiguous" || l == "flood" || l == "border" || l == "edge" || l == "edges" ||
        l == "local") {
      opts.contiguous = true;
      --end;
      continue;
    }
    if ((l == "no-feather" || l == "nofeather" || l == "sharp") && feather_pending < 0) {
      opts.feather = 0;
      --end;
      continue;
    }
    std::uint8_t r = 0, g = 0, b = 0;
    if (!color_set && parse_hex_color(t, r, g, b)) {
      opts.r = r;
      opts.g = g;
      opts.b = b;
      opts.has_color = true;
      color_set = true;
      --end;
      continue;
    }
    if (!color_set &&
        (l == "white" || l == "black" || l == "red" || l == "green" || l == "blue" ||
         l == "yellow" || l == "magenta" || l == "cyan" || l == "gray" || l == "grey")) {
      if (l == "white") { opts.r = 255; opts.g = 255; opts.b = 255; }
      else if (l == "black") { opts.r = 0; opts.g = 0; opts.b = 0; }
      else if (l == "red") { opts.r = 255; opts.g = 0; opts.b = 0; }
      else if (l == "green") { opts.r = 0; opts.g = 128; opts.b = 0; }
      else if (l == "blue") { opts.r = 0; opts.g = 0; opts.b = 255; }
      else if (l == "yellow") { opts.r = 255; opts.g = 255; opts.b = 0; }
      else if (l == "magenta") { opts.r = 255; opts.g = 0; opts.b = 255; }
      else if (l == "cyan") { opts.r = 0; opts.g = 255; opts.b = 255; }
      else { opts.r = 128; opts.g = 128; opts.b = 128; }
      opts.has_color = true;
      color_set = true;
      --end;
      continue;
    }
    int v = 0;
    if (!tol_set && is_int(t, v) && v >= 0 && v <= 100) {
      // Avoid eating a source that is just a number: require at least one
      // token left for the source.
      opts.tolerance = v;
      tol_set = true;
      --end;
      continue;
    }
    break;
  }
  if (feather_pending == 1) {
    // Trailing bare "feather" with no number: keep default.
  }
  std::string src;
  for (std::size_t i = 0; i < end; ++i) {
    if (i) src.push_back(' ');
    src += toks[i];
  }
  src = strip_quotes(trim_c(src));
  if (src.empty()) return false;
  out.src = src;
  out.dst = dst;
  out.opts = opts;
  return true;
}

std::string encode_convert_payload(const std::string& src, const std::string& fmt,
                                   const std::string& dst, int rate, int channels) {
  return src + "\n" + fmt + "\n" + dst + "\n" + std::to_string(rate) + "\n" +
         std::to_string(channels);
}

bool decode_convert_payload(const std::string& payload, std::string& src, std::string& fmt,
                            std::string& dst, int& rate, int& channels) {
  src.clear();
  fmt.clear();
  dst.clear();
  rate = 0;
  channels = 0;
  std::vector<std::string> parts;
  std::size_t pos = 0;
  while (true) {
    auto nl = payload.find('\n', pos);
    if (nl == std::string::npos) {
      parts.push_back(payload.substr(pos));
      break;
    }
    parts.push_back(payload.substr(pos, nl - pos));
    pos = nl + 1;
  }
  if (parts.size() < 3) return false;
  src = parts[0];
  fmt = parts[1];
  dst = parts[2];
  if (parts.size() > 3) {
    try {
      rate = std::stoi(parts[3]);
    } catch (...) {
      rate = 0;
    }
  }
  if (parts.size() > 4) {
    try {
      channels = std::stoi(parts[4]);
    } catch (...) {
      channels = 0;
    }
  }
  return !src.empty();
}

std::string encode_bgremove_payload(const std::string& src, const BgRemoveOptions& opts,
                                    const std::string& dst) {
  char col[16]{};
  std::snprintf(col, sizeof(col), "#%02x%02x%02x", opts.r, opts.g, opts.b);
  return src + "\n" + col + "\n" + std::to_string(opts.tolerance) + "\n" +
         (opts.contiguous ? "1" : "0") + "\n" + std::to_string(opts.feather) + "\n" +
         (opts.has_color ? "1" : "0") + "\n" + dst;
}

bool decode_bgremove_payload(const std::string& payload, std::string& src,
                             BgRemoveOptions& opts, std::string& dst) {
  src.clear();
  dst.clear();
  opts = BgRemoveOptions{};
  std::vector<std::string> parts;
  std::size_t pos = 0;
  while (true) {
    auto nl = payload.find('\n', pos);
    if (nl == std::string::npos) {
      parts.push_back(payload.substr(pos));
      break;
    }
    parts.push_back(payload.substr(pos, nl - pos));
    pos = nl + 1;
  }
  if (parts.size() < 3) return false;
  src = parts[0];
  std::uint8_t r = 255, g = 255, b = 255;
  if (!parse_hex_color(parts[1], r, g, b)) return false;
  opts.r = r;
  opts.g = g;
  opts.b = b;
  try {
    opts.tolerance = std::stoi(parts[2]);
  } catch (...) {
    opts.tolerance = 32;
  }
  if (parts.size() > 3) opts.contiguous = parts[3] != "0";
  if (parts.size() > 4) {
    try {
      opts.feather = std::stoi(parts[4]);
    } catch (...) {
      opts.feather = 2;
    }
  }
  if (parts.size() > 5) opts.has_color = parts[5] != "0";
  if (parts.size() > 6) dst = parts[6];
  opts.tolerance = std::max(0, std::min(100, opts.tolerance));
  opts.feather = std::max(0, std::min(8, opts.feather));
  return !src.empty();
}

std::vector<std::string> find_convertible_in_index(IndexEngine& index,
                                                   const std::string& needle, int limit) {
  std::vector<std::string> out;
  if (limit <= 0) limit = 8;
  auto q = lower_c(needle);
  auto& store = index.store();
  std::lock_guard<std::recursive_mutex> lock(store.mutex());
  for (auto& rec : store.records()) {
    if (static_cast<int>(out.size()) >= limit) break;
    if (has_flag(rec.flags, RecordFlags::Directory)) continue;
    std::string_view pv = store.pool().get(rec.path_id);
    if (pv.empty()) continue;
    std::string path(pv);
    if (!is_convertible(path)) continue;
    if (!q.empty() && lower_c(path).find(q) == std::string::npos) continue;
    out.push_back(std::move(path));
  }
  return out;
}

std::vector<std::string> find_image_in_index(IndexEngine& index, const std::string& needle,
                                             int limit) {
  std::vector<std::string> out;
  if (limit <= 0) limit = 8;
  auto q = lower_c(needle);
  auto& store = index.store();
  std::lock_guard<std::recursive_mutex> lock(store.mutex());
  for (auto& rec : store.records()) {
    if (static_cast<int>(out.size()) >= limit) break;
    if (has_flag(rec.flags, RecordFlags::Directory)) continue;
    std::string_view pv = store.pool().get(rec.path_id);
    if (pv.empty()) continue;
    std::string path(pv);
    if (!is_image_convertible(path)) continue;
    if (!q.empty() && lower_c(path).find(q) == std::string::npos) continue;
    out.push_back(std::move(path));
  }
  return out;
}

}  // namespace wilfred
