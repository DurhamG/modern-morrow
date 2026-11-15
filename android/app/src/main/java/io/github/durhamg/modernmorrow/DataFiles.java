package io.github.durhamg.modernmorrow;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.function.BooleanSupplier;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * The parts of DataImport that work on plain files, with nothing from Android, so that the JVM unit tests can run them:
 * unpacking a zip, finding Data Files in it, and moving it into place.
 */
final class DataFiles {
    private DataFiles() {}

    // The files Files looks for by name, which might come in another case.
    private static final String[] sGameFiles = { "Morrowind.esm", "Morrowind.bsa", "Tribunal.esm", "Tribunal.bsa",
        "Bloodmoon.esm", "Bloodmoon.bsa" };

    /**
     * Unpacks a zip into a folder, and closes the stream. Returns false if it found nothing to unpack, which is also
     * what something that isn't a zip gives, or if it stopped because it was cancelled.
     */
    static boolean unpack(InputStream in, File dir, BooleanSupplier cancelled) throws IOException {
        try (ZipInputStream zip = new ZipInputStream(in)) {
            byte[] buffer = new byte[1 << 20];
            boolean any = false;
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (cancelled.getAsBoolean())
                    return false;
                any = true;
                File target = zipTarget(dir, entry.getName());
                if (entry.isDirectory()) {
                    makeDir(target);
                    continue;
                }
                makeDir(target.getParentFile());
                try (OutputStream out = new FileOutputStream(target)) {
                    int count;
                    while ((count = zip.read(buffer)) > 0) {
                        if (cancelled.getAsBoolean())
                            return false;
                        out.write(buffer, 0, count);
                    }
                }
            }
            return any;
        }
    }

    /** Finds Data Files in an unpacked zip, and moves it into the target, Morrowind.esm last. */
    static void install(File unpacked, File target) throws IOException {
        File dataFiles = findDataFiles(unpacked, 3);
        if (dataFiles == null)
            throw new IOException("There's no Morrowind.esm in that zip file.");
        makeDir(target);
        moveTree(dataFiles, target, true);
    }

    /** Where a zip entry goes, which mustn't be outside the folder. Zips made on Windows can use backslashes. */
    static File zipTarget(File dir, String name) throws IOException {
        File target = dir;
        for (String part : name.replace('\\', '/').split("/")) {
            if (part.isEmpty() || part.equals("."))
                continue;
            if (part.equals(".."))
                throw new IOException("The zip file has a file outside its folders: " + name);
            target = new File(target, part);
        }
        return target;
    }

    /** Looks for the folder that holds Morrowind.esm, in the folder and the folders in it, this many deep. */
    static File findDataFiles(File folder, int depth) {
        File[] children = folder.listFiles();
        if (children == null)
            return null;
        for (File child : children) {
            if (child.isFile() && child.getName().equalsIgnoreCase("Morrowind.esm"))
                return folder;
        }
        if (depth == 0)
            return null;
        for (File child : children) {
            if (child.isDirectory()) {
                File found = findDataFiles(child, depth - 1);
                if (found != null)
                    return found;
            }
        }
        return null;
    }

    static String gameFileName(String name) {
        for (String gameFile : sGameFiles) {
            if (name.equalsIgnoreCase(gameFile))
                return gameFile;
        }
        return name;
    }

    /** Moves the files into a folder on the same storage, over any there by the same name, Morrowind.esm last. */
    static void moveTree(File from, File to, boolean top) throws IOException {
        File[] children = from.listFiles();
        if (children == null)
            throw new IOException("Can't look in " + from);
        File esm = null;
        for (File child : children) {
            String name = top ? gameFileName(child.getName()) : child.getName();
            if (top && name.equals("Morrowind.esm")) {
                esm = child;
                continue;
            }
            moveFile(child, new File(to, name));
        }
        if (esm != null)
            moveFile(esm, new File(to, "Morrowind.esm"));
    }

    static void moveFile(File from, File to) throws IOException {
        if (from.isDirectory()) {
            makeDir(to);
            moveTree(from, to, false);
            return;
        }
        to.delete();
        if (!from.renameTo(to))
            throw new IOException("Can't create " + to);
    }

    static void checkSpace(File dir, long needed) throws IOException {
        long free = dir.getUsableSpace();
        if (needed > free)
            throw new IOException(String.format(Locale.ROOT,
                "The files need %s, and the device has %s free.", size(needed), size(free)));
    }

    static void makeDir(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Can't create " + dir);
    }

    static void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children)
                deleteRecursively(child);
        }
        file.delete();
    }

    static String size(long bytes) {
        if (bytes >= 1L << 30)
            return String.format(Locale.ROOT, "%.1f GB", bytes / (double) (1L << 30));
        return String.format(Locale.ROOT, "%d MB", bytes >> 20);
    }
}
