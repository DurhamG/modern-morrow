package io.github.durhamg.modernmorrow;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;

/** The openmw.cfg files that Files writes, with nothing from Android, so that the JVM unit tests can run them. */
final class GameConfig {
    private GameConfig() {}

    /**
     * The user openmw.cfg lists the data folder and the game files in it. It is written again on each start, so it
     * follows the data if the player moves it. Settings live in settings.cfg, which this leaves alone.
     */
    static void writeUserConfig(File configDir, File dataFiles) throws IOException {
        if (!configDir.isDirectory() && !configDir.mkdirs())
            throw new IOException("can't create " + configDir);
        try (PrintWriter cfg = new PrintWriter(new File(configDir, "openmw.cfg"), "UTF-8")) {
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

    /** The build's openmw.cfg points at ./resources. The engine needs the folder the app copied them to. */
    static void fixGlobalConfig(File cfg, File resources) throws IOException {
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

    static String readFile(File file) throws IOException {
        try (InputStream in = new java.io.FileInputStream(file)) {
            return readStream(in);
        }
    }

    static String readStream(InputStream in) throws IOException {
        StringBuilder text = new StringBuilder();
        BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8));
        String line;
        while ((line = reader.readLine()) != null)
            text.append(line).append('\n');
        return text.toString();
    }
}
