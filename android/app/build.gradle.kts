plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.wilfred.launcher"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.wilfred.launcher"
        minSdk = 26
        targetSdk = 34
        // Release tags (vX.Y.Z) override these via
        // -PversionName / -PversionCode or WILFRED_VERSION_NAME /
        // WILFRED_VERSION_CODE (see .github/workflows/release.yml).
        // Defaults keep local/fork builds working without any env.
        versionCode = (findProperty("versionCode") as String?)
            ?.toIntOrNull()
            ?: System.getenv("WILFRED_VERSION_CODE")?.toIntOrNull()
            ?: 1
        versionName = (findProperty("versionName") as String?)
            ?: System.getenv("WILFRED_VERSION_NAME")
            ?: "1.0.0"

        ndk {
            // arm64 covers modern phones; x86_64 covers the emulator.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                // Build only the JNI lib (which links wilfred_core).
                targets += listOf("wilfred_jni")
                arguments += listOf(
                    "-DWILFRED_BUILD_TESTS=OFF",
                    "-DWILFRED_BUILD_BENCH=OFF",
                    "-DCMAKE_BUILD_TYPE=Release"
                )
            }
        }
    }

    // Release signing comes from env (CI decodes ANDROID_KEYSTORE_BASE64
    // into ANDROID_KEYSTORE_PATH). When absent the release APK/AAB stays
    // unsigned so forks still build; release.yml logs a warning.
    signingConfigs {
        create("release") {
            val storeFilePath = System.getenv("ANDROID_KEYSTORE_PATH")
            if (!storeFilePath.isNullOrBlank()) {
                storeFile = file(storeFilePath)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS") ?: "wilfred"
                keyPassword = System.getenv("ANDROID_KEY_PASSWORD")
                    ?: System.getenv("ANDROID_KEYSTORE_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            // Only sign when a keystore was provided; otherwise Gradle
            // produces app-release-unsigned.apk and the workflow warns.
            if (System.getenv("ANDROID_KEYSTORE_PATH")?.isNotBlank() == true) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
        debug {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            // Relative to android/app: ../../CMakeLists.txt is the repo root,
            // which defines wilfred_core + wilfred_jni when ANDROID is set.
            path = file("../../CMakeLists.txt")
            version = "3.22.1+"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.3")
}
