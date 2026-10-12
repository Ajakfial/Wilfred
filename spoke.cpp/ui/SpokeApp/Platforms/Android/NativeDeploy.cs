// Android stub: place per-ABI builds at
// Platforms/Android/jniLibs/arm64-v8a/libspoke.so (and armeabi-v7a if needed).
// Build them from this repo with an NDK clang toolchain:
//   cmake -S . -B build-android -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
//     -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
//   cmake --build build-android --config Release
namespace SpokeApp.Platforms.Android;

public static class NativeDeploy
{
    public const string LibraryName = "libspoke.so";
}
