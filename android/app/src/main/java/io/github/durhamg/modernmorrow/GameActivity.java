package io.github.durhamg.modernmorrow;

import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.View;

import java.util.Arrays;

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

        excludeEdgeGestures();
    }

    /**
     * Swiping in from a side edge is Android's back gesture, but the touch controls use swipes in from the middle half
     * of each side edge (apps/openmw/mwinput/touchmanager.cpp), so that half is kept from Android. Android normally
     * lets an app keep only 200 dp of each edge, but SDL's fullscreen window is immersive sticky, which lifts that
     * limit. The ends of the edges still go back.
     */
    private void excludeEdgeGestures() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q)
            return;
        mLayout.addOnLayoutChangeListener((View view, int left, int top, int right, int bottom, int oldLeft,
                int oldTop, int oldRight, int oldBottom) -> {
            float density = getResources().getDisplayMetrics().density;
            int edge = (int) (48 * density);
            int width = right - left;
            int screenBottom = bottom - top;
            view.setSystemGestureExclusionRects(Arrays.asList(
                    new Rect(0, screenBottom / 4, edge, screenBottom * 3 / 4),
                    new Rect(width - edge, screenBottom / 4, width, screenBottom * 3 / 4)));
        });
    }
}
