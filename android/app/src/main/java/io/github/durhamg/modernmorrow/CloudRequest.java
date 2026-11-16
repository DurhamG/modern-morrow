package io.github.durhamg.modernmorrow;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * Sends one of cloud saves' requests, for the engine's cloudtransportandroid.cpp, which calls sendFromEngine through
 * JNI. It does what the other platforms' transports do. See ui-rework/cloud-saves.md.
 *
 * Nothing here comes from Android, so that the JVM unit tests can run it. It doesn't log either, since android.util.Log
 * doesn't work in those tests. A reply says why a request failed instead, and the engine logs that.
 */
final class CloudRequest {
    private CloudRequest() {}

    /** Connecting gives up after this long. */
    static final int sConnectTimeout = 15000;
    /** A transfer gives up when nothing goes either way for this long. */
    static final int sStallTimeout = 60000;

    private static final int sBlockSize = 64 * 1024;

    /** Told how many bytes of an upload or download have gone, and how many there are in all. */
    interface Progress {
        void report(long done, long total);
    }

    /** What came back. The fields are read by the engine through JNI, so their names matter. */
    static final class Reply {
        /** The HTTP status, or 0 when no reply came, such as without a connection. */
        final int mStatus;
        /** The reply's body, as it came. Empty when it went to the download's file. */
        final byte[] mBody;
        /** Why no reply came, for the log. It never holds the address or the phrase. Null when a reply came. */
        final String mFailure;

        Reply(int status, byte[] body, String failure) {
            mStatus = status;
            mBody = body;
            mFailure = failure;
        }
    }

    /** A problem with a file on this device, rather than with the request. Its message is fit for the log. */
    private static final class FileProblem extends IOException {
        FileProblem(String message) {
            super(message);
        }
    }

    /**
     * What the engine calls. The paths are UTF-8, because the strings JNI makes from C++ can't hold characters such as
     * emoji, which a save's name can. Each is null when not given. progress is the address of the engine's
     * CloudProgress, or 0 for none.
     */
    static Reply sendFromEngine(String method, String url, String authorization, String userAgent, byte[] bodyFile,
        byte[] downloadTo, long progress) {
        return send(method, url, authorization, userAgent, file(bodyFile), file(downloadTo),
            progress == 0 ? null : (done, total) -> reportProgress(progress, done, total), sConnectTimeout,
            sStallTimeout);
    }

    /** Calls the engine's CloudProgress at this address. */
    private static native void reportProgress(long progress, long done, long total);

    private static File file(byte[] path) {
        return path == null ? null : new File(new String(path, StandardCharsets.UTF_8));
    }

    /**
     * Sends a request and waits for the reply. The body comes from bodyFile when one is given. When downloadTo is
     * given and the status is 200, the reply's body is written to that file instead of the reply. It goes to a file
     * beside it first, and only takes its name once the whole body has come, so a download cut short leaves nothing.
     */
    static Reply send(String method, String url, String authorization, String userAgent, File bodyFile,
        File downloadTo, Progress progress, int connectTimeout, int stallTimeout) {
        File partial = downloadTo == null ? null : new File(downloadTo.getPath() + ".part");
        HttpURLConnection connection = null;
        try (InputStream body = bodyFile == null ? null : openUpload(bodyFile)) {
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setRequestMethod(method);
            connection.setConnectTimeout(connectTimeout);
            connection.setReadTimeout(stallTimeout);
            connection.setRequestProperty("Authorization", authorization);
            connection.setRequestProperty("User-Agent", userAgent);
            if (body != null)
                upload(connection, body, bodyFile.length(), progress, stallTimeout);

            int status = connection.getResponseCode();
            // -1 when what came back wasn't HTTP.
            if (status < 0)
                throw new ProtocolException();
            // An error's body comes from getErrorStream, which is null when there's none.
            try (InputStream in = status >= 400 ? connection.getErrorStream() : connection.getInputStream()) {
                if (downloadTo != null && status == 200) {
                    download(in, connection.getContentLengthLong(), partial, progress);
                    if (!partial.renameTo(downloadTo))
                        throw new FileProblem("the downloaded save can't be written");
                    return new Reply(status, new byte[0], null);
                }
                return new Reply(status, in == null ? new byte[0] : readAll(in), null);
            }
        } catch (FileProblem e) {
            return new Reply(0, new byte[0], e.getMessage());
        } catch (IOException e) {
            // The exception's message can hold the address, so only its kind goes in the log.
            return new Reply(0, new byte[0], "the request failed: " + e.getClass().getSimpleName());
        } finally {
            if (connection != null)
                connection.disconnect();
            // Gone already when the download took its name.
            if (partial != null)
                partial.delete();
        }
    }

    private static InputStream openUpload(File file) throws FileProblem {
        try {
            return new FileInputStream(file);
        } catch (FileNotFoundException e) {
            throw new FileProblem("the save to upload can't be read");
        }
    }

    /**
     * Sends the body from the file, a block at a time. HttpURLConnection only limits how long a read waits, and on
     * Android writing an upload can wait until the system gives up on the connection, which takes many minutes. So
     * this closes the connection when one block waits as long as a read may.
     */
    private static void upload(HttpURLConnection connection, InputStream body, long size, Progress progress,
        int stallTimeout) throws IOException {
        connection.setDoOutput(true);
        // Without this, HttpURLConnection keeps the whole body in memory and sends it at the end.
        connection.setFixedLengthStreamingMode(size);
        ScheduledExecutorService watchdog = Executors.newSingleThreadScheduledExecutor();
        try (OutputStream out = connection.getOutputStream()) {
            byte[] buffer = new byte[sBlockSize];
            long done = 0;
            int count;
            while ((count = body.read(buffer)) > 0) {
                ScheduledFuture<?> stall
                    = watchdog.schedule(connection::disconnect, stallTimeout, TimeUnit.MILLISECONDS);
                out.write(buffer, 0, count);
                stall.cancel(false);
                done += count;
                if (progress != null)
                    progress.report(done, size);
            }
        } finally {
            watchdog.shutdownNow();
        }
    }

    private static void download(InputStream in, long size, File partial, Progress progress) throws IOException {
        OutputStream out;
        try {
            out = new FileOutputStream(partial);
        } catch (FileNotFoundException e) {
            throw new FileProblem("the downloaded save can't be written");
        }
        try {
            byte[] buffer = new byte[sBlockSize];
            long done = 0;
            int count;
            while ((count = in.read(buffer)) > 0) {
                try {
                    out.write(buffer, 0, count);
                } catch (IOException e) {
                    throw new FileProblem("the downloaded save can't be written");
                }
                done += count;
                // The size is -1 when the reply doesn't give it.
                if (progress != null && size > 0)
                    progress.report(done, size);
            }
        } finally {
            out.close();
        }
    }

    private static byte[] readAll(InputStream in) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buffer = new byte[sBlockSize];
        int count;
        while ((count = in.read(buffer)) > 0)
            bytes.write(buffer, 0, count);
        return bytes.toByteArray();
    }
}
