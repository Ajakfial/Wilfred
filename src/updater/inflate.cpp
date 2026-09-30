// Minimal inflate implementation (RFC 1951) for ZIP and gzip extraction.
// Supports all block types (stored, fixed Huffman, dynamic Huffman).

#include "wilfred/updater/inflate.hpp"

#include <cstring>
#include <vector>

namespace wilfred {

// Bit reader that reads from a byte stream, LSB first.
class BitReader {
public:
  BitReader(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size), pos_(0), bit_buf_(0), bit_cnt_(0) {}

  int read_bits(int n) {
    int result = 0;
    for (int i = 0; i < n; ++i) {
      result |= (read_bit() << i);
    }
    return result;
  }

  int read_bit() {
    if (bit_cnt_ == 0) {
      if (pos_ >= size_) return -1;  // out of data
      bit_buf_ = data_[pos_++];
      bit_cnt_ = 8;
    }
    int bit = bit_buf_ & 1;
    bit_buf_ >>= 1;
    --bit_cnt_;
    return bit;
  }

  void align_to_byte() {
    bit_cnt_ = 0;
    bit_buf_ = 0;
  }

  std::size_t remaining() const { return size_ - pos_; }

private:
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t pos_;
  std::uint32_t bit_buf_;
  int bit_cnt_;
};

// Huffman decoder
class Huffman {
public:
  bool build(const int* lengths, int count) {
    std::memset(counts_, 0, sizeof(counts_));
    for (int i = 0; i < count; ++i) {
      if (lengths[i] > 0) ++counts_[lengths[i]];
    }
    counts_[0] = 0;

    int left = 1;
    for (int len = 1; len <= 15; ++len) {
      left <<= 1;
      left -= counts_[len];
      if (left < 0) return false;
    }

    std::memset(symbols_, 0, sizeof(symbols_));
    int offs = 0;
    for (int len = 1; len <= 15; ++len) {
      for (int i = 0; i < count; ++i) {
        if (lengths[i] == len) {
          symbols_[offs++] = i;
        }
      }
    }
    return true;
  }

  int decode(BitReader& br) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= 15; ++len) {
      int bit = br.read_bit();
      if (bit < 0) return -1;
      code |= bit;
      int count = counts_[len];
      if (code - first < count) {
        return symbols_[index + (code - first)];
      }
      index += count;
      first = (first + count) << 1;
      code <<= 1;
    }
    return -1;
  }

private:
  int counts_[16]{};
  int symbols_[288]{};
};

// Length and distance base values and extra bits (RFC 1951)
static const int kLengthBase[] = {
    3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23,  27,  31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,  0};
static const int kLengthExtra[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0};
static const int kDistBase[] = {
    1,    2,    3,    4,    5,    7,    9,    13,    17,    25,    33,   49,
    65,   97,   129,  193,  257,  385,  513,   769,   1025,  1537, 2049, 3073,
    4097, 6145, 8193, 12289, 16385, 24577, 0,     0};
static const int kDistExtra[] = {
    0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0};

static const int kCLengthOrder[] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

bool inflate_decompress(const std::uint8_t* src, std::size_t src_len,
                        std::vector<std::uint8_t>& out) {
  BitReader br(src, src_len);
  Huffman ldecode, ddecode;

  for (;;) {
    int bfinal = br.read_bit();
    if (bfinal < 0) return false;
    int btype = br.read_bits(2);
    if (btype < 0) return false;

    if (btype == 0) {
      // Stored block
      br.align_to_byte();
      if (br.remaining() < 4) return false;
      std::uint16_t len = static_cast<std::uint16_t>(br.read_bits(8));
      len |= static_cast<std::uint16_t>(br.read_bits(8)) << 8;
      std::uint16_t nlen = static_cast<std::uint16_t>(br.read_bits(8));
      nlen |= static_cast<std::uint16_t>(br.read_bits(8)) << 8;
      if (len != (~nlen & 0xFFFF)) return false;
      for (std::uint16_t i = 0; i < len; ++i) {
        int byte = br.read_bits(8);
        if (byte < 0) return false;
        out.push_back(static_cast<std::uint8_t>(byte));
      }
    } else if (btype == 1) {
      // Fixed Huffman
      int lengths[288];
      for (int i = 0; i < 144; ++i) lengths[i] = 8;
      for (int i = 144; i < 256; ++i) lengths[i] = 9;
      for (int i = 256; i < 280; ++i) lengths[i] = 7;
      for (int i = 280; i < 288; ++i) lengths[i] = 8;
      if (!ldecode.build(lengths, 288)) return false;

      int dlengths[30];
      for (int i = 0; i < 30; ++i) dlengths[i] = 5;
      if (!ddecode.build(dlengths, 30)) return false;

      if (!decode_block(br, ldecode, ddecode, out)) return false;
    } else if (btype == 2) {
      // Dynamic Huffman
      int hlit = br.read_bits(5);
      int hdist = br.read_bits(5);
      int hclen = br.read_bits(4);
      if (hlit < 0 || hdist < 0 || hclen < 0) return false;
      hlit += 257;
      hdist += 1;
      hclen += 4;

      int clengths[19];
      std::memset(clengths, 0, sizeof(clengths));
      for (int i = 0; i < hclen; ++i) {
        clengths[kCLengthOrder[i]] = br.read_bits(3);
      }

      Huffman cdecode;
      if (!cdecode.build(clengths, 19)) return false;

      int total = hlit + hdist;
      int lengths[320];
      std::memset(lengths, 0, sizeof(lengths));
      int idx = 0;
      while (idx < total) {
        int sym = cdecode.decode(br);
        if (sym < 0) return false;
        if (sym < 16) {
          lengths[idx++] = sym;
        } else if (sym == 16) {
          if (idx == 0) return false;
          int rep = br.read_bits(2);
          if (rep < 0) return false;
          int prev = lengths[idx - 1];
          for (int i = 0; i < rep + 3; ++i) {
            if (idx >= total) return false;
            lengths[idx++] = prev;
          }
        } else if (sym == 17) {
          int rep = br.read_bits(3);
          if (rep < 0) return false;
          for (int i = 0; i < rep + 3; ++i) {
            if (idx >= total) return false;
            lengths[idx++] = 0;
          }
        } else if (sym == 18) {
          int rep = br.read_bits(7);
          if (rep < 0) return false;
          for (int i = 0; i < rep + 11; ++i) {
            if (idx >= total) return false;
            lengths[idx++] = 0;
          }
        } else {
          return false;
        }
      }

      if (!ldecode.build(lengths, hlit)) return false;
      if (!ddecode.build(lengths + hlit, hdist)) return false;

      if (!decode_block(br, ldecode, ddecode, out)) return false;
    } else {
      return false;  // reserved block type
    }

    if (bfinal) break;
  }

  return true;
}

bool decode_block(BitReader& br, Huffman& ldecode, Huffman& ddecode,
                  std::vector<std::uint8_t>& out) {
  for (;;) {
    int sym = ldecode.decode(br);
    if (sym < 0) return false;

    if (sym < 256) {
      out.push_back(static_cast<std::uint8_t>(sym));
    } else if (sym == 256) {
      return true;
    } else {
      // Length/distance pair
      int li = sym - 257;
      if (li < 0 || li >= 29) return false;
      int length = kLengthBase[li];
      if (kLengthExtra[li] > 0) {
        int extra = br.read_bits(kLengthExtra[li]);
        if (extra < 0) return false;
        length += extra;
      }

      int dsym = ddecode.decode(br);
      if (dsym < 0 || dsym >= 30) return false;
      int dist = kDistBase[dsym];
      if (kDistExtra[dsym] > 0) {
        int extra = br.read_bits(kDistExtra[dsym]);
        if (extra < 0) return false;
        dist += extra;
      }

      if (static_cast<std::size_t>(dist) > out.size()) return false;
      std::size_t start = out.size() - dist;
      for (int i = 0; i < length; ++i) {
        out.push_back(out[start + i]);
      }
    }
  }
}

}  // namespace wilfred
