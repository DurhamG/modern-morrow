package io.github.durhamg.modernmorrow;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class CloudRequestTest {
    @Rule
    public TemporaryFolder mTemp = new TemporaryFolder();

    private static final String sAuthorization = "Phrase a%20test%20phrase";
    private static final String sUserAgent = "ModernMorrow/test";

    private ServerSocket mSocket;
    // What the server was sent, by name.
    private final Map<String, Object> mReceived = new ConcurrentHashMap<>();

    @After
    public void stop() throws IOException {
        if (mSocket != null)
            mSocket.close();
    }

    /**
     * Starts a server on this computer that answers one request with this status and body, and records the request.
     * The tests compile against Android's classes, which don't have the JDK's own HTTP server, so this is a small one.
     */
    private String serve(int status, byte[] reply) throws IOException {
        return serveRaw(socket -> {
            InputStream in = socket.getInputStream();
            String[] request = readHeaders(in).split("\r\n");
            String[] start = request[0].split(" ");
            mReceived.put("method", start[0]);
            mReceived.put("path", start[1]);
            int length = 0;
            for (int i = 1; i < request.length; ++i) {
                String[] header = request[i].split(": ", 2);
                for (String name : new String[] { "Content-Length", "Authorization", "User-Agent" }) {
                    if (header[0].equalsIgnoreCase(name))
                        mReceived.put(name, header[1]);
                }
                if (header[0].equalsIgnoreCase("Content-Length"))
                    length = Integer.parseInt(header[1]);
            }
            byte[] body = new byte[length];
            int done = 0;
            while (done < length) {
                int count = in.read(body, done, length - done);
                if (count < 0)
                    break;
                done += count;
            }
            mReceived.put("body", body);

            OutputStream out = socket.getOutputStream();
            out.write(("HTTP/1.1 " + status + " Status\r\nContent-Length: " + reply.length
                          + "\r\nConnection: close\r\n\r\n")
                          .getBytes(StandardCharsets.US_ASCII));
            out.write(reply);
            out.flush();
        });
    }

    /** Starts a server that takes a connection and then does this with it, which can be nothing at all. */
    private String serveRaw(SocketAction action) throws IOException {
        mSocket = new ServerSocket(0, 1, InetAddress.getLoopbackAddress());
        Thread thread = new Thread(() -> {
            try (Socket socket = mSocket.accept()) {
                action.run(socket);
            } catch (IOException | InterruptedException e) {
                // The test has ended, and closed the socket.
            }
        });
        thread.setDaemon(true);
        thread.start();
        return "http://127.0.0.1:" + mSocket.getLocalPort() + "/save";
    }

    private interface SocketAction {
        void run(Socket socket) throws IOException, InterruptedException;
    }

    private static byte[] readAll(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = in.read(buffer)) > 0)
            bytes.write(buffer, 0, count);
        return bytes.toByteArray();
    }

    private static byte[] randomBytes(int size) {
        byte[] bytes = new byte[size];
        new Random(size).nextBytes(bytes);
        return bytes;
    }

    private File file(String name, byte[] bytes) throws IOException {
        File file = new File(mTemp.getRoot(), name);
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        }
        return file;
    }

    private static CloudRequest.Reply send(String method, String url, File body, File downloadTo,
        CloudRequest.Progress progress) {
        return CloudRequest.send(method, url, sAuthorization, sUserAgent, body, downloadTo, progress,
            CloudRequest.sConnectTimeout, CloudRequest.sStallTimeout);
    }

    private static void assertNoAddress(CloudRequest.Reply reply, String url) {
        assertNotNull(reply.mFailure);
        assertFalse(reply.mFailure, reply.mFailure.contains("127.0.0.1"));
        assertFalse(reply.mFailure, reply.mFailure.contains(url.split(":")[2].split("/")[0]));
        assertFalse(reply.mFailure, reply.mFailure.contains("phrase"));
    }

    @Test
    public void uploadShouldArriveByteForByteWithItsHeaders() throws IOException {
        byte[] save = randomBytes(1 << 20);
        String url = serve(200, "{\"player\":\"Nerevar\"}".getBytes(StandardCharsets.UTF_8));
        CloudRequest.Reply reply = send("PUT", url, file("save.omwsave", save), null, null);

        assertEquals(200, reply.mStatus);
        assertNull(reply.mFailure);
        assertEquals("{\"player\":\"Nerevar\"}", new String(reply.mBody, StandardCharsets.UTF_8));
        assertEquals("PUT", mReceived.get("method"));
        assertEquals("/save", mReceived.get("path"));
        assertEquals(String.valueOf(save.length), mReceived.get("Content-Length"));
        assertEquals(sAuthorization, mReceived.get("Authorization"));
        assertEquals(sUserAgent, mReceived.get("User-Agent"));
        assertArrayEquals(save, (byte[]) mReceived.get("body"));
    }

    @Test
    public void uploadProgressShouldEndAtTheFileSize() throws IOException {
        byte[] save = randomBytes(1000 * 1000);
        String url = serve(200, "{}".getBytes(StandardCharsets.UTF_8));
        List<long[]> reports = new ArrayList<>();
        send("PUT", url, file("save.omwsave", save), null, (done, total) -> reports.add(new long[] { done, total }));

        assertTrue(reports.size() > 1);
        long last = 0;
        for (long[] report : reports) {
            assertTrue(report[0] > last);
            assertEquals(save.length, report[1]);
            last = report[0];
        }
        assertEquals(save.length, last);
    }

    @Test
    public void requestWithoutABodyShouldSendNone() throws IOException {
        String url = serve(200, "{}".getBytes(StandardCharsets.UTF_8));
        CloudRequest.Reply reply = send("GET", url, null, null, null);
        assertEquals(200, reply.mStatus);
        assertEquals("GET", mReceived.get("method"));
        assertEquals(0, ((byte[]) mReceived.get("body")).length);
        assertEquals(sAuthorization, mReceived.get("Authorization"));
        assertEquals(sUserAgent, mReceived.get("User-Agent"));
    }

    @Test
    public void downloadShouldWriteTheFileAndNoPartFile() throws IOException {
        byte[] save = randomBytes(300 * 1000);
        String url = serve(200, save);
        File to = new File(mTemp.getRoot(), "Nerevar.omwsave");
        List<long[]> reports = new ArrayList<>();
        CloudRequest.Reply reply
            = send("GET", url, null, to, (done, total) -> reports.add(new long[] { done, total }));

        assertEquals(200, reply.mStatus);
        assertEquals(0, reply.mBody.length);
        assertArrayEquals(save, Files.readAllBytes(to.toPath()));
        assertFalse(new File(to.getPath() + ".part").exists());
        assertEquals(save.length, reports.get(reports.size() - 1)[0]);
        assertEquals(save.length, reports.get(reports.size() - 1)[1]);
    }

    @Test
    public void failedDownloadShouldWriteNoFileAndKeepTheServersText() throws IOException {
        String url = serve(404, "{\"error\":\"no save\"}".getBytes(StandardCharsets.UTF_8));
        File to = new File(mTemp.getRoot(), "Nerevar.omwsave");
        CloudRequest.Reply reply = send("GET", url, null, to, null);

        assertEquals(404, reply.mStatus);
        assertNull(reply.mFailure);
        assertEquals("{\"error\":\"no save\"}", new String(reply.mBody, StandardCharsets.UTF_8));
        assertFalse(to.exists());
        assertFalse(new File(to.getPath() + ".part").exists());
    }

    @Test
    public void errorWithoutABodyShouldGiveAnEmptyBody() throws IOException {
        String url = serve(500, new byte[0]);
        CloudRequest.Reply reply = send("GET", url, null, null, null);
        assertEquals(500, reply.mStatus);
        assertEquals(0, reply.mBody.length);
    }

    @Test
    public void downloadCutShortShouldLeaveNoFile() throws IOException {
        String url = serveRaw(socket -> {
            readHeaders(socket.getInputStream());
            OutputStream out = socket.getOutputStream();
            out.write("HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n".getBytes(StandardCharsets.US_ASCII));
            out.write(new byte[500]);
            out.flush();
        });
        File to = new File(mTemp.getRoot(), "Nerevar.omwsave");
        CloudRequest.Reply reply = send("GET", url, null, to, null);

        assertEquals(0, reply.mStatus);
        assertNoAddress(reply, url);
        assertFalse(to.exists());
        assertFalse(new File(to.getPath() + ".part").exists());
    }

    /** Reads a request's start line and headers, up to the blank line after them. */
    private static String readHeaders(InputStream in) throws IOException {
        StringBuilder text = new StringBuilder();
        int b;
        while (text.indexOf("\r\n\r\n") < 0 && (b = in.read()) != -1)
            text.append((char) b);
        return text.toString();
    }

    @Test
    public void nothingListeningShouldGiveStatusZero() throws IOException {
        int port;
        try (ServerSocket socket = new ServerSocket(0, 1, InetAddress.getLoopbackAddress())) {
            port = socket.getLocalPort();
        }
        String url = "http://127.0.0.1:" + port + "/save";
        CloudRequest.Reply reply = send("GET", url, null, null, null);
        assertEquals(0, reply.mStatus);
        assertEquals(0, reply.mBody.length);
        assertNoAddress(reply, url);
    }

    // A request that never gives up would stop the tests, so this fails it instead.
    @Test(timeout = 20000)
    public void serverThatStopsSendingShouldTimeOut() throws IOException {
        String url = serveRaw(socket -> {
            readHeaders(socket.getInputStream());
            Thread.sleep(30000);
        });
        long start = System.nanoTime();
        CloudRequest.Reply reply = CloudRequest.send("GET", url, sAuthorization, sUserAgent, null, null, null,
            CloudRequest.sConnectTimeout, 500);
        long took = (System.nanoTime() - start) / 1000000;

        assertEquals(0, reply.mStatus);
        assertNoAddress(reply, url);
        assertTrue(reply.mFailure, reply.mFailure.contains("Timeout"));
        assertTrue("took " + took + " ms", took < 10000);
    }

    // A request that never gives up would stop the tests, so this fails it instead.
    @Test(timeout = 20000)
    public void uploadThatStopsGoingShouldTimeOut() throws IOException {
        // The server never reads, so once the connection's buffers are full, the upload can't go on.
        String url = serveRaw(socket -> Thread.sleep(30000));
        File save = file("save.omwsave", new byte[64 << 20]);
        long start = System.nanoTime();
        CloudRequest.Reply reply = CloudRequest.send("PUT", url, sAuthorization, sUserAgent, save, null, null,
            CloudRequest.sConnectTimeout, 500);
        long took = (System.nanoTime() - start) / 1000000;

        assertEquals(0, reply.mStatus);
        assertNoAddress(reply, url);
        assertTrue("took " + took + " ms", took < 10000);
    }

    @Test
    public void uploadOfAMissingFileShouldSendNothing() throws IOException {
        String url = serve(200, "{}".getBytes(StandardCharsets.UTF_8));
        CloudRequest.Reply reply = send("PUT", url, new File(mTemp.getRoot(), "gone.omwsave"), null, null);
        assertEquals(0, reply.mStatus);
        assertEquals("the save to upload can't be read", reply.mFailure);
        assertTrue(mReceived.isEmpty());
    }

    @Test
    public void pathsFromTheEngineShouldKeepEveryCharacter() throws IOException {
        // A save's file name comes from its description, which can hold characters such as emoji.
        byte[] save = randomBytes(1000);
        String url = serve(200, save);
        String name = "Caius 🗡️ Cosades.omwsave";
        byte[] path = new File(mTemp.getRoot(), name).getPath().getBytes(StandardCharsets.UTF_8);
        CloudRequest.Reply reply
            = CloudRequest.sendFromEngine("GET", url, sAuthorization, sUserAgent, null, path, 0);

        assertEquals(200, reply.mStatus);
        assertArrayEquals(save, Files.readAllBytes(new File(mTemp.getRoot(), name).toPath()));
    }
}
