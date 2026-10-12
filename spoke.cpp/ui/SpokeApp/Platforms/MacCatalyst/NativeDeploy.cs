// macOS (MacCatalyst) stub: bundle libspoke.dylib inside the app.
// In the .csproj for the MacCatalyst target, add the dylib as a bundled resource:
//   <BundleResource Include="..\..\..\build\macos-ninja-release\libspoke.dylib" />
// At runtime SpokeNativeService probes AppContext.BaseDirectory first, then
// @rpath / system paths, so no code changes are needed here.
// NOTE (iOS devices): the App Store forbids dlopen of loose dylibs; ship the
// engine as a static xcframework and DllImport("__Internal") instead.
// That packaging step is tracked as a roadmap item in docs/ui-ux.md.
namespace SpokeApp.Platforms.MacCatalyst;

public static class NativeDeploy
{
    public const string LibraryName = "libspoke.dylib";
}
