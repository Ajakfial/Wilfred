#pragma once

// C ABI for the .NET MAUI layer (P/Invoke) and any other FFI consumer.
// All strings returned are UTF-8, heap-allocated; free with spoke_free_string.
// All functions are thread-safe and never throw across the boundary.

#ifdef _WIN32
#if defined(SPOKE_BUILDING_DLL)
#define SPOKE_API __declspec(dllexport)
#elif defined(SPOKE_USE_DLL)
#define SPOKE_API __declspec(dllimport)
#else
#define SPOKE_API
#endif
#else
#define SPOKE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SpokeHandle SpokeHandle;

/// Create engine with default tiny config. Returns NULL on failure.
SPOKE_API SpokeHandle* spoke_create(void);

/// Create engine from explicit knobs (threads<=0 means auto).
SPOKE_API SpokeHandle* spoke_create_with_options(int num_threads, int beam_width);

/// Destroy engine created by spoke_create*.
SPOKE_API void spoke_destroy(SpokeHandle* handle);

/// Transcribe a WAV file (16-bit PCM, any sample rate; resampled inside).
/// Returns heap string (JSON when json_output=1, else plain text).
/// On error returns NULL and sets *error_code (>0). Caller frees with spoke_free_string.
/// Caller frees *error_message too (may be NULL).
SPOKE_API char* spoke_transcribe_file(SpokeHandle* handle, const char* wav_path,
                                      int json_output, int* error_code,
                                      char** error_message);

/// Transcribe raw 16 kHz float PCM ([-1,1], mono).
SPOKE_API char* spoke_transcribe_pcm(SpokeHandle* handle, const float* pcm,
                                     int num_samples, int json_output,
                                     int* error_code, char** error_message);

/// Version string, e.g. "0.1.0". Do NOT free.
SPOKE_API const char* spoke_version(void);

/// Free strings returned by spoke_transcribe_*.
SPOKE_API void spoke_free_string(char* s);

#ifdef __cplusplus
}
#endif
