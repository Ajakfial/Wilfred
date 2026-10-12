// spoke-console — cross-platform .NET host for the spoke.cpp engine.
// Same service layer as the MAUI app (shared sources, see .csproj).
// The native library is located per-OS by SpokeNativeService:
//   Windows: spoke.dll | Linux: libspoke.so | macOS: libspoke.dylib
// Override the search directory with --lib DIR or SPOKE_LIBRARY_PATH.
using SpokeApp.Services;

static void Usage()
{
    Console.WriteLine("spoke-console — offline English transcription (.NET host)");
    Console.WriteLine();
    Console.WriteLine("Usage:");
    Console.WriteLine("  spoke-console info [--lib DIR]");
    Console.WriteLine("  spoke-console transcribe <audio.wav> [--json] [--threads N] [--beam N] [--lib DIR]");
    Console.WriteLine();
    Console.WriteLine("Native library lookup: --lib DIR, then SPOKE_LIBRARY_PATH, then app directory.");
}

string? libDir = null;
var rest = new List<string>();
for (int i = 0; i < args.Length; i++)
{
    if (args[i] == "--lib" && i + 1 < args.Length) { libDir = args[++i]; }
    else if (args[i] == "-h" || args[i] == "--help") { Usage(); return 0; }
    else rest.Add(args[i]);
}
if (!string.IsNullOrWhiteSpace(libDir))
    SpokeNativeService.LibrarySearchPath = libDir;

if (rest.Count == 0) { Usage(); return 1; }

try
{
    if (rest[0] == "info")
    {
        using var engine = new SpokeNativeService();
        Console.WriteLine($"spoke-console  engine={engine.EngineVersion}  rid={System.Runtime.InteropServices.RuntimeInformation.RuntimeIdentifier}");
        return 0;
    }

    if (rest[0] == "transcribe" && rest.Count >= 2)
    {
        string wav = rest[1];
        bool json = rest.Contains("--json");
        int threads = TakeInt(rest, "--threads", 0);
        int beam = TakeInt(rest, "--beam", 8);

        using var engine = new SpokeNativeService(threads, beam);
        var started = DateTime.UtcNow;
        var result = engine.TranscribeFileJson(wav);
        double secs = (DateTime.UtcNow - started).TotalSeconds;
        if (json)
        {
            Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new
            {
                text = result.Text,
                segments = result.Segments.Select(s => new { start = s.StartSec, end = s.EndSec, text = s.Text, confidence = s.Confidence }),
                rtf = result.Rtf,
                model = result.Model,
            }));
        }
        else
        {
            Console.WriteLine(result.Text);
            Console.Error.WriteLine($"[spoke-console] {result.Segments.Count} segment(s), RTF {result.Rtf:F3}, {secs:F2}s wall");
        }
        return 0;
    }

    Usage();
    return 1;
}
catch (Exception ex)
{
    Console.Error.WriteLine("error: " + ex.Message);
    return 1;
}

static int TakeInt(List<string> argv, string flag, int fallback)
{
    int i = argv.IndexOf(flag);
    if (i >= 0 && i + 1 < argv.Count && int.TryParse(argv[i + 1], out int v))
        return v;
    return fallback;
}
