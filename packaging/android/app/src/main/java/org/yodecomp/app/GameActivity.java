package org.yodecomp.app;

import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * Yodecomp Android entry activity.
 *
 * SDL3 (and SDL3_mixer) are static-linked into libmain.so — the whole engine is one shared
 * object — so we override getLibraries() to load ONLY "main" (the default would try to
 * System.loadLibrary("SDL3") first, which does not exist here). SDLActivity resolves the
 * "SDL_main" symbol out of libmain.so and runs it on the SDL thread; that IS the game's
 * main() (renamed by <SDL3/SDL_main.h>, force-included in cmake/Android.cmake).
 */
public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // ⭐ Lay the content out EDGE-TO-EDGE (ignore the system-bar insets) BEFORE SDL's
        // SurfaceView is first measured. Otherwise the surface is sized to the content area (full
        // screen MINUS the navigation bar) and SDL keeps rendering at that height even after the
        // bars are hidden — the letterbox is then computed against the short surface, leaving a
        // navbar-sized dead strip at the bottom ("still cropping as if the navbar is there").
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        hideSystemUI();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    // True immersive fullscreen: hide the status + navigation bars (they slide back on a swipe, then
    // auto-hide). Reasserted on every focus gain so a dialog / keyboard / the OS "viewing full
    // screen" toast can't leave a bar showing that overlaps the game's top menu-bar chrome.
    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController c = getWindow().getInsetsController();   // non-null once attached
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            hideSystemUILegacy();
        }
    }

    @SuppressWarnings("deprecation")
    private void hideSystemUILegacy() {
        getWindow().getDecorView().setSystemUiVisibility(
              View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
}
