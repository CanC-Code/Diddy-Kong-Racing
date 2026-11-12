
// native_main.cpp — NativeActivity entry and EGL render loop.
#include <android_native_app_glue.h>
#include <jni.h>
extern "C" void init_png_texture_loader(const char* base_path);
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <unistd.h>

extern "C" JNIEXPORT jboolean JNICALL
Java_com_dkrwrapper_Native_isEngineRunning(JNIEnv* env, jclass /*cls*/) {
    extern uint8_t* gN64_RDRAM;
    extern uint8_t* gVideoCurrFramebuffer;
    bool running = (gN64_RDRAM != nullptr) && (gVideoCurrFramebuffer != nullptr);
    return running ? JNI_TRUE : JNI_FALSE;
}


#define LOG_TAG "BKA-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" void bka_surface_ready(int w, int h);
extern "C" void bka_update_texture(int texId);
extern "C" void bka_native_game_boot(const char* dir, AAssetManager* mgr);

static bool g_booted = false;

struct RS {
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLSurface surf = EGL_NO_SURFACE;
    EGLContext ctx = EGL_NO_CONTEXT;
    int w = 0, h = 0;
    bool ready = false;
    GLuint prog = 0, tex = 0, vboV = 0, vboT = 0;
    int posLoc = -1, texLoc = -1, uTex = -1;
    int64_t lastNs = 0;
    int frames = 0;
} g_rs;

static const char* VS =
    "attribute vec4 aPosition;attribute vec2 aTexCoord;"
    "varying vec2 vTexCoord;"
    "void main(){gl_Position=aPosition;vTexCoord=aTexCoord;}";
static const char* FS =
    "precision mediump float;varying vec2 vTexCoord;"
    "uniform sampler2D uTexture;"
    "void main(){gl_FragColor=texture2D(uTexture,vTexCoord);}";

static GLuint mkShader(GLenum t, const char* s) {
    GLuint x = glCreateShader(t);
    glShaderSource(x, 1, &s, nullptr);
    glCompileShader(x);
    GLint ok = 0; glGetShaderiv(x, GL_COMPILE_STATUS, &ok);
    if (!ok) { char l[512]={0}; glGetShaderInfoLog(x,511,nullptr,l); LOGE("shader: %s", l); return 0; }
    return x;
}

static void initGL() {
    GLuint v = mkShader(GL_VERTEX_SHADER, VS);
    GLuint f = mkShader(GL_FRAGMENT_SHADER, FS);
    if (!v || !f) return;
    g_rs.prog = glCreateProgram();
    glAttachShader(g_rs.prog, v); glAttachShader(g_rs.prog, f);
    glLinkProgram(g_rs.prog);
    g_rs.posLoc = glGetAttribLocation(g_rs.prog, "aPosition");
    g_rs.texLoc = glGetAttribLocation(g_rs.prog, "aTexCoord");
    g_rs.uTex   = glGetUniformLocation(g_rs.prog, "uTexture");

    glGenTextures(1, &g_rs.tex);
    glBindTexture(GL_TEXTURE_2D, g_rs.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // DIAG 2026-09-24: bright red placeholder -- if the screen turns red
    // the quad + swap path is working and the failure is in the framebuffer
    // upload.  If it stays green-sliver, the quad is off-screen or the
    // surface is wrong.  Restore to magenta once diagnosed.
    uint8_t placeholder_rgba[4*4*4];
    for (int i = 0; i < 64; i += 4) {
        placeholder_rgba[i+0] = 0xFF;  // R
        placeholder_rgba[i+1] = 0x00;  // G
        placeholder_rgba[i+2] = 0x00;  // B
        placeholder_rgba[i+3] = 0xFF;  // A
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, placeholder_rgba);

    static const float V[8] = { -1,-1,  1,-1, -1,1,  1,1 };
    static const float T[8] = {  0, 1,  1, 1,  0,0,  1,0 };
    glGenBuffers(1, &g_rs.vboV);
    glBindBuffer(GL_ARRAY_BUFFER, g_rs.vboV);
    glBufferData(GL_ARRAY_BUFFER, sizeof(V), V, GL_STATIC_DRAW);
    glGenBuffers(1, &g_rs.vboT);
    glBindBuffer(GL_ARRAY_BUFFER, g_rs.vboT);
    glBufferData(GL_ARRAY_BUFFER, sizeof(T), T, GL_STATIC_DRAW);
}

extern "C" {
extern uint8_t* gN64_RDRAM;
extern uint32_t g_active_fb_offset;
extern volatile int g_bka_pixels_drawn;
}

static ANativeWindow* g_native_window = nullptr;

static void termEGL();   // forward — defined below initEGL
static bool initEGL(ANativeWindow* win) {
    g_native_window = win;
    if (g_rs.dpy != EGL_NO_DISPLAY) termEGL();

    g_rs.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_rs.dpy == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(g_rs.dpy, nullptr, nullptr)) return false;

    const EGLint cfg[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8, EGL_ALPHA_SIZE,8,
        EGL_DEPTH_SIZE,16, EGL_NONE };
    EGLConfig c; EGLint n = 0;
    if (!eglChooseConfig(g_rs.dpy, cfg, &c, 1, &n) || n == 0) return false;

    const EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    g_rs.ctx = eglCreateContext(g_rs.dpy, c, EGL_NO_CONTEXT, ca);
    if (g_rs.ctx == EGL_NO_CONTEXT) return false;

    const EGLint pbAttr[] = { EGL_WIDTH, 320, EGL_HEIGHT, 240, EGL_NONE };
    g_rs.surf = eglCreatePbufferSurface(g_rs.dpy, c, pbAttr);
    if (g_rs.surf == EGL_NO_SURFACE) return false;
    if (!eglMakeCurrent(g_rs.dpy, g_rs.surf, g_rs.surf, g_rs.ctx)) return false;

    initGL();
    g_rs.w = 320; g_rs.h = 240;
    g_rs.ready = true;
    LOGI("EGL pbuffer up 292x216");
    return true;
}

// Destroy only the window surface; keep the EGL context alive.  The
// Android 14 Motorola libgui UAF is triggered by window-handle churn
// (window destroy + create); holding the context across pause/resume
// minimises the number of lifetimes the framework's transaction
// listener has to track.
static void termSurfaceOnly() {
    if (g_rs.dpy == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_rs.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_rs.ctx);
    if (g_rs.surf != EGL_NO_SURFACE) {
        eglDestroySurface(g_rs.dpy, g_rs.surf);
        g_rs.surf = EGL_NO_SURFACE;
    }
    g_rs.ready = false;
}

// Full EGL teardown.  Only called when the process is exiting.
static void termEGL() {
    if (g_rs.dpy == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_rs.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_rs.surf != EGL_NO_SURFACE) eglDestroySurface(g_rs.dpy, g_rs.surf);
    if (g_rs.ctx  != EGL_NO_CONTEXT) eglDestroyContext(g_rs.dpy, g_rs.ctx);
    eglTerminate(g_rs.dpy);
    g_rs.dpy = EGL_NO_DISPLAY; g_rs.surf = EGL_NO_SURFACE; g_rs.ctx = EGL_NO_CONTEXT;
    g_rs.ready = false;
}

static int64_t nowNs() {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000000LL + t.tv_nsec;
}

static void dumpFramebufferPPM(int frame_num) {
    if (!gN64_RDRAM) return;
    uint16_t* fb = (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
    char path[256];
    snprintf(path, sizeof(path), "/data/data/com.dkrwrapper/files/fb_%04d.ppm", frame_num);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n292 240\n255\n");
    for (int i = 0; i < 320 * 240; i++) {
        uint16_t px = fb[i];
        uint8_t r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
        uint8_t g = (px >> 5) & 0x3F;  g = (g << 2) | (g >> 4);
        uint8_t b = px & 0x1F;         b = (b << 3) | (b >> 2);
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
    LOGI("PPM written: %s", path);
}
extern "C" void bka_dump_fb(int n) { dumpFramebufferPPM(n); }

static void renderFrame() {
    if (!g_rs.ready || !g_native_window) return;
    if (!gN64_RDRAM) return;

    int64_t t = nowNs();
    if (g_rs.lastNs && t - g_rs.lastNs < 33000000LL) return;
    g_rs.lastNs = t;

    if (!gN64_RDRAM || g_active_fb_offset > (0x04000000u - (320u*240u*2u))) { static int _fb_guard = 0; if (_fb_guard++ < 5) __android_log_print(ANDROID_LOG_ERROR, "BKA-STREAM", "guard: bad fb_offset=%u rddram=%p", g_active_fb_offset, (void*)gN64_RDRAM); return; }
    uint16_t* src = (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
    if (!gN64_RDRAM || g_active_fb_offset > 0x04000000u - (320u*240u*2u)) { memset(gN64_RDRAM ? (void*)(gN64_RDRAM + 0x1000) : (void*)0, 0, 0); goto fillArgb_skip; }
    fillArgb_skip: (void)0;

    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(g_native_window, &buf, nullptr) == 0) {
        int W = 320, H = 240;
        int dw = buf.width, dh = buf.height;
        int scale = dw / W < dh / H ? dw / W : dh / H;
        if (scale < 1) scale = 1;
        int outW = W * scale, outH = H * scale;
        int ox = (dw - outW) / 2, oy = (dh - outH) / 2;
        uint32_t* dst = (uint32_t*)buf.bits;
        memset(dst, 0, (size_t)dw * dh * 4);
        for (int y = 0; y < outH; y++) {
            int sy = y / scale;
            const uint16_t* srow = src + sy * W;
            uint32_t* drow = dst + (oy + y) * dw + ox;
            for (int x = 0; x < outW; x++) {
                uint16_t px = srow[x / scale];
                uint8_t r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                uint8_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                uint8_t b = px & 0x1F;         b = (b << 3) | (b >> 2);
                drow[x] = 0xFF000000u | (b << 16) | (g << 8) | r;
            }
        }
        ANativeWindow_unlockAndPost(g_native_window);
    }

    if (++g_rs.frames <= 3 || g_rs.frames % 120 == 0) LOGI("frame %d", g_rs.frames);
}

static void onAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                g_native_window = app->window;
                ANativeWindow_setBuffersGeometry(app->window, 0, 0, WINDOW_FORMAT_RGBA_8888);
                initEGL(app->window);
                if (!g_booted) {
                    g_booted = true;
                    const char* dir = app->activity->internalDataPath;
                    LOGI("boot engine dir=%s", dir);
                    bka_native_game_boot(dir, app->activity->assetManager);
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            g_native_window = nullptr;
            break;
        case APP_CMD_WINDOW_RESIZED:
            if (app->window) g_native_window = app->window;
            break;
        default: break;
    }
}

static int32_t onInput(android_app*, AInputEvent*) { return 0; }

// Watchdog: if the render loop never produces frames (SurfFlinger UAF,
// game-thread deadlock, etc.), exit cleanly so the user can relaunch
// without the "app has stopped" dialog.
static void* bka_startup_watchdog(void* arg) {
    android_app* app = (android_app*)arg;
    for (int i = 0; i < 30; i++) {      // up to 15s
        usleep(500000);
        // Not healthy merely because the loop ran — the game must have
        // produced at least one frame with actual pixels.  Check the
        // value that lowlevel_bridge.cpp updates on every upload.
        extern volatile int g_bka_real_frame_count;
        if (g_bka_real_frame_count > 0) return nullptr;
    }
    LOGE("Watchdog: no frames in 12s — exiting for clean retry");
    ANativeActivity_finish(app->activity);
    return nullptr;
}

extern "C" void bka_install_segv_handler(void);

#include <time.h>
static inline int64_t bka_uptime_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void android_main(android_app* app) {
    bka_install_segv_handler();

    // --- PORT: INIT PNG LOADER ---
    init_png_texture_loader("/data/data/com.dkrwrapper/files/extractor/assets/.vanilla/us.v80/");
    // --- END PORT ---
    
    app->onAppCmd = onAppCmd;
    app->onInputEvent = onInput;
    LOGI("android_main enter");

    // Watchdog disabled for diagnostic run
    (void)bka_startup_watchdog;

    for (;;) {
        int events = 0;
        android_poll_source* src = nullptr;
        int timeout = (app->window && g_rs.ready) ? 0 : -1;
        while (ALooper_pollAll(timeout, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) {
                __android_log_print(ANDROID_LOG_FATAL, "BKA-EXIT",
                    "android_main: destroyRequested at uptime_ms=%lld", bka_uptime_ms());
                termEGL();
                return;
            }
            timeout = 0;
        }
        if (app->window && g_rs.ready) {
            { static int _rf = 0; if ((_rf++ & 0xFF) == 0)
                __android_log_print(ANDROID_LOG_INFO, "BKA-RENDER",
                    "renderFrame #%d uptime_ms=%lld", _rf,
                    bka_uptime_ms()); }
            renderFrame();
        }
    }
}


/* PORT: file-scope C-linkage decls for probe counters defined in gfx_interpreter.cpp */
extern "C" int gfx_loadblock_count();
extern "C" int gfx_loadtile_count();
extern "C" int gfx_settimg_count();

extern "C" uint32_t gfx_opcode_hist_get(int i);
extern "C" uint8_t* gfx_tmem_ptr();
extern "C" int gfx_tmem_size();
extern "C" uint32_t pi_dma_count();
extern "C" uint32_t pi_dma_success();
extern "C" uint32_t pi_dma_fail();

static void bka_draw_palette_bar(jint* out, int W, int H) {
    if (!out) return;
    uint8_t* tm = gfx_tmem_ptr();
    if (!tm) return;
    for (int i = 0; i < 16; i++) {
        uint32_t palByte = 2048 + i * 2;
        uint16_t p = ((uint16_t)tm[palByte & 0xFFF] << 8) | tm[(palByte + 1) & 0xFFF];
        uint8_t pr = ((p >> 11) & 0x1F) << 3;
        uint8_t pg = ((p >> 6) & 0x1F) << 3;
        uint8_t pb = ((p >> 1) & 0x1F) << 3;
        jint color = (0xFF << 24) | ((jint)pr << 16) | ((jint)pg << 8) | pb;
        for (int y = 0; y < 8; y++)
            for (int x = i * 16; x < (i + 1) * 16; x++)
                if (x < W && y < H) out[y * W + x] = color;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_dkrwrapper_Native_fillArgb(JNIEnv* env, jobject /*thiz*/, jintArray outArr) {
    { static int _fa = 0; if ((_fa++ & 0xFF) == 0)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-FILLARGB",
            "fillArgb #%d", _fa); }
    extern uint8_t* gN64_RDRAM;
    extern uint32_t g_active_fb_offset;
    extern uint8_t *gVideoLastFramebuffer;
    extern uint8_t *gVideoCurrFramebuffer;

    if (!gN64_RDRAM || !outArr) return;
    jint* out = env->GetIntArrayElements(outArr, nullptr);
    if (!out) return;

    const int W = 320, H = 240;

    if (!gVideoLastFramebuffer || !gVideoCurrFramebuffer) {
        // PORT: gVideo* are null; read from gN64_RDRAM + g_active_fb_offset
        // (the walker's actual write target)
        uint16_t* src_fb = (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
        int nb = 0;
        for (int i = 0; i < W * H; i++) { if (src_fb[i]) nb++; }
        if (nb > 0) {
            static int _fbf = 0;
            if (_fbf++ < 3) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA-FBFALL",
                    "fillArgb using RDRAM+0x%X, %d non-zero pixels", g_active_fb_offset, nb);
            }
            for (int i = 0; i < W * H; i++) {
                uint16_t px = src_fb[i];
                uint8_t r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                uint8_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                uint8_t b = px & 0x1F;         b = (b << 3) | (b >> 2);
                out[i] = (jint)(0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
            }
            bka_draw_palette_bar(out, W, H);
            env->ReleaseIntArrayElements(outArr, out, 0);
            return;
        }
        int len = env->GetArrayLength(outArr);
        memset(out, 0, (size_t)len * sizeof(jint));
        bka_draw_palette_bar(out, W, H);
        env->ReleaseIntArrayElements(outArr, out, 0);
        return;
    }

    /* BKA_DIRECT_READ: gVideo* are the swap-target pointers, but the
     * rasterizer writes to gN64_RDRAM + g_active_fb_offset. Read from
     * g_active_fb_offset directly to eliminate the race (BKA-FB showed
     * 4096 pixels on one buffer while fillArgb read 0). */
    extern uint32_t g_active_fb_offset;
    uint16_t* _direct = (gN64_RDRAM && g_active_fb_offset < 0x04000000u - 320u*240u*2u)
                      ? (uint16_t*)(gN64_RDRAM + g_active_fb_offset) : NULL;
    int _direct_nz = 0;
    if (_direct) for (int _i = 0; _i < 320*240; _i++) if (_direct[_i]) _direct_nz++;

    /* BKA-SRCFIX: RasterizeTriangle writes to gN64_RDRAM + g_active_fb_offset
     * (0x6e3c440000 in run 81 with px_written=6179/triangle). fillArgb was
     * reading from gVideoCurrFramebuffer (0x7b... host heap) which is a
     * DIFFERENT memory region — always empty. Read from RDRAM first, at both
     * framebuffer offsets (0x400000 + 0x440000 double buffer), fall back
     * to gVideo* only if RDRAM pointers are null. */
    extern uint8_t* gN64_RDRAM;
    extern uint32_t g_active_fb_offset;
    uint16_t* src_a = NULL;
    uint16_t* src_b = NULL;
    if (gN64_RDRAM) {
        if (g_active_fb_offset < 0x04000000u)
            src_a = (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
        uint32_t _other = (g_active_fb_offset == 0x440000u) ? 0x400000u : 0x440000u;
        if (_other < 0x04000000u)
            src_b = (uint16_t*)(gN64_RDRAM + _other);
    }
    if (!src_a) src_a = (uint16_t*)gVideoCurrFramebuffer;
    if (!src_b) src_b = (uint16_t*)gVideoLastFramebuffer;
    { static int _sf = 0; if (_sf++ < 5)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-SRCFIX",
            "RDRAM=%p activeOff=0x%X src_a=%p src_b=%p",
            (void*)gN64_RDRAM, (unsigned)g_active_fb_offset,
            (void*)src_a, (void*)src_b); }
    if (_direct_nz > 100) { src_a = _direct; src_b = _direct; }
    { static int _dir = 0; if (_dir++ < 3)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-DIRECT",
            "off=0x%X direct_nz=%d using_direct=%d",
            (unsigned)g_active_fb_offset, _direct_nz, (_direct_nz > 100) ? 1 : 0); }
    { static int _fa = 0; if (_fa++ < 5) __android_log_print(ANDROID_LOG_ERROR, "BKA-STREAM", "fillArgb curr=%p last=%p", (void*)gVideoCurrFramebuffer, (void*)gVideoLastFramebuffer); }
    int nb_a = 0, nb_b = 0;
    for (int i = 0; i < 292 * 216; i++) { if (src_a[i]) nb_a++; if (src_b[i]) nb_b++; }
    /* Pick the framebuffer with more content. Either curr or last can be
     * empty depending on double-buffer state — the one that was just filled
     * has the visible frame. Reading from the empty one yields all-black. */
    uint16_t* src = src_a;
    if (src_a && src_b) {
        int na = 0, nb2 = 0;
        for (int _i = 0; _i < 320*240; _i++) {
            if (src_a[_i]) na++;
            if (src_b[_i]) nb2++;
        }
        if (nb2 > na) src = src_b;
    } else if (src_b) {
        src = src_b;
    }

    /* CRITICAL: Java callers pass int[292*216]=63072 but this code used
     * to write W*H=76800 -> 13728-element heap overflow. Clamp to actual
     * array length to prevent corruption of adjacent StreamServer buffers. */
    int alen = env->GetArrayLength(outArr);
    int out_count = (alen < W * H) ? alen : W * H;
    for (int i = 0; i < out_count; i++) {
        uint16_t px = (i < 320*240 && src) ? src[i] : 0;
        uint8_t r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
        uint8_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
        uint8_t b = px & 0x1F;         b = (b << 3) | (b >> 2);
        out[i] = (jint)(0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
    }

    /* Log what we're actually streaming — colors not just count */
    { static int _pix = 0; if (_pix++ < 5) {
        int bright = 0, dark = 0;
        for (int i = 0; i < out_count; i++) {
            int rr = (out[i] >> 16) & 0xFF;
            int gg = (out[i] >> 8)  & 0xFF;
            int bb = out[i] & 0xFF;
            if (rr > 0x80 || gg > 0x80 || bb > 0x80) bright++;
            else if (rr | gg | bb) dark++;
        }
        __android_log_print(ANDROID_LOG_ERROR, "BKA-PIXELS",
            "alen=%d pick=%s bright=%d dark=%d total=%d",
            alen, (src == src_b) ? "curr" : "last", bright, dark, out_count);
        int shown = 0;
        for (int i = 0; i < out_count && shown < 8; i++) {
            if ((out[i] & 0x00FFFFFF) != 0) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA-PIXELS",
                    "px[%d]=(%d,%d) rgba=0x%08X",
                    i, i%W, i/W, (unsigned)out[i]);
                shown++;
            }
        }
    } }

    /* Cache last visible frame so we never show black to Chrome. */
    {
        static int s_last_nonzero[320*240];
        static int s_have_last = 1; /* BKA-NOCACHE-DIAG */
        int _nz = 0;
        for (int _i = 0; _i < W*H; _i++) if (out[_i] & 0x00FFFFFF) _nz++;
        /* Only cache frames with color variety (game content), not uniform
         * boot-clear white. Reject frames where <500 pixels differ from
         * pixel[0] by >60 total RGB. */
        int _distinct = 0;
        if (_nz >= 100) {
            int r0 = (out[0] >> 16) & 0xFF, g0 = (out[0] >> 8) & 0xFF, b0 = out[0] & 0xFF;
            for (int _i = 1; _i < W*H; _i++) {
                int r = (out[_i] >> 16) & 0xFF, g = (out[_i] >> 8) & 0xFF, b = out[_i] & 0xFF;
                int d = (r>r0?r-r0:r0-r) + (g>g0?g-g0:g0-g) + (b>b0?b-b0:b0-b);
                if (d > 60) { _distinct++; if (_distinct > 500) break; }
            }
        }
        /* BKA-CLEANPASS: boot-clear white has r==g==b for all pixels.
         * Game content has colored triangles (r!=g or g!=b).
         * Reject grayscale-only frames so cache never locks onto white clear. */
        int _color_px = 0;
        if (_nz >= 100) {
            for (int _i = 1; _i < W*H && _color_px < 500; _i++) {
                int r = (out[_i] >> 16) & 0xFF, g = (out[_i] >> 8) & 0xFF, b = out[_i] & 0xFF;
                if (r != g || g != b) _color_px++;
            }
        }
        int _accept = (_nz >= 100 && _color_px >= 500);
        if (_accept) {
            memcpy(s_last_nonzero, out, sizeof(s_last_nonzero));
            s_have_last = 1;
            static int _ch = 0; if (_ch++ < 5)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-CACHE",
                    "CACHE_UPDATE nz=%d distinct=%d", _nz, _distinct);
        } else if (s_have_last) {
            static int _cr2 = 0; if (_cr2++ < 3)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-CACHE",
                    "REJECT nz=%d distinct=%d (uniform)", _nz, _distinct);
            memcpy(out, s_last_nonzero, sizeof(s_last_nonzero));
            static int _cr = 0; if (_cr++ < 5)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-CACHE",
                    "CACHE_REPLAY (curr_nz=%d)", _nz);
        }
    }

    /* BKA-ART: 80x30 ASCII preview of the final output every 120 frames.
     * Shows WHERE in the framebuffer content is — shape of game vs palette. */
    { static int _art = 0; if (_art++ % 120 == 60) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-ART", "=== ASCII 80x30 (frame %d) ===", _art);
        for (int py = 0; py < 30; py++) {
            char line[81];
            for (int px = 0; px < 80; px++) {
                int gy = py * 240 / 30, gx = px * 320 / 80;
                int p = out[gy * W + gx];
                int lum = ((p >> 16) & 0xFF) + ((p >> 8) & 0xFF) + (p & 0xFF);
                line[px] = (lum > 400) ? '#' : (lum > 150) ? '+' : (lum > 30) ? '.' : ' ';
            }
            line[80] = 0;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-ART", "%s", line);
        }
        /* Per-row non-zero count to know which rows have content */
        char rowbuf[241];
        for (int ry = 0; ry < 240; ry++) {
            int nz = 0;
            for (int rx = 0; rx < W; rx++) if (out[ry * W + rx] & 0x00FFFFFF) nz++;
            rowbuf[ry] = nz > 200 ? '#' : nz > 50 ? '+' : nz > 0 ? '.' : ' ';
        }
        rowbuf[240] = 0;
        __android_log_print(ANDROID_LOG_ERROR, "BKA-ART", "rows(240): %s", rowbuf);
    } }

    /* BKA-ACCUM: framebuffer double-buffer race means ~95% of reads
     * catch the empty buffer. Rasterizer writes 2189+ pixels/frame
     * (verified BKA-RAST-COUNT) but fillArgb reads at wrong instant.
     * Accumulate all non-zero pixels across frames so content persists
     * regardless of which buffer we happened to read. */
    {
        static int s_accum[320*240];
        static int s_accum_have = 0;
        static int s_accum_empty_streak = 0;
        static int s_accum_log = 0;

        int accum_new = 0, accum_total = 0;
        for (int _i = 0; _i < W*H; _i++) {
            uint32_t _p = (uint32_t)out[_i];
            if (_p & 0x00FFFFFF) {
                if ((s_accum[_i] & 0x00FFFFFF) == 0) accum_new++;
                s_accum[_i] = (int)_p;
            }
            if (s_accum[_i] & 0x00FFFFFF) accum_total++;
        }

        if (accum_new > 0) {
            s_accum_have = 1;
            s_accum_empty_streak = 0;
        } else {
            s_accum_empty_streak++;
        }

        /* If accumulator has >500 pixels, override out[] with accumulated view */
        if (s_accum_have && accum_total > 500) {
            memcpy(out, s_accum, sizeof(s_accum));
        }

        if (s_accum_log++ % 60 == 0 || accum_new > 100) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-ACCUM",
                "total=%d new=%d empty_streak=%d have=%d",
                accum_total, accum_new, s_accum_empty_streak, s_accum_have);
        }

        /* Reset accumulator only if truly stuck (10+ seconds of empty) */
        if (s_accum_empty_streak > 600) {
            memset(s_accum, 0, sizeof(s_accum));
            s_accum_have = 0;
            s_accum_empty_streak = 0;
            if (s_accum_log < 20)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-ACCUM", "RESET (starved)");
        }
    }

    /* BKA-CLEANPASS: palette bar disabled — it was overwriting game pixels. */
    /* bka_draw_palette_bar(out, W, H); */

    /* BKA-CACHE-SAMPLE: dump the actual colors we send to Chrome.
     * Also dump a raw 320x240 ASCII art preview into probe.txt so
     * we can see the shape (not just the count). */
    { static int _cs = 0; if (_cs++ < 8 || _cs % 120 == 0) {
        int _nz = 0, _bright = 0, _white = 0, _black = 0;
        for (int _i = 0; _i < W*H; _i++) {
            int p = out[_i];
            int r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            if (p & 0x00FFFFFF) _nz++;
            if (r+g+b > 60) _bright++;
            if (r > 200 && g > 200 && b > 200) _white++;
            if (r+g+b < 20) _black++;
        }
        __android_log_print(ANDROID_LOG_ERROR, "BKA-CACHE-SAMPLE",
            "out nz=%d bright=%d white=%d black=%d | p0=%08X p1000=%08X p10000=%08X p40000=%08X p70000=%08X",
            _nz, _bright, _white, _black,
            (unsigned)out[0], (unsigned)out[1000], (unsigned)out[10000],
            (unsigned)out[40000], (unsigned)out[70000]);
    } }

    /* BKA-RAWCHK: bypass ALL conversion/picker logic — read raw RDRAM
     * and count pixels directly at both framebuffer offsets. Uncapped,
     * fires every 60 frames. This tells us definitively whether the
     * framebuffer memory has content, independent of fillArgb's
     * picker/cache/conversion path. */
    { static int _rawchk = 0; if (_rawchk++ % 60 == 0) {
        extern uint8_t* gN64_RDRAM;
        extern uint32_t g_active_fb_offset;
        if (gN64_RDRAM) {
            uint16_t* fbA = (uint16_t*)(gN64_RDRAM + 0x400000);
            uint16_t* fbB = (uint16_t*)(gN64_RDRAM + 0x440000);
            int nzA = 0, nzB = 0, brA = 0, brB = 0;
            for (int i = 0; i < 320*240; i++) {
                uint16_t ca = fbA[i], cb = fbB[i];
                if (ca) nzA++;
                if (cb) nzB++;
                if ((ca & 0x7FFF) > 0x100) brA++;
                if ((cb & 0x7FFF) > 0x100) brB++;
            }
            __android_log_print(ANDROID_LOG_ERROR, "BKA-RAWCHK",
                "frame=%d fbA_nz=%d fbA_br=%d fbB_nz=%d fbB_br=%d off=0x%X",
                _rawchk, nzA, brA, nzB, brB, (unsigned)g_active_fb_offset);
        }
    } }

    static int s_frame_count = 0;
    s_frame_count++;
    if (s_frame_count % 30 == 0) {
        FILE* f = fopen("/data/data/com.dkrwrapper/files/probe.txt", "w");
        if (f) {
            int lb = gfx_loadblock_count();
            int lt = gfx_loadtile_count();
            int sti = gfx_settimg_count();
            uint8_t* tmem = gfx_tmem_ptr();
            int tmem_sz = gfx_tmem_size();
            int nonzero = 0;
            if (tmem) for (int i = 0; i < tmem_sz; i++) if (tmem[i]) nonzero++;
            fprintf(f, "frame=%d lb=%d lt=%d sti=%d\n", s_frame_count, lb, lt, sti);
            fprintf(f, "tmem_size=%d tmem_nonzero=%d\n", tmem_sz, nonzero);
            fprintf(f, "pi_dma total=%u success=%u fail=%u\n", pi_dma_count(), pi_dma_success(), pi_dma_fail());
            fprintf(f, "first16_tmem=");
            if (tmem) for (int i = 0; i < 16 && i < tmem_sz; i++) fprintf(f, "%02X", tmem[i]);
            fprintf(f, "\n");
            fprintf(f, "opcode histogram:\n");
            for (int i = 0; i < 256; i++) {
                uint32_t n = gfx_opcode_hist_get(i);
                if (n > 0) fprintf(f, "  %02X: %u\n", i, n);
            }
            fclose(f);
        }
    }

    env->ReleaseIntArrayElements(outArr, out, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_dkrwrapper_Native_startEngine(JNIEnv* env, jobject /*thiz*/, jstring dir) {
    if (!dir) return;
    const char* cdir = env->GetStringUTFChars(dir, nullptr);
    if (!cdir) return;
    bka_native_game_boot(cdir, nullptr);
    env->ReleaseStringUTFChars(dir, cdir);
}
