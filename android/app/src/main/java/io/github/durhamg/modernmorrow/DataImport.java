package io.github.durhamg.modernmorrow;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Copies Morrowind's Data Files into the app's own Morrowind folder, from a folder or a zip file the player picks, such
 * as one in Downloads. Android's pickers give the app what was picked without any permission, but only through content
 * URIs, and the engine reads its data with file paths, so the files are copied rather than read where they are.
 *
 * The copy runs on a thread of its own, which outlives the activity, so folding the phone doesn't stop it.
 *
 * - A folder's files are copied straight into place. Each is written under a temporary name and renamed when it's
 *   complete, and Morrowind.esm goes last, so a copy that stops part way isn't taken for a finished one. Picking the
 *   folder again skips the files already copied.
 * - A zip file is read from start to end, as a stream, so all of it is unpacked into a folder beside Data Files first.
 *   Then the folder in it that holds Morrowind.esm is moved into place, with Morrowind.esm last. Each try starts again.
 */
final class DataImport {
    private static DataImport sCurrent;

    private final Context mContext;
    private final Uri mUri;
    private final boolean mZip;
    private final Thread mThread;
    private volatile boolean mCancelled;
    private volatile boolean mFinished;
    private volatile String mError;
    /** Bytes, with 0 for the total while it isn't known. A zip's are its packed bytes. */
    private volatile long mDone;
    private volatile long mTotal;

    private static final class Entry {
        String mId;
        String mName;
        boolean mFolder;
        long mSize;
    }

    private static final class Copy {
        Entry mEntry;
        File mTarget;
    }

    private static final String[] sColumns = { Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
        Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE };

    // The files Files looks for by name, which might come in another case.
    private static final String[] sGameFiles = { "Morrowind.esm", "Morrowind.bsa", "Tribunal.esm", "Tribunal.bsa",
        "Bloodmoon.esm", "Bloodmoon.bsa" };

    private DataImport(Context context, Uri uri, boolean zip) {
        mContext = context.getApplicationContext();
        mUri = uri;
        mZip = zip;
        mThread = new Thread(this::run, "DataImport");
    }

    /**
     * Starts copying from a folder or a zip file a picker gave, unless a copy is already going. The picker's grant
     * must be persistable, so that it lasts as long as the copy.
     */
    static synchronized void start(Context context, Uri uri, boolean zip) {
        if (sCurrent != null && !sCurrent.mFinished)
            return;
        context.getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        sCurrent = new DataImport(context, uri, zip);
        sCurrent.mThread.start();
    }

    /** The copy going on, or the last one if nothing has forgotten it yet, or null. */
    static synchronized DataImport current() {
        return sCurrent;
    }

    static synchronized void forget() {
        sCurrent = null;
    }

    boolean isZip() {
        return mZip;
    }

    boolean isFinished() {
        return mFinished;
    }

    /** Why the copy failed, or null if it worked or was cancelled. */
    String error() {
        return mError;
    }

    long done() {
        return mDone;
    }

    long total() {
        return mTotal;
    }

    void cancel() {
        mCancelled = true;
    }

    private void run() {
        try {
            if (mZip)
                unpackZip();
            else
                copyFolder();
        } catch (IOException | RuntimeException e) {
            if (!mCancelled)
                mError = e.getMessage() != null ? e.getMessage() : e.toString();
        } finally {
            deleteRecursively(unpackDir());
            try {
                mContext.getContentResolver().releasePersistableUriPermission(
                    mUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (SecurityException e) {
                // Already gone.
            }
            mFinished = true;
        }
    }

    private File dataFilesDir() {
        return new File(Files.appMorrowindDir(mContext), "Data Files");
    }

    private File unpackDir() {
        return new File(Files.appMorrowindDir(mContext), "Unpacking");
    }

    private void copyFolder() throws IOException {
        ContentResolver resolver = mContext.getContentResolver();
        String dataFiles = findDataFiles(resolver, DocumentsContract.getTreeDocumentId(mUri), 2);
        if (dataFiles == null)
            throw new IOException("There's no Morrowind.esm in that folder.");

        File target = dataFilesDir();
        List<Copy> copies = new ArrayList<>();
        listFiles(resolver, dataFiles, target, true, copies);
        if (mCancelled)
            return;
        Copy esm = null;
        long total = 0;
        long needed = 0;
        for (Copy copy : copies) {
            if (copy.mTarget.getName().equals("Morrowind.esm") && target.equals(copy.mTarget.getParentFile()))
                esm = copy;
            total += copy.mEntry.mSize;
            if (!isCopied(copy))
                needed += copy.mEntry.mSize;
        }
        copies.remove(esm);
        copies.add(esm);

        makeDir(target);
        checkSpace(target, needed);
        mTotal = total;
        for (Copy copy : copies) {
            if (mCancelled)
                return;
            if (isCopied(copy))
                mDone += copy.mEntry.mSize;
            else
                copyFile(resolver, copy);
        }
    }

    private void unpackZip() throws IOException {
        ContentResolver resolver = mContext.getContentResolver();
        File unpack = unpackDir();
        deleteRecursively(unpack);
        makeDir(unpack);

        // The files come out about as big as the zip, as the BSAs hardly pack.
        long zipSize = 0;
        try (Cursor cursor = resolver.query(mUri, new String[] { OpenableColumns.SIZE }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0))
                zipSize = cursor.getLong(0);
        }
        checkSpace(unpack, zipSize);
        mTotal = zipSize;

        InputStream raw = resolver.openInputStream(mUri);
        if (raw == null)
            throw new IOException("Can't read the zip file.");
        try (ZipInputStream zip = new ZipInputStream(new FilterInputStream(raw) {
                 @Override
                 public int read(byte[] buffer, int offset, int length) throws IOException {
                     int count = super.read(buffer, offset, length);
                     if (count > 0)
                         mDone += count;
                     return count;
                 }
             })) {
            byte[] buffer = new byte[1 << 20];
            boolean any = false;
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (mCancelled)
                    return;
                any = true;
                File target = zipTarget(unpack, entry.getName());
                if (entry.isDirectory()) {
                    makeDir(target);
                    continue;
                }
                makeDir(target.getParentFile());
                try (OutputStream out = new FileOutputStream(target)) {
                    int count;
                    while ((count = zip.read(buffer)) > 0) {
                        if (mCancelled)
                            return;
                        out.write(buffer, 0, count);
                    }
                }
            }
            if (!any)
                throw new IOException("That isn't a zip file, or it's empty.");
        }

        File dataFiles = findDataFiles(unpack, 3);
        if (dataFiles == null)
            throw new IOException("There's no Morrowind.esm in that zip file.");
        File target = dataFilesDir();
        makeDir(target);
        moveTree(dataFiles, target, true);
    }

    /** Where a zip entry goes, which mustn't be outside the folder. Zips made on Windows can use backslashes. */
    private static File zipTarget(File dir, String name) throws IOException {
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

    /** Looks for the folder that holds Morrowind.esm, in the picked folder and the folders in it, this many deep. */
    private String findDataFiles(ContentResolver resolver, String folder, int depth) throws IOException {
        List<Entry> entries = list(resolver, folder);
        for (Entry entry : entries) {
            if (!entry.mFolder && entry.mName.equalsIgnoreCase("Morrowind.esm"))
                return folder;
        }
        if (depth == 0)
            return null;
        for (Entry entry : entries) {
            if (entry.mFolder) {
                String found = findDataFiles(resolver, entry.mId, depth - 1);
                if (found != null)
                    return found;
            }
        }
        return null;
    }

    private static File findDataFiles(File folder, int depth) {
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

    private static String gameFileName(String name) {
        for (String gameFile : sGameFiles) {
            if (name.equalsIgnoreCase(gameFile))
                return gameFile;
        }
        return name;
    }

    private void listFiles(ContentResolver resolver, String folder, File target, boolean top, List<Copy> copies)
        throws IOException {
        for (Entry entry : list(resolver, folder)) {
            if (mCancelled)
                return;
            String name = top ? gameFileName(entry.mName) : entry.mName;
            if (entry.mFolder) {
                listFiles(resolver, entry.mId, new File(target, name), false, copies);
            } else {
                Copy copy = new Copy();
                copy.mEntry = entry;
                copy.mTarget = new File(target, name);
                copies.add(copy);
            }
        }
    }

    private List<Entry> list(ContentResolver resolver, String folder) throws IOException {
        List<Entry> entries = new ArrayList<>();
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(mUri, folder);
        try (Cursor cursor = resolver.query(children, sColumns, null, null, null)) {
            if (cursor == null)
                throw new IOException("Can't look in the folder.");
            while (cursor.moveToNext()) {
                Entry entry = new Entry();
                entry.mId = cursor.getString(0);
                entry.mName = cursor.getString(1);
                entry.mFolder = Document.MIME_TYPE_DIR.equals(cursor.getString(2));
                entry.mSize = cursor.isNull(3) ? 0 : cursor.getLong(3);
                entries.add(entry);
            }
        }
        return entries;
    }

    private static boolean isCopied(Copy copy) {
        return copy.mTarget.isFile() && copy.mTarget.length() == copy.mEntry.mSize;
    }

    private void copyFile(ContentResolver resolver, Copy copy) throws IOException {
        File parent = copy.mTarget.getParentFile();
        makeDir(parent);
        File part = new File(parent, copy.mTarget.getName() + ".part");
        Uri uri = DocumentsContract.buildDocumentUriUsingTree(mUri, copy.mEntry.mId);
        boolean done = false;
        try (InputStream in = resolver.openInputStream(uri); OutputStream out = new FileOutputStream(part)) {
            if (in == null)
                throw new IOException("Can't read " + copy.mEntry.mName);
            byte[] buffer = new byte[1 << 20];
            int count;
            while ((count = in.read(buffer)) > 0) {
                if (mCancelled)
                    return;
                out.write(buffer, 0, count);
                mDone += count;
            }
            done = true;
        } finally {
            if (!done)
                part.delete();
        }
        copy.mTarget.delete();
        if (!part.renameTo(copy.mTarget))
            throw new IOException("Can't create " + copy.mTarget);
    }

    /** Moves the files into a folder on the same storage, over any there by the same name, Morrowind.esm last. */
    private static void moveTree(File from, File to, boolean top) throws IOException {
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

    private static void moveFile(File from, File to) throws IOException {
        if (from.isDirectory()) {
            makeDir(to);
            moveTree(from, to, false);
            return;
        }
        to.delete();
        if (!from.renameTo(to))
            throw new IOException("Can't create " + to);
    }

    private static void checkSpace(File dir, long needed) throws IOException {
        long free = dir.getUsableSpace();
        if (needed > free)
            throw new IOException(String.format(Locale.ROOT,
                "The files need %s, and the device has %s free.", size(needed), size(free)));
    }

    private static void makeDir(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Can't create " + dir);
    }

    private static void deleteRecursively(File file) {
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
