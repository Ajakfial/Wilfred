using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using SpokeApp.Models;
using SpokeApp.Services;

namespace SpokeApp.ViewModels;

/// <summary>
/// Bindable state for MainPage. Framework-agnostic by design: only
/// INotifyPropertyChanged + ICommand, so the workflow is unit-testable
/// without a MAUI host. Navigation / pickers stay in the code-behind.
/// </summary>
public sealed class MainViewModel : INotifyPropertyChanged
{
    private readonly TranscriptionService _service;

    private string _status = "Ready -- pick an audio file or record.";
    private bool _isBusy;
    private double _progress;
    private TranscriptionResult? _result;
    private string _selectedFile = string.Empty;
    private int _beamWidth = 8;
    private int _threads;

    public event PropertyChangedEventHandler? PropertyChanged;

    public MainViewModel(TranscriptionService service)
    {
        _service = service;
        TranscribeCommand = new RelayCommand(async () => await TranscribeAsync(), () => !IsBusy && !string.IsNullOrEmpty(SelectedFile));
        CopyCommand = new RelayCommand(async () => await CopyAsync(), () => Result is { IsEmpty: false });
        ClearCommand = new RelayCommand(Clear);
    }

    // For XAML previewer / unit tests (no native DLL required).
    public MainViewModel() : this(new TranscriptionService(new FakeEngine())) { }

    public string Status { get => _status; private set => Set(ref _status, value); }
    public bool IsBusy { get => _isBusy; private set { if (Set(ref _isBusy, value)) RefreshCanExecute(); } }
    public double Progress { get => _progress; private set => Set(ref _progress, value); }
    public TranscriptionResult? Result { get => _result; private set { if (Set(ref _result, value)) { OnPropertyChanged(nameof(HasResult)); OnPropertyChanged(nameof(Segments)); RefreshCanExecute(); } } }
    public bool HasResult => Result is { IsEmpty: false };
    public ObservableCollection<Segment> Segments { get; } = new();
    public string SelectedFile { get => _selectedFile; set { if (Set(ref _selectedFile, value)) RefreshCanExecute(); } }
    public int BeamWidth { get => _beamWidth; set => Set(ref _beamWidth, Math.Clamp(value, 1, 16)); }
    public int Threads { get => _threads; set => Set(ref _threads, Math.Clamp(value, 0, 8)); }
    public string EngineLabel => $"Engine {_service.EngineVersion} · local CPU · offline";

    public ICommand TranscribeCommand { get; }
    public ICommand CopyCommand { get; }
    public ICommand ClearCommand { get; }

    public Func<string, Task<string>>? PickFileAsync { get; set; }
    public Func<string, Task>? CopyTextAsync { get; set; }

    public async Task BrowseAsync()
    {
        if (PickFileAsync is null || IsBusy) return;
        string path = await PickFileAsync("Audio (*.wav, *.mp3)|*.wav;*.mp3;*.m4a;*.flac;*.ogg");
        if (!string.IsNullOrEmpty(path))
        {
            SelectedFile = path;
            Status = $"Selected: {Path.GetFileName(path)}";
        }
    }

    public async Task TranscribeAsync()
    {
        if (IsBusy || string.IsNullOrEmpty(SelectedFile)) return;
        IsBusy = true;
        Progress = 0;
        Status = "Transcribing locally…";
        try
        {
            var progress = new Progress<double>(p => Progress = p);
            TranscriptionResult r = await _service.TranscribeAsync(SelectedFile, progress);
            Result = r;
            Segments.Clear();
            foreach (var s in r.Segments) Segments.Add(s);
            Status = r.IsEmpty
                ? "Done -- no speech detected (silence is never hallucinated)."
                : $"Done -- {r.Segments.Count} segment(s) · {r.RtfChip}.";
        }
        catch (OperationCanceledException)
        {
            Status = "Cancelled.";
        }
        catch (Exception ex)
        {
            Status = "Error: " + ex.Message;
        }
        finally
        {
            IsBusy = false;
            Progress = 0;
        }
    }

    private async Task CopyAsync()
    {
        if (Result is null || CopyTextAsync is null) return;
        await CopyTextAsync(Result.Text);
        Status = "Copied to clipboard.";
    }

    private void Clear()
    {
        Result = null;
        Segments.Clear();
        SelectedFile = string.Empty;
        Status = "Ready -- pick an audio file or record.";
    }

    private void RefreshCanExecute()
    {
        (TranscribeCommand as RelayCommand)?.RaiseCanExecuteChanged();
        (CopyCommand as RelayCommand)?.RaiseCanExecuteChanged();
    }

    private bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        return true;
    }

    private void OnPropertyChanged(string? name) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    /// <summary>Test/Designer stub so the VM never hard-requires spoke.dll.</summary>
    private sealed class FakeEngine : ISpokeEngine
    {
        public string EngineVersion => "0.1.0-stub";
        public string TranscribeFile(string wavPath) => string.Empty;
        public TranscriptionResult TranscribeFileJson(string wavPath) =>
            new(string.Empty, Array.Empty<Segment>(), 0, "spoke-tiny.en");
        public TranscriptionResult TranscribePcm(float[] pcm16kMono) =>
            new(string.Empty, Array.Empty<Segment>(), 0, "spoke-tiny.en");
    }
}

/// <summary>
/// Minimal ICommand implementation (no MAUI dependency) so the ViewModel
/// compiles and unit-tests both inside the MAUI app and in plain .NET.
/// XAML binds to any ICommand, so this works unchanged on all platforms.
/// </summary>
public sealed class RelayCommand : System.Windows.Input.ICommand
{
    private readonly Func<Task>? _async;
    private readonly Action? _sync;
    private readonly Func<bool>? _canExecute;

    public RelayCommand(Func<Task> async, Func<bool>? canExecute = null)
    {
        _async = async;
        _canExecute = canExecute;
    }

    public RelayCommand(Action sync, Func<bool>? canExecute = null)
    {
        _sync = sync;
        _canExecute = canExecute;
    }

    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) => _canExecute?.Invoke() ?? true;

    public async void Execute(object? parameter)
    {
        if (_async is not null) await _async();
        else _sync?.Invoke();
    }

    public void RaiseCanExecuteChanged() =>
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
