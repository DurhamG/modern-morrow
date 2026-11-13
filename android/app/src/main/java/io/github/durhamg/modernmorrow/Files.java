package io.github.durhamg.modernmorrow;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Environment;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * The folders the engine uses, and the files the app puts in them before it starts.
 *
 * - The global folder is the app's private data folder. It holds the engine's resources, the global openmw.cfg
 *   and defaults.bin, all copied out of the APK's assets whenever the APK changes.
 * - The user folder is the app's folder on shared storage, /sdcard/Android/data/io.github.durhamg.modernmorrow/files.
 *   It holds settings, saves and logs, and players can reach it over USB.
 *
 * The Morrowind data is looked for in the user folder's Morrowind folder, which needs no permission, then in
 * /sdcard/Morrowind, which needs "All files access". Either holds Data Files, as a Morrowind install does.
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

    /** Returns null when the game can start, or else what the player has to do first. */
    static String prepare(Context context) throws IOException {
        copyAssetsIfChanged(context);

        File dataFiles = findDataFiles(context);
        if (dataFiles == null) {
            return "Modern Morrow needs the files of a Morrowind install. Copy the folder that holds \"Data Files\" "
                + "to " + new File(userDir(context), "Morrowind") + " or to /sdcard/Morrowind.\n\n"
                + "/sdcard/Morrowind needs access to all files.";
        }
        writeUserConfig(context, dataFiles);
        return null;
    }

    private static File findDataFiles(Context context) {
        List<File> installs = new ArrayList<>();
        installs.add(new File(userDir(context), "Morrowind"));
        if (Environment.isExternalStorageManager())
            installs.add(new File(Environment.getExternalStorageDirectory(), "Morrowind"));
        for (File install : installs) {
            File dataFiles = new File(install, "Data Files");
            if (new File(dataFiles, "Morrowind.esm").isFile())
                return dataFiles;
        }
        return null;
    }

    /**
     * The user openmw.cfg lists the data folder and the game files in it. It is written again on each start, so it
     * follows the data if the player moves it. Settings live in settings.cfg, which this leaves alone.
     */
    private static void writeUserConfig(Context context, File dataFiles) throws IOException {
        File dir = userConfigDir(context);
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("can't create " + dir);
        try (PrintWriter cfg = new PrintWriter(new File(dir, "openmw.cfg"), "UTF-8")) {
            cfg.println("# Written by the app on each start. Changes here are lost.");
            cfg.println("data=\"" + dataFiles.getAbsolutePath() + "\"");
            for (String name : new String[] { "Morrowind", "Tribunal", "Bloodmoon" }) {
                if (new File(dataFiles, name + ".esm").isFile())
                    cfg.println("content=" + name + ".esm");
                if (new File(dataFiles, name + ".bsa").isFile())
                    cfg.println("fallback-archive=" + name + ".bsa");
            }
        }
    }

    private static void copyAssetsIfChanged(Context context) throws IOException {
        AssetManager assets = context.getAssets();
        String version = readAsset(assets, "version");
        File global = new File(globalDir(context), "files");
        File stamp = new File(global, "assets-version");
        if (stamp.isFile() && version.equals(readFile(stamp)))
            return;

        File resources = new File(global, "resources");
        deleteRecursively(resources);
        copyAssetTree(assets, "resources", resources);

        File config = globalConfigDir(context);
        deleteRecursively(config);
        copyAssetTree(assets, "config", config);
        fixGlobalConfig(new File(config, "openmw.cfg"), resources);

        try (OutputStream out = new FileOutputStream(stamp)) {
            out.write(version.getBytes(StandardCharsets.UTF_8));
        }
    }

    /** The build's openmw.cfg points at ./resources. The engine needs the folder the app copied them to. */
    private static void fixGlobalConfig(File cfg, File resources) throws IOException {
        StringBuilder text = new StringBuilder();
        for (String line : readFile(cfg).split("\n", -1)) {
            if (line.startsWith("resources="))
                line = "resources=\"" + resources.getAbsolutePath() + "\"";
            else if (line.startsWith("data=./resources/"))
                line = "data=\"" + resources.getAbsolutePath() + line.substring("data=./resources".length()) + "\"";
            text.append(line).append('\n');
        }
        try (OutputStream out = new FileOutputStream(cfg)) {
            out.write(text.toString().getBytes(StandardCharsets.UTF_8));
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
            return readStream(in);
        }
    }

    private static String readFile(File file) throws IOException {
        try (InputStream in = new java.io.FileInputStream(file)) {
            return readStream(in);
        }
    }

    private static String readStream(InputStream in) throws IOException {
        StringBuilder text = new StringBuilder();
        BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8));
        String line;
        while ((line = reader.readLine()) != null)
            text.append(line).append('\n');
        return text.toString();
    }
}
