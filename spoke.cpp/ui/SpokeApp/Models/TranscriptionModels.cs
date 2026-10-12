namespace SpokeApp.Models;

/// <summary>One decoded utterance with word-level timing metadata.</summary>
public sealed record Segment(double StartSec, double EndSec, string Text, float Confidence)
{
    public string Timestamp => $"{TimeSpan.FromSeconds(StartSec):mm\\:ss} → {TimeSpan.FromSeconds(EndSec):mm\\:ss}";

    /// UI confidence tier, aligned with the engine's 0.55 suppression gate.
    public string Tier => Confidence >= 0.8f ? "High" : Confidence >= 0.55f ? "Medium" : "Low";
}

/// <summary>Full transcription result surfaced to XAML bindings.</summary>
public sealed record TranscriptionResult(
    string Text,
    IReadOnlyList<Segment> Segments,
    double Rtf,
    string Model)
{
    public string RtfChip => $"RTF {Rtf:F2} · {Model} · local";
    public bool IsEmpty => string.IsNullOrWhiteSpace(Text);
}
