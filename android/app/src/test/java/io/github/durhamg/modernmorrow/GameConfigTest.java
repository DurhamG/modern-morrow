package io.github.durhamg.modernmorrow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class GameConfigTest {
    @Rule
    public TemporaryFolder mTemp = new TemporaryFolder();

    private static void write(File file, String text) throws IOException {
        file.getParentFile().mkdirs();
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(text.getBytes(StandardCharsets.UTF_8));
        }
    }

    private static String read(File file) throws IOException {
        return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
    }

    /** A Data Files folder that holds these files. */
    private File dataFiles(String... names) throws IOException {
        File dir = new File(mTemp.newFolder("Morrowind"), "Data Files");
        dir.mkdirs();
        for (String name : names)
            write(new File(dir, name), name);
        return dir;
    }

    /** The user openmw.cfg's lines after the comment and the data folder. */
    private static List<String> gameFiles(File configDir) throws IOException {
        List<String> lines = new ArrayList<>(Files.readAllLines(new File(configDir, "openmw.cfg").toPath()));
        return lines.subList(2, lines.size());
    }

    @Test
    public void userConfigShouldNameTheDataFolderInQuotes() throws IOException {
        File data = dataFiles("Morrowind.esm");
        File config = mTemp.newFolder("config");
        GameConfig.writeUserConfig(config, data);
        List<String> lines = Files.readAllLines(new File(config, "openmw.cfg").toPath());
        assertTrue(lines.get(0).startsWith("#"));
        assertEquals("data=\"" + data.getAbsolutePath() + "\"", lines.get(1));
        assertTrue(lines.get(1).contains("Data Files"));
    }

    @Test
    public void userConfigShouldListOnlyTheGameFilesThatExist() throws IOException {
        File config = mTemp.newFolder("config");
        GameConfig.writeUserConfig(config, dataFiles("Morrowind.esm", "Morrowind.bsa", "Tribunal.esm"));
        assertEquals(Arrays.asList("content=Morrowind.esm", "fallback-archive=Morrowind.bsa", "content=Tribunal.esm"),
            gameFiles(config));
    }

    @Test
    public void userConfigShouldListGameFilesInMorrowindTribunalBloodmoonOrder() throws IOException {
        File config = mTemp.newFolder("config");
        GameConfig.writeUserConfig(config, dataFiles("Bloodmoon.bsa", "Bloodmoon.esm", "Tribunal.bsa", "Tribunal.esm",
            "Morrowind.bsa", "Morrowind.esm"));
        assertEquals(Arrays.asList("content=Morrowind.esm", "fallback-archive=Morrowind.bsa", "content=Tribunal.esm",
                         "fallback-archive=Tribunal.bsa", "content=Bloodmoon.esm", "fallback-archive=Bloodmoon.bsa"),
            gameFiles(config));
    }

    @Test
    public void userConfigShouldBeWrittenAgainOverAnOldOne() throws IOException {
        File config = mTemp.newFolder("config");
        write(new File(config, "openmw.cfg"), "data=\"/somewhere/else\"\ncontent=Old.esm\ncontent=Older.esm\n"
            + "content=Oldest.esm\ncontent=More.esm\ncontent=Still more.esm\ncontent=Last.esm\n");
        File data = dataFiles("Morrowind.esm");
        GameConfig.writeUserConfig(config, data);
        String text = read(new File(config, "openmw.cfg"));
        assertTrue(text, text.contains("data=\"" + data.getAbsolutePath() + "\""));
        assertEquals(Arrays.asList("content=Morrowind.esm"), gameFiles(config));
    }

    @Test
    public void userConfigShouldCreateItsFolder() throws IOException {
        File config = new File(new File(mTemp.getRoot(), "files"), "config");
        GameConfig.writeUserConfig(config, dataFiles("Morrowind.esm"));
        assertTrue(new File(config, "openmw.cfg").isFile());
    }

    @Test
    public void globalConfigShouldPointResourcesAtTheCopiedFolder() throws IOException {
        File cfg = new File(mTemp.getRoot(), "openmw.cfg");
        write(cfg, "resources=./resources\n");
        File resources = mTemp.newFolder("files", "resources");
        GameConfig.fixGlobalConfig(cfg, resources);
        assertEquals("resources=\"" + resources.getAbsolutePath() + "\"\n", read(cfg));
    }

    @Test
    public void globalConfigShouldPointDataFoldersUnderResourcesAtTheCopiedFolder() throws IOException {
        File cfg = new File(mTemp.getRoot(), "openmw.cfg");
        write(cfg, "data=./resources/vfs\ndata=./resources/vfs-mw\n");
        File resources = mTemp.newFolder("files", "resources");
        GameConfig.fixGlobalConfig(cfg, resources);
        String path = resources.getAbsolutePath();
        assertEquals("data=\"" + path + "/vfs\"\ndata=\"" + path + "/vfs-mw\"\n", read(cfg));
    }

    @Test
    public void globalConfigShouldKeepOtherLines() throws IOException {
        // The start of the build's openmw.cfg, and lines that android/morrowind-ini.cfg adds to it.
        String before = "# This is the local openmw.cfg file. Do not modify!\n"
            + "\n"
            + "data-local=\"?userdata?data\"\n"
            + "user-data=\"?userdata?\"\n"
            + "config=\"?userconfig?\"\n"
            + "resources=./resources\n"
            + "data=./resources/vfs-mw\n"
            + "data=./elsewhere/resources/vfs\n"
            + "\n"
            + "# lighting\n"
            + "fallback=LightAttenuation_UseConstant,0\n"
            + "fallback=Weather_Ashstorm_Cloud_Texture,Tx_Sky_Ashstorm.tga\n"
            + "fallback=Weather_Ashstorm_Transition_Delta,.035\n"
            + "fallback=General_Werewolf_FOV,100\n";
        File cfg = new File(mTemp.getRoot(), "openmw.cfg");
        write(cfg, before);
        File resources = mTemp.newFolder("files", "resources");
        GameConfig.fixGlobalConfig(cfg, resources);

        String path = resources.getAbsolutePath();
        String after = before.replace("resources=./resources\n", "resources=\"" + path + "\"\n")
                           .replace("data=./resources/vfs-mw\n", "data=\"" + path + "/vfs-mw\"\n");
        assertEquals(after, read(cfg));
    }
}
