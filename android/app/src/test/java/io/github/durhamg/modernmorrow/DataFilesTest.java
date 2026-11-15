package io.github.durhamg.modernmorrow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class DataFilesTest {
    @Rule
    public TemporaryFolder mTemp = new TemporaryFolder();

    /** A zip of these names and contents, in pairs. A name that ends in a slash is a folder, with null contents. */
    private static InputStream zip(String... namesAndContents) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            for (int i = 0; i < namesAndContents.length; i += 2) {
                zip.putNextEntry(new ZipEntry(namesAndContents[i]));
                if (namesAndContents[i + 1] != null)
                    zip.write(namesAndContents[i + 1].getBytes(StandardCharsets.UTF_8));
                zip.closeEntry();
            }
        }
        return new ByteArrayInputStream(bytes.toByteArray());
    }

    private static File write(File file, String text) throws IOException {
        file.getParentFile().mkdirs();
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(text.getBytes(StandardCharsets.UTF_8));
        }
        return file;
    }

    private static String read(File file) throws IOException {
        return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
    }

    /** The names in a folder as the disk has them. On a disk that ignores case, exists() can't tell them apart. */
    private static List<String> names(File dir) {
        String[] names = dir.list();
        Arrays.sort(names);
        return Arrays.asList(names);
    }

    private static File path(File dir, String... parts) {
        File file = dir;
        for (String part : parts)
            file = new File(file, part);
        return file;
    }

    @Test
    public void sizeShouldUseGigabytesFromOneGigabyte() {
        assertEquals("1.0 GB", DataFiles.size(1L << 30));
        assertEquals("512 MB", DataFiles.size(512L << 20));
    }

    @Test
    public void zipEntryShouldLandInsideTheFolder() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertEquals(path(dir, "Data Files", "Morrowind.esm"), DataFiles.zipTarget(dir, "Data Files/Morrowind.esm"));
        assertEquals(path(dir, "Data Files"), DataFiles.zipTarget(dir, "./Data Files/"));
    }

    @Test
    public void zipEntryWithDotDotShouldBeRefused() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertThrows(IOException.class, () -> DataFiles.zipTarget(dir, "../evil.txt"));
        assertThrows(IOException.class, () -> DataFiles.unpack(zip("../evil.txt", "evil"), dir, () -> false));
        assertFalse(new File(mTemp.getRoot(), "evil.txt").exists());
    }

    @Test
    public void zipEntryWithDotDotInTheMiddleShouldBeRefused() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertThrows(IOException.class, () -> DataFiles.zipTarget(dir, "Data Files/../../evil.txt"));
    }

    @Test
    public void zipEntryWithBackslashesShouldBeSplitIntoFolders() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertEquals(path(dir, "Data Files", "Morrowind.esm"), DataFiles.zipTarget(dir, "Data Files\\Morrowind.esm"));
    }

    @Test
    public void zipEntryWithBackslashDotDotShouldBeRefused() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertThrows(IOException.class, () -> DataFiles.zipTarget(dir, "..\\evil.txt"));
    }

    @Test
    public void absoluteZipEntryShouldLandInsideTheFolder() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertEquals(path(dir, "etc", "passwd"), DataFiles.zipTarget(dir, "/etc/passwd"));
    }

    @Test
    public void unpackShouldWriteEveryFileAndFolder() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertTrue(DataFiles.unpack(zip("Morrowind/", null, "Morrowind/Data Files/Morrowind.esm", "esm",
            "Morrowind/Data Files/Meshes/a.nif", "nif", "Morrowind/Empty/", null), dir, () -> false));
        assertEquals("esm", read(path(dir, "Morrowind", "Data Files", "Morrowind.esm")));
        assertEquals("nif", read(path(dir, "Morrowind", "Data Files", "Meshes", "a.nif")));
        assertTrue(path(dir, "Morrowind", "Empty").isDirectory());
    }

    @Test
    public void unpackOfAnEmptyZipShouldSayThereWasNothing() throws IOException {
        // Java won't write a zip with no entries, so this is the end record that is all an empty zip has.
        byte[] empty = new byte[22];
        empty[0] = 'P';
        empty[1] = 'K';
        empty[2] = 5;
        empty[3] = 6;
        File dir = mTemp.newFolder("unpack");
        assertFalse(DataFiles.unpack(new ByteArrayInputStream(empty), dir, () -> false));
        assertEquals(0, dir.list().length);
    }

    @Test
    public void unpackOfSomethingThatIsNotAZipShouldSayThereWasNothing() throws IOException {
        File dir = mTemp.newFolder("unpack");
        InputStream text = new ByteArrayInputStream("Not a zip file.".getBytes(StandardCharsets.UTF_8));
        assertFalse(DataFiles.unpack(text, dir, () -> false));
        assertEquals(0, dir.list().length);
    }

    @Test
    public void unpackShouldStopWhenCancelled() throws IOException {
        File dir = mTemp.newFolder("unpack");
        assertFalse(DataFiles.unpack(zip("a.txt", "a", "b.txt", "b"), dir, () -> true));
        assertEquals(0, dir.list().length);

        // Cancelled once the first file is written.
        File first = new File(dir, "a.txt");
        assertFalse(DataFiles.unpack(zip("a.txt", "a", "b.txt", "b"), dir, first::exists));
        assertEquals(Arrays.asList("a.txt"), names(dir));
    }

    @Test
    public void dataFilesShouldBeFoundAtTheTop() throws IOException {
        File dir = mTemp.newFolder("unpack");
        write(new File(dir, "Morrowind.esm"), "esm");
        assertEquals(dir, DataFiles.findDataFiles(dir, 3));
    }

    @Test
    public void dataFilesShouldBeFoundThreeFoldersDown() throws IOException {
        File dir = mTemp.newFolder("unpack");
        write(path(dir, "a", "b", "Data Files", "Morrowind.esm"), "esm");
        write(path(dir, "a", "other.txt"), "other");
        assertEquals(path(dir, "a", "b", "Data Files"), DataFiles.findDataFiles(dir, 3));
    }

    @Test
    public void dataFilesShouldNotBeFoundFourFoldersDown() throws IOException {
        File dir = mTemp.newFolder("unpack");
        write(path(dir, "a", "b", "c", "Data Files", "Morrowind.esm"), "esm");
        assertNull(DataFiles.findDataFiles(dir, 3));
    }

    @Test
    public void dataFilesShouldBeFoundWhateverTheEsmsCase() throws IOException {
        File dir = mTemp.newFolder("unpack");
        write(path(dir, "Data Files", "morrowind.ESM"), "esm");
        assertEquals(path(dir, "Data Files"), DataFiles.findDataFiles(dir, 3));
    }

    @Test
    public void folderWithoutMorrowindEsmShouldNotBeFound() throws IOException {
        File dir = mTemp.newFolder("unpack");
        write(path(dir, "Data Files", "Morrowind.bsa"), "bsa");
        write(path(dir, "Data Files", "Tribunal.esm"), "esm");
        // A folder by that name doesn't count.
        path(dir, "Other", "Morrowind.esm").mkdirs();
        assertNull(DataFiles.findDataFiles(dir, 3));
    }

    @Test
    public void gameFileNamesShouldGetTheirUsualCase() {
        assertEquals("Morrowind.bsa", DataFiles.gameFileName("MORROWIND.BSA"));
        assertEquals("Bloodmoon.esm", DataFiles.gameFileName("bloodmoon.esm"));
        assertEquals("readme.txt", DataFiles.gameFileName("readme.txt"));
    }

    @Test
    public void installShouldFixTheCaseOfTopLevelGameFilesOnly() throws IOException {
        File unpacked = mTemp.newFolder("unpack");
        write(path(unpacked, "Morrowind", "Data Files", "MORROWIND.ESM"), "esm");
        write(path(unpacked, "Morrowind", "Data Files", "tribunal.esm"), "tribunal");
        write(path(unpacked, "Morrowind", "Data Files", "Meshes", "tribunal.esm"), "mesh");
        File target = new File(mTemp.getRoot(), "Data Files");

        DataFiles.install(unpacked, target);
        assertEquals(Arrays.asList("Meshes", "Morrowind.esm", "Tribunal.esm"), names(target));
        assertEquals(Arrays.asList("tribunal.esm"), names(new File(target, "Meshes")));
        assertEquals("tribunal", read(new File(target, "Tribunal.esm")));
    }

    @Test
    public void installShouldMoveFoldersIntoExistingFolders() throws IOException {
        File target = mTemp.newFolder("Data Files");
        write(path(target, "Meshes", "old.nif"), "old");
        write(path(target, "Meshes", "same.nif"), "before");
        File unpacked = mTemp.newFolder("unpack");
        write(path(unpacked, "Data Files", "Morrowind.esm"), "esm");
        write(path(unpacked, "Data Files", "Meshes", "same.nif"), "after");
        write(path(unpacked, "Data Files", "Meshes", "new.nif"), "new");

        DataFiles.install(unpacked, target);
        assertEquals(Arrays.asList("new.nif", "old.nif", "same.nif"), names(new File(target, "Meshes")));
        assertEquals("old", read(path(target, "Meshes", "old.nif")));
        assertEquals("after", read(path(target, "Meshes", "same.nif")));
        assertEquals("esm", read(new File(target, "Morrowind.esm")));
    }

    @Test
    public void installShouldMoveMorrowindEsmLast() throws IOException {
        // The disk lists a folder's files in an order of its own, so this tries many names for the file whose move
        // fails. Moving Morrowind.esm before any one of them would leave it in place.
        for (int i = 0; i < 20; ++i) {
            String blocked = "File" + i + ".bsa";
            File target = mTemp.newFolder("Data Files " + i);
            // A folder that isn't empty where a file must go, so that its move fails.
            write(path(target, blocked, "in the way.txt"), "in the way");
            File unpacked = mTemp.newFolder("unpack " + i);
            write(path(unpacked, "Data Files", "Morrowind.esm"), "esm");
            write(path(unpacked, "Data Files", blocked), "bsa");

            assertThrows(IOException.class, () -> DataFiles.install(unpacked, target));
            // The app finds the data by Morrowind.esm, so a move that stopped part way isn't taken for an install.
            assertFalse(blocked, new File(target, "Morrowind.esm").exists());
        }
    }

    @Test
    public void installWithoutMorrowindEsmShouldFail() throws IOException {
        File unpacked = mTemp.newFolder("unpack");
        write(path(unpacked, "Data Files", "Morrowind.bsa"), "bsa");
        File target = new File(mTemp.getRoot(), "Data Files");

        IOException e = assertThrows(IOException.class, () -> DataFiles.install(unpacked, target));
        assertEquals("There's no Morrowind.esm in that zip file.", e.getMessage());
        assertFalse(target.exists());
    }

    @Test
    public void spaceCheckShouldNameBothSizes() throws IOException {
        File dir = mTemp.newFolder("unpack");
        long needed = dir.getUsableSpace() + (1L << 40);
        IOException e = assertThrows(IOException.class, () -> DataFiles.checkSpace(dir, needed));
        // The free space can change while the test runs, so only its form is checked.
        String expected = "The files need " + DataFiles.size(needed).replace(".", "\\.")
            + ", and the device has [0-9.]+ [GM]B free\\.";
        assertTrue(e.getMessage(), e.getMessage().matches(expected));

        DataFiles.checkSpace(dir, 0);
    }
}
