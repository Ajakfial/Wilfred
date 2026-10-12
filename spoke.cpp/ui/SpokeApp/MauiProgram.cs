using Microsoft.Extensions.Logging;
using SpokeApp.Services;
using SpokeApp.ViewModels;

namespace SpokeApp;

public static class MauiProgram
{
    public static MauiApp CreateMauiApp()
    {
        var builder = MauiApp.CreateBuilder();
        builder
            .UseMauiApp<App>()
            .ConfigureFonts(fonts =>
            {
                fonts.AddFont("OpenSans-Regular.ttf", "OpenSansRegular");
                fonts.AddFont("OpenSans-Semibold.ttf", "OpenSansSemibold");
            });

        // Engine services: thin wrappers over the C++ spoke.dll (see SpokeNativeService).
        builder.Services.AddSingleton<SpokeNativeService>(_ =>
            new SpokeNativeService(threads: 0, beamWidth: 8));
        builder.Services.AddSingleton<TranscriptionService>();
        builder.Services.AddTransient<MainViewModel>();
        builder.Services.AddTransient<MainPage>();

#if DEBUG
        builder.Logging.AddDebug();
#endif
        return builder.Build();
    }
}
