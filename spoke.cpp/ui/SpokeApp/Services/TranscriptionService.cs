using SpokeApp.Models;

namespace SpokeApp.Services;

/// <summary>
/// App-level transcription workflow: validation, progress, export formatting.
/// Delegates all inference to <see cref="SpokeNativeService"/> (C++ engine).
/// </summary>
public sealed class TranscriptionService
{
    private static readonly HashSet<string> AllowedExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".wav", ".mp3", ".m4a", ".flac", ".ogg",
    };

    private readonly ISpokeEngine _engine;

    public TranscriptionService(ISpokeEngine engine)
    {
        _engine = engine;
    }

    public string EngineVersion => _engine.EngineVersion;

    public static bool IsSupportedFile(string path) =>
        AllowedExtensions.Contains(Path.GetExtension(path));

    /// <summary>Transcribe with coarse progress (pick → decode → done).</summary>
    public Task<TranscriptionResult> TranscribeAsync(
        string path,
        IProgress<double>? progress = null,
        CancellationToken ct = default)
    {
        if (!File.Exists(path))
            throw new FileNotFoundException("Audio file not found.", path);
        if (!IsSupportedFile(path))
            throw new SpokeException($"Unsupported file type '{Path.GetExtension(path)}'. Convert to WAV (16-bit PCM) first.");

        return Task.Run(() =>
        {
            ct.ThrowIfCancellationRequested();
            progress?.Report(0.1);
            TranscriptionResult result;
            try
            {
                result = _engine.TranscribeFileJson(path);
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                throw new SpokeException("Transcription failed: " + ex.Message, ex);
            }
            progress?.Report(1.0);
            return result;
        }, ct);
    }

    public static Task ExportTextAsync(TranscriptionResult result, string destPath) =>
        File.WriteAllTextAsync(destPath, result.Text + Environment.NewLine);

    public static string FormatSegmentLine(Segment s) =>
        $"[{TimeSpan.FromSeconds(s.StartSec):mm\\:ss}–{TimeSpan.FromSeconds(s.EndSec):mm\\:ss}] ({s.Tier}, {s.Confidence:P0}) {s.Text}";
}
