package com.dkrwrapper;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;

/**
 * Bootstrap. Uses Theme.NoDisplay (Motorola libgui UAF workaround) and
 * finishes immediately. If the ROM is missing, launches RomPickerActivity
 * so the user can choose a .z64 via the system file picker.
 */
public class MainActivity extends Activity {
    private static final String TAG = "BKA-Main";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "bootstrap");

        File romFile = new File(getFilesDir(), "auto_rom.z64");
        if (!romFile.exists() || romFile.length() < 1024) {
            Log.w(TAG, "ROM missing or too small — launching picker");
            Intent picker = new Intent(this, RomPickerActivity.class);
            picker.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try { startActivity(picker); } catch (Exception e) {
                Log.e(TAG, "picker launch failed: " + e.getMessage());
            }
            finish();
            return;
        }

        startBkaService();
        finish();
    }

    static void ensureExtracted(android.content.Context ctx) {
    try {
        File filesDir = ctx.getFilesDir();
        File extractorDir = new File(filesDir, "extractor");
        File pngCheck = new File(extractorDir, "assets/.vanilla/us.v80/textures/3d");
        if (pngCheck.exists() && pngCheck.list() != null && pngCheck.list().length > 0) {
            android.util.Log.i("BKA-Main", "PNGs already present at " + pngCheck.getAbsolutePath());
            return;
        }
        android.util.Log.i("BKA-Main", "First launch — on-device extraction starting");

        // 1. Copy tool + configs to app data
        File toolsDir = new File(extractorDir, "tools");
        toolsDir.mkdirs();
        File tool = new File(toolsDir, "dkr_assets_tool");
        if (!tool.exists() || tool.length() < 1024) {
            copyAsset(ctx, "extractor/tools/dkr_assets_tool", tool);
            tool.setExecutable(true, false);
            android.util.Log.i("BKA-Main", "Copied tool len=" + tool.length() + " exec=" + tool.canExecute());
        }
        copyAsset(ctx, "extractor/tools/dkr_assets_tool_extract.json",
                  new File(toolsDir, "dkr_assets_tool_extract.json"));
        copyAsset(ctx, "extractor/tools/dkr_assets_tool_settings.json",
                  new File(toolsDir, "dkr_assets_tool_settings.json"));

        // 2. Copy extract-ver configs
        File extractVerDir = new File(extractorDir, "extract-ver");
        extractVerDir.mkdirs();
        android.content.res.AssetManager am = ctx.getAssets();
        for (String fn : am.list("extractor/extract-ver")) {
            copyAsset(ctx, "extractor/extract-ver/" + fn, new File(extractVerDir, fn));
        }

        // 3. Copy ROM to baseroms/
        File baseromsDir = new File(extractorDir, "baseroms");
        baseromsDir.mkdirs();
        File romDst = new File(baseromsDir, "baserom.us.v80.z64");
        if (!romDst.exists() || romDst.length() < 1024) {
            File romSrc = new File(filesDir, "auto_rom.z64");
            if (!romSrc.exists()) {
                android.util.Log.e("BKA-Main", "auto_rom.z64 missing — cannot extract");
                return;
            }
            copyFile(romSrc, romDst);
            android.util.Log.i("BKA-Main", "ROM copied len=" + romDst.length());
        }

        // 4. Create marker dirs so find_decomp_root() succeeds
        for (String marker : new String[]{"src", "include", "asm", "assets"}) {
            new File(extractorDir, marker).mkdirs();
        }

        // 5. LAUNCH VIA LINKER64 — /data/data/<pkg> is noexec on Android 10+
        File toolLog = new File(extractorDir, "extractor.log");
        android.util.Log.i("BKA-Main", "Launching linker64 " + tool.getAbsolutePath()
                + " → log " + toolLog.getAbsolutePath());

        Process p = new ProcessBuilder(
                "/system/bin/linker64",
                tool.getAbsolutePath(),
                "extract", "-dkrv", "us.v80")
            .directory(extractorDir)
            .redirectErrorStream(true)
            .redirectOutput(ProcessBuilder.Redirect.to(toolLog))
            .start();

        int rc = p.waitFor();
        int pngCount = 0;
        if (pngCheck.exists() && pngCheck.list() != null) pngCount = pngCheck.list().length;
        android.util.Log.i("BKA-Main", "Extractor rc=" + rc + " PNG dir count=" + pngCount);

        // 6. Dump first 30 lines of extractor.log to logcat for diagnosis
        if (toolLog.exists()) {
            java.io.BufferedReader br = new java.io.BufferedReader(
                new java.io.FileReader(toolLog));
            String line;
            int n = 0;
            while ((line = br.readLine()) != null && n < 30) {
                android.util.Log.i("BKA-ExtractLog", line);
                n++;
            }
            br.close();
        }
    } catch (Throwable t) {
        android.util.Log.e("BKA-Main", "ensureExtracted failed", t);
    }
}

    static void copyAsset(android.content.Context ctx, String assetPath, File dst) throws Exception {
        try (InputStream in = ctx.getAssets().open(assetPath);
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }

    static void copyFile(File src, File dst) throws Exception {
        try (InputStream in = new java.io.FileInputStream(src);
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }

    private void startBkaService() {
        Intent svc = new Intent(this, BkaService.class);
        if (Build.VERSION.SDK_INT >= 26) {
            try { startForegroundService(svc); }
            catch (Exception e) { Log.w(TAG, "startForegroundService failed: " + e.getMessage()); }
        } else {
            startService(svc);
        }
    }
}
