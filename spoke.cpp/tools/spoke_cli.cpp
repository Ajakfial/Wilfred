// spoke_cli — command-line frontend for the spoke engine.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "spoke/audio.h"
#include "spoke/config.h"
#include "spoke/features.h"
#include "spoke/model.h"
#include "spoke/platform.h"
#include "spoke/transcriber.h"
#include "spoke/version.h"

static void print_usage(const char* prog) {
  std::cout << "spoke " << SPOKE_VERSION_STRING << " — from-scratch English transcriber\n\n"
            << "Usage:\n"
            << "  " << prog << " transcribe <file.wav> [--threads N] [--beam N] [--json]\n"
            << "  " << prog << " info\n"
            << "  " << prog << " synth <out.wav> [seconds]\n\n"
            << "Notes:\n"
            << "  - Input WAV: 16-bit PCM mono/stereo, any sample rate (resampled to 16k).\n"
            << "  - All inference is local CPU, no network, no whisper code.\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
  std::string cmd = argv[1];
  if (cmd == "info") {
    spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
    spoke::Transcriber t(cfg);
    std::cout << "spoke version : " << SPOKE_VERSION_STRING << "\n"
              << "platform      : " << spoke::platformPretty() << "\n"
              << "native lib    : " << spoke::nativeLibraryName() << "\n"
              << "model         : spoke-tiny.en\n"
              << "config        : " << cfg.describe() << "\n"
              << "params        : " << t.modelParams() << "\n"
              << "sample rate   : 16000 Hz\n"
              << "license       : MIT\n";
    return 0;
  }
  if (cmd == "synth") {
    if (argc < 3) {
      std::cerr << "synth needs <out.wav>\n";
      return 1;
    }
    double secs = argc >= 4 ? std::stod(argv[3]) : 2.0;
    auto a = spoke::generateNoisySpeechLike(secs);
    auto s = spoke::saveWavFile(argv[2], a);
    if (!s.ok()) {
      std::cerr << "write failed: " << s.message << "\n";
      return 1;
    }
    std::cout << "wrote " << argv[2] << " (" << secs << "s speech-like)\n";
    return 0;
  }
  if (cmd == "transcribe") {
    if (argc < 3) {
      std::cerr << "transcribe needs <file.wav>\n";
      return 1;
    }
    std::string path = argv[2];
    int threads = 0, beam = 8;
    bool json = false;
    for (int i = 3; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--threads" && i + 1 < argc) threads = std::stoi(argv[++i]);
      else if (a == "--beam" && i + 1 < argc) beam = std::stoi(argv[++i]);
      else if (a == "--json") json = true;
    }
    spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
    if (threads > 0) cfg.num_threads = threads;
    if (beam > 0) cfg.beam_width = beam;
    spoke::Transcriber engine(cfg);
    spoke::TranscriptionResult r;
    auto st = engine.transcribeFile(path, r);
    if (!st.ok()) {
      std::cerr << "error: " << st.message << "\n";
      return 1;
    }
    if (json) {
      std::cout << "{\"text\":\"" << r.text << "\",\"rtf\":" << r.rtf
                << ",\"segments\":" << r.segments.size() << "}\n";
    } else {
      std::cout << r.text << "\n";
      std::cerr << "[spoke] " << r.audio_duration_sec << "s audio in "
                << r.processing_time_sec << "s (RTF " << r.rtf << ", "
                << r.segments.size() << " segments)\n";
    }
    return 0;
  }
  print_usage(argv[0]);
  return 1;
}
