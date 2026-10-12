// Minimal streaming-over-stdin example (advanced users).
// Pipes raw 16 kHz float32 PCM via stdin, prints partial transcripts per chunk.
// Build: part of spoke_cli? Kept as documentation-grade example instead.
#include <cstdio>
#include <iostream>
#include <vector>
#include "spoke/transcriber.h"

int main() {
  spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
  spoke::StreamingTranscriber stream(cfg);
  constexpr size_t CHUNK = 16000;  // 1 s
  std::vector<float> buf(CHUNK);
  while (std::fread(buf.data(), sizeof(float), CHUNK, stdin) == CHUNK) {
    stream.feed(buf);
  }
  spoke::TranscriptionResult r;
  auto st = stream.flush(r);
  if (!st.ok()) {
    std::cerr << "error: " << st.message << "\n";
    return 1;
  }
  std::cout << r.text << "\n";
  return 0;
}
