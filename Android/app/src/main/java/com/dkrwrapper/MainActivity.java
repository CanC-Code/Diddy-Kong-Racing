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

        startBkaService();
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
