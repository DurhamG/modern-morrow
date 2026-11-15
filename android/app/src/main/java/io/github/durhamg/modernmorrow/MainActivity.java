package io.github.durhamg.modernmorrow;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.IOException;

/**
 * Gets the files ready, then starts the game. If the Morrowind data can't be found, offers the ways to provide it
 * instead, and shows the copy while DataImport makes one.
 *
 * The intent's extras pass on to GameActivity, so the automation harness can start the game with options:
 *
 *   adb shell am start -n io.github.durhamg.modernmorrow.debug/io.github.durhamg.modernmorrow.MainActivity \
 *       --esa args --automation-port,4242
 */
public class MainActivity extends Activity {
    private static final int sPickFolder = 1;
    private static final int sPickZip = 2;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private String mProblem;

    @Override
    protected void onResume() {
        super.onResume();
        mHandler.removeCallbacksAndMessages(null);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        DataImport copy = DataImport.current();
        if (copy != null) {
            if (!copy.isFinished()) {
                showCopy(copy);
                return;
            }
            DataImport.forget();
            if (copy.error() != null)
                mProblem = "Couldn't copy the files. " + copy.error();
        }

        boolean ready;
        try {
            ready = Files.prepare(this);
        } catch (IOException e) {
            showText("Couldn't set up the game's files: " + e.getMessage());
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
        showOptions();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mHandler.removeCallbacksAndMessages(null);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null)
            return;
        mProblem = null;
        try {
            DataImport.start(this, data.getData(), requestCode == sPickZip);
        } catch (SecurityException e) {
            mProblem = "Couldn't read what was picked. " + e.getMessage();
        }
        // onResume comes next, and shows the copy.
    }

    /**
     * The three ways to give the game Morrowind's data, each with its own buttons. The first two copy the data into the
     * app's own folder, which needs no permission. The last reads it in place, which needs access to all files, but
     * keeps it when the app is uninstalled.
     */
    private void showOptions() {
        LinearLayout layout = page();

        addText(layout, "Modern Morrow needs the Data Files folder from your copy of Morrowind.", 20, true);
        if (mProblem != null) {
            TextView problem = addText(layout, mProblem, 18, false);
            problem.setTextColor(Color.rgb(0xff, 0x8a, 0x65));
        }

        addHeading(layout, "1. From this device");
        addText(layout, "Pick the Data Files folder, the folder that holds it, or a zip file of either. Modern Morrow "
            + "copies the files in.", 18, false);
        LinearLayout pick = addButtons(layout);
        Button folder = addButton(pick, "Choose folder", v -> pick(Intent.ACTION_OPEN_DOCUMENT_TREE, sPickFolder));
        addButton(pick, "Choose zip file", v -> pick(Intent.ACTION_OPEN_DOCUMENT, sPickZip));

        addHeading(layout, "2. From a computer");
        String appFolder = "Android/data/" + getPackageName() + "/files/Morrowind";
        addText(layout, "Data Files is in the folder Morrowind is installed in:\n"
            + "    Steam: C:\\Program Files (x86)\\Steam\\steamapps\\common\\Morrowind\n"
            + "    GOG Galaxy: C:\\Program Files (x86)\\GOG Galaxy\\Games\\Morrowind\n"
            + "Connect the device over USB, and copy Data Files into this folder:\n    " + appFolder, 18, false);
        addButton(addButtons(layout), "Check again", v -> recreate());

        addHeading(layout, "3. From the top of the device's storage");
        if (Environment.isExternalStorageManager()) {
            addText(layout, "Put Data Files in a Morrowind folder at the top of the device's storage.", 18, false);
            addButton(addButtons(layout), "Check again", v -> recreate());
        } else {
            addText(layout, "Put Data Files in a Morrowind folder at the top of the device's storage, and allow "
                + "access to all files. The files there stay when the app is uninstalled.", 18, false);
            addButton(addButtons(layout), "Allow access to all files", v -> startActivity(new Intent(
                Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName()))));
        }

        setPage(layout);
        folder.requestFocus();
    }

    private void pick(String action, int request) {
        Intent pick = new Intent(action);
        if (request == sPickZip) {
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("application/zip");
        }
        // Downloads, where a phone keeps what it downloads.
        pick.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
            DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:Download"));
        // DataImport keeps the grant for as long as the copy takes.
        pick.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        try {
            startActivityForResult(pick, request);
        } catch (ActivityNotFoundException e) {
            mProblem = "This device has no way to pick files.";
            showOptions();
        }
    }

    /** Shows how far the copy has got, until it's done, and then looks for the data again. */
    private void showCopy(DataImport copy) {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        LinearLayout layout = page();
        addText(layout, copy.isZip() ? "Unpacking the zip file…" : "Copying the files…", 20, true);
        ProgressBar bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(1000);
        layout.addView(bar, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT));
        TextView amount = addText(layout, "", 18, false);
        Button cancel = addButton(addButtons(layout), "Cancel", v -> copy.cancel());
        setPage(layout);
        cancel.requestFocus();

        Runnable update = new Runnable() {
            @Override
            public void run() {
                if (copy.isFinished()) {
                    recreate();
                    return;
                }
                long total = copy.total();
                bar.setIndeterminate(total == 0);
                if (total > 0) {
                    bar.setProgress((int) (copy.done() * 1000 / total));
                    amount.setText(DataFiles.size(copy.done()) + " of " + DataFiles.size(total));
                }
                mHandler.postDelayed(this, 200);
            }
        };
        update.run();
    }

    private void showText(String text) {
        LinearLayout layout = page();
        addText(layout, text, 18, false);
        setPage(layout);
    }

    private LinearLayout page() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(64, 48, 64, 48);
        return layout;
    }

    // The options are long for a phone held sideways, so they scroll.
    private void setPage(LinearLayout layout) {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(layout);
        setContentView(scroll);
    }

    private TextView addText(LinearLayout layout, String text, int size, boolean bold) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        if (bold)
            view.setTypeface(Typeface.DEFAULT_BOLD);
        view.setPadding(0, 0, 0, 16);
        layout.addView(view);
        return view;
    }

    private void addHeading(LinearLayout layout, String text) {
        TextView view = addText(layout, text, 20, true);
        view.setPadding(0, 32, 0, 8);
    }

    private LinearLayout addButtons(LinearLayout layout) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        layout.addView(row);
        return row;
    }

    private Button addButton(LinearLayout row, String text, View.OnClickListener listener) {
        Button button = new Button(this);
        button.setText(text);
        button.setOnClickListener(listener);
        row.addView(button);
        return button;
    }
}
