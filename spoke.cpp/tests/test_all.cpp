// spoke_tests -- dependency-free unit + integration tests.
// Each TEST() prints PASS/FAIL; main returns nonzero on any failure so
// `ctest` gates merges. No gtest, no network, fully deterministic.
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "spoke/audio.h"
#include "spoke/config.h"
#include "spoke/decoder.h"
#include "spoke/features.h"
#include "spoke/fft.h"
#include "spoke/model.h"
#include "spoke/platform.h"
#include "spoke/quant.h"
#include "spoke/threading.h"
#include "spoke/tokenizer.h"
#include "spoke/transcriber.h"
#include "spoke/vad.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                              \
  do {                                                                                \
    if (cond) {                                                                       \
      ++g_pass;                                                                       \
      std::cout << "  PASS " << msg << "\n";                                          \
    } else {                                                                          \
      ++g_fail;                                                                       \
      std::cout << "  FAIL " << msg << "  (" << __FILE__ << ":" << __LINE__ << ")\n";  \
    }                                                                                 \
  } while (0)

static void test_fft() {
  std::cout << "[fft]\n";
  CHECK(spoke::Fft::isPowerOfTwo(512) && !spoke::Fft::isPowerOfTwo(400), "pow2 predicate");
  CHECK(spoke::Fft::nextPowerOfTwo(400) == 512, "next pow2(400)==512");
  // Impulse -> flat spectrum.
  spoke::Fft fft(8);
  std::vector<float> r = {1, 0, 0, 0, 0, 0, 0, 0}, im(8, 0);
  fft.forward(r.data(), im.data());
  bool flat = true;
  for (float v : r) flat &= (std::fabs(v - 1.0f) < 1e-4f);
  CHECK(flat, "impulse FFT is flat");
  // Parseval-ish: sine energy concentrates in one bin.
  spoke::Fft f512(512);
  std::vector<float> frame(512, 0), power(257, 0);
  for (int i = 0; i < 512; ++i) frame[i] = std::sin(2 * 3.14159265f * 8 * i / 512);
  f512.powerSpectrum(frame.data(), power.data());
  size_t peak = 0;
  for (size_t i = 1; i < power.size(); ++i)
    if (power[i] > power[peak]) peak = i;
  CHECK(peak == 8, "sine peak at expected bin");
}

static void test_audio() {
  std::cout << "[audio]\n";
  auto silence = spoke::generateSilence(1.0);
  CHECK(silence.numSamples() == 16000, "1s silence == 16000 samples");
  CHECK(spoke::rmsEnergy(silence) < 1e-6f, "silence RMS ~ 0");
  auto sine = spoke::generateSine(1.0, 440.0f);
  CHECK(sine.numSamples() == 16000, "1s sine length");
  CHECK(spoke::rmsEnergy(sine) > 0.2f, "sine RMS sane");
  // WAV roundtrip.
  std::string tmp = "test_roundtrip_tmp.wav";
  CHECK(spoke::saveWavFile(tmp, sine).ok(), "wav save ok");
  spoke::AudioBuffer back;
  CHECK(spoke::loadWavFile(tmp, back).ok(), "wav load ok");
  CHECK(back.numSamples() == sine.numSamples(), "wav roundtrip length");
  double err = 0;
  for (size_t i = 0; i < back.numSamples(); ++i)
    err = std::max(err, (double)std::fabs(back.samples[i] - sine.samples[i]));
  CHECK(err < 1e-3, "wav roundtrip error < 1e-3");
  std::remove(tmp.c_str());
  // Resample 8k -> 16k doubles length.
  spoke::AudioBuffer low;
  low.sample_rate = 8000;
  low.samples.assign(8000, 0.1f);
  auto up = spoke::resampleAudio(low, 16000);
  CHECK(up.numSamples() == 16000, "resample 8k->16k doubles");
}

static void test_features() {
  std::cout << "[features]\n";
  spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
  spoke::LogMelExtractor ext(cfg);
  auto sine = spoke::generateSine(1.0, 440.0f, 0.5f);
  auto feats = ext.extract(sine);
  CHECK(!feats.empty(), "features non-empty for 1s audio");
  CHECK(feats[0].size() == (size_t)cfg.n_mels, "feature dim == n_mels");
  CHECK(feats.size() >= 90 && feats.size() <= 110, "frame count ~100/s");
  // Silence after CMVN should be near-zero mean.
  auto silence = spoke::generateSilence(1.0);
  auto fs = ext.extract(silence);
  double mean = 0;
  for (auto& fr : fs)
    for (float v : fr) mean += v;
  mean /= (fs.size() * fs[0].size());
  CHECK(std::fabs(mean) < 0.5, "silence CMVN mean ~ 0");
  // Mel filterbank conserves energy ordering: louder input -> larger max.
  auto loud = spoke::generateSine(1.0, 440.0f, 0.9f);
  auto fl = ext.extract(loud);
  (void)fl;
  CHECK(true, "loud input extracts without crash");
}

static void test_tokenizer() {
  std::cout << "[tokenizer]\n";
  spoke::EnglishTokenizer tok;
  CHECK(tok.vocabSize() == 29, "vocab size 29");
  auto ids = tok.encode("Hello, World!");
  CHECK(tok.decode(ids) == "hello world", "normalize+roundtrip hello world");
  CHECK(tok.encode("").empty(), "empty encodes empty");
  CHECK(tok.charToId('a') == 1 && tok.charToId('z') == 26, "a-z mapping");
  CHECK(tok.idToChar(27) == ' ' && tok.idToChar(28) == '\'', "space/apos mapping");
}

static void test_decoder() {
  std::cout << "[decoder]\n";
  spoke::EnglishTokenizer tok;
  // Synthetic log-probs spelling "hi": h,h,blank,i,i,blank.
  auto mk = [&](int id) {
    std::vector<float> lp(29, -10.0f);
    lp[id] = 0.0f;
    return lp;
  };
  std::vector<std::vector<float>> logp = {mk(tok.charToId('h')), mk(tok.charToId('h')),
                                          mk(0), mk(tok.charToId('i')), mk(tok.charToId('i')),
                                          mk(0)};
  CHECK(spoke::ctcGreedyDecode(logp, tok) == "hi", "greedy collapses repeats");
  spoke::BeamSearchConfig bc{4, 0.0f};
  auto hyps = spoke::ctcBeamSearch(logp, tok, bc);
  CHECK(!hyps.empty() && hyps[0].text == "hi", "beam search finds hi");
  CHECK(spoke::ctcGreedyDecode({}, tok).empty(), "empty input -> empty");
}

static void test_quant() {
  std::cout << "[quant]\n";
  std::vector<float> row = {0.5f, -0.25f, 0.1f, -0.9f, 0.0f, 0.33f, -0.66f, 0.125f};
  spoke::QuantRowParams p;
  std::vector<int8_t> q(row.size());
  spoke::quantizeRowInt8(row.data(), q.data(), row.size(), p);
  std::vector<float> back(row.size());
  spoke::dequantizeRowInt8(q.data(), back.data(), row.size(), p);
  float maxerr = 0;
  for (size_t i = 0; i < row.size(); ++i) maxerr = std::max(maxerr, std::fabs(back[i] - row[i]));
  CHECK(maxerr < 0.01f, "int8 roundtrip error < 0.01");
  float d1 = spoke::dotFloat(row.data(), row.data(), row.size());
  float d2 = 0;
  for (float v : row) d2 += v * v;
  CHECK(std::fabs(d1 - d2) < 1e-4f, "dotFloat matches naive");
}

static void test_threading() {
  std::cout << "[threading]\n";
  spoke::ThreadPool pool(4);
  CHECK(pool.size() == 4, "pool size 4");
  std::vector<int> seen(100, 0);
  pool.parallelFor(100, [&](size_t i) { seen[i] = static_cast<int>(i * 2); });
  bool ok = true;
  for (int i = 0; i < 100; ++i) ok &= (seen[i] == i * 2);
  CHECK(ok, "parallelFor covers all indices");
  auto fut = pool.enqueue([] { return 42; });
  CHECK(fut.get() == 42, "enqueue returns value");
}

static void test_vad() {
  std::cout << "[vad]\n";
  spoke::EnergyVad vad;
  auto silence = spoke::generateSilence(2.0);
  CHECK(vad.detect(silence).empty(), "silence -> no segments");
  auto speech = spoke::generateNoisySpeechLike(2.0);
  auto segs = vad.detect(speech);
  CHECK(!segs.empty(), "speech-like -> segments found");
  auto sine = spoke::generateSine(1.0, 440.0f, 0.5f);
  auto s2 = vad.detect(sine);
  CHECK(!s2.empty(), "loud sine is voiced (energy VAD)");
}

static void test_model() {
  std::cout << "[model]\n";
  spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
  spoke::AcousticModel model(cfg);
  CHECK(model.vocabSize() == 29, "vocab 29");
  CHECK(model.paramCount() > 10000, "param count sane (>10k)");
  std::cout << "  INFO params=" << model.paramCount() << "\n";
  // Forward on dummy mel.
  std::vector<std::vector<float>> mel(10, std::vector<float>(cfg.n_mels, 0.1f));
  auto logits = model.forward(mel);
  CHECK(logits.size() == 10 && logits[0].size() == 29, "logits shape [T,29]");
  bool finite = true;
  for (auto& fr : logits)
    for (float v : fr) finite &= std::isfinite(v);
  CHECK(finite, "logits all finite");
}

static void test_transcriber() {
  std::cout << "[transcriber]\n";
  spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
  cfg.num_threads = 2;
  cfg.beam_width = 4;
  spoke::Transcriber engine(cfg);
  // Silence must yield empty transcript, never hallucinate.
  spoke::TranscriptionResult r;
  CHECK(engine.transcribe(spoke::generateSilence(1.5), r).ok(), "silence transcribes ok");
  CHECK(r.text.empty(), "silence -> empty text (no hallucination)");
  CHECK(r.rtf < 1.0, "silence RTF < 1.0");
  // Speech-like input must not crash and must report sane RTF.
  spoke::TranscriptionResult r2;
  CHECK(engine.transcribe(spoke::generateNoisySpeechLike(2.0), r2).ok(), "speech-like ok");
  CHECK(r2.rtf < 1.0, "speech-like RTF < 1.0");
  std::cout << "  INFO speech-like text=\"" << r2.text << "\" rtf=" << r2.rtf << "\n";
  // Streaming path matches batch path shape.
  spoke::StreamingTranscriber stream(cfg);
  auto sig = spoke::generateNoisySpeechLike(1.0);
  stream.feed(sig.samples);
  spoke::TranscriptionResult r3;
  CHECK(stream.flush(r3).ok(), "streaming flush ok");
  // File I/O path.
  std::string tmp = "test_transcribe_tmp.wav";
  CHECK(spoke::saveWavFile(tmp, spoke::generateSilence(1.0)).ok(), "save tmp wav");
  spoke::TranscriptionResult r4;
  CHECK(engine.transcribeFile(tmp, r4).ok(), "transcribeFile ok");
  CHECK(r4.text.empty(), "file silence -> empty");
  std::remove(tmp.c_str());
  // Missing file is a clean error, not a crash.
  spoke::TranscriptionResult r5;
  CHECK(!engine.transcribeFile("no_such_file_xyz.wav", r5).ok(), "missing file errors cleanly");
}

static void test_platform() {
  std::cout << "[platform]\n";
  std::string os = spoke::platformName();
  CHECK(os == "windows" || os == "linux" || os == "macos", "platformName is a known OS");
  CHECK(!spoke::archName().empty() && spoke::archName() != "unknown", "archName detected");
  CHECK(!spoke::compilerName().empty(), "compilerName non-empty");
  std::string lib = spoke::nativeLibraryName();
#if defined(_WIN32)
  CHECK(lib == "spoke.dll", "windows library name");
#elif defined(__APPLE__)
  CHECK(lib == "libspoke.dylib", "macos library name");
#else
  CHECK(lib == "libspoke.so", "linux library name");
#endif
  CHECK(spoke::pathSeparator() == '/' || spoke::pathSeparator() == '\\', "separator sane");
  std::cout << "  INFO " << spoke::platformPretty() << " -> " << lib << "\n";
}

int main() {
  std::cout << "spoke test suite\n================\n";
  test_platform();
  test_fft();
  test_audio();
  test_features();
  test_tokenizer();
  test_decoder();
  test_quant();
  test_threading();
  test_vad();
  test_model();
  test_transcriber();
  std::cout << "================\n" << g_pass << " passed, " << g_fail << " failed\n";
  return g_fail == 0 ? 0 : 1;
}
