package io.github.durhamg.modernmorrow;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Environment;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * The folders the engine uses, and the files the app puts in them before it starts.
 *
 * - The global folder is the app's private data folder. It holds the engine's resources, the global openmw.cfg,
 *   defaults.bin and settings.cfg, all copied out of the APK's assets whenever the APK changes.
 * - The user folder is the app's folder on shared storage, /sdcard/Android/data/io.github.durhamg.modernmorrow/files.
 *   It holds settings, saves and logs, and players can reach it over USB.
 *
 * The Morrowind data is looked for in the user folder's Morrowind folder, which needs no permission, then in
 * /sdcard/Morrowind, which needs "All files access". Either holds Data Files, as a Morrowind install does. The user
 * folder's Morrowind folder is made empty at the start, so that it's there to copy into over USB. DataImport copies
 * into it too, from a folder or zip file the player picks.
 */
final class Files {
    private Files() {}

    static File globalDir(Context context) {
        return new File(context.getApplicationInfo().dataDir);
    }

    static File userDir(Context context) {
        return context.getExternalFilesDir(null);
    }

    // Matches AndroidPath::getGlobalConfigPath and getUserConfigPath.
    static File globalConfigDir(Context context) {
        return new File(new File(globalDir(context), "files"), "config");
    }

    static File userConfigDir(Context context) {
        return new File(userDir(context), "config");
    }

    /** The Morrowind folder in the app's own folder, which needs no permission. */
    static File appMorrowindDir(Context context) {
        return new File(userDir(context), "Morrowind");
    }

    /** The Morrowind folder at the top of the device's storage, which needs access to all files. */
    static File sharedMorrowindDir() {
        return new File(Environment.getExternalStorageDirectory(), "Morrowind");
    }

    /** Returns true when the game can start, or false when the Morrowind data hasn't been found. */
    static boolean prepare(Context context) throws IOException {
        copyAssetsIfChanged(context);
        appMorrowindDir(context).mkdirs();

        File dataFiles = findDataFiles(context);
        if (dataFiles == null)
            return false;
        GameConfig.writeUserConfig(userConfigDir(context), dataFiles);
        return true;
    }

    private static File findDataFiles(Context context) {
        List<File> installs = new ArrayList<>();
        installs.add(appMorrowindDir(context));
        if (Environment.isExternalStorageManager())
            installs.add(sharedMorrowindDir());
        for (File install : installs) {
            File dataFiles = new File(install, "Data Files");
            if (new File(dataFiles, "Morrowind.esm").isFile())
                return dataFiles;
        }
        return null;
    }

    private static void copyAssetsIfChanged(Context context) throws IOException {
        AssetManager assets = context.getAssets();
        String version = readAsset(assets, "version");
        File global = new File(globalDir(context), "files");
        File stamp = new File(global, "assets-version");
        if (stamp.isFile() && version.equals(GameConfig.readFile(stamp)))
            return;

        File resources = new File(global, "resources");
        deleteRecursively(resources);
        copyAssetTree(assets, "resources", resources);

        File config = globalConfigDir(context);
        deleteRecursively(config);
        copyAssetTree(assets, "config", config);
        GameConfig.fixGlobalConfig(new File(config, "openmw.cfg"), resources);

        try (OutputStream out = new FileOutputStream(stamp)) {
            out.write(version.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void copyAssetTree(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            File parent = target.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs())
                throw new IOException("can't create " + parent);
            try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(target)) {
                byte[] buffer = new byte[1 << 16];
                int count;
                while ((count = in.read(buffer)) > 0)
                    out.write(buffer, 0, count);
            }
            return;
        }
        if (!target.isDirectory() && !target.mkdirs())
            throw new IOException("can't create " + target);
        for (String child : children)
            copyAssetTree(assets, path + "/" + child, new File(target, child));
    }

    private static void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children)
                deleteRecursively(child);
        }
        file.delete();
    }

    private static String readAsset(AssetManager assets, String path) throws IOException {
        try (InputStream in = assets.open(path)) {
            return GameConfig.readStream(in);
        }
    }
}
