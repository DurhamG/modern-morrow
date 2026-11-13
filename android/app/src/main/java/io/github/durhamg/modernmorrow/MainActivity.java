package io.github.durhamg.modernmorrow;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.IOException;

/**
 * Gets the files ready, then starts the game. If the Morrowind data can't be found, says where to put it instead.
 *
 * The intent's extras pass on to GameActivity, so the automation harness can start the game with options:
 *
 *   adb shell am start -n io.github.durhamg.modernmorrow.debug/io.github.durhamg.modernmorrow.MainActivity \
 *       --esa args --automation-port,4242
 */
public class MainActivity extends Activity {
    @Override
    protected void onResume() {
        super.onResume();

        boolean ready;
        try {
            ready = Files.prepare(this);
        } catch (IOException e) {
            showProblem("Couldn't set up the game's files: " + e.getMessage(), false);
            return;
        }

        if (ready) {
            Intent game = new Intent(this, GameActivity.class);
            Bundle extras = getIntent().getExtras();
            if (extras != null)
                game.putExtras(extras);
            startActivity(game);
            finish();
            return;
        }

        // The app's own folder needs no permission, so it comes first. /sdcard/Morrowind stays when the app is
        // uninstalled and can be shared, but only access to all files lets the app read it, which some players won't
        // want to give.
        String appFolder = "Android/data/" + getPackageName() + "/files/Morrowind";
        String text = "Modern Morrow needs the files of your copy of Morrowind.\n\n"
            + "Copy the \"Data Files\" folder from Morrowind's install folder on a computer into this folder on "
            + "the device, over USB:\n\n    " + appFolder + "\n\n"
            + "Data Files/Morrowind.esm should end up in it. That needs no permission, but uninstalling the app "
            + "deletes the folder.\n\n"
            + "On Windows, Steam installs Morrowind in C:\\Program Files (x86)\\Steam\\steamapps\\common\\Morrowind.";
        if (!Environment.isExternalStorageManager()) {
            text += "\n\nOr put Data Files in a Morrowind folder at the top of the device's internal storage, which "
                + "stays when the app is uninstalled. The app can only read it there with access to all files.";
        }
        showProblem(text, true);
    }

    private void showProblem(String problem, boolean missingData) {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(64, 64, 64, 64);

        TextView text = new TextView(this);
        text.setText(problem);
        text.setTextSize(18);
        layout.addView(text);

        if (missingData) {
            // After copying the files over USB, this looks again without leaving the app.
            Button check = new Button(this);
            check.setText("Check again");
            check.setOnClickListener(v -> recreate());
            layout.addView(check);
            check.requestFocus();

            if (!Environment.isExternalStorageManager()) {
                Button allow = new Button(this);
                allow.setText("Allow access to all files");
                allow.setOnClickListener(v -> startActivity(new Intent(
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName()))));
                layout.addView(allow);
            }
        }

        // The text is long for a phone held sideways, so it scrolls.
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(layout);
        setContentView(scroll);
    }
}
