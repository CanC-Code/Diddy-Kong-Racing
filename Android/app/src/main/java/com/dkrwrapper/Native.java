package com.dkrwrapper;

public class Native {
    static { System.loadLibrary("dkrwrapper"); }
    public static native void startEngine(String dir);
    public static native void fillArgb(int[] out);
}
