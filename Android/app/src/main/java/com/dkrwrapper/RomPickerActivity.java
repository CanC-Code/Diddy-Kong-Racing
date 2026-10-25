package com.dkrwrapper;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * One-shot picker activity. Shows system file picker, copies the selected
 * file to filesDir/auto_rom.z64, then starts BkaService.
 * Finishes without starting service if user cancels.
 */
public class RomPickerActivity extends Activity {
    private static final String TAG = "BKA-RomPicker";
    private static final int REQ_PICK_ROM = 0xDB0B;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "picker onCreate");

        // If ROM is already there (e.g. user re-launches), skip picker.
        File romFile = new File(getFilesDir(), "auto_rom.z64");
        if (romFile.exists() && romFile.length() > 1024) {
            Log.i(TAG, "ROM already present, starting service");
            startBkaService();
            finish();
            return;
        }

        Intent pick = new Intent(Intent.ACTION_GET_CONTENT);
        pick.setType("*/*");
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        try {
            startActivityForResult(Intent.createChooser(pick, "Select DKR ROM (.z64)"), REQ_PICK_ROM);
        } catch (Exception e) {
            Log.e(TAG, "startActivityForResult failed: " + e.getMessage());
            finish();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK_ROM) return;

        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            Log.w(TAG, "user cancelled");
            finish();
            return;
        }

        Uri uri = data.getData();
        try (InputStream in = getContentResolver().openInputStream(uri);
             FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "auto_rom.z64"))) {
            if (in == null) { Log.e(TAG, "null input"); finish(); return; }
            byte[] buf = new byte[65536];
            int n; long total = 0;
            while ((n = in.read(buf)) > 0) { out.write(buf, 0, n); total += n; }
            Log.i(TAG, "copied " + total + " bytes");
            if (total < 1024) {
                Log.e(TAG, "ROM too small, deleting");
                new File(getFilesDir(), "auto_rom.z64").delete();
                finish();
                return;
            }
            startBkaService();
        } catch (Exception e) {
            Log.e(TAG, "copy failed: " + e.getMessage());
        }
        finish();
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
