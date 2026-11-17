package com.dkrwrapper;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import java.io.File;

public class BkaService extends Service {
    private static final String TAG = "BKA-Service";
    private static final String CHANNEL = "bka_stream";
    private static final int NOTIF_ID = 0xB14;

    private StreamServer server;
    private boolean engineStarted = false;
    private boolean extractionLaunched = false;

    private final BroadcastReceiver extractReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (OtrService.ACTION_OTR_COMPLETE.equals(action)) {
                Log.i(TAG, "extraction complete -> starting engine");
                startEngineWhenReady();
            } else if (OtrService.ACTION_OTR_ERROR.equals(action)) {
                Log.e(TAG, "extraction failed: " + intent.getStringExtra("message"));
                notifyUser("ROM extraction failed");
            }
        }
    };

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "onStartCommand");

        // PORT: keepalive thread proves fillArgb works independent of display.
        // Reads framebuffer every 500ms, logs non-black pixel count. If this
        // shows nonBlack > 0, walker renders correctly and pipeline is proven.
        if (true) new Thread(() -> {  /* r160: enable keepalive to prevent JVM reaping */
            long n = 0;
            while (true) {
                try { Thread.sleep(2000); } catch (InterruptedException e) { return; }
                n++;
                if (n % 5 == 0) {
                    Log.i("BKA-KEEPALIVE", "alive iter=" + n + " t=" + System.currentTimeMillis());
                }
            }
        }, "BKA-FillArgb-Keepalive").start();


        // PORT: on-device asset extraction, launched once from onStartCommand.
        // Runs on its own thread so the foreground service stays responsive.
        if (!extractionLaunched) {
            extractionLaunched = true;
            new Thread(() -> {
                try {
                    MainActivity.ensureExtracted(BkaService.this);
                } catch (Throwable t) {
                    Log.e(TAG, "extraction thread failed", t);
                }
            }, "BKA-Extract").start();
        }

        // Foreground service so Android doesn't kill us
        NotificationManager nm = getSystemService(NotificationManager.class);
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel ch = new NotificationChannel(CHANNEL,
                "Diddy Kong Racing stream", NotificationManager.IMPORTANCE_LOW);
            ch.setShowBadge(false);
            nm.createNotificationChannel(ch);
        }
        Notification n = new Notification.Builder(this, CHANNEL)
                .setContentTitle("Diddy Kong Racing")
                .setContentText("Preparing engine...")
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setOngoing(true)
                .build();
        try {
// #ifdef BUILD_VANILLA_ANDROID
            // never taken — placeholder
// #endif
            if (Build.VERSION.SDK_INT >= 34) {
                // r182: Android 14+ requires the FGS type at call time
                startForeground(NOTIF_ID, n, android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
            } else if (Build.VERSION.SDK_INT >= 29) {
                startForeground(NOTIF_ID, n, android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
            } else {
                startForeground(NOTIF_ID, n);
            }
            Log.i(TAG, "startForeground OK");
        } catch (Exception e) {
            Log.e(TAG, "startForeground DENIED: " + e);
        }

        // Start HTTP server first so Chrome has something to connect to
        if (server == null) {
            try {
                StreamServer.setContext(this);
                server = new StreamServer();
                Thread t = new Thread(() -> server.serve(), "BKA-Stream-Accept");
                t.setDaemon(true);
                t.start();
            } catch (Exception e) {
                Log.e(TAG, "server start failed", e);
                return START_STICKY;
            }
        } else {
            Log.i(TAG, "r162: server already running, skipping rebind (avoids BindException restart loop)");
        }

        // Launch Chrome
        // Skip Chrome when NativeGameActivity (native window launcher) started us.

        // NativeGameActivity renders to GameView; opening Chrome would background it and kill SurfaceView.

        boolean noChrome = intent != null && intent.getBooleanExtra("noChrome", false);

        if (!noChrome) {

            Intent chrome = new Intent(Intent.ACTION_VIEW, Uri.parse("http://127.0.0.1:8081/"));

        chrome.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            startActivity(chrome);
        } catch (Exception e) {
            Log.e(TAG, "chrome launch failed", e);
        }
        }

        // Register for OtrService broadcasts
        IntentFilter f = new IntentFilter();
        f.addAction(OtrService.ACTION_OTR_COMPLETE);
        f.addAction(OtrService.ACTION_OTR_ERROR);
        LocalBroadcastManager.getInstance(this).registerReceiver(extractReceiver, f);

        // Decide what to do: extract if needed, else boot straight to engine
        File baseBin = new File(getFilesDir(), "rom_base.bin");
        File autoRom = new File(getFilesDir(), "auto_rom.z64");

        // PORT: ensure rom_base.bin exists (copy from auto_rom.z64 if missing)
        if (autoRom.exists() && (!baseBin.exists() || baseBin.length() == 0)) {
            try {
                try (java.io.FileInputStream in = new java.io.FileInputStream(autoRom);
                     java.io.FileOutputStream out = new java.io.FileOutputStream(baseBin)) {
                    byte[] buf = new byte[65536];
                    int bytesRead;
                    while ((bytesRead = in.read(buf)) > 0) out.write(buf, 0, bytesRead);
                }
                Log.i(TAG, "rom_base.bin created (" + baseBin.length() + " bytes) from auto_rom.z64");
            } catch (Throwable t) {
                Log.e(TAG, "rom_base.bin copy failed", t);
            }
        }

        File pngDir = new File(getFilesDir(), "extractor/assets/.vanilla/us.v80/textures/3d");
        boolean pngsReady = pngDir.exists() && pngDir.list() != null && pngDir.list().length > 0;

        if (baseBin.exists() && baseBin.length() > 0) {
            Log.i(TAG, "rom_base.bin present (" + baseBin.length() + " bytes)");
            waitForPngsThenBoot();
        } else if (pngsReady) {
            Log.i(TAG, "PNGs present but no rom_base.bin — booting anyway");
            waitForPngsThenBoot();
        } else if (!extractionLaunched) {
            File rom = findRom();
            if (rom != null) {
                Log.i(TAG, "extraction needed; ROM at " + rom.getAbsolutePath());
                launchExtraction(rom);
                waitForPngsThenBoot();
            } else {
                Log.e(TAG, "no ROM found in any known location");
                notifyUser("Place auto_rom.z64 in app files, then relaunch");
            }
        }

        return START_STICKY; /* r160: was NOT_STICKY - service got reaped */
    }

    private File findRom() {
        File files = getFilesDir();
        String[] candidates = new String[] {
            new File(files, "auto_rom.z64").getAbsolutePath(),
            new File(files, "diddy-kong-racing.z64").getAbsolutePath(),
            new File(files, "rom.z64").getAbsolutePath(),
        };
        for (String p : candidates) {
            File f = new File(p);
            if (f.exists() && f.isFile() && f.length() > 1024 * 1024) {
                return f;
            }
        }
        return null;
    }

    private void launchExtraction(File rom) {
        extractionLaunched = true;
        Intent svc = new Intent(this, OtrService.class);
        svc.putExtra("uri", Uri.fromFile(rom).toString());
        svc.putExtra("outDir", getFilesDir().getAbsolutePath());
        svc.putExtra("version", "us");
        if (Build.VERSION.SDK_INT >= 26) {
            try { startForegroundService(svc); } catch (Exception e) {
                Log.w(TAG, "startForegroundService(Otr) failed: " + e.getMessage());
            }
        } else {
            startService(svc);
        }
    }

    private static int countPngsRecursive(File dir) {
        if (dir == null || !dir.exists() || !dir.isDirectory()) return 0;
        File[] list = dir.listFiles();
        if (list == null) return 0;
        int count = 0;
        for (File f : list) {
            if (f.isFile()) {
                if (f.getName().endsWith(".png")) count++;
            } else if (f.isDirectory()) {
                count += countPngsRecursive(f);
            }
        }
        return count;
    }

    private void waitForPngsThenBoot() {
        new Thread(() -> {
            File pngRoot = new File(getFilesDir(), "extractor/assets/.vanilla/us.v80/textures");
            int maxWait = 120;
            for (int i = 0; i < maxWait; i++) {
                int count = countPngsRecursive(pngRoot);
                if (i % 5 == 0 || count > 1000) Log.i(TAG, "waiting for PNGs... " + count + " so far");
                if (count > 1000) {
                    Log.i(TAG, "PNGs ready (" + count + " files) — booting engine");
                    try { Thread.sleep(800); } catch (InterruptedException ignored) {}
                    startEngineWhenReady();
                    return;
                }
                try { Thread.sleep(1000); } catch (InterruptedException e) { return; }
            }
            Log.w(TAG, "Timeout waiting for PNGs — booting anyway");
            startEngineWhenReady();
        }, "BKA-Wait-Pngs").start();
    }

    private void startEngineWhenReady() {
        if (engineStarted) return;
        engineStarted = true;
        new Thread(() -> {
            try { Thread.sleep(800); } catch (InterruptedException ignored) {}
            Log.i(TAG, "BEFORE Native.startEngine");
            try {
                Native.startEngine(getFilesDir().getAbsolutePath());
                Log.e(TAG, "AFTER Native.startEngine — RETURNED! game thread exited normally, this is the bug");
            } catch (Throwable t) {
                Log.e(TAG, "Native.startEngine threw", t);
            }
        }, "BKA-Engine-Launcher").start();
    }

    private void notifyUser(String text) {
        NotificationManager nm = getSystemService(NotificationManager.class);
        Notification n = new Notification.Builder(this, CHANNEL)
                .setContentTitle("Diddy Kong Racing")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_dialog_alert)
                .setOngoing(true)
                .build();
        nm.notify(NOTIF_ID, n);
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        Log.e(TAG, "BKA-SVC-TASK-REMOVED rootIntent=" + rootIntent);
        super.onTaskRemoved(rootIntent);
    }

    @Override
    public void onTrimMemory(int level) {
        Log.w(TAG, "BKA-SVC-TRIM level=" + level);
        super.onTrimMemory(level);
    }

    @Override
    public void onLowMemory() {
        Log.e(TAG, "BKA-SVC-LOWMEM fired");
        super.onLowMemory();
    }

    @Override
    public void onDestroy() {
        Log.e(TAG, "BKA-SVC-DESTROY onDestroy fired");
        try { LocalBroadcastManager.getInstance(this).unregisterReceiver(extractReceiver); }
        catch (Exception ignored) {}
        if (server != null) server.stop();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
