// The native libraries and the assets come from the Docker build: run `android/build.sh openmw` first.
// See ui-rework/android.md.
plugins {
    id("com.android.application")
}

android {
    namespace = "io.github.durhamg.modernmorrow"
    compileSdk = 35

    defaultConfig {
        applicationId = "io.github.durhamg.modernmorrow"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1"
        ndk {
            abiFilters += "arm64-v8a"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    packaging {
        jniLibs {
            // Installs the libraries as files, which gl4es and OpenMW's library loading expect.
            useLegacyPackaging = true
            // Already stripped in the Docker build.
            keepDebugSymbols += "**/*.so"
        }
    }
}
