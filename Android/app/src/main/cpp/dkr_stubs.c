#include "emulator/ptr_shim.h"
#include <android/log.h>
#include <stdlib.h>

/* ============================================================
 * mempool_* stubs — host malloc-backed.
 * src/memory.c is EXCLUDED from the Android build (CMakeLists line 51).
 * <stdlib.h> MUST be above so malloc has the 8-byte return type on ARM64.
 * ============================================================ */
void *mempool_alloc_safe(int size, int tag)  {
    void *p = (size <= 0) ? NULL : malloc((size_t)size);
    __android_log_print(ANDROID_LOG_ERROR, "DKR-MPAS", "mempool_alloc_safe(%d) -> %p", size, p);
    return p;
}
void *mempool_alloc(int size, int tag) {
    __android_log_print(ANDROID_LOG_ERROR, "DKR-MPAS", "mempool_alloc(%d) ENTER", size);
    (void)tag;
    if (size <= 0) return NULL;
    void *p = malloc((size_t)size);
    __android_log_print(ANDROID_LOG_ERROR, "DKR-MPAS", "mempool_alloc(%d) -> %p", size, p);
    ptd_register_alloc(p, (size_t)size);
    __android_log_print(ANDROID_LOG_ERROR, "DKR-MPAS", "mempool_alloc(%d) REGISTERED", size);
    return p;
}
void *mempool_alloc_fixed(int size, int tag) { (void)tag; if (size <= 0) return NULL; return malloc((size_t)size); }
void *mempool_alloc_pool(void *slots, int size) { (void)slots; if (size <= 0) return NULL; return malloc((size_t)size); }
void *mempool_new_sub(int size, int slots)   { (void)slots; if (size <= 0) return NULL; return malloc((size_t)size); }
void  mempool_free(void *addr)                { if (addr) free(addr); }
void  mempool_free_queue_clear(void)          {}
void  mempool_free_timer(int state)           { (void)state; }
void  mempool_init_main(void)                 {}
void  mempool_locked_set(void *addr)          { (void)addr; }
void  mempool_locked_unset(void)              {}
/* ============================================================ */


/* ============================================================
 * mempool_* stubs — host malloc-backed.
 * src/memory.c is EXCLUDED from the Android build (CMakeLists line 51).
 * These stubs are the only definitions of the mempool interface.
 * <stdlib.h> MUST be included above so malloc has the correct 8-byte
 * return type on ARM64; otherwise it's implicit-int and truncates.
 * ============================================================ */
/* ============================================================ */

/* dkr_stubs.c — stub markers */
/* dkr_stubs.c — link stubs for DKR port.
 * Audio, OS, PFS, GU, RSP blobs. Silent/no-op for first boot. */
#include <stdint.h>
#include <string.h>
#include <strings.h>

/* RSP microcode blobs — unused, software renderer */
long long int rspbootTextStart[1] = {0};
long long int rspbootTextEnd[1] = {0};
long long int rspF3DDKRXbusStart[1] = {0};
long long int rspF3DDKRFifoStart[1] = {0};
long long int rspF3DDKRDataXbusStart[1] = {0};
long long int rspF3DDKRDataFifoStart[1] = {0};
long long int aspMainDataStart[1] = {0};
long long int aspMainTextStart[1] = {0};

/* ROM/linker symbols — populated by wrapper at runtime */
u32 gBkaLutSize = 0xD0; // 208 bytes from splat YAML
u32 gBkaLutRomOffset = 0xED0E0; // assets.lut start from splat YAML
uint32_t gBkaAssetsRomOffset = 0xED1B0; // assets start from splat YAML
uint8_t __ROM_END[0];
uint8_t D_B0000578[16];
uint8_t gzip_inflate_block[4096];
uint8_t align16[16];

/* Video modes — software renderer ignores */
static uint8_t _vi_dummy[64];
OSViMode osViModeTable[17];
OSViMode osViModeNtscLan1;
OSViMode osViModeNtscLpn1;
OSViMode osViModePalLan1;
OSViMode osViModePalLpn1;
OSViMode osViModeMpalLan1;
OSViMode osViModeMpalLpn1;

/* Audio — silent boot */
Acmd *alAudioFrame(Acmd *cl, s32 *l, s16 *ob, s32 ol) { (void)cl; (void)l; (void)ob; (void)ol; return 0; }
void alCSPNew(ALCSPlayer *sp, ALSeqpConfig *cfg) { (void)sp; (void)cfg; }
void alSynNew(ALSynth *s, ALSynConfig *c) { (void)s; (void)c; }
void alSynDelete(ALSynth *s) { (void)s; }
u8 alCSPGetChlVol(ALCSPlayer *sp, u8 ch) { (void)sp; (void)ch; return 0; }
s32 alCSPGetFadeIn(ALCSPlayer *sp) { (void)sp; return 0; }
s32 alCSPGetState(ALCSPlayer *sp) { (void)sp; return 0; }
void alCSPSetChlPan(ALCSPlayer *sp, u8 ch, ALPan p) { (void)sp; (void)ch; (void)p; }
void alCSPSetChlVol(ALCSPlayer *sp, u8 ch, u8 v) { (void)sp; (void)ch; (void)v; }
void alCSPSetFadeIn(ALCSPlayer *sp, void *ft, f32 t) { (void)sp; (void)ft; (void)t; }
void alFxReverbSet(void *fx, void *rv) { (void)fx; (void)rv; }
void alSeqChOff(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; }
void alSeqChOn(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; }
u8 alSeqpGetChlFXMix(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; return 0; }
void alSynAddPlayer(ALSynth *sy, ALPlayer *pl) { (void)sy; (void)pl; }
s32 alSynAllocVoice(ALSynth *s, ALVoice *v, ALVoiceConfig *vc) { (void)s; (void)v; (void)vc; return 0; }
void alSynFreeVoice(ALSynth *sy, ALVoice *v) { (void)sy; (void)v; }
void alSynSetFXMix(ALSynth *sy, ALVoice *v, u8 mx) { (void)sy; (void)v; (void)mx; }
void alSynSetPan(ALSynth *sy, ALVoice *v, ALPan p) { (void)sy; (void)v; (void)p; }
void alSynSetPitch(ALSynth *sy, ALVoice *v, f32 p) { (void)sy; (void)v; (void)p; }
void alSynSetVol(ALSynth *s, ALVoice *v, s16 vol, ALMicroTime delta) { (void)s; (void)v; (void)vol; (void)delta; }
void alSynStartVoice(ALSynth *sy, ALVoice *v, ALWaveTable *w) { (void)sy; (void)v; (void)w; }
void alSynStopVoice(ALSynth *sy, ALVoice *v) { (void)sy; (void)v; }

/* Scheduler */
void osCreateScheduler(OSSched *s, void *stack, OSPri priority, u8 mode, u8 numFields) { (void)s; (void)stack; (void)priority; (void)mode; (void)numFields; }
void osScAddClient(OSSched *s, OSScClient *c, OSMesgQueue *msgQ, u8 id) { (void)s; (void)c; (void)msgQ; (void)id; }
OSMesgQueue *osScGetCmdQ(OSSched *s) { (void)s; return 0; }
OSMesgQueue *osScGetInterruptQ(OSSched *s) { (void)s; return 0; }
s32 osDpSetNextBuffer(void *buf, u64 size) { (void)buf; (void)size; return 0; }
void osSetTime(uint64_t t)    { (void)t; }

/* PFS — Controller Pak, not on Android */
s32 osPfsAllocateFile(OSPfs *pfs, u16 company, u32 game, u8 *gameName, u8 *extName, int fileNum, s32 *fileSize) { (void)pfs; (void)company; (void)game; (void)gameName; (void)extName; (void)fileNum; (void)fileSize; return -1; }
s32 osPfsChecker(OSPfs *pfs) { (void)pfs; return -1; }
s32 osPfsDeleteFile(OSPfs *pfs, u16 company, u32 game, u8 *gameName, u8 *extName) { (void)pfs; (void)company; (void)game; (void)gameName; (void)extName; return -1; }
s32 osPfsFileState(OSPfs *pfs, s32 fileNum, OSPfsState *state) { (void)pfs; (void)fileNum; (void)state; return -1; }
s32 osPfsFindFile(OSPfs *pfs, u16 company, u32 game, u8 *gameName, u8 *extName, s32 *fileNum) { (void)pfs; (void)company; (void)game; (void)gameName; (void)extName; (void)fileNum; return -1; }
s32 osPfsFreeBlocks(OSPfs *pfs, s32 *bytesNotUsed) { (void)pfs; (void)bytesNotUsed; return -1; }
s32 osPfsIsPlug(OSMesgQueue *mq, u8 *pattern) { (void)mq; (void)pattern; return -1; }
s32 osPfsNumFiles(OSPfs *pfs, s32 *maxFiles, s32 *filesUsed) { (void)pfs; (void)maxFiles; (void)filesUsed; return -1; }
s32 osPfsReFormat(OSPfs *pfs, OSMesgQueue *queue, int channel) { (void)pfs; (void)queue; (void)channel; return -1; }
s32 osPfsReadWriteFile(OSPfs *pfs, s32 fileNum, u8 flag, int offset, int size, u8 *dataBuffer) { (void)pfs; (void)fileNum; (void)flag; (void)offset; (void)size; (void)dataBuffer; return -1; }

/* Internal OS */
void *__osGetActiveQueue(void) { return 0; }
void __osSpSetStatus(u32 status) { (void)status; }

/* GU — matrices (wrong values but defined) */
/* Real _guMtxF2L: convert float[4][4] -> N64 fixed-point int16.uint16 layout.
 * Also registers the buffer's truncated low-32 with the address map so
 * RDP_TranslateAddr(w1) can recover the full 64-bit pointer.
 *
 * Layout written (64 bytes at offset 0 of the Mtx buffer):
 *   int32_t slot[16], each = (int16 int_part << 16) | uint16 frac_part
 * Standard N64 Mtx format. sizeof(Mtx)=128 on aarch64 (long=64-bit), so the
 * second 64 bytes of the slot remain zero and are never read.
 */
#include <stdint.h>
extern void bka_add_addr_mapping_c(uint32_t key, void *ptr);

/* Write a packed N64 Mtx into dst[0..63] using the layout described in
 * NU6-06-0030-001G p.144: all 16 int16 int-parts (big-endian) at bytes
 * 0x00-0x1F, then all 16 int16 frac-parts (big-endian) at bytes 0x20-0x3F,
 * both row-major. Matrix_LoadFromN64 in gfx_interpreter.cpp reads exactly
 * this layout. */
/* Emit the same LE-packed layout as src/hasm/math_util.c:mtxf_to_mtx on ARM64.
 * Matrix_LoadFromN64 in gfx_interpreter.cpp reads this layout. */
static void bka_pack_mtx(float mf[4][4], uint8_t *dst)
{
    for (int k = 0; k < 64; k++) dst[k] = 0;
    int32_t *ai = (int32_t *)dst;
    int32_t *af = (int32_t *)(dst + 32);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j += 2) {
            float f1 = mf[i][j];
            float f2 = mf[i][j+1];
            int32_t e1, e2;
            if (f1 > 32767.999f) e1 = 0x7FFFFFFF;
            else if (f1 < -32768.0f) e1 = (int32_t)0x80000000;
            else e1 = (int32_t)(f1 * 65536.0f);
            if (f2 > 32767.999f) e2 = 0x7FFFFFFF;
            else if (f2 < -32768.0f) e2 = (int32_t)0x80000000;
            else e2 = (int32_t)(f2 * 65536.0f);
            *ai++ = (e1 & 0xFFFF0000) | ((e2 >> 16) & 0xFFFF);
            *af++ = ((e1 << 16) & 0xFFFF0000) | (e2 & 0xFFFF);
        }
    }
}

void _guMtxF2L(float mf[4][4], void *m)
{
    bka_pack_mtx(mf, (uint8_t *)m);
    bka_add_addr_mapping_c((uint32_t)(uintptr_t)m, m);
    { extern int __android_log_print(int, const char*, const char*, ...);
      static int _f2l = 0;
      if (_f2l++ < 20)
        __android_log_print(6, "BKA-F2L",
            "mf=%p m=%p [0][0]=%.4f [2][2]=%.4f b[0..3]=%02X%02X%02X%02X b[32..35]=%02X%02X%02X%02X",
            mf, m, mf[0][0], mf[2][2],
            ((uint8_t*)m)[0], ((uint8_t*)m)[1], ((uint8_t*)m)[2], ((uint8_t*)m)[3],
            ((uint8_t*)m)[32], ((uint8_t*)m)[33], ((uint8_t*)m)[34], ((uint8_t*)m)[35]); }
}

/* Replacement for OS_K0_TO_PHYSICAL. Registers the full 64-bit pointer
 * under BOTH the raw low-32 and the KSEG0-bias-stripped low-32, so the
 * emulator's RDP_TranslateAddr can find the real matrix when it sees
 * only the truncated value from the display list. */

uint32_t bka_k0_to_phys(void *ptr)
{
    {
        static int _k0p_n = 0;
        uint32_t _low = (uint32_t)(uintptr_t)ptr;
        uint32_t _out = _low - 0x80000000u;
        /* Log only high-byte EA/EB/DA/DB range (polygons + matrices).
         * These are the w1 values that Cmd_Polygon/Cmd_Vtx receive. */
        uint8_t _hi = (_out >> 24) & 0xFF;
        if ((_hi == 0xEA || _hi == 0xEB || _hi == 0xDA || _hi == 0xDB) && _k0p_n++ < 1000)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-K0P-REG",
                "in=%p low=0x%08X out=0x%08X", ptr, _low, _out);
    }

    { extern void bka_diag_logfile(const char*); char _b[80];
      extern int snprintf(char*, unsigned long, const char*, ...);
      snprintf(_b, sizeof(_b), "BKA-K0P in=%p out=0x%08X", ptr,
               (uint32_t)((uintptr_t)ptr) - 0x80000000u);
      bka_diag_logfile(_b); }
    uintptr_t u = (uintptr_t)ptr;
    if (u == 0) return 0;
    uint32_t low = (uint32_t)u;
    uint32_t out = low - 0x80000000u;
    /* Only register if ptr is a REAL host pointer (>= 4GB). Values < 4GB
     * are truncated/garbage — registering them makes bka_lookup_addr_by_low32
     * return a bad pointer (e.g. 0x6aexxxxx) that fails bka_is_readable,
     * causing POLYFAIL/VTXFAIL everywhere. */
    if (u >= 0x100000000ULL) {
        bka_add_addr_mapping_c(low, ptr);
        bka_add_addr_mapping_c(out, ptr);
    }
    return out;
}
#include <math.h>
/* stdint types come via existing includes */
/* Real libultra matrix builders. Without these, game calls to
 * guPerspective / guRotate / guTranslate leave the Mtx buffer at
 * whatever was on the stack — BKA-MTX-FULL64 showed leftover JSON
 * text ("ECTS_BUM...") at offset 0x20 of the projection Mtx, proving
 * the pointer was correct but the data was never written.
 * Format (NU6-06-0030-001G p.144): Mtx packs int16 parts at 0x00-0x1F
 * and int16 frac parts at 0x20-0x3F. Same layout libultra guMtxF2L
 * produces; here we emit it directly. */

static void bka_f2l(float mf[4][4], Mtx *out) {
    bka_pack_mtx(mf, (uint8_t *)out);
}

void _guRotateF(float m[4][4], float a, float x, float y, float z) {
    float mag = sqrtf(x*x + y*y + z*z);
    if (mag < 1e-6f) {
        for (int i=0;i<4;i++) for (int j=0;j<4;j++) m[i][j]=(i==j)?1.0f:0.0f;
        return;
    }
    x /= mag; y /= mag; z /= mag;
    float rad = a * 3.14159265358979f / 180.0f;
    float c = cosf(rad), s = sinf(rad), t = 1.0f - c;
    m[0][0]=t*x*x+c;    m[0][1]=t*x*y-s*z;  m[0][2]=t*x*z+s*y;  m[0][3]=0.0f;
    m[1][0]=t*x*y+s*z;  m[1][1]=t*y*y+c;    m[1][2]=t*y*z-s*x;  m[1][3]=0.0f;
    m[2][0]=t*x*z-s*y;  m[2][1]=t*y*z+s*x;  m[2][2]=t*z*z+c;    m[2][3]=0.0f;
    m[3][0]=0.0f;       m[3][1]=0.0f;       m[3][2]=0.0f;       m[3][3]=1.0f;
}
/* guRotate: defined in linker_stubs.c — do not redefine here. */


void _guTranslateF(float m[4][4], float x, float y, float z) {
    for (int i=0;i<4;i++) for (int j=0;j<4;j++) m[i][j]=(i==j)?1.0f:0.0f;
    m[0][3]=x; m[1][3]=y; m[2][3]=z;
}
/* guTranslate: defined in linker_stubs.c — do not redefine here. */


void guPerspectiveF(float mf[4][4], u16 *perspNorm, float fovy,
                    float aspect, float near_, float far_, float scale) {
    (void)scale;
    float cot = 1.0f / tanf(fovy * 0.5f * 3.14159265358979f / 180.0f);
    for (int i=0;i<4;i++) for (int j=0;j<4;j++) mf[i][j]=(i==j)?1.0f:0.0f;
    mf[0][0] = cot / aspect;
    mf[1][1] = cot;
    mf[2][2] = (near_ + far_) / (near_ - far_);
    mf[2][3] = 2.0f * near_ * far_ / (near_ - far_);
    mf[3][2] = -1.0f;
    mf[3][3] = 0.0f;
    if (perspNorm) {
        float c = (near_ * far_) / (near_ + far_) * scale;
        if (c > 0.0f) {
            union { float f; u32 u; } u; u.f = c;
            int exp = (int)((u.u >> 23) & 0xFF) - 127 + 31;
            if (exp < 0) exp = 0; if (exp > 255) exp = 255;
            *perspNorm = (u16)((exp << 1) & 0x1FE);
        } else {
            *perspNorm = 0;
        }
    }
    { extern int __android_log_print(int, const char*, const char*, ...);
      static int _gpf = 0;
      if (_gpf++ < 20)
        __android_log_print(6, "BKA-GUPF",
            "mf=%p fovy=%.1f aspect=%.3f near=%.2f far=%.2f "
            "mf00=%.4f mf11=%.4f mf22=%.4f mf23=%.2f mf32=%.2f",
            mf, fovy, aspect, near_, far_,
            mf[0][0], mf[1][1], mf[2][2], mf[2][3], mf[3][2]); }
}
void guPerspective(Mtx *m, u16 *pn, float fovy, float aspect,
                   float near_, float far_, float scale) {
    float mf[4][4];
    guPerspectiveF(mf, pn, fovy, aspect, near_, far_, scale);
    bka_f2l(mf, m);
    { extern int __android_log_print(int, const char*, const char*, ...);
      static int _gp = 0;
      if (_gp++ < 20)
        __android_log_print(6, "BKA-GUP",
            "m=%p first4=%08X %08X %08X %08X last4=%08X %08X %08X %08X",
            m,
            ((uint32_t*)m)[0], ((uint32_t*)m)[1],
            ((uint32_t*)m)[2], ((uint32_t*)m)[3],
            ((uint32_t*)m)[12], ((uint32_t*)m)[13],
            ((uint32_t*)m)[14], ((uint32_t*)m)[15]); }
}

/* Game functions from asm — no C fallback */
void obj_shade_fast(void) {}
int32_t obj_animate(void) { return 0; }
void func_80000450(void) {}
void func_80063A90(void) {}
void func_80065A80(void) {}
void calc_dynamic_lighting_for_object_2(void) {}
void set_voice_limit(void) {}
void set_stereo_pan_mode(void) {}

/* Misc */
int bcmp(const void *a, const void *b, int n) { return memcmp(a, b, (size_t)n); }

/* Framebuffer dims — wrapper reads these */
int32_t gFramebufferWidth  = 320;
int32_t gFramebufferHeight = 240;

const char* dkr_stubs_build_id = "mempool_removed_2026_10_20";


/* ============================================================
 * Android malloc-backed memory pool (replaces src/memory.c)
 * The N64 memory pool code in src/memory.c is not compiled into
 * the Android build. These functions provide the same interface
 * using standard host malloc/free.
 * ============================================================ */








/* ============================================================ */

/* ==========================================================================
 * SIGSEGV recovery: catch SEGV_ACCERR (write to non-writable page), mprotect
 * the page RW, retry the faulting instruction. Required because the recomp
 * places several game data buffers (notably decrypt_magic_codes' destination)
 * in read-only sections (.rodata / .data.rel.ro), which crash on first write.
 * Without this handler, DKR dies ~9s into init_game.
 * ========================================================================== */
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <android/log.h>

static void bka_segv_handler(int sig, siginfo_t *info, void *ctx) {
    if (sig != SIGSEGV) {
        __android_log_print(ANDROID_LOG_FATAL, "BKA-SIG",
            "signal %d (si_code=%d) addr=%p — not handled, re-raising",
            sig, info->si_code, info->si_addr);
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    if (sig == SIGSEGV && info->si_code == SEGV_ACCERR) {
        uintptr_t page = (uintptr_t)info->si_addr & ~(uintptr_t)0xFFF;
        if (mprotect((void*)page, 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC) == 0) {
            static int _c = 0;
            if (_c++ < 40)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGV",
                    "mprotect(RW) addr=%p page=%p OK",
                    info->si_addr, (void*)page);
            return;  /* re-executes faulting instruction; page now writable */
        }
        __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGV",
            "mprotect FAILED addr=%p errno-return", info->si_addr);
    }
    /* Not our case — log everything then restore default and re-raise */
    __android_log_print(ANDROID_LOG_FATAL, "BKA-SIG",
        "signal %d si_code=%d addr=%p — falling through, re-raising to SIG_DFL",
        sig, (int)info->si_code, info->si_addr);
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor, used))
void bka_install_segv_handler(void) {
    struct sigaction sa = {
        .sa_sigaction = bka_segv_handler,
        .sa_flags = SA_SIGINFO | SA_NODEFER,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    /* Also catch these so we can log which signal fires; without this,
       Android debuggerd sometimes swallows them without a crash-buffer
       entry, leaving us unable to tell SIGABRT from clean exit(). */
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGV",
        "handler installed pid=%d", (int)getpid());
}
