package com.dkrwrapper;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

import java.io.File;

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

        ensureExtracted();
        startBkaService();
        finish();
    }

    private void ensureExtracted() {
        try {
            File filesDir = getFilesDir();
            File extractorDir = new File(filesDir, "extractor");
            File pngCheck = new File(extractorDir, "assets/.vanilla/us.v80/textures/3d");
            if (pngCheck.exists() && pngCheck.list() != null && pngCheck.list().length > 0) {
                Log.i(TAG, "PNGs already extracted at " + pngCheck.getAbsolutePath());
                return;
            }
            Log.i(TAG, "First launch — extracting assets on-device");

            File toolsDir = new File(extractorDir, "tools");
            toolsDir.mkdirs();
            File tool = new File(toolsDir, "dkr_assets_tool");
            copyAsset("extractor/tools/dkr_assets_tool", tool);
            tool.setExecutable(true);
            copyAsset("extractor/tools/dkr_assets_tool_extract.json",
                      new File(toolsDir, "dkr_assets_tool_extract.json"));
            copyAsset("extractor/tools/dkr_assets_tool_settings.json",
                      new File(toolsDir, "dkr_assets_tool_settings.json"));

            AssetManager am = getAssets();
            File extractVerDir = new File(extractorDir, "extract-ver");
            extractVerDir.mkdirs();
            for (String fn : am.list("extractor/extract-ver")) {
                copyAsset("extractor/extract-ver/" + fn, new File(extractVerDir, fn));
            }

            File baseromsDir = new File(extractorDir, "baseroms");
            baseromsDir.mkdirs();
            File romDst = new File(baseromsDir, "baserom.us.v80.z64");
            if (!romDst.exists() || romDst.length() < 1024) {
                File romSrc = new File(filesDir, "auto_rom.z64");
                if (!romSrc.exists()) {
                    Log.e(TAG, "auto_rom.z64 not found — cannot extract");
                    return;
                }
                copyFile(romSrc, romDst);
            }

            Log.i(TAG, "Running extractor (this takes ~15s)");
            Process p = new ProcessBuilder(tool.getAbsolutePath(), "extract", "-dkrv", "us.v80")
                .directory(extractorDir)
                .redirectErrorStream(true)
                .start();
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
            String line;
            while ((line = br.readLine()) != null) {
                Log.i(TAG, "extractor: " + line);
            }
            int rc = p.waitFor();
            Log.i(TAG, "Extractor done rc=" + rc);
            int count = pngCheck.exists() && pngCheck.list() != null ? pngCheck.list().length : 0;
            Log.i(TAG, "PNG dir count: " + count);
        } catch (Exception e) {
            Log.e(TAG, "ensureExtracted failed: " + e);
        }
    }

    private void copyAsset(String assetPath, File dst) throws Exception {
        try (InputStream in = getAssets().open(assetPath);
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }

    private void copyFile(File src, File dst) throws Exception {
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
