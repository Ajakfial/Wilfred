using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using SpokeApp.Models;

namespace SpokeApp.Services;

/// <summary>
/// Thin, disposable P/Invoke wrapper over the C++ engine
/// (spoke.dll on Windows, libspoke.so on Linux, libspoke.dylib on macOS).
/// Owns no DSP logic: every method marshals to the C ABI in include/spoke/api.h
/// and parses the JSON envelope back into records. Thread-safe; one instance per app.
/// </summary>
public sealed class SpokeNativeService : IDisposable, ISpokeEngine
{
    private const string LibraryName = "spoke";
    private IntPtr _handle;
    private bool _disposed;

    /// <summary>
    /// Optional directory override for native-library probing
    /// (e.g. a folder holding libspoke.so / libspoke.dylib / spoke.dll).
    /// Falls back to SPOKE_LIBRARY_PATH, then the app directory, then the OS loader.
    /// </summary>
    public static string? LibrarySearchPath { get; set; }

    static SpokeNativeService()
    {
        NativeLibrary.SetDllImportResolver(typeof(SpokeNativeService).Assembly, ResolveSpokeLibrary);
    }

    public string EngineVersion => Marshal.PtrToStringUTF8(spoke_version()) ?? "unknown";

    public SpokeNativeService(int threads = 0, int beamWidth = 8)
    {
        _handle = spoke_create_with_options(threads, beamWidth);
        if (_handle == IntPtr.Zero)
            throw new SpokeException("Failed to initialize the spoke engine (native create returned NULL).");
    }

    public string TranscribeFile(string wavPath)
    {
        EnsureAlive(wavPath);
        int code = 0;
        IntPtr errPtr = IntPtr.Zero;
        IntPtr outPtr = spoke_transcribe_file(_handle, wavPath, 0, ref code, ref errPtr);
        return TakeString(outPtr, code, errPtr);
    }

    public TranscriptionResult TranscribeFileJson(string wavPath)
    {
        EnsureAlive(wavPath);
        int code = 0;
        IntPtr errPtr = IntPtr.Zero;
        IntPtr outPtr = spoke_transcribe_file(_handle, wavPath, 1, ref code, ref errPtr);
        string json = TakeString(outPtr, code, errPtr);
        return Parse(json);
    }

    public TranscriptionResult TranscribePcm(float[] pcm16kMono)
    {
        if (pcm16kMono is null || pcm16kMono.Length == 0)
            throw new ArgumentException("PCM buffer is empty.", nameof(pcm16kMono));
        int code = 0;
        IntPtr errPtr = IntPtr.Zero;
        IntPtr outPtr = spoke_transcribe_pcm(_handle, pcm16kMono, pcm16kMono.Length, 1, ref code, ref errPtr);
        return Parse(TakeString(outPtr, code, errPtr));
    }

    // --- internals -----------------------------------------------------------

    private void EnsureAlive(string wavPath)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        if (string.IsNullOrWhiteSpace(wavPath) || !File.Exists(wavPath))
            throw new FileNotFoundException("Audio file not found.", wavPath);
    }

    private static string TakeString(IntPtr outPtr, int code, IntPtr errPtr)
    {
        string? err = errPtr != IntPtr.Zero ? Marshal.PtrToStringUTF8(errPtr) : null;
        if (errPtr != IntPtr.Zero) spoke_free_string(errPtr);
        if (outPtr == IntPtr.Zero)
            throw new SpokeException($"Engine error (code {code}): {err ?? "unknown"}");
        string text = Marshal.PtrToStringUTF8(outPtr) ?? string.Empty;
        spoke_free_string(outPtr);
        return text;
    }

    public static TranscriptionResult Parse(string json)
    {
        using var doc = JsonDocument.Parse(json);
        var root = doc.RootElement;
        string text = root.GetProperty("text").GetString() ?? string.Empty;
        string model = root.TryGetProperty("model", out var m) ? m.GetString() ?? "spoke-tiny.en" : "spoke-tiny.en";
        double rtf = root.TryGetProperty("rtf", out var r) ? r.GetDouble() : 0;
        var segs = new List<Segment>();
        if (root.TryGetProperty("segments", out var arr))
        {
            foreach (var s in arr.EnumerateArray())
            {
                segs.Add(new Segment(
                    s.GetProperty("start").GetDouble(),
                    s.GetProperty("end").GetDouble(),
                    s.GetProperty("text").GetString() ?? string.Empty,
                    (float)s.GetProperty("confidence").GetDouble()));
            }
        }
        return new TranscriptionResult(text, segs, rtf, model);
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            if (_handle != IntPtr.Zero) { spoke_destroy(_handle); _handle = IntPtr.Zero; }
            _disposed = true;
        }
        GC.SuppressFinalize(this);
    }

    // Keep signatures byte-identical to include/spoke/api.h.
    // The DllImport name "spoke" is resolved by ResolveSpokeLibrary below,
    // which is what makes one C# codebase work on Windows (.dll),
    // Linux (.so) and macOS (.dylib) without per-platform builds.
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr spoke_create_with_options(int num_threads, int beam_width);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void spoke_destroy(IntPtr handle);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr spoke_transcribe_file(IntPtr handle, string wav_path, int json_output, ref int error_code, ref IntPtr error_message);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr spoke_transcribe_pcm(IntPtr handle, [In] float[] pcm, int num_samples, int json_output, ref int error_code, ref IntPtr error_message);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr spoke_version();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void spoke_free_string(IntPtr s);

    // --- cross-platform library probing --------------------------------------

    private static string[] CandidateFileNames()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            return new[] { "spoke.dll" };
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
            return new[] { "libspoke.dylib", "libspoke.0.dylib" };
        return new[] { "libspoke.so", "libspoke.so.0" };
    }

    private static IntPtr ResolveSpokeLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName != LibraryName)
            return IntPtr.Zero;

        var dirs = new List<string>();
        if (!string.IsNullOrWhiteSpace(LibrarySearchPath))
            dirs.Add(LibrarySearchPath);
        string? env = Environment.GetEnvironmentVariable("SPOKE_LIBRARY_PATH");
        if (!string.IsNullOrWhiteSpace(env))
        {
            if (File.Exists(env) && NativeLibrary.TryLoad(env, out IntPtr direct))
                return direct;
            if (Directory.Exists(env))
                dirs.Add(env);
            else if (!string.IsNullOrWhiteSpace(Path.GetDirectoryName(env)))
                dirs.Add(Path.GetDirectoryName(env)!);
        }
        dirs.Add(AppContext.BaseDirectory);

        foreach (string dir in dirs)
        {
            if (string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir))
                continue;
            foreach (string file in CandidateFileNames())
            {
                string full = Path.Combine(dir, file);
                if (File.Exists(full) && NativeLibrary.TryLoad(full, out IntPtr h))
                    return h;
            }
        }

        // Last resort: OS default probing (app dir, rpath, system paths).
        if (NativeLibrary.TryLoad(LibraryName, assembly, searchPath, out IntPtr fallback))
            return fallback;
        return IntPtr.Zero;
    }
}

/// <summary>Typed engine failure (native error codes, file problems, parse issues).</summary>
public sealed class SpokeException : Exception
{
    public SpokeException(string message) : base(message) { }
    public SpokeException(string message, Exception inner) : base(message, inner) { }
}
