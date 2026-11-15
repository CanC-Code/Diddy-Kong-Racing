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
    static final int W = 292, H = 216;
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
        /* r139: Socket$2.run shutdown thread SIGSEGV fix. Chrome /diag
         * auto-refresh opens/closes 30 sockets/min; each Socket.close()
         * spawns an internal thread that races with itself in
         * AbstractPlainSocketImpl.getInputStream -> SIGSEGV kills process.
         * setSoLinger(false, 0) makes close() send RST instead of FIN,
         * skipping that shutdown handshake thread entirely. */
        try { s.setSoLinger(false, 0); } catch (Throwable ignored) {}
        try { s.setTcpNoDelay(true); } catch (Throwable ignored) {}
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

            if ("GET".equals(method) && path.startsWith("/input")) {
                int btn = 0;
                int sx = 0;
                int sy = 0;
                try {
                    String query = path.contains("?") ? path.substring(path.indexOf("?") + 1) : "";
                    for (String param : query.split("&")) {
                        String[] kv = param.split("=");
                        if (kv.length == 2) {
                            if (kv[0].equals("btn")) btn = Integer.parseInt(kv[1], 16);
                            if (kv[0].equals("sx")) sx = Integer.parseInt(kv[1]);
                            if (kv[0].equals("sy")) sy = Integer.parseInt(kv[1]);
                        }
                    }
                } catch (Exception e) {}
                android.util.Log.d("BKA-Stream", "setVirtualInput: btn=" + Integer.toHexString(btn) + " sx=" + sx + " sy=" + sy);
                NativeBridge.setVirtualInput(btn, sx, sy);
                out.write(("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n").getBytes());
                out.flush();
                return;
            }
            if ("GET".equals(method) && path.startsWith("/diag")) {
                // r134 diag: mobile-data-independent debug page.
                // Captures logcat BKA_ and DKR_ tags, auto-refresh 2s.
                // No rish or ADB needed - app reads own logcat.
                StringBuilder sb = new StringBuilder();
                sb.append("<!doctype html><html><head><meta charset='utf-8'>");
                sb.append("<meta http-equiv='refresh' content='2'>");
                sb.append("<title>DKR diag</title>");
                sb.append("<style>body{margin:0;background:#111;color:#0f0;font:11px monospace}");
                sb.append("pre{padding:6px;margin:0;white-space:pre-wrap;word-break:break-all}");
                sb.append("h3{color:#ff0;margin:4px 0;padding:4px;background:#333}");
                sb.append("a{color:#0ff}</style></head><body>");
                sb.append("<h3>DKR diag - <a href='/'>stream</a> | ");
                sb.append("<a href='/diag?").append(System.currentTimeMillis()).append("'>reload</a> | pid=");
                sb.append(android.os.Process.myPid()).append("</h3><pre>");

                int captured = 0;
                try {
                    Process p = Runtime.getRuntime().exec(new String[]{
                        "/system/bin/logcat", "-d", "-t", "800"
                    });
                    java.io.BufferedReader br = new java.io.BufferedReader(
                        new java.io.InputStreamReader(p.getInputStream()));
                    String line;
                    while ((line = br.readLine()) != null && captured < 500) {
                        if (line.contains(" BKA-") || line.contains(" DKR-")
                            || line.contains(" BKA_") || line.contains(" DKR_")
                            || line.contains("AndroidRuntime")) {
                            sb.append(line.replace("&","&amp;").replace("<","&lt;")).append("\n");
                            captured++;
                        }
                    }
                    br.close();
                    p.waitFor();
                } catch (Throwable t) {
                    sb.append("logcat error: ").append(t).append("\n");
                }
                sb.append("\n=== diag.log tail ===\n");
                try {
                    java.io.File f = new java.io.File(getFilesDir(), "diag.log");
                    if (f.exists()) {
                        java.io.RandomAccessFile raf = new java.io.RandomAccessFile(f, "r");
                        long len = raf.length();
                        long startPos = Math.max(0, len - 16384);
                        raf.seek(startPos);
                        byte[] tail = new byte[(int)(len - startPos)];
                        raf.readFully(tail);
                        raf.close();
                        sb.append(new String(tail, "UTF-8"));
                    } else {
                        sb.append("(no diag.log)\n");
                    }
                } catch (Throwable t) {
                    sb.append("diag.log err: ").append(t).append("\n");
                }
                sb.append("\n=== RAW LOGCAT (last 200) ===\n");
                try {
                    Process pr = Runtime.getRuntime().exec(new String[]{
                        "/system/bin/logcat", "-d", "-t", "200", "-b", "crash,main"
                    });
                    java.io.BufferedReader br2 = new java.io.BufferedReader(
                        new java.io.InputStreamReader(pr.getInputStream()));
                    String ln2;
                    int cap2 = 0;
                    while ((ln2 = br2.readLine()) != null && cap2 < 200) {
                        sb.append(ln2.replace("&","&amp;").replace("<","&lt;")).append("\n");
                        cap2++;
                    }
                    br2.close();
                    pr.waitFor();
                } catch (Throwable t) {
                    sb.append("raw logcat err: ").append(t).append("\n");
                }
                try {
                    java.io.File tombDir = new java.io.File("/data/tombstones");
                    java.io.File[] tombs = tombDir.listFiles();
                    if (tombs != null && tombs.length > 0) {
                        java.util.Arrays.sort(tombs, (a,b) -> Long.compare(b.lastModified(), a.lastModified()));
                        java.io.File last = tombs[0];
                        sb.append("\n=== LATEST TOMBSTONE: ").append(last.getName()).append(" ===\n");
                        sb.append("(not readable — file perms)\n");
                    }
                } catch (Throwable t) {}
                sb.append("</pre></body></html>");
                byte[] body = sb.toString().getBytes("UTF-8");
                out.write(("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " + body.length + "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n").getBytes());
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
        } catch (Throwable t) {
            /* r139: never let a socket thread's exception kill the process */
            Log.e(TAG, "handleClient crashed: " + t);
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
                if (frame == 1 || frame % 150 == 0) {
                    Log.i(TAG, "raw frame " + frame + " px0=" + pixels[0]);
                }
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
        "<div id=\"ui-container\">\n" +
        "  <div id=\"settings-menu\" style=\"position:absolute; top:10px; right:10px; z-index:100; background:rgba(0,0,0,0.8); padding:10px; color:white; border-radius:10px; display:none;\">\n" +
        "    <label><input type=\"checkbox\" id=\"toggle-cstick\"> C-Buttons as Right Stick</label>\n" +
        "  </div>\n" +
        "  <div id=\"btn-settings\" style=\"position:absolute; top:10px; right:10px; z-index:100; width:40px; height:40px; background:rgba(255,255,255,0.2); border:2px solid white; border-radius:50%; color:white; text-align:center; line-height:40px; font-size:20px;\">⚙</div>\n" +
        
        "  <!-- D-Pad -->\n" +
        "  <div class=\"dpad\" id=\"dpad\">\n" +
        "    <div class=\"dpad-btn\" id=\"d-up\" style=\"top:0;left:50px;\">▲</div>\n" +
        "    <div class=\"dpad-btn\" id=\"d-down\" style=\"bottom:0;left:50px;\">▼</div>\n" +
        "    <div class=\"dpad-btn\" id=\"d-left\" style=\"top:50px;left:0;\">◀</div>\n" +
        "    <div class=\"dpad-btn\" id=\"d-right\" style=\"top:50px;right:0;\">▶</div>\n" +
        "  </div>\n" +
        
        "  <!-- Analog Stick -->\n" +
        "  <div id=\"analog-stick\" style=\"position:absolute; bottom:50px; left:150px; width:120px; height:120px; background:rgba(255,255,255,0.1); border:2px solid rgba(255,255,255,0.5); border-radius:50%; touch-action:none;\">\n" +
        "    <div id=\"analog-knob\" style=\"position:absolute; top:35px; left:35px; width:50px; height:50px; background:rgba(255,255,255,0.5); border-radius:50%;\"></div>\n" +
        "  </div>\n" +
        
        "  <!-- A, B, C Buttons -->\n" +
        "  <div class=\"action-btn\" id=\"btn-a\" style=\"bottom:100px; right:60px; width:60px; height:60px;\">A</div>\n" +
        "  <div class=\"action-btn\" id=\"btn-b\" style=\"bottom:50px; right:130px; width:60px; height:60px;\">B</div>\n" +
        "  <div class=\"action-btn\" id=\"btn-z\" style=\"bottom:30px; right:60px; width:60px; height:60px;\">Z</div>\n" +
        
        "  <!-- C Buttons -->\n" +
        "  <div class=\"action-btn c-btn\" id=\"c-up\" style=\"bottom:180px; right:70px; width:40px; height:40px; font-size:14px;\">C▲</div>\n" +
        "  <div class=\"action-btn c-btn\" id=\"c-down\" style=\"bottom:100px; right:130px; width:40px; height:40px; font-size:14px;\">C▼</div>\n" +
        "  <div class=\"action-btn c-btn\" id=\"c-left\" style=\"bottom:140px; right:160px; width:40px; height:40px; font-size:14px;\">C◀</div>\n" +
        "  <div class=\"action-btn c-btn\" id=\"c-right\" style=\"bottom:140px; right:100px; width:40px; height:40px; font-size:14px;\">C▶</div>\n" +
        
        "  <!-- L, R, Start -->\n" +
        "  <div class=\"action-btn\" id=\"btn-l\" style=\"top:10px; left:10px; width:80px; height:40px;\">L</div>\n" +
        "  <div class=\"action-btn\" id=\"btn-r\" style=\"top:10px; right:60px; width:80px; height:40px;\">R</div>\n" +
        "  <div class=\"action-btn\" id=\"btn-start\" style=\"top:10px; left:50%; transform:translateX(-50%); width:80px; height:40px;\">START</div>\n" +
        "</div>\n" +
        "<style>\n" +
        "  body { margin: 0; overflow: hidden; background: black; touch-action: none; }\n" +
        "  #ui-container { position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; pointer-events: none; }\n" +
        "  .dpad-btn, .action-btn { position: absolute; background: rgba(255,255,255,0.2); border: 2px solid rgba(255,255,255,0.6); color: white; display: flex; justify-content: center; align-items: center; border-radius: 10px; font-weight: bold; font-family: sans-serif; pointer-events: auto; user-select: none; -webkit-user-select: none; cursor: pointer; }\n" +
        "  .dpad { position: absolute; bottom: 20px; left: 20px; width: 150px; height: 150px; }\n" +
        "  .dpad-btn { width: 50px; height: 50px; font-size: 20px; }\n" +
        "  .action-btn { width: 60px; height: 60px; font-size: 24px; border-radius: 50%; }\n" +
        "  .dpad-btn:active, .action-btn:active { background: rgba(255,255,255,0.6); }\n" +
        "</style>\n" +
        "<script>\n" +
        "  let btnState = 0;\n" +
        "  let stickX = 0, stickY = 0;\n" +
        "  let cStickMode = false;\n" +
        "  const send = () => { fetch('/input?btn=' + btnState.toString(16) + '&sx=' + stickX + '&sy=' + stickY).catch(e => console.log(e)); };\n" +
        "  \n" +
        "  const bindBtn = (id, mask) => {\n" +
        "    const el = document.getElementById(id);\n" +
        "    if (!el) return;\n" +
        "    el.addEventListener('pointerdown', (e) => { e.preventDefault(); btnState |= mask; send(); el.setPointerCapture(e.pointerId); });\n" +
        "    el.addEventListener('pointerup', (e) => { e.preventDefault(); el.releasePointerCapture(e.pointerId); setTimeout(() => { btnState &= ~mask; send(); }, 100); });\n" +
        "    el.addEventListener('pointercancel', (e) => { e.preventDefault(); btnState &= ~mask; send(); });\n" +
        "  };\n" +
        "  \n" +
        "  // N64 Button Masks\n" +
        "  bindBtn('d-up', 0x0800); bindBtn('d-down', 0x0400); bindBtn('d-left', 0x0200); bindBtn('d-right', 0x0100);\n" +
        "  bindBtn('btn-a', 0x8000); bindBtn('btn-b', 0x4000); bindBtn('btn-z', 0x2000); bindBtn('btn-start', 0x1000);\n" +
        "  bindBtn('btn-l', 0x0020); bindBtn('btn-r', 0x0010);\n" +
        "  const bindC = () => { bindBtn('c-up', 0x0008); bindBtn('c-down', 0x0004); bindBtn('c-left', 0x0002); bindBtn('c-right', 0x0001); };\n" +
        "  bindC();\n" +
        "  \n" +
        "  const menu = document.getElementById('settings-menu');\n" +
        "  document.getElementById('btn-settings').addEventListener('pointerdown', (e) => { e.preventDefault(); menu.style.display = menu.style.display === 'none' ? 'block' : 'none'; });\n" +
        "  document.getElementById('toggle-cstick').addEventListener('change', (e) => { cStickMode = e.target.checked; });\n" +
        "  \n" +
        "  const stick = document.getElementById('analog-stick');\n" +
        "  const knob = document.getElementById('analog-knob');\n" +
        "  let stickCenter = { x: 60, y: 60 };\n" +
        "  let stickActive = false;\n" +
        "  stick.addEventListener('pointerdown', (e) => { e.preventDefault(); stickActive = true; stick.setPointerCapture(e.pointerId); updateStick(e); });\n" +
        "  stick.addEventListener('pointermove', (e) => { if (stickActive) updateStick(e); });\n" +
        "  stick.addEventListener('pointerup', (e) => { e.preventDefault(); stickActive = false; knob.style.transform = ''; stickX = 0; stickY = 0; send(); });\n" +
        "  stick.addEventListener('pointercancel', (e) => { e.preventDefault(); stickActive = false; knob.style.transform = ''; stickX = 0; stickY = 0; send(); });\n" +
        "  function updateStick(e) {\n" +
        "    const rect = stick.getBoundingClientRect();\n" +
        "    let dx = e.clientX - (rect.left + rect.width / 2);\n" +
        "    let dy = e.clientY - (rect.top + rect.height / 2);\n" +
        "    const dist = Math.sqrt(dx*dx + dy*dy);\n" +
        "    const max = 50;\n" +
        "    if (dist > max) { dx = dx / dist * max; dy = dy / dist * max; }\n" +
        "    knob.style.transform = 'translate(' + dx + 'px, ' + dy + 'px)';\n" +
        "    if (cStickMode) {\n" +
        "      if (dx < -15) btnState |= 0x0002; else btnState &= ~0x0002;\n" +
        "      if (dx > 15) btnState |= 0x0001; else btnState &= ~0x0001;\n" +
        "      if (dy < -15) btnState |= 0x0008; else btnState &= ~0x0008;\n" +
        "      if (dy > 15) btnState |= 0x0004; else btnState &= ~0x0004;\n" +
        "      stickX = 0; stickY = 0;\n" +
        "    } else {\n" +
        "      stickX = Math.round(dx / max * 127); stickY = Math.round(-dy / max * 127);\n" +
        "    }\n" +
        "    send();\n" +
        "  }\n" +
        "</script>\n" +
        
        "<canvas id='screen' style='position:absolute;top:0;left:0;border:2px solid yellow;z-index:1;'></canvas>\n" +
        "<div id='hud' style='position:fixed;left:8px;top:8px;color:#0f0;font:12px monospace;background:rgba(0,0,0,.7);padding:4px 8px;border-radius:4px;pointer-events:none;z-index:10;white-space:pre;'>waiting</div>\n" +
        "<script>\n" +
        "var W=" + W + ",H=" + H + ";\n" +
        "var cv=document.getElementById('screen');\n" +
        "var hud=document.getElementById('hud');\n" +
        "cv.width=W;cv.height=H;\n" +
        "var cx=cv.getContext('2d');\n" +
        "var im=cx.createImageData(W,H);\n" +
        "for(var k=0;k<im.data.length;k+=4){im.data[k]=80;im.data[k+1]=0;im.data[k+2]=80;im.data[k+3]=255;}\n" +
        "cx.putImageData(im,0,0);\n" +
        "hud.textContent='W='+W+' H='+H+' need='+(W*H*4);\n" +
        "var buf=new Uint8Array(0);\n" +
        "var frameCount=0;\n" +
        "var lastRead=0;\n" +
        "var badLen=0;\n" +
        "function tick(){\n" +
        "fetch('/stream?'+Date.now(),{cache:'no-store'}).then(function(r){\n" +
        "var rd=r.body.getReader();var need=W*H*4;\n" +
        "function pump(){rd.read().then(function(x){\n" +
        "if(x.done)return;\n" +
        "lastRead=x.value.length;\n" +
        "var nb=new Uint8Array(buf.length+x.value.length);\n" +
        "nb.set(buf,0);nb.set(x.value,buf.length);buf=nb;\n" +
        "while(buf.length>=4){\n" +
        "  var len=(buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];\n" +
        "  if(buf.length<4+len)break;\n" +
        "  var frame=buf.subarray(4,4+len);\n" +
        "  if(frame.length===need){im.data.set(frame);cx.putImageData(im,0,0);frameCount++;}\n" +
        "  else {badLen++;}\n" +
        "  buf=buf.subarray(4+len);\n" +
        "}\n" +
        "hud.textContent='frames='+frameCount+' badLen='+badLen+' bufRemain='+buf.length+' lastRead='+lastRead+' need='+need;\n" +
        "pump();}).catch(function(e){hud.textContent='pumpErr: '+e;});}\n" +
        "pump();\n" +
        "}).catch(function(e){hud.textContent='fetchErr: '+e;setTimeout(tick,2000);});\n" +
        "}\n" +
        "tick();\n" +
        "</script>\n" +
        "</body>\n" +
        "</html>\n";
}
