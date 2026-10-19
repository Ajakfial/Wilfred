using SpokeApp.Services;
using SpokeApp.ViewModels;

namespace SpokeApp;

public partial class MainPage : ContentPage
{
    private readonly MainViewModel _vm;

    public MainPage(MainViewModel vm)
    {
        InitializeComponent();
        _vm = vm;
        BindingContext = vm;

        vm.PickFileAsync = async filter =>
        {
            var options = new PickOptions { PickerTitle = "Select audio file" };
            var file = await FilePicker.Default.PickAsync(options);
            return file?.FullPath ?? string.Empty;
        };
        vm.CopyTextAsync = async text =>
        {
            await Clipboard.Default.SetTextAsync(text);
        };
    }

    private async void OnBrowseClicked(object? sender, EventArgs e) =>
        await _vm.BrowseAsync();

    private async void OnRecordClicked(object? sender, EventArgs e) =>
        await DisplayAlert("Record",
            "Microphone capture streams 16 kHz PCM into StreamingTranscriber via SpokeNativeService.TranscribePcm. Wire your platform recorder here -- the engine side is ready.",
            "OK");

    private async void OnSettingsClicked(object? sender, EventArgs e) =>
        await DisplayAlert("Settings",
            $"Threads 0=auto (max 8) · Beam 1–16 · VAD −38 dB default. Engine {_vm.EngineLabel}.",
            "OK");

    private async void OnExportClicked(object? sender, EventArgs e)
    {
        if (_vm.Result is null) return;
        string choice = await DisplayActionSheet("Export transcript", "Cancel", null, "Copy text", "Save .txt file");
        if (choice == "Copy text")
        {
            await Clipboard.Default.SetTextAsync(_vm.Result.Text);
        }
        else if (choice == "Save .txt file")
        {
            string path = Path.Combine(FileSystem.CacheDirectory, "spoke_transcript.txt");
            await TranscriptionService.ExportTextAsync(_vm.Result, path);
            await DisplayAlert("Exported", path, "OK");
        }
    }
}
