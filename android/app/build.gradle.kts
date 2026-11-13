// The native libraries and the assets come from the Docker build: run `android/build.sh openmw` first.
// See ui-rework/android.md.
plugins {
    id("com.android.application")
}

// CI passes unset values as empty strings.
fun env(name: String): String? = System.getenv(name)?.takeIf { it.isNotEmpty() }

android {
    namespace = "io.github.durhamg.modernmorrow"
    compileSdk = 35

    defaultConfig {
        applicationId = "io.github.durhamg.modernmorrow"
        minSdk = 26
        targetSdk = 35
        // CI sets these from the run number and the release tag.
        versionCode = env("MM_VERSION_CODE")?.toInt() ?: 1
        versionName = env("MM_VERSION_NAME") ?: "0.1"
        manifestPlaceholders["appName"] = "Modern Morrow"
        ndk {
            abiFilters += "arm64-v8a"
        }
    }

    // The release key stays out of the repository. android/build.sh release checks that these are set, and
    // ui-rework/releasing.md says where the key is kept. CI also signs master's debug builds with it, so testers can
    // update them in place.
    val releaseKeystore = env("ANDROID_KEYSTORE")
    signingConfigs {
        if (releaseKeystore != null) {
            create("release") {
                storeFile = file(releaseKeystore)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                // A PKCS12 keystore, as keytool makes by default, uses the same password for the key.
                keyPassword = System.getenv("ANDROID_KEYSTORE_PASSWORD")
            }
        }
    }

    buildTypes {
        // A debug build is an app of its own, so it installs beside the released app rather than over it, with its
        // own settings and saves.
        debug {
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-debug"
            manifestPlaceholders["appName"] = "Modern Morrow Debug"
            if (releaseKeystore != null) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
        release {
            isMinifyEnabled = false
            if (releaseKeystore != null) {
                signingConfig = signingConfigs.getByName("release")
            }
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
