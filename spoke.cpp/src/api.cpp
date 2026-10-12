#include "spoke/api.h"
#include <cstring>
#include <string>
#include <vector>

#include "spoke/audio.h"
#include "spoke/config.h"
#include "spoke/transcriber.h"
#include "spoke/version.h"

struct SpokeHandle {
  spoke::Transcriber* engine = nullptr;
};

static char* dup_str(const std::string& s) {
  char* p = new char[s.size() + 1];
  std::memcpy(p, s.c_str(), s.size() + 1);
  return p;
}

static std::string escape_json(const std::string& s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default: o += c;
    }
  }
  return o;
}

static std::string result_to_json(const spoke::TranscriptionResult& r) {
  std::string j = "{\"text\":\"" + escape_json(r.text) + "\",\"segments\":[";
  for (size_t i = 0; i < r.segments.size(); ++i) {
    if (i) j += ",";
    j += "{\"start\":" + std::to_string(r.segments[i].start_sec) + ",\"end\":" +
         std::to_string(r.segments[i].end_sec) + ",\"text\":\"" +
         escape_json(r.segments[i].text) + "\",\"confidence\":" +
         std::to_string(r.segments[i].confidence) + "}";
  }
  j += "],\"rtf\":" + std::to_string(r.rtf) + ",\"model\":\"" + r.model_name + "\"}";
  return j;
}

extern "C" {

SpokeHandle* spoke_create(void) { return spoke_create_with_options(0, 8); }

SpokeHandle* spoke_create_with_options(int num_threads, int beam_width) {
  try {
    auto* h = new SpokeHandle();
    spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
    if (num_threads > 0) cfg.num_threads = num_threads;
    if (beam_width > 0) cfg.beam_width = beam_width;
    h->engine = new spoke::Transcriber(cfg);
    return h;
  } catch (...) {
    return nullptr;
  }
}

void spoke_destroy(SpokeHandle* handle) {
  if (!handle) return;
  delete handle->engine;
  delete handle;
}

char* spoke_transcribe_file(SpokeHandle* handle, const char* wav_path, int json_output,
                            int* error_code, char** error_message) {
  if (error_code) *error_code = 0;
  if (error_message) *error_message = nullptr;
  if (!handle || !handle->engine || !wav_path) {
    if (error_code) *error_code = 1;
    if (error_message) *error_message = dup_str("null handle or path");
    return nullptr;
  }
  try {
    spoke::TranscriptionResult r;
    spoke::Status s = handle->engine->transcribeFile(wav_path, r);
    if (!s.ok()) {
      if (error_code) *error_code = static_cast<int>(s.code) + 1;
      if (error_message) *error_message = dup_str(s.message);
      return nullptr;
    }
    std::string out = json_output ? result_to_json(r) : r.text;
    return dup_str(out);
  } catch (const std::exception& e) {
    if (error_code) *error_code = 99;
    if (error_message) *error_message = dup_str(e.what());
    return nullptr;
  } catch (...) {
    if (error_code) *error_code = 99;
    if (error_message) *error_message = dup_str("unknown error");
    return nullptr;
  }
}

char* spoke_transcribe_pcm(SpokeHandle* handle, const float* pcm, int num_samples,
                           int json_output, int* error_code, char** error_message) {
  if (error_code) *error_code = 0;
  if (error_message) *error_message = nullptr;
  if (!handle || !handle->engine || !pcm || num_samples <= 0) {
    if (error_code) *error_code = 1;
    if (error_message) *error_message = dup_str("null handle or pcm");
    return nullptr;
  }
  try {
    spoke::AudioBuffer a;
    a.sample_rate = 16000;
    a.samples.assign(pcm, pcm + num_samples);
    spoke::TranscriptionResult r;
    spoke::Status s = handle->engine->transcribe(a, r);
    if (!s.ok()) {
      if (error_code) *error_code = static_cast<int>(s.code) + 1;
      if (error_message) *error_message = dup_str(s.message);
      return nullptr;
    }
    return dup_str(json_output ? result_to_json(r) : r.text);
  } catch (const std::exception& e) {
    if (error_code) *error_code = 99;
    if (error_message) *error_message = dup_str(e.what());
    return nullptr;
  } catch (...) {
    if (error_code) *error_code = 99;
    if (error_message) *error_message = dup_str("unknown error");
    return nullptr;
  }
}

const char* spoke_version(void) { return SPOKE_VERSION_STRING; }

void spoke_free_string(char* s) { delete[] s; }

}  // extern "C"
