// Top-level build file for the Wilfred Android app.
// The C++ core (wilfred_core) is built from the repo root via the NDK:
// see app/build.gradle.kts -> externalNativeBuild -> ../../CMakeLists.txt.
plugins {
    id("com.android.application") version "8.5.2" apply false
    id("org.jetbrains.kotlin.android") version "1.9.24" apply false
}
