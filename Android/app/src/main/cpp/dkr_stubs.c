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

void _guMtxF2L(float mf[4][4], void *m)
{
    int32_t *ip = (int32_t *)m;
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float f = mf[i][j];
            int32_t v;
            if (f > 32767.999f) v = 0x7FFFFFFF;
            else if (f < -32768.0f) v = (int32_t)0x80000000;
            else v = (int32_t)(f * 65536.0f);
            uint16_t ipart = (uint16_t)((v >> 16) & 0xFFFF);
            uint16_t fpart = (uint16_t)(v & 0xFFFF);
            ip[i * 4 + j] = ((uint32_t)ipart << 16) | fpart;
        }
    }
    /* Register so RDP_TranslateAddr(w1) with truncated low-32 resolves back
     * to this full 64-bit pointer. Matches how gbi.h macros register via
     * linker_stubs.c:bka_add_addr_mapping_c. */
    bka_add_addr_mapping_c((uint32_t)(uintptr_t)m, m);
}

/* Replacement for OS_K0_TO_PHYSICAL. Registers the full 64-bit pointer
 * under BOTH the raw low-32 and the KSEG0-bias-stripped low-32, so the
 * emulator's RDP_TranslateAddr can find the real matrix when it sees
 * only the truncated value from the display list. */

uint32_t bka_k0_to_phys(void *ptr)
{
    uintptr_t u = (uintptr_t)ptr;
    uint32_t low = (uint32_t)u;
    uint32_t out = low - 0x80000000u;
    bka_add_addr_mapping_c(low, ptr);
    bka_add_addr_mapping_c(out, ptr);
    return out;
}
void _guRotateF(void) {}
void _guTranslateF(void) {}
void guPerspectiveF(float mf[4][4], u16 *pn, float fovy, float aspect, float near, float far, float scale) { (void)mf; (void)pn; (void)fovy; (void)aspect; (void)near; (void)far; (void)scale; }

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
    /* Not our case — restore default and re-raise */
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor, used))
static void bka_install_segv_handler(void) {
    struct sigaction sa = {
        .sa_sigaction = bka_segv_handler,
        .sa_flags = SA_SIGINFO | SA_NODEFER,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGV",
        "handler installed");
}
