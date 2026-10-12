using SpokeApp.Models;

namespace SpokeApp.Services;

/// <summary>Minimal engine contract so ViewModels stay testable without spoke.dll.</summary>
public interface ISpokeEngine
{
    string EngineVersion { get; }
    string TranscribeFile(string wavPath);
    TranscriptionResult TranscribeFileJson(string wavPath);
    TranscriptionResult TranscribePcm(float[] pcm16kMono);
}
