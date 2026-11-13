package io.github.durhamg.modernmorrow;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

/**
 * Runs the engine. SDL's activity loads the libraries, makes the window and calls SDL_main in libopenmw.so on a
 * thread of its own. MainActivity has set up the files by the time this starts.
 */
public class GameActivity extends SDLActivity {
    // components/files/androidpath.cpp
    private static native void getPathToJni(String global, String user);

    @Override
    protected String[] getLibraries() {
        // libopenmw.so last: SDL calls SDL_main in the last library.
        return new String[] { "c++_shared", "SDL2", "openal", "ng_gl4es", "openmw" };
    }

    @Override
    protected String[] getArguments() {
        String[] args = getIntent().getStringArrayExtra("args");
        return args != null ? args : new String[0];
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mBrokenLibraries)
            return;

        getPathToJni(Files.globalDir(this).getAbsolutePath(), Files.userDir(this).getAbsolutePath());

        // SDL reads its hints from the environment too. Some controllers send B as Android's Back key as well, which
        // would otherwise close the app.
        nativeSetenv("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
        // Nintendo controllers' face buttons go by position, as on an Xbox controller, rather than by their labels, so
        // the bottom button is A for a Switch Pro Controller too. SDL goes by the labels by default.
        nativeSetenv("SDL_GAMECONTROLLER_USE_BUTTON_LABELS", "0");
        // SDL_HINT_ORIENTATIONS, which has this name on Android too.
        nativeSetenv("SDL_IOS_ORIENTATIONS", "LandscapeLeft LandscapeRight");
        // OpenSceneGraph would draw on a thread of its own. When the app goes to the background, Android takes the
        // window's surface away, and that thread deadlocks with SDL's, so coming back showed a black screen while the
        // music played. Drawing on SDL's thread avoids it, and was faster too in the emulator.
        nativeSetenv("OSG_THREADING", "SingleThreaded");
    }
}
