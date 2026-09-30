#pragma once

#include <cstdint>
#include <vector>

namespace wilfred {

class BitReader;
class Huffman;

// Decompress a raw deflate stream (RFC 1951) into out.
bool inflate_decompress(const std::uint8_t* src, std::size_t src_len,
                        std::vector<std::uint8_t>& out);

// Decode a compressed block using the given literal/length and distance Huffman trees.
bool decode_block(class BitReader& br, class Huffman& ldecode, class Huffman& ddecode,
                  std::vector<std::uint8_t>& out);

}  // namespace wilfred
