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
import android.widget.TextView;

import java.io.IOException;

/**
 * Gets the files ready, then starts the game. If the Morrowind data can't be found, says where to put it instead.
 *
 * The intent's extras pass on to GameActivity, so the automation harness can start the game with options:
 *
 *   adb shell am start -n io.github.durhamg.modernmorrow/.MainActivity --esa args --automation-port,4242
 */
public class MainActivity extends Activity {
    @Override
    protected void onResume() {
        super.onResume();

        String problem;
        try {
            problem = Files.prepare(this);
        } catch (IOException e) {
            problem = "Couldn't set up the game's files: " + e.getMessage();
        }

        if (problem == null) {
            Intent game = new Intent(this, GameActivity.class);
            Bundle extras = getIntent().getExtras();
            if (extras != null)
                game.putExtras(extras);
            startActivity(game);
            finish();
            return;
        }

        showProblem(problem);
    }

    private void showProblem(String problem) {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(64, 64, 64, 64);

        TextView text = new TextView(this);
        text.setText(problem);
        text.setTextSize(20);
        layout.addView(text);

        if (!Environment.isExternalStorageManager()) {
            Button allow = new Button(this);
            allow.setText("Allow access to all files");
            allow.setOnClickListener(v -> startActivity(new Intent(
                Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName()))));
            layout.addView(allow);
            allow.requestFocus();
        }

        setContentView(layout);
    }
}
