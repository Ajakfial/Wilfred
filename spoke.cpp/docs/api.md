# API reference

## C++ engine

```cpp
#include "spoke/transcriber.h"

spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
cfg.num_threads = 4;   // 0 = auto
cfg.beam_width = 8;

spoke::Transcriber engine(cfg);
spoke::TranscriptionResult r;
spoke::Status st = engine.transcribeFile("speech.wav", r);
if (!st.ok()) { /* st.message */ }
// r.text, r.segments[{start_sec,end_sec,text,confidence}], r.rtf
```

Streaming:

```cpp
spoke::StreamingTranscriber s(cfg);
s.feed(pcm16k.data(), pcm16k.size());  // call repeatedly
spoke::TranscriptionResult r;
s.flush(r);
```

Key knobs (`SpokeConfig`): `sample_rate`, `n_mels` (64), `encoder_dim` (128),
`encoder_layers` (3), `num_threads`, `use_int8`, `beam_width`, `lm_weight`,
`vad_threshold_db` (−38), `max_segment_sec` (30). `validate()` rejects
nonsense; `describe()` prints the effective config.

## C ABI (FFI / P-Invoke surface)

```c
SpokeHandle* spoke_create(void);
SpokeHandle* spoke_create_with_options(int num_threads, int beam_width);
void  spoke_destroy(SpokeHandle*);
char* spoke_transcribe_file(SpokeHandle*, const char* wav_path, int json_output,
                            int* error_code, char** error_message);
char* spoke_transcribe_pcm(SpokeHandle*, const float* pcm, int num_samples,
                           int json_output, int* error_code, char** error_message);
const char* spoke_version(void);
void  spoke_free_string(char* s);
```

Rules: free returned strings with `spoke_free_string` (also free
`error_message` when non-NULL). `json_output=1` returns
`{"text":…, "segments":[{start,end,text,confidence}], "rtf":…, "model":…}`.
Never throws; NULL return means `*error_code > 0`.

## C# interop (MAUI)

`ui/SpokeApp/Services/SpokeNativeService.cs` wraps the ABI:

```csharp
using var engine = new SpokeNativeService(threads: 0, beamWidth: 8);
string text = engine.TranscribeFile("speech.wav");
TranscriptionResult r = engine.TranscribeFileJson("speech.wav"); // parsed JSON
```

`TranscriptionService.cs` adds file-picking, WAV validation, and
`IProgress<double>` reporting on top; `MainViewModel` is the bindable
surface the XAML consumes. No DSP code lives in C# — presentation only.
