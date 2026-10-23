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
            startForeground(NOTIF_ID, n);
        } catch (Exception e) {
            Log.w(TAG, "startForeground denied (background restart): " + e.getMessage());
        }

        // Start HTTP server first so Chrome has something to connect to
        if (server == null) {
            try {
                server = new StreamServer();
                Thread t = new Thread(() -> server.serve(), "BKA-Stream-Accept");
                t.setDaemon(true);
                t.start();
            } catch (Exception e) {
                Log.e(TAG, "server start failed", e);
                return START_NOT_STICKY;
            }
        }

        // Launch Chrome
        // Skip Chrome when NativeGameActivity (native window launcher) started us.

        // NativeGameActivity renders to GameView; opening Chrome would background it and kill SurfaceView.

        boolean noChrome = getIntent().getBooleanExtra("noChrome", false);

        if (!noChrome) {

            Intent chrome = new Intent(Intent.ACTION_VIEW, Uri.parse("http://127.0.0.1:8081/"));

        }
        chrome.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            startActivity(chrome);
        } catch (Exception e) {
            Log.e(TAG, "chrome launch failed", e);
        }

        // Register for OtrService broadcasts
        IntentFilter f = new IntentFilter();
        f.addAction(OtrService.ACTION_OTR_COMPLETE);
        f.addAction(OtrService.ACTION_OTR_ERROR);
        LocalBroadcastManager.getInstance(this).registerReceiver(extractReceiver, f);

        // Decide what to do: extract if needed, else boot straight to engine
        File baseBin = new File(getFilesDir(), "rom_base.bin");
        if (baseBin.exists() && baseBin.length() > 0) {
            Log.i(TAG, "rom_base.bin present (" + baseBin.length() + " bytes)");
            startEngineWhenReady();
        } else if (!extractionLaunched) {
            File rom = findRom();
            if (rom != null) {
                Log.i(TAG, "extraction needed; ROM at " + rom.getAbsolutePath());
                launchExtraction(rom);
            } else {
                Log.e(TAG, "no ROM found in any known location");
                notifyUser("Place auto_rom.z64 in app files, then relaunch");
            }
        }

        return START_NOT_STICKY;
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

    private void startEngineWhenReady() {
        if (engineStarted) return;
        engineStarted = true;
        new Thread(() -> {
            try { Thread.sleep(800); } catch (InterruptedException ignored) {}
            Log.i(TAG, "starting engine");
            Native.startEngine(getFilesDir().getAbsolutePath());
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
    public void onDestroy() {
        try { LocalBroadcastManager.getInstance(this).unregisterReceiver(extractReceiver); }
        catch (Exception ignored) {}
        if (server != null) server.stop();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
