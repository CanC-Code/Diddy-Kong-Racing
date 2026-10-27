package com.dkrwrapper;

import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicBoolean;

public class StreamServer {
    public static android.content.Context sCtx;
    public static void setContext(android.content.Context ctx) { sCtx = ctx.getApplicationContext(); }

    private static final String TAG = "BKA-Stream";
    static final int W = 320, H = 240;
    private static final int PORT = 8081;
    private static final int FPS = 30;

    private final ServerSocket server;
    private final AtomicBoolean running = new AtomicBoolean(true);
    private final int[] pixels = new int[W * H];
    private final byte[] raw = new byte[W * H * 4];

    public StreamServer() throws IOException {
        server = new ServerSocket(PORT);
        Log.i(TAG, "HTTP listening on :" + PORT);
    }

    public void serve() {
        while (running.get()) {
            try {
                Socket s = server.accept();
                Thread t = new Thread(() -> handleClient(s), "BKA-Stream-Client");
                t.setDaemon(true);
                t.start();
            } catch (IOException e) {
                if (running.get()) Log.e(TAG, "accept err", e);
                break;
            }
        }
    }

    public void stop() {
        running.set(false);
        try { server.close(); } catch (IOException ignored) {}
    }

    private void handleClient(Socket s) {
        try (Socket sock = s) {
            sock.setTcpNoDelay(true);
            InputStream in = sock.getInputStream();
            OutputStream out = new BufferedOutputStream(sock.getOutputStream(), 65536);

            String requestLine = readLine(in);
            if (requestLine == null) return;
            String[] parts = requestLine.split(" ");
            if (parts.length < 2) return;
            String method = parts[0];
            String path = parts[1];

            String hdr;
            while ((hdr = readLine(in)) != null && !hdr.isEmpty()) { }

            if ("GET".equals(method) && path.startsWith("/diag")) {
                StringBuilder sb = new StringBuilder();
                try {
                    java.io.File f = new java.io.File(getFilesDir(), "diag.log");
                    if (f.exists()) {
                        java.io.BufferedReader br = new java.io.BufferedReader(new java.io.FileReader(f));
                        java.util.LinkedList<String> lines = new java.util.LinkedList<>();
                        String line;
                        java.util.HashMap<String, Integer> lineCount = new java.util.HashMap<>();
                        while ((line = br.readLine()) != null) {
                            // Skip zero-param SetTile spam
                            if (line.contains("BKA-DIAG-ST") && line.contains("fmt=0 siz=0 line=0 tmem=0x0")) continue;
                            if (line.contains("BKA-ST-ENTRY") && line.contains("w0=F7000000")) continue;
                            // Extract tag prefix for deduplication
                            int colon = line.indexOf(':');
                            String key = (colon > 0) ? line.substring(0, colon) + "|" + line.substring(colon+1, Math.min(colon+80, line.length())) : line;
                            Integer cnt = lineCount.get(key);
                            if (cnt == null) {
                                lineCount.put(key, 1);
                                lines.add(line);
                            } else {
                                lineCount.put(key, cnt + 1);
                                if (cnt < 10) lines.add(line);
                                else if (cnt == 10) lines.add("  (repeated " + (cnt + 1) + "x)");
                            }
                            if (lines.size() > 2000) lines.removeFirst();
                        }
                        br.close();
                        sb.append("=== DIAG.LOG (last ").append(lines.size()).append(" lines) ===\n");
                        for (String l : lines) sb.append(l).append("\n");
                    } else {
                        sb.append("(diag.log not found at ").append(getFilesDir()).append(")\n");
                    }
                } catch (Throwable t) {
                    sb.append("Error reading diag.log: ").append(t).append("\n");
                }
                try {
                    Process p = Runtime.getRuntime().exec(new String[]{
                        "logcat", "-d", "-t", "300", "-s",
                        "BKA-ST-ENTRY:*", "BKA-DIAG-LB:*", "BKA-SETIMG-DIAG:*",
                        "BKA-PNG:*", "BKA-BADHDR:*", "BKA-TEXFMT:*",
                        "BKA-Stream:*", "BKA-PNG-FETCH:*"
                    });
                    java.io.BufferedReader br = new java.io.BufferedReader(new java.io.InputStreamReader(p.getInputStream()));
                    String line;
                    sb.append("\n=== LOGCAT (last 300) ===\n");
                    int n = 0;
                    while ((line = br.readLine()) != null && n++ < 300) {
                        sb.append(line).append("\n");
                    }
                    br.close();
                    p.waitFor();
                } catch (Throwable t) {
                    sb.append("Error reading logcat: ").append(t).append("\n");
                }
                byte[] body = sb.toString().getBytes("UTF-8");
                out.write(("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: " + body.length + "\r\nConnection: close\r\n\r\n").getBytes());
                out.write(body);
                out.flush();
                return;
            }
            if ("GET".equals(method) && path.startsWith("/status")) {
                int pngs = 0;
                boolean engineRunning = false;
                try {
                    java.io.File filesDir = sCtx != null ? sCtx.getFilesDir() : null;
                    if (filesDir != null) {
                        java.io.File pngDir = new java.io.File(filesDir, "extractor/assets/.vanilla/us.v80/textures");
                        if (pngDir.exists()) {
                            java.util.ArrayDeque<java.io.File> queue = new java.util.ArrayDeque<>();
                            queue.add(pngDir);
                            while (!queue.isEmpty()) {
                                java.io.File d = queue.poll();
                                java.io.File[] list = d.listFiles();
                                if (list == null) continue;
                                for (java.io.File f : list) {
                                    if (f.isFile() && f.getName().endsWith(".png")) pngs++;
                                    else if (f.isDirectory()) queue.add(f);
                                }
                            }
                        }
                    }
                    engineRunning = Native.isEngineRunning();
                } catch (Throwable t) {
                    android.util.Log.e("BKA", "status error", t);
                }
                String json = "{\"pngs\":" + pngs + ",\"engine\":" + engineRunning + "}";
                byte[] body = json.getBytes("UTF-8");
                out.write(("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + body.length + "\r\nConnection: close\r\n\r\n").getBytes());
                out.write(body);
                out.flush();
                return;
            }
            if ("GET".equals(method) && (path.equals("/") || path.startsWith("/?"))) {
                sendHtmlPage(out);
            } else if ("GET".equals(method) && path.startsWith("/stream")) {
                sendRawStream(out);
            } else if ("GET".equals(method) && path.startsWith("/ping")) {
                byte[] body = "pong".getBytes("UTF-8");
                out.write(("HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\n").getBytes());
                out.write(body);
                out.flush();
            } else {
                out.write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".getBytes());
                out.flush();
            }
        } catch (IOException e) {
            Log.w(TAG, "client ended: " + e.getMessage());
        }
    }

    private static String readLine(InputStream in) throws IOException {
        StringBuilder sb = new StringBuilder(128);
        int c;
        while ((c = in.read()) != -1) {
            if (c == '\n') break;
            if (c == '\r') continue;
            sb.append((char) c);
            if (sb.length() > 4096) break;
        }
        if (sb.length() == 0 && c == -1) return null;
        return sb.toString();
    }

    private void sendHtmlPage(OutputStream out) throws IOException {
        byte[] body = HTML_PAGE.getBytes("UTF-8");
        String hdr = "HTTP/1.1 200 OK\r\n"
                + "Content-Type: text/html; charset=utf-8\r\n"
                + "Content-Length: " + body.length + "\r\n"
                + "Cache-Control: no-cache\r\n"
                + "Connection: close\r\n\r\n";
        out.write(hdr.getBytes("UTF-8"));
        out.write(body);
        out.flush();
    }

    private java.io.File getFilesDir() { return sCtx != null ? sCtx.getFilesDir() : null; }

    private void sendRawStream(OutputStream out) throws IOException {
        String hdr = "HTTP/1.1 200 OK\r\n"
                + "Content-Type: application/octet-stream\r\n"
                + "Cache-Control: no-cache\r\n"
                + "Pragma: no-cache\r\n"
                + "Connection: close\r\n\r\n";
        out.write(hdr.getBytes("UTF-8"));
        out.flush();

        long frameInterval = 1000L / FPS;
        long nextFrame = System.currentTimeMillis();
        int frame = 0;

        while (running.get()) {
            long now = System.currentTimeMillis();
            if (now < nextFrame) {
                try { Thread.sleep(nextFrame - now); } catch (InterruptedException e) { break; }
            }
            nextFrame += frameInterval;
            if (nextFrame < System.currentTimeMillis() - frameInterval * 2) {
                nextFrame = System.currentTimeMillis();
            }

            try {
                Native.fillArgb(pixels);
                for (int i = 0; i < pixels.length; i++) {
                    int p = pixels[i];
                    raw[i*4]   = (byte)((p >> 16) & 0xFF);
                    raw[i*4+1] = (byte)((p >> 8)  & 0xFF);
                    raw[i*4+2] = (byte)( p        & 0xFF);
                    raw[i*4+3] = (byte)((p >> 24) & 0xFF);
                }
                int len = raw.length;
                out.write((len >>> 24) & 0xFF);
                out.write((len >>> 16) & 0xFF);
                out.write((len >>>  8) & 0xFF);
                out.write( len         & 0xFF);
                out.write(raw);
                out.flush();
                frame++;
                if (frame == 1 || frame % 150 == 0) {
                    Log.i(TAG, "raw frame " + frame);
                }
            } catch (IOException e) {
                Log.i(TAG, "stream closed after " + frame + " frames");
                break;
            }
        }
    }

    private static final String HTML_PAGE =
        "<!doctype html>\n" +
        "<html>\n" +
        "<head>\n" +
        "<meta charset='utf-8'>\n" +
        "<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no,viewport-fit=cover'>\n" +
        "<title>DKR</title>\n" +
        "<style>\n" +
        "html,body{margin:0;padding:0;background:#000;overflow:hidden;height:100%;width:100%}\n" +
        "#screen{width:100vw;height:100vh;object-fit:contain;image-rendering:pixelated;image-rendering:crisp-edges;display:block;background:#000}\n" +
        "#hud{position:fixed;left:8px;top:8px;color:#0f0;font:12px monospace;background:rgba(0,0,0,.5);padding:4px 8px;border-radius:4px;pointer-events:none}\n" +
        "</style>\n" +
        "</head>\n" +
        "<body>\n" +
        "<canvas id='screen'></canvas>\n" +
        "<div id='hud'>DKR streaming</div>\n" +
        "<script>\n" +
        "var W=" + W + ",H=" + H + ";\n" +
        "var cv=document.getElementById('screen');\n" +
        "cv.width=W;cv.height=H;\n" +
        "var cx=cv.getContext('2d');\n" +
        "var im=cx.createImageData(W,H);\n" +
        "var buf=new Uint8Array(0);\n" +
        "function tick(){\n" +
        "fetch('/stream?'+Date.now()).then(function(r){\n" +
        "var rd=r.body.getReader();var need=W*H*4;\n" +
        "function pump(){rd.read().then(function(x){\n" +
        "if(x.done)return;\n" +
        "var nb=new Uint8Array(buf.length+x.value.length);\n" +
        "nb.set(buf,0);nb.set(x.value,buf.length);buf=nb;\n" +
        "while(buf.length>=need){im.data.set(buf.subarray(0,need));cx.putImageData(im,0,0);buf=buf.subarray(need);}\n" +
        "pump();});}\n" +
        "pump();\n" +
        "}).catch(function(){setTimeout(tick,1000);});\n" +
        "}\n" +
        "tick();\n" +
        "</script>\n" +
        "</body>\n" +
        "</html>\n";
}
