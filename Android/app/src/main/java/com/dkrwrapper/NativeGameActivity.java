package com.dkrwrapper;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

public class NativeGameActivity extends Activity {
    static { System.loadLibrary("dkrwrapper"); }

    

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Content view MUST be set before touching window insets —
        // the decor view doesn't exist until this call.
        GameView gv = new GameView(this);
        // --- r140 START_TEST: Wrap GameView to allow overlay UI ---
        android.widget.FrameLayout rootLayout = new android.widget.FrameLayout(this);
        rootLayout.addView(gv);
        
        android.widget.Button startBtn = new android.widget.Button(this);
        startBtn.setText("START");
        startBtn.setAlpha(0.7f);
        startBtn.setBackgroundColor(0xFFFF0000); // Red for visibility
        
        android.widget.FrameLayout.LayoutParams params = new android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.WRAP_CONTENT,
                android.widget.FrameLayout.LayoutParams.WRAP_CONTENT
        );
        params.gravity = android.view.Gravity.TOP | android.view.Gravity.END;
        params.setMargins(50, 100, 50, 50);
        
        startBtn.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, android.view.MotionEvent event) {
                if (event.getAction() == android.view.MotionEvent.ACTION_DOWN) {
                    NativeBridge.setVirtualInput(0x1000, 0, 0); // N64 START_BUTTON
                    android.util.Log.d("BKA-Native", "VirtualInput: START DOWN");
                } else if (event.getAction() == android.view.MotionEvent.ACTION_UP) {
                    NativeBridge.setVirtualInput(0, 0, 0);
                    android.util.Log.d("BKA-Native", "VirtualInput: START UP");
                }
                return false;
            }
        });
        
        rootLayout.addView(startBtn, params);
        setContentView(rootLayout);
        // --------------------------------------


        final View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
              View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

        decor.post(new Runnable() {
            @Override public void run() {
                if (Build.VERSION.SDK_INT >= 30) {
                    WindowInsetsController c = decor.getWindowInsetsController();
                    if (c != null) {
                        c.hide(WindowInsets.Type.systemBars());
                        c.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    }
                }
            }
        });

        // Start BkaService which handles: extraction (first launch), engine
        // boot (background thread, keeps UI responsive), and keeps process
        // alive as a foreground service. GameView above renders RDRAM frames.
        try {
            android.content.Intent svc = new android.content.Intent(this, BkaService.class);
            svc.putExtra("noChrome", true);
            if (Build.VERSION.SDK_INT >= 26) {
                startForegroundService(svc);
            } else {
                startService(svc);
            }
        } catch (Exception e) {
            android.util.Log.w("BKA-Native", "startForegroundService failed: " + e.getMessage());
        }
    }
}
