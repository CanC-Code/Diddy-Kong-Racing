/* BKA_FORCE_REBUILD_v4_2026_10_15 */
#include <setjmp.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "gfx_interpreter.h"
#include "rare_decompression.h"
extern "C" uint32_t decompress_rare_to_offset(const uint8_t*, uint32_t, uint8_t*, uint32_t, uint32_t);

static uint32_t decomp_rare_to_offset_safe(const uint8_t* src, uint32_t src_size, uint8_t* out, uint32_t out_size) {
    return decompress_rare_to_offset(src, src_size, out, 0, out_size);
}


extern "C" volatile int g_bka_pixels_drawn = 0;
#include <android/log.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <time.h>
#include <algorithm>

#define LOG_TAG "BKA_GFX"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/* High-volume per-command traces.  These were essential for bootstrapping the
 * decoder but each RSP task fires ~1000 of them, throttling the RSP thread to
 * ~1 Hz on device.  Flip to true to re-enable for a focused debugging session. */
static const bool BKA_GFX_VERBOSE = false;
#define LOGV(...) do { if (BKA_GFX_VERBOSE) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); } while (0)

extern "C" {
    extern uint32_t g_active_fb_offset;
extern uint16_t gFramebuffers[2][FB_WIDTH * FB_HEIGHT];
    int getActiveFramebuffer(void);
    extern uint8_t* gN64_RDRAM;
    extern "C" void* bka_lookup_addr_by_low32(uint32_t low32);
void* bka_lookup_addr_mapping(uint32_t key);
void* bka_lookup_addr_by_low32(uint32_t low32);
    void* bka_lookup_addr_mapping_range_c(uint32_t key);
    int bka_is_mapped(void* ptr);
    int bka_is_readable(void* ptr);
    uintptr_t bka_get_mapped_end(void* ptr);
    void* bka_find_registered_in_range(uintptr_t lo, uintptr_t hi);
}

static uint8_t* g_bka_dl_cur = nullptr;   // walker sets before dispatch; Cmd_* helpers read for context

static RDPState s_rdp;
extern uint32_t g_png_rdram_start;
extern uint32_t g_png_rdram_end;
static bool g_bka_force_rgba = false;
static int g_bka_task_tri_count = 0;
/* BKA-DIRTY: set by RasterizeTriangle on pixel write, cleared by
 * swap. Survives task boundary — fills skip if set. */
extern "C" { volatile int g_bka_fb_dirty = 0; }

/* DKR MVP slots — declared here so RSP_ProcessGfxTask's task-start reset
 * (around line 629) sees them. Populated by Cmd_Mtx (DKR p3 bits 6-7 =
 * slot index) and read by TransformVertex via s_rdp.projection, which
 * Cmd_MoveWord/Cmd_Mtx keep synced to s_dkrMvpSlots[s_dkrActiveSlot]. */
static BKMatrix s_dkrMvpSlots[3];
static int s_dkrActiveSlot = 0;
int g_bka_last_tri2_count = 0;   /* Fix DD: total TRI2 calls, monotonic */
static int g_mtx_flag_hist[256] = {0};
volatile uint32_t g_bka_frame_gen = 0;
static int g_bka_task_max_depth = 0;
static int g_bka_task_pops = 0;
static int s_rsp_dump_sizes = [](){ LOGV("RDP-SIZE=%zu sizeof dmem=%zu offsetof(dmem)=%zu offsetof(dmemVertexCount)=%zu", sizeof(RDPState), sizeof(s_rdp.dmem), offsetof(RDPState, dmem), offsetof(RDPState, dmemVertexCount)); return 0; }();
static uint64_t bka_canary_post = 0xBEEFCAFEBABE5678ull;
static inline bool bka_in_rdram(void* p, size_t n) {
    uintptr_t a = (uintptr_t)p;
    uintptr_t lo = (uintptr_t)gN64_RDRAM;
    uintptr_t hi = lo + 0x04000000;
    return a >= lo && (a + n) <= hi;
}
static inline void bka_guard_write(void* p, size_t n, const char* tag) {
    if (bka_in_rdram(p, n)) {
        static int _gw = 0;
        if (_gw++ < 20) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "RDRAM-WRITE %s dst=%p len=%zu", tag, p, n);
    }
}
static int s_mtx_log_frame = 0;
static int s_mtx_dump_frame = 0;
static const uint8_t* s_current_cmd = nullptr;

#include <signal.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>

#include <cstdarg>
static void diag_log(const char* tag, const char* fmt, ...) {


    FILE* f = fopen("/data/user/0/com.dkrwrapper/files/diag.log", "a");
    if (!f) return;
    fprintf(f, "%s: ", tag);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

extern "C" void bka_diag_logfile(const char* msg) {
    diag_log("BKA-FILE", "%s", msg);
}


#define BKA_RING_SZ 32
static struct {
    uintptr_t dl_cur;
    uint32_t w0;
    uint32_t w1;
    uint8_t op;
    uint8_t enc;
} s_bka_ring[BKA_RING_SZ];
static volatile int s_bka_ring_n = 0;
static void bka_install_watch(void);

static void bka_sigsegv_handler(int sig, siginfo_t* si, void* uc) {
    ucontext_t* ctx = (ucontext_t*)uc;
    uintptr_t pc = (uintptr_t)ctx->uc_mcontext.pc;
    uintptr_t lr = (uintptr_t)ctx->uc_mcontext.regs[30];
    uintptr_t page = (uintptr_t)si->si_addr & ~0xFFFULL;

    Dl_info di_pc = {0}, di_lr = {0};
    dladdr((void*)pc, &di_pc);
    dladdr((void*)lr, &di_lr);

    const char* pc_name = di_pc.dli_sname ? di_pc.dli_sname : "?";
    const char* pc_lib  = di_pc.dli_fname ? di_pc.dli_fname : "?";
    uintptr_t pc_off = di_pc.dli_saddr ? (pc - (uintptr_t)di_pc.dli_saddr) : pc;
    uintptr_t pc_base = di_pc.dli_fbase ? ((uintptr_t)di_pc.dli_fbase) : 0;

    const char* lr_name = di_lr.dli_sname ? di_lr.dli_sname : "?";
    const char* lr_lib  = di_lr.dli_fname ? di_lr.dli_fname : "?";
    uintptr_t lr_off = di_lr.dli_saddr ? (lr - (uintptr_t)di_lr.dli_saddr) : lr;
    uintptr_t lr_base = di_lr.dli_fbase ? ((uintptr_t)di_lr.dli_fbase) : 0;

    static int s_seen = 0;
    if (s_seen++ < 20)
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "SEGVDUMP addr=%p page=0x%lX pc=0x%lX pc_off=0x%lX pc_base=0x%lX pc_sym=%s+0x%lX lib=%s lr=0x%lX lr_off=0x%lX lr_sym=%s+0x%lX",
            si->si_addr, (unsigned long)page,
            (unsigned long)pc, (unsigned long)pc_off, (unsigned long)pc_base,
            pc_name, (unsigned long)pc_off, pc_lib,
            (unsigned long)lr, (unsigned long)lr_off,
            lr_name, (unsigned long)lr_off);
    { int n = __atomic_load_n(&s_bka_ring_n, __ATOMIC_ACQUIRE); int cnt = (n < BKA_RING_SZ) ? n : BKA_RING_SZ; __android_log_print(ANDROID_LOG_ERROR, "BKA-RING", "ring_n=%d last %d DL commands:", n, cnt); for (int k = 0; k < cnt; k++) { int idx = (n - 1 - k) & (BKA_RING_SZ - 1); __android_log_print(ANDROID_LOG_ERROR, "BKA-RING", "  -%02d cur=%p w0=0x%08X w1=0x%08X op=0x%02X enc=%u", k, (void*)s_bka_ring[idx].dl_cur, s_bka_ring[idx].w0, s_bka_ring[idx].w1, s_bka_ring[idx].op, s_bka_ring[idx].enc); } }
    (void)page; /* do NOT re-enable; would re-execute the faulting insn and loop */
    signal(sig, SIG_DFL);
    raise(sig);
}
static void bka_install_watch(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = bka_sigsegv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
}

int g_bka_force_identity_mv = 0;  /* set per-frame based on current map */
extern "C" int gsworld_getMap(void);
static inline int bka_should_force_identity_mv(void) {
    /* MAP_91 (file select) is a UI map drawn near origin; the port loads a
     * stale player-shadow-derived modelview for it that translates by
     * ~16000, pushing all geometry out of frustum.  Force identity here
     * until the camera-source bug is fixed.
     * TODO: investigate why modelview r3 = (-shadow_pos). */
    return 0; /* disabled: camera source fix in place */
}
static uintptr_t s_dl_base = 0;

/* F3DEX opcodes that we recognize.  Used to distinguish LE-encoded runtime
 * commands from BE-encoded ROM-copied geometry commands. */
static inline bool bka_is_f3dex_opcode(uint8_t op) {
    /* Fix N: F3DEX2-only whitelist.  B-K uses F3DEX2; F3DEX 1.0 opcodes
     * 0x02, 0x05, 0x07, 0x08, 0xE0-0xE3 are drift tokens in this data.
     *
     * DMA: 0x00 SPNOOP, 0x01 MTX, 0x03 MOVEMEM, 0x04 VTX, 0x06 DL
     * Imm: 0xAF-0xBF (LOAD_UCODE..TRI1)
     * RDP: 0xE4-0xFF (TEXRECT..SETCIMG) */
    /* 0x00 (SPNOOP) deliberately excluded: any all-zero byte slot would
     * otherwise look like a valid opcode, making LE/BE encoding detection
     * ambiguous for sub-DLs whose first command starts with 0x00 (as N64
     * BE bytes) or ends with 0x00 (as port-emitted LE bytes). */
    if (op == 0x01 || op == 0x03 ||
        op == 0x04 || op == 0x06 || op == 0x07) return true; /* 0x07 = Rare SetTextureImage (DKR) */
    if (op >= 0xAF && op <= 0xBF) return true;
    if (op >= 0xE4) return true;
    return false;
}


// Deterministic default segment bases (from decomp overlay layout)
// Populated before any RDP command runs, fixing many unmapped address errors.
static struct RDPStateDefaultSegments {
    RDPStateDefaultSegments() {
        s_rdp.segmentBase[0x00] = 0x00000000u;
        s_rdp.segmentBase[0x01] = 0x80000000u;
        s_rdp.segmentBase[0x02] = 0x80000000u;
        s_rdp.segmentBase[0x03] = 0x80000000u;
        s_rdp.segmentBase[0x04] = 0x08000000u;
        s_rdp.segmentBase[0x05] = 0x08000000u;
        s_rdp.segmentBase[0x06] = 0x08000000u;
        s_rdp.segmentBase[0x07] = 0x08000000u;
        s_rdp.segmentBase[0x08] = 0x08000000u;
        s_rdp.segmentBase[0x09] = 0x08000000u;
        s_rdp.segmentBase[0x0A] = 0x08000000u;
        s_rdp.segmentBase[0x0B] = 0x08000000u;
        s_rdp.segmentBase[0x0C] = 0x0C000000u;
        s_rdp.segmentBase[0x0D] = 0x80000000u;
        s_rdp.segmentBase[0x0E] = 0x80000000u;
        s_rdp.segmentBase[0x0F] = 0x80000000u;
    }
} s_rdpDefaultSegments;


// Heuristic: the two known bad DL addresses (0xFFF9153F, 0xFFFF153F)
// share low16 = 0x153F and differ by 0x60000.  Search readable heap
// regions for a base with that low16 whose two Vtx arrays look real.
static inline int bka_plausible_vtx(const uint8_t* b) {
    int16_t x = (int16_t)(b[0] | (b[1] << 8));
    int16_t y = (int16_t)(b[2] | (b[3] << 8));
    int16_t z = (int16_t)(b[4] | (b[5] << 8));
    if (x < -0x4000 || x > 0x4000) return 0;
    if (y < -0x4000 || y > 0x4000) return 0;
    if (z < -0x4000 || z > 0x4000) return 0;
    // Not all-zero, not all-same, not all-FF.
    int nonZero = 0, nonFF = 0;
    for (int i = 0; i < 16; i++) {
        if (b[i] != 0) nonZero = 1;
        if (b[i] != 0xFF) nonFF = 1;
    }
    if (!nonZero || !nonFF) return 0;
    return 1;
}

static uint8_t* bka_scan_heap_for_vertex_base(uint32_t dl_addr) {
    uint32_t off16 = dl_addr & 0xFFFF;
    uint32_t delta = (dl_addr == 0xFFFF153F) ? 0x60000u : 0u;

    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return nullptr;
    char line[512];
    uint8_t* found = nullptr;
    while (fgets(line, sizeof line, f)) {
        uintptr_t start, end;
        char perms[8];
        if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) != 3) continue;
        if (perms[0] != 'r') continue;
        if (start < 0x7000000000ULL || start > 0x7700000000ULL) continue;
        if (end - start < (0x60000ULL + 0x40)) continue;

        // First 64K-aligned address in this region with low16 == off16.
        uintptr_t c = (start & ~(uintptr_t)0xFFFF) | (uintptr_t)off16;
        if (c < start) c += 0x10000;
        for (; c + 0x60000 + 0x40 <= end; c += 0x10000) {
            const uint8_t* p1 = (const uint8_t*)c;
            const uint8_t* p2 = p1 + delta;
            if (!bka_plausible_vtx(p1)) continue;
            if (delta && !bka_plausible_vtx(p2)) continue;

            static int s_h = 0;
            if (s_h++ < 2) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "HEAPVTX dl=0x%08X cand=%p delta=0x%X "
                    "b1=%02X%02X%02X%02X  b2=%02X%02X%02X%02X",
                    dl_addr, (void*)c, delta,
                    p1[0],p1[1],p1[2],p1[3], p2[0],p2[1],p2[2],p2[3]);
            }
            found = (uint8_t*)c;
            break;
        }
        if (found) break;
    }
    fclose(f);
    return found;
}

static inline uint8_t* RDP_TranslateAddr(uint32_t addr) {
    /* PORT: framebuffer pointer reconstruction */
    /* PORT: PNG texture range — our loader allocates at RDRAM[0x1000000..) */
    if (gN64_RDRAM && addr >= g_png_rdram_start && addr < g_png_rdram_end) {
        return gN64_RDRAM + addr;
    }

    {
        extern uint8_t *gVideoLastFramebuffer;
        extern uint8_t *gVideoCurrFramebuffer;
        if (gVideoLastFramebuffer) {
            uint32_t lo = (uint32_t)(uintptr_t)gVideoLastFramebuffer;
            if (addr >= lo && (uint32_t)(addr - lo) < 0x80000u) {
                static int s1 = 0; if (s1++ < 5)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-FBCHECK",
                        "last addr=0x%08X off=0x%X", addr, addr - lo);
                return gVideoLastFramebuffer + (addr - lo);
            }
        }
        if (gVideoCurrFramebuffer) {
            uint32_t lo = (uint32_t)(uintptr_t)gVideoCurrFramebuffer;
            if (addr >= lo && (uint32_t)(addr - lo) < 0x80000u) {
                static int s2 = 0; if (s2++ < 5)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-FBCHECK",
                        "curr addr=0x%08X off=0x%X", addr, addr - lo);
                return gVideoCurrFramebuffer + (addr - lo);
            }
        }
    }

#ifdef PORT
    {
        extern uint8_t *gN64_RDRAM;
        static uint32_t s_rd32 = 0;
        if (s_rd32 == 0) s_rd32 = (uint32_t)(uintptr_t)gN64_RDRAM;
        if (addr != 0 && s_rd32 != 0 && addr >= s_rd32 && (uint32_t)(addr - s_rd32) < 0x00800000u) {
            static int s_rl = 0;
            if (s_rl++ < 5)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-RDTR",
                    "addr=0x%08X truncated_host->rddram off=0x%X", addr, addr - s_rd32);
            return gN64_RDRAM + (addr - s_rd32);
        }
    }
#endif

    // Prefer the 64-bit host pointer in the recomp's 16-byte DL entry payload.

    if (addr == 0x7A7B0620) {
        static int s_trace = 0;
        if (s_trace++ < 5)
            LOGV("XLT TRACE enter addr=0x%08X", addr);
    }

    { static int s_xtrace2 = 0; uint32_t __s = (addr >> 24) & 0x0F;      if (__s >= 1 && __s <= 15 && s_xtrace2++ < 100) {        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",          "XT2 addr=0x%08X seg=%u base=0x%lX",          addr, __s, (unsigned long)s_rdp.segmentBase[__s]); } }
    if (addr == 0) return nullptr;

    // Known-bad DL address?  Try scanning the heap for the real buffer.
    if (addr == 0xFFF9153F || addr == 0xFFFF153F) {
        // Cache the resolved base for the process lifetime.
        static uint8_t* cached_ff = nullptr;
        static uint8_t* cached_ffff = nullptr;
        if (addr == 0xFFF9153F) {
            if (!cached_ff) cached_ff = bka_scan_heap_for_vertex_base(addr);
            if (cached_ff) return cached_ff;
        } else {
            if (!cached_ffff) cached_ffff = bka_scan_heap_for_vertex_base(addr);
            if (cached_ffff) return cached_ffff;
        }
    }

    if (addr == 0xFFF9153F || addr == 0xFFFF153F) {
        static int s_xtrace = 0;
        if (s_xtrace++ < 40) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "XLTPATH addr=0x%08X seg=%X segBase=%08lX gRDRAM=%p",
                addr, (addr >> 24) & 0x0F,
                (unsigned long)s_rdp.segmentBase[(addr >> 24) & 0x0F],
                (void*)gN64_RDRAM);
        }
    }

    if (addr >= 0x60000000u && addr < 0x80000000u) { uint64_t c = 0x7900000000ULL | (uint64_t)addr; if (bka_is_readable((void*)c)) return (uint8_t*)c; c = 0x7A00000000ULL | (uint64_t)addr; if (bka_is_readable((void*)c)) return (uint8_t*)c; }
    // Try exact-match mapping table first.
    void* p = bka_lookup_addr_mapping(addr);
    if (p) return (uint8_t*)p;
    /* DKR: DL pointers are truncated 64-bit heap addrs. Top byte 0x20-0x3F = truncation of 0x7D... */
    if ((((addr)>>24)&0xFF) >= 0x20 && (((addr)>>24)&0xFF) <= 0x3F) {
        uint64_t _cdkr = 0x7D00000000ULL | (uint64_t)(addr);
        if (bka_is_readable((void*)_cdkr)) {
            static int _d = 0;
            if (_d++ < 30) __android_log_print(ANDROID_LOG_ERROR, "BKA-DKR7D", "addr=0x%08X -> 0x%llX", (unsigned)(addr), (unsigned long long)_cdkr);
            return (uint8_t*)_cdkr;
        }
    }

    // Fallback: reconstruct the 64-bit host pointer from a 32-bit truncation.
    // Every allocation in this build lands in the 0x72xxxxxxxx / 0x73xxxxxxxx
    // heap range. If the low32 matches an allocation, use it directly.
    // Reconstruct only when the low32 looks like a real heap pointer.
    // Flag/length fields (e.g. G_VTX's 0xFFFF153F) must not be treated
    // as truncated host addresses.
    // Skip addresses that live inside RDRAM (below 16 MB) and flag/\n    // length fields (above 0x7F000000). Only reconstruct in between.
    // Segment-relative translation.
    {
        uint32_t seg = (addr >> 24) & 0x0F;
        if (seg >= 1 && seg <= 15) {
            uintptr_t base = s_rdp.segmentBase[seg];
            if (base != 0 && base != (uintptr_t)-1) {
                uint32_t off = addr & 0x00FFFFFFu;
                // Case A: heap low-32 (0x60..0x7F): map lookup on base, add off.
                if (base < 0x80000000u) {
                    void* mapped = bka_lookup_addr_mapping((uint32_t)base);
                    if (mapped) {
                        uintptr_t cand = (uintptr_t)mapped + off;
                        uintptr_t mend = bka_get_mapped_end(mapped);
                        /* Bounds check: B-K segment buffers are small (a few
                         * hundred KB).  Without this,  values in the
                         * multi-MB range walked past the containing mmap
                         * region; bka_is_readable(cand) only checked byte 0,
                         * so we silently read zeros (or faulted) instead of
                         * refusing the resolution.  64 = max read width for
                         * this API (matrix / Vtx / DL stride). */
                        if (off >= 0x100000) {
                            static int s_bigoff = 0;
                            if (s_bigoff++ < 40)
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-BIGOFF",
                                    "seg=%u base=0x%08X off=0x%06X mapped=%p cand=%p mend=%p",
                                    seg, (uint32_t)base, off, mapped,
                                    (void*)cand, (void*)mend);
                        }
                        if (mend != 0 && cand + 64 <= mend &&
                            bka_is_readable((void*)cand)) {
                            return (uint8_t*)cand;
                        }
                    }
                }
                // Case B: physical RDRAM (0x00..0x03FFFFFF).
                if (base < 0x04000000u && gN64_RDRAM) {
                    uint32_t phys = (uint32_t)base + off;
                    if (phys < 0x04000000u) return gN64_RDRAM + phys;
                }
                // Case C: KSEG0 (0x80..0x83FFFFFF).
                if (base >= 0x80000000u && base < 0x84000000u && gN64_RDRAM) {
                    uint32_t phys = (uint32_t)(base - 0x80000000u) + off;
                    if (phys < 0x04000000u) return gN64_RDRAM + phys;
                }
                // Case D (port): gSPSegment(3, osVirtualToPhysical(host_ptr)) leaves the
                // low-32 of a host pointer (>= 0x84000000) as the segment base.
                if (base >= 0x84000000u && base < 0xA0000000u) {
                    uintptr_t cand = 0;
                    void* mapped = bka_lookup_addr_mapping((uint32_t)base);
                    if (mapped) cand = (uintptr_t)mapped + off;
                    else {
                        uintptr_t g = 0x7900000000ULL | (uintptr_t)base;
                        if (bka_is_readable((void*)g)) cand = g + off;
                    }
                    { static int s_s3 = 0; if (s_s3++ < 8)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-SEG3",
                            "base=0x%08X off=0x%X mapped=%p cand=%p",
                            (uint32_t)base, (unsigned)off, mapped, (void*)cand); }
                    if (cand && bka_is_readable((void*)cand)) return (uint8_t*)cand;
                }
            }
        }
    }
        if ((addr >= 0x20000000ULL && addr < 0x22F00000ULL) || (addr >= 0x70000000ULL && addr < 0x7F000000ULL)) {
        uint64_t cand72 = (addr >= 0x70000000ULL ? 0x7900000000ULL : 0x7200000000ULL) | (uint64_t)addr;
        if (bka_is_readable((void*)cand72)) {
            static int rec_log72 = 0;
            if (rec_log72++ < 6) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "XLT RECONSTRUCT 0x%08X -> %p (0x72 base)", addr, (void*)cand72);
            }
            return (uint8_t*)cand72;
        }
        uint64_t cand73 = (addr >= 0x70000000ULL ? 0x7A00000000ULL : 0x7300000000ULL) | (uint64_t)addr;
        if (bka_is_readable((void*)cand73)) {
            static int rec_log73 = 0;
            if (rec_log73++ < 6) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "XLT RECONSTRUCT 0x%08X -> %p (0x73 base)", addr, (void*)cand73);
            }
            return (uint8_t*)cand73;
        }
    }

    // By-low32 lookup: only meaningful for values that could plausibly be
    // truncated heap pointers (>=16 MB, <2 GB).  Small values like 0x800 are
    // never truncated heap addresses; matching them just picks up random
    // allocations whose offset happens to end in those low bits.
    if (addr >= 0x10000000u && addr < 0x80000000u) {
        void* byLow = bka_lookup_addr_by_low32(addr);
        if (byLow) {
            static int b1 = 0;
            if (b1++ < 8) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "XLT LOW32MATCH addr=0x%08X -> %p", addr, byLow);
            return (uint8_t*)byLow;
        }
    }
    // Prefix reconstruction with a "looks like vertex data" heuristic.
    // Accept a candidate iff:
    //   - first 16 bytes are not all-zero and not all-identical, AND
    //   - the first three s16 fields (x,y,z) are in a plausible range.
    if ((addr & 0xFF000000u) == 0xFF000000u || (addr & 0xC0000000u) == 0xC0000000u) {
        for (uint64_t pfx = 0x7000000000ULL; pfx <= 0x7F00000000ULL; pfx += 0x0100000000ULL) {
            uint64_t cand = pfx | (uint64_t)addr;
            if (!bka_is_readable((void*)cand)) continue;
            uint8_t* b = (uint8_t*)cand;

            int allZero = 1, allSame = 1;
            uint8_t v0 = b[0];
            for (int k = 0; k < 16; k++) {
                if (b[k] != 0) allZero = 0;
                if (b[k] != v0) allSame = 0;
            }
            if (allZero || allSame) continue;

            int16_t x = (int16_t)(b[0] | (b[1] << 8));
            int16_t y = (int16_t)(b[2] | (b[3] << 8));
            int16_t z = (int16_t)(b[4] | (b[5] << 8));
            if (x < -2048 || x > 2048) continue;
            if (y < -2048 || y > 2048) continue;
            if (z < -2048 || z > 2048) continue;

            static int b2 = 0;
            if (b2++ < 12) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "VTXRECON addr=0x%08X pfx=0x%llX -> %p x=%d y=%d z=%d",
                addr, (unsigned long long)(pfx >> 32), (void*)cand, x, y, z);
            return (uint8_t*)cand;
        }
        static int b3 = 0;
        if (b3++ < 3) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "VTXRECON FAILED addr=0x%08X (no plausible prefix)", addr);
    }

    if (addr == 0x7A7B0620) {
        LOGV("XLT TRACE about to miss addr=0x%08X", addr);
    }

    // Diagnostic: log misses so we can see what the game asked for
    static int miss_log = 0;
    if (miss_log++ < 12) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "XLT miss addr=0x%08X map_size=?", addr);
    }

    if (addr == 0x7A7B0620) {
        LOGV("XLT TRACE reached segment block addr=0x%08X", addr);
    }

    // Segment address (F3DEX_GBI)
    uint32_t seg = (addr >> 24) & 0x0F;
    uint32_t off = addr & 0x00FFFFFF;
    { static int s_sc = 0; if (s_sc++ < 6) LOGV("SEGCHK addr=0x%08X seg=%u segB1=0x%lX dlbase=0x%lX cur=%p", addr, seg, (unsigned long)s_rdp.segmentBase[1], (unsigned long)s_dl_base, (void*)s_current_cmd); }
    if (seg == 1 && s_rdp.segmentBase[1] == 0x80000000u && s_current_cmd) { uintptr_t c = (uintptr_t)s_current_cmd; uintptr_t cands[6] = { c - 0x1D0, c - 0x1C0, c - 0x1A0, c - 0xC80, c - 0x400, c - 0x200 }; for (int k = 0; k < 6; k++) { uintptr_t v = cands[k]; if (!bka_is_readable((void*)v)) continue; uint8_t* b = (uint8_t*)v; int hits = 0; for (int i = 0; i < 8; i++) { uint8_t* q = b + i*16; if (!bka_is_readable((void*)(q+16))) break; int16_t z = (int16_t)((q[4] << 8) | q[5]); int cn = (q[12] == 0x80 && q[13] == 0x7F) || (q[12] == 0x7F && q[13] == 0x80); if (z == 0 && cn) hits++; } static int s_v = 0; if (s_v++ < 20) LOGV("SEG1VTX2 k=%d cand=0x%lX hits=%d/8 cur=0x%lX", k, (unsigned long)v, hits, (unsigned long)c); if (hits >= 3) return (uint8_t*)(v + off); } }
    if (seg != 0 && s_rdp.segmentBase[seg] != 0) {
        uint32_t base = s_rdp.segmentBase[seg];
        void *base_pm = bka_lookup_addr_mapping(base);
        if (base_pm) {
            if (addr == 0xFFF9153F || addr == 0xFFFF153F)
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "XLTPATH-USE segbase (base=%08X off=%X)", base, off);
            return (uint8_t*)base_pm + off;
        }
        uint32_t combined = base + off;
        void *pm = bka_lookup_addr_mapping(combined);
        if (pm) {
            if (addr == 0xFFF9153F || addr == 0xFFFF153F)
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "XLTPATH-USE combined (combined=%08X)", combined);
            return (uint8_t*)pm;
        }
        if (combined < 0x4000000u && gN64_RDRAM) {
            if (addr == 0xFFF9153F || addr == 0xFFFF153F)
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "XLTPATH-USE combined-rdram (combined=%08X)", combined);
            return gN64_RDRAM + combined;
        }
        if (combined >= 0x80000000u && combined < 0x84000000u && gN64_RDRAM) {
            uint32_t off2 = combined - 0x80000000u;
            if (off2 < 0x800000u) {   // must be within an 8 MB RDRAM
                return gN64_RDRAM + off2;
            }
        }
        if (combined >= 0xA0000000u && combined < 0xA4000000u && gN64_RDRAM) {
            uint32_t off2 = combined - 0xA0000000u;
            if (off2 < 0x04000000u) {
                if (addr == 0xFFF9153F || addr == 0xFFFF153F)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "XLTPATH-USE kseg1 (combined=%08X off=%X)", combined, off2);
                return gN64_RDRAM + off2;
            }
        }
        // fall through if none matched
    }

    // Direct physical RDRAM fallback
    if ((addr < 0x4000000u || (addr >= 0x80000000u && addr < 0x84000000u) || (addr >= 0xA0000000u && addr < 0xA4000000u)) && gN64_RDRAM) {
        uint32_t off = addr & 0x00FFFFFFu;
        if (off < 0x04000000u) return gN64_RDRAM + off;
    }

    // Only allow the "low addr is RDRAM offset" fallback for small addresses.
    // Anything >= 0x10000000 that got here is a bogus lookup key; return null
    // so the caller logs a real failure instead of reading random RDRAM.
    if (gN64_RDRAM && addr < 0x10000000u) {
        uint32_t off2 = addr & 0x00FFFFFF;
        return gN64_RDRAM + off2;
    }

    static int miss2 = 0;
    if (miss2++ < 20) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "XLT GIVEUP addr=0x%08X (not in map, not segment, not RDRAM)", addr);
    }
    return nullptr;
}

static int s_frameCount = 0;

// =======================================================================
// Helpers
// =======================================================================

static inline uint32_t RDP_BPP(uint32_t size) {
    static const uint32_t bpp[] = {0, 1, 2, 2};
    return bpp[size & 3];
}

static inline uint16_t RGBA8_TO_RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline int16_t read_int16(const uint8_t* ptr) {
    return (int16_t)((ptr[0] << 8) | ptr[1]);
}

// =======================================================================
// RDP State Management
// =======================================================================

extern "C" size_t resource_rom_size(void);

static void RDP_InitState() {
    // Seed matrices to identity so the first MUL/PUSH ops are well-defined.
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            s_rdp.projection[i][j] = s_rdp.modelview[i][j] =
                s_rdp.viewProj[i][j] = (i == j) ? 1.0f : 0.0f;
    s_rdp.modelviewStackDepth = 0;
    for (int _si = 0; _si < 3; _si++)
        for (int _i = 0; _i < 4; _i++) for (int _j = 0; _j < 4; _j++)
            s_dkrMvpSlots[_si][_i][_j] = (_i == _j) ? 1.0f : 0.0f;
    s_dkrActiveSlot = 0;

    // Preserve vertex buffer and count across display list tasks.
    static BKVertex saved_dmem[DMEM_VERTEX_COUNT];
    static int saved_dmemVertexCount = 0;
    memcpy(saved_dmem, s_rdp.dmem, sizeof(saved_dmem));
    saved_dmemVertexCount = s_rdp.dmemVertexCount;
    // Preserve texture state across task boundary (2026-09-23).
    static uint8_t  saved_tmem[4096];
    static uint8_t  saved_tiles[sizeof(s_rdp.tiles)];
    static uint8_t* saved_texAddr = nullptr;
    static uint32_t saved_texWidth = 0, saved_texFmt = 0, saved_texSize = 0;
    static int      saved_activeTile = 0, saved_textureEnabled = 0;
    memcpy(saved_tmem, s_rdp.tmem, sizeof(saved_tmem));
    memcpy(saved_tiles, &s_rdp.tiles, sizeof(saved_tiles));
    saved_texAddr = s_rdp.texAddr;
    saved_texWidth = s_rdp.texWidth;
    saved_texFmt = s_rdp.texFmt;
    saved_texSize = s_rdp.texSize;
    saved_activeTile = s_rdp.activeTile;
    saved_textureEnabled = s_rdp.textureEnabled;

    // Clear all state except vertices.
    memset(&s_rdp, 0, sizeof(s_rdp));
    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX", "INIT-MEMSET-RAN");
    
    // Re-seed matrices to identity so the first MUL/PUSH ops are well-defined.
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            s_rdp.projection[i][j] = s_rdp.modelview[i][j] =
                s_rdp.viewProj[i][j] = (i == j) ? 1.0f : 0.0f;
    s_rdp.modelviewStackDepth = 0;

    // Re-seed segment bases — the memset above wipes them.
    // These match the defaults set by the RDPStateDefaultSegments constructor.
    s_rdp.segmentBase[0x00] = 0x00000000u;
    s_rdp.segmentBase[0x01] = 0x80000000u;
    s_rdp.segmentBase[0x02] = 0x80000000u;
    s_rdp.segmentBase[0x03] = 0x80000000u;
    s_rdp.segmentBase[0x04] = 0x08000000u;
    s_rdp.segmentBase[0x05] = 0x08000000u;
    s_rdp.segmentBase[0x06] = 0x08000000u;
    s_rdp.segmentBase[0x07] = 0x08000000u;
    s_rdp.segmentBase[0x08] = 0x08000000u;
    s_rdp.segmentBase[0x09] = 0x08000000u;
    s_rdp.segmentBase[0x0A] = 0x08000000u;
    s_rdp.segmentBase[0x0B] = 0x08000000u;
    s_rdp.segmentBase[0x0C] = 0x0C000000u;
    s_rdp.segmentBase[0x0D] = 0x80000000u;
    s_rdp.segmentBase[0x0E] = 0x80000000u;
    s_rdp.segmentBase[0x0F] = 0x80000000u;

    // Restore vertices and count.
    memcpy(s_rdp.dmem, saved_dmem, sizeof(saved_dmem));
    s_rdp.dmemVertexCount = saved_dmemVertexCount;
    // Restore texture state (2026-09-23).
    memcpy(s_rdp.tmem, saved_tmem, sizeof(saved_tmem));
    memcpy(&s_rdp.tiles, saved_tiles, sizeof(saved_tiles));
    s_rdp.texAddr = saved_texAddr;
    s_rdp.texWidth = saved_texWidth;
    s_rdp.texFmt = saved_texFmt;
    s_rdp.texSize = saved_texSize;
    s_rdp.activeTile = saved_activeTile;
    s_rdp.textureEnabled = saved_textureEnabled;

    s_rdp.primR = s_rdp.primG = s_rdp.primB = s_rdp.primA = 255;
    s_rdp.envR = s_rdp.envG = s_rdp.envB = s_rdp.envA = 255;
    s_rdp.blendR = s_rdp.blendG = s_rdp.blendB = 255; s_rdp.blendA = 255;
    s_rdp.fillR = s_rdp.fillG = s_rdp.fillB = 255; s_rdp.fillA = 255;
    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX", "INIT-FILL-SET (%d,%d,%d)", s_rdp.fillR, s_rdp.fillG, s_rdp.fillB);
    s_rdp.fogR = s_rdp.fogG = s_rdp.fogB = 255; s_rdp.fogA = 255;
    // s_rdp.activeTile preserved above — do NOT reset
    // s_rdp.textureEnabled preserved above — do NOT reset
    s_rdp.matrixMode = 0;

    // Initialize matrices to identity
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            s_rdp.projection[i][j] = s_rdp.modelview[i][j] = (i == j) ? 1.0f : 0.0f;
}

// =======================================================================
// Matrix Operations
// =======================================================================

static void Matrix_Identity(BKMatrix m) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

static void Matrix_MultVec(const BKMatrix m, float x, float y, float z, float w,
                           float* ox, float* oy, float* oz, float* ow) {
    /* N64 Mtx layout has translation in ROW 3 (verified from MTXFULL dump:
     * modelview row 3 = (15991, 15306, 16305, 1)). The correct transform
     * of a column vector under this layout is v' = v * M (row-vector
     * times matrix), equivalently M^T * v. Under this form the w output
     * stays 1 for a rigid transform, and translation is applied via
     * column 3. */
    *ox = m[0][0]*x + m[1][0]*y + m[2][0]*z + m[3][0]*w;
    *oy = m[0][1]*x + m[1][1]*y + m[2][1]*z + m[3][1]*w;
    *oz = m[0][2]*x + m[1][2]*y + m[2][2]*z + m[3][2]*w;
    *ow = m[0][3]*x + m[1][3]*y + m[2][3]*z + m[3][3]*w;
}

// Load N64 fixed-point matrix (int16_t[4][4] with 32-bit integer parts)
static void Matrix_Multiply(BKMatrix result, const BKMatrix a, const BKMatrix b);
static void bka_recompute_matrices(void);
static void Matrix_LoadFromN64(BKMatrix* out, const void* src) {
    /* Decode the LE-packed layout produced by src/hasm/math_util.c:mtxf_to_mtx
     * on ARM64. The reference C code writes:
     *   *ai++ = (int_e1 << 16) | int_e2
     *   *af++ = (frac_e1 << 16) | frac_e2
     * where e1 = mf[i][j], e2 = mf[i][j+1], j iterates 0,2 per row.
     * As LE s32 stores, bytes 0-1 = int_e2 (LE), bytes 2-3 = int_e1 (LE).
     * Same for af (offsets 32-63). Previous decode read BE int16 pairs at
     * even offsets, getting byteswap + column-swap wrong. */
    { static int _f64 = 0; if (_f64++ < 8) {
        const uint8_t* _p = (const uint8_t*)src;
        diag_log("BKA-MTX-FULL64",
            "src=%p 00=%02X%02X%02X%02X 04=%02X%02X%02X%02X 08=%02X%02X%02X%02X "
            "20=%02X%02X%02X%02X 24=%02X%02X%02X%02X 28=%02X%02X%02X%02X",
            (void*)src,
            _p[0],_p[1],_p[2],_p[3], _p[4],_p[5],_p[6],_p[7],
            _p[8],_p[9],_p[10],_p[11],
            _p[32],_p[33],_p[34],_p[35], _p[36],_p[37],_p[38],_p[39],
            _p[40],_p[41],_p[42],_p[43]); } }
    const uint8_t* b = (const uint8_t*)src;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j += 2) {
            int pair_idx = i * 2 + j / 2;
            int ai_off = pair_idx * 4;
            int af_off = 32 + pair_idx * 4;
            int16_t int_j1  = (int16_t)(b[ai_off]   | (b[ai_off+1]   << 8));
            int16_t int_j   = (int16_t)(b[ai_off+2] | (b[ai_off+3]   << 8));
            int16_t frac_j1 = (int16_t)(b[af_off]   | (b[af_off+1]   << 8));
            int16_t frac_j  = (int16_t)(b[af_off+2] | (b[af_off+3]   << 8));
            (*out)[i][j]   = (float)int_j  + (float)frac_j  / 65536.0f;
            (*out)[i][j+1] = (float)int_j1 + (float)frac_j1 / 65536.0f;
        }
    }
}

// =======================================================================
// Texture Fetch from TMEM
// =======================================================================

static void RDP_FetchTexel(int tile, uint32_t s, uint32_t t, uint8_t* outRGBA) {
    /* PORT: DKR passes 16.16 fixed-point texture coordinates.
       Shifting by 5 (10.5 format) caused rapid wrapping (stripes).
       Shift by 16 for correct pixel mapping. */
    uint32_t u = s >> 5, v = t >> 5;
    auto& tdesc = s_rdp.tiles[tile];
    uint32_t texW = (tdesc.sh >> 2) + 1, texH = (tdesc.th >> 2) + 1;
    if (texW == 0) texW = 1;
    if (texH == 0) texH = 1;

    /* PORT: texW is 56 (not a power of 2). Bitwise AND aliases heavily.
       Use modulo for correct wrapping. */
    if (tdesc.clampS) { if (u >= texW) u = texW - 1; } else { u %= texW; }
    if (tdesc.clampT) { if (v >= texH) v = texH - 1; } else { v %= texH; }

    uint32_t bppBits = 4u << (tdesc.size & 3);

    uint32_t _line = tdesc.line; if (_line == 0) { uint32_t _bpp = 4u << (tdesc.size & 3); _line = (texW * _bpp) / 64u; if (_line == 0) _line = 1; } uint32_t rowWord = (tdesc.tmemAddr + v * _line) & 0x1FFu;
    { static int _fllog = 0; if (_fllog++ < 10) __android_log_print(ANDROID_LOG_ERROR, "BKA-LINEFIX", "tile=%d tdesc.line=%u _line=%u texW=%u texH=%u u=%u v=%u rowWord=%u", tile, tdesc.line, _line, texW, texH, u, v, rowWord); }
    uint32_t bitInRow  = u * bppBits;
    uint32_t wordInRow = bitInRow >> 6;
    uint32_t bitInWord = bitInRow & 63u;
    uint32_t word      = (rowWord + wordInRow) & 0x1FFu;
    // PORT: Disabled N64 parity swap. PNGs are linear, so we skip the hardware swizzle.
    if ((tdesc.size & 3) >= 2 && (v & 1)) word ^= 1u;
    uint32_t byteAddr  = word * 8u + (bitInWord >> 3);

    if (byteAddr >= 4096) { outRGBA[0]=outRGBA[1]=outRGBA[2]=outRGBA[3]=0; return; }

    {
        static int s_td = 0;
        /* port 2026-10-11: only log tile samples once its sh has
           been configured (i.e. after the preceding SetTileSize
           or SetTile has run).  The previous cap of 12 filled up
           on frame-1 pre-load samples, so we never saw the state
           the sampler reads after the texture was uploaded. */
        if (0) { /* SILENCED: log evictor */
            __android_log_print(ANDROID_LOG_ERROR, "BKA-TEX",
                "TILEDESC tile=%d fmt=%u siz=%u line=%u tmem=%u pal=%u sh=%u th=%u uv=(%u,%u) bpp=%u word=%u byte=%u raw=%02X%02X%02X%02X%02X%02X%02X%02X",
                tile, tdesc.format, tdesc.size, tdesc.line, tdesc.tmemAddr,
                tdesc.palette, tdesc.sh, tdesc.th, u, v, bppBits, word, byteAddr,
                s_rdp.tmem[(byteAddr+0) & 0xFFF], s_rdp.tmem[(byteAddr+1) & 0xFFF],
                s_rdp.tmem[(byteAddr+2) & 0xFFF], s_rdp.tmem[(byteAddr+3) & 0xFFF],
                s_rdp.tmem[(byteAddr+4) & 0xFFF], s_rdp.tmem[(byteAddr+5) & 0xFFF],
                s_rdp.tmem[(byteAddr+6) & 0xFFF], s_rdp.tmem[(byteAddr+7) & 0xFFF]);
        }
    }

    switch (tdesc.size & 3) {
    case 0: {
        uint8_t b = s_rdp.tmem[byteAddr];
        uint32_t idx = (bitInWord & 4u) ? (b & 0xFu) : (b >> 4);
        if (tdesc.format == 2) {
            /* ARM64 port 2026-10-15: sampler reads bank 0.
               Every LoadTLUT in the RARE-logo bin issues tmem=256
               (1472 calls, all tmemBase=2048 bytes = word 256 = bank 0).
               The render tile pal field (4) points at bank 4 which is
               never loaded, so reading tdesc.palette*4 yields
               uninitialized TMEM and the particle draw path faults.
               Load and sample must agree: bank 0. */
            uint32_t palWord = 256u + (idx >> 2);
            uint32_t palByte = (palWord & 0x1FFu) * 8u + ((idx & 3u) * 2u);
            if (palByte + 1 >= 4096) { outRGBA[0]=outRGBA[1]=outRGBA[2]=outRGBA[3]=0; return; }
            uint16_t p = ((uint16_t)s_rdp.tmem[palByte] << 8) | s_rdp.tmem[palByte+1];
            outRGBA[0] = ((p >> 11) & 0x1F) << 3;
            outRGBA[1] = ((p >>  6) & 0x1F) << 3;
            outRGBA[2] = ((p >>  1) & 0x1F) << 3;
            outRGBA[3] = (p & 1) ? 255 : 0;
            { static int s_rb = 0; if (s_rb++ < 40)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-TEXRES",
                    "TEXRES pal=%u idx=%u palWord=%u palByte=%u p=%04X rgba=%02X%02X%02X%02X",
                    tdesc.palette, idx, palWord, palByte, p,
                    outRGBA[0],outRGBA[1],outRGBA[2],outRGBA[3]); }
        } else {
            outRGBA[0] = outRGBA[1] = outRGBA[2] = idx << 4;
            outRGBA[3] = idx << 4;
        }
        return;
    }
    case 1: {
        uint8_t b = s_rdp.tmem[byteAddr];
        if (tdesc.format == 2) {
            /* ARM64 port 2026-10-15: sampler reads bank 0 (see CI4 above). */
            uint32_t palWord = 256u + (b >> 2);
            uint32_t palByte = (palWord & 0x1FFu) * 8u + ((b & 3u) * 2u);
            if (palByte + 1 >= 4096) { outRGBA[0]=outRGBA[1]=outRGBA[2]=outRGBA[3]=0; return; }
            uint16_t p = ((uint16_t)s_rdp.tmem[palByte] << 8) | s_rdp.tmem[palByte+1];
            outRGBA[0] = ((p >> 11) & 0x1F) << 3;
            outRGBA[1] = ((p >>  6) & 0x1F) << 3;
            outRGBA[2] = ((p >>  1) & 0x1F) << 3;
            outRGBA[3] = (p & 1) ? 255 : 0;
        } else {
            outRGBA[0] = outRGBA[1] = outRGBA[2] = b;
            outRGBA[3] = b;
        }
        return;
    }
    case 2: {
        if (byteAddr + 1 >= 4096) { outRGBA[0]=outRGBA[1]=outRGBA[2]=outRGBA[3]=0; return; }
        uint16_t p = ((uint16_t)s_rdp.tmem[byteAddr] << 8) | s_rdp.tmem[byteAddr+1];
        if (tdesc.format == 3) {
            outRGBA[0] = outRGBA[1] = outRGBA[2] = (p >> 8) & 0xFF;
            outRGBA[3] = p & 0xFF;
        } else {
            outRGBA[0] = ((p >> 11) & 0x1F) << 3;
            outRGBA[1] = ((p >>  6) & 0x1F) << 3;
            outRGBA[2] = ((p >>  1) & 0x1F) << 3;
            outRGBA[3] = (p & 1) ? 255 : 0;
        }
        return;
    }
    case 3: {
        if (byteAddr + 3 >= 4096) { outRGBA[0]=outRGBA[1]=outRGBA[2]=outRGBA[3]=0; return; }
        outRGBA[0] = s_rdp.tmem[byteAddr];
        outRGBA[1] = s_rdp.tmem[byteAddr+1];
        outRGBA[2] = s_rdp.tmem[byteAddr+2];
        outRGBA[3] = s_rdp.tmem[byteAddr+3];
        return;
    }
    }
}

// =======================================================================
// Triangle Rasterizer (flat shaded, no Z-buffer yet)
// =======================================================================

static int s_triangleCount = 0;
static int s_invalidTriCount = 0;

static int s_synth_test_done = 0;
static void RasterizeTriangle(
    float x0, float y0, float x1, float y1, float x2, float y2,
    uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0,
    uint8_t r1, uint8_t g1, uint8_t b1, uint8_t a1,
    uint8_t r2, uint8_t g2, uint8_t b2, uint8_t a2,
    int16_t s0 = 0, int16_t t0 = 0, int16_t s1 = 0, int16_t t1 = 0, int16_t s2 = 0, int16_t t2 = 0)
{
    {
        static unsigned long s_in = 0, s_out = 0, s_frame = 0;
        s_frame++;
        float mnx = fminf(fminf(x0,x1),x2), mxx = fmaxf(fmaxf(x0,x1),x2);
        float mny = fminf(fminf(y0,y1),y2), mxy = fmaxf(fmaxf(y0,y1),y2);
        bool visible = !(mxx < 0.0f || mnx >= (float)FB_WIDTH || mxy < 0.0f || mny >= (float)FB_HEIGHT);
        if (visible) s_in++; else s_out++;
        if ((s_frame % 500) == 0 && 0) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-NDC",
                "frame=%lu on_screen=%lu off_screen=%lu (%.1f%% visible) last=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) fb=%dx%d",
                s_frame, s_in, s_out,
                (s_in + s_out) ? (100.0 * s_in / (s_in + s_out)) : 0.0,
                x0, y0, x1, y1, x2, y2, FB_WIDTH, FB_HEIGHT);
        }
    }

    { static int s_rt = 0; if (s_rt++ < 30 || s_rt % 100 == 0)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
            "RAST-ENTER #%d s1=%08lX s3=%08lX texEn=%d tile=%d xy=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)",
            s_rt, (unsigned long)s_rdp.segmentBase[1], (unsigned long)s_rdp.segmentBase[3],
            (int)s_rdp.textureEnabled, (int)s_rdp.activeTile,
            x0, y0, x1, y1, x2, y2); }

    {
        static int s_raster_enter = 0;
        if (++s_raster_enter < 5 || s_raster_enter % 5000 == 1) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "RasterizeTriangle #%d ENTER: (%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)",
                s_raster_enter, x0, y0, x1, y1, x2, y2);
        }
    }
    if (!std::isfinite(x0) || !std::isfinite(y0) ||
        !std::isfinite(x1) || !std::isfinite(y1) ||
        !std::isfinite(x2) || !std::isfinite(y2)) {
        return;
    }
    { static int _rf = 0; if (_rf++ < 30)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST-FIRST",
            "#%d xy=(%.0f,%.0f)(%.0f,%.0f)(%.0f,%.0f) rgb0=(%d,%d,%d) texEn=%d",
            _rf, x0, y0, x1, y1, x2, y2, r0, g0, b0, (int)s_rdp.textureEnabled); }

    int _px_written = 0;
    s_triangleCount++;
    if (s_triangleCount % 5000 == 1 || s_triangleCount < 5) {
        __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
            "RasterizeTriangle #%d: (%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f)",
            s_triangleCount, x0, y0, x1, y1, x2, y2);
    }
    // Sort vertices by Y (y0 <= y1 <= y2)
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); std::swap(r0, r1); std::swap(g0, g1); std::swap(b0, b1); std::swap(a0, a1); std::swap(s0, s1); std::swap(t0, t1); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); std::swap(r1, r2); std::swap(g1, g2); std::swap(b1, b2); std::swap(a1, a2); std::swap(s1, s2); std::swap(t1, t2); }
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); std::swap(r0, r1); std::swap(g0, g1); std::swap(b0, b1); std::swap(a0, a1); std::swap(s0, s1); std::swap(t0, t1); }

    // Barycentric UV denominator (2026-09-23): constant for the triangle.
    float t_area = (x1-x0)*(y2-y0) - (x2-x0)*(y1-y0);
    float t_invArea = (t_area != 0.0f) ? (1.0f/t_area) : 0.0f;

    int iy0 = (int)ceilf(y0), iy1 = (int)ceilf(y1), iy2 = (int)ceilf(y2);
    if (iy0 < 0) iy0 = 0;
    if (iy1 < 0) iy1 = 0;                       // bottom-half loop starts here; negative causes OOB write
    if (iy2 > FB_HEIGHT) iy2 = FB_HEIGHT;
    if (iy1 > FB_HEIGHT) iy1 = FB_HEIGHT;
    if (iy0 >= iy2) return;

    int activeFb = getActiveFramebuffer();
    /* PORT: force walker to write to gVideoCurrFramebuffer (what fb_swap
       will promote to gVideoLastFramebuffer, which fillArgb reads).
       g_active_fb_offset is stale; doesn't track fb_swap alternation. */
    extern uint8_t *gVideoCurrFramebuffer;
    uint16_t* fb = gVideoCurrFramebuffer
                   ? (uint16_t*)gVideoCurrFramebuffer
                   : (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
    { static int _bfc = 0; if (_bfc++ < 3)
          __android_log_print(ANDROID_LOG_ERROR, "BKA-FBFORCE",
              "walker fb=%p (curr=%p)", (void*)fb, (void*)gVideoCurrFramebuffer); }
    static int s_pix_total = 0;
    static int s_call = 0;
    s_call++;
    if (s_call <= 3 || s_call % 500 == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
            "call #%d activeFb=%d fb=%p x0y0=%.1f,%.1f x1y1=%.1f,%.1f x2y2=%.1f,%.1f",
            s_call, activeFb, (void*)fb, x0, y0, x1, y1, x2, y2);
    }

    float dy10 = y1 - y0, dy21 = y2 - y1, dy20 = y2 - y0;
    { static int s_geom = 0; if (s_geom++ < 20) __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST", "TRI-GEOM iy0=%d iy1=%d iy2=%d dy10=%.2f dy21=%.2f dy20=%.2f", iy0, iy1, iy2, dy10, dy21, dy20); }
    float dx10 = x1 - x0, dx21 = x2 - x1, dx20 = x2 - x0;
    if (dy10 <= 0.0f && dy20 <= 0.0f) return;

    // Top half (y0 to y1)
    if (dy10 > 0.0f) {
        for (int y = iy0; y < iy1 && y < FB_HEIGHT; y++) {
            float fy = (float)y + 0.5f;
            float st0 = (fy - y0) / dy20;
            float st1 = (fy - y0) / dy10;
            
            float lx = x0 + st0 * dx20;
            float rx = x0 + st1 * dx10;
            if (lx > rx) std::swap(lx, rx);
            
            int ilx = (int)ceilf(lx), irx = (int)ceilf(rx);
            if (ilx < 0) ilx = 0; if (irx > FB_WIDTH) irx = FB_WIDTH;
            
            for (int x = ilx; x < irx; x++) {
                // Flat shading - use average color
                uint8_t r = (uint8_t)(((int)r0 + r1 + r2) / 3);
                uint8_t g = (uint8_t)(((int)g0 + g1 + g2) / 3);
                uint8_t b = (uint8_t)(((int)b0 + b1 + b2) / 3);
                if (s_rdp.textureEnabled) {
                    uint8_t tex[4] = {255,255,255,255};
                    float fx = (float)x + 0.5f;
                    float fy = (float)y + 0.5f;
                    float bw0 = ((x1-fx)*(y2-fy) - (x2-fx)*(y1-fy)) * t_invArea;
                    float bw1 = ((x2-fx)*(y0-fy) - (x0-fx)*(y2-fy)) * t_invArea;
                    float bw2 = 1.0f - bw0 - bw1;
                    int32_t uu = (int32_t)(bw0*(float)s0 + bw1*(float)s1 + bw2*(float)s2);
                    int32_t vv = (int32_t)(bw0*(float)t0 + bw1*(float)t1 + bw2*(float)t2);
                    uint32_t uuc = (uu < 0) ? 0u : (uint32_t)uu;
                    uint32_t vvc = (vv < 0) ? 0u : (uint32_t)vv;
                    { static int s_uv = 0; if (s_uv++ < 30)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
                            "UVTRACE #%d s=(%d,%d,%d) t=(%d,%d,%d) bw=(%.3f,%.3f,%.3f) uu=%d vv=%d",
                            s_uv, s0, s1, s2, t0, t1, t2, bw0, bw1, bw2, uu, vv); }
                    RDP_FetchTexel(s_rdp.activeTile, uuc, vvc, tex);
                    { static int s_txdbg = 0; if (0) { /* SILENCED */
                        auto& td = s_rdp.tiles[s_rdp.activeTile];
                        uint32_t tb = td.tmemAddr * 8;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
                            "SILENCED fmt=%u size=%u line=%u tmemAddr=0x%X sh=%u th=%u "
                            "tmem[%u..%u]=%02X%02X%02X%02X%02X%02X%02X%02X "
                            "texAddr=%p texWidth=%u texFmt=%u texSize=%u",
                            td.format, td.size, td.line, td.tmemAddr, td.sh, td.th,
                            tb, tb+7,
                            s_rdp.tmem[tb+0],s_rdp.tmem[tb+1],s_rdp.tmem[tb+2],s_rdp.tmem[tb+3],
                            s_rdp.tmem[tb+4],s_rdp.tmem[tb+5],s_rdp.tmem[tb+6],s_rdp.tmem[tb+7],
                            (void*)s_rdp.texAddr, s_rdp.texWidth, s_rdp.texFmt, s_rdp.texSize); } }
                    { static int s_tx = 0; if (0)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
                            "TEXFETCH #%d tile=%d texel=%02X%02X%02X%02X vtxin=(%d,%d,%d)",
                            s_tx, s_rdp.activeTile, tex[0], tex[1], tex[2], tex[3], r, g, b); }
                    r = (uint8_t)(((int)r * tex[0]) / 255);
                    g = (uint8_t)(((int)g * tex[1]) / 255);
                    b = (uint8_t)(((int)b * tex[2]) / 255);
                }
            g_bka_pixels_drawn++;
            { static int s_px = 0; if (s_px++ < 500) __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST", "PIXWRITE #%d y=%d x=%d rgb=(%d,%d,%d) rgb565=0x%04X fb=%p rdr=%p ofs=0x%X idx=%d", s_px, (int)y, (int)x, r, g, b, RGBA8_TO_RGB565(r,g,b), (void*)&fb[y*FB_WIDTH+x], (void*)gN64_RDRAM, g_active_fb_offset, (int)(y*FB_WIDTH+x)); }
                bka_guard_write(&fb[y * FB_WIDTH + x], 2, "Raster.fb");
                { *(volatile uint16_t *)&fb[y * FB_WIDTH + x] = RGBA8_TO_RGB565(r, g, b); _px_written++; }
            }
        }
    }
    
    // Bottom half (y1 to y2)
    if (dy21 > 0.0f) {
        for (int y = iy1; y < iy2 && y < FB_HEIGHT; y++) {
            float fy = (float)y + 0.5f;
            float st0 = (fy - y0) / dy20;
            float st1 = (fy - y1) / dy21;
            
            float lx = x0 + st0 * dx20;
            float rx = x1 + st1 * dx21;
            if (lx > rx) std::swap(lx, rx);
            
            int ilx = (int)ceilf(lx), irx = (int)ceilf(rx);
            if (ilx < 0) ilx = 0; if (irx > FB_WIDTH) irx = FB_WIDTH;
            
            for (int x = ilx; x < irx; x++) {
                uint8_t r = (uint8_t)(((int)r0 + r1 + r2) / 3);
                uint8_t g = (uint8_t)(((int)g0 + g1 + g2) / 3);
                uint8_t b = (uint8_t)(((int)b0 + b1 + b2) / 3);
                if (s_rdp.textureEnabled) {
                    uint8_t tex[4] = {255,255,255,255};
                    float fx = (float)x + 0.5f;
                    float fy = (float)y + 0.5f;
                    float bw0 = ((x1-fx)*(y2-fy) - (x2-fx)*(y1-fy)) * t_invArea;
                    float bw1 = ((x2-fx)*(y0-fy) - (x0-fx)*(y2-fy)) * t_invArea;
                    float bw2 = 1.0f - bw0 - bw1;
                    int32_t uu = (int32_t)(bw0*(float)s0 + bw1*(float)s1 + bw2*(float)s2);
                    int32_t vv = (int32_t)(bw0*(float)t0 + bw1*(float)t1 + bw2*(float)t2);
                    uint32_t uuc = (uu < 0) ? 0u : (uint32_t)uu;
                    uint32_t vvc = (vv < 0) ? 0u : (uint32_t)vv;
                    RDP_FetchTexel(s_rdp.activeTile, uuc, vvc, tex);
                    { static int s_tx = 0; if (0)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
                            "TEXFETCH #%d tile=%d texel=%02X%02X%02X%02X vtxin=(%d,%d,%d)",
                            s_tx, s_rdp.activeTile, tex[0], tex[1], tex[2], tex[3], r, g, b); }
                    r = (uint8_t)(((int)r * tex[0]) / 255);
                    g = (uint8_t)(((int)g * tex[1]) / 255);
                    b = (uint8_t)(((int)b * tex[2]) / 255);
                }
                { *(volatile uint16_t *)&fb[y * FB_WIDTH + x] = RGBA8_TO_RGB565(r, g, b); _px_written++; }
            }
        }
    }
    {
            if (_px_written > 0) {
                g_bka_fb_dirty = 1;
                extern uint8_t* gVideoCurrFramebuffer;
                if (gVideoCurrFramebuffer) {
                    uint16_t* _fb = (uint16_t*)gVideoCurrFramebuffer;
                    _fb[0] = 0xDEAD;
                    _fb[320*240-1] = 0xBEEF;
                }
            } static int _rc = 0; if (_rc++ < 20 || _rc % 2000 == 0)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST-COUNT",
            "#%d px_written=%d xy=(%.0f,%.0f)(%.0f,%.0f)(%.0f,%.0f) fb=%p",
            _rc, _px_written, x0,y0,x1,y1,x2,y2, (void*)fb); }
}

// =======================================================================
// Vertex Transform: apply combined matrix + viewport
// =======================================================================

// Returns the clip-space w for a vertex.  Used to reject triangles with
// any vertex at or behind the near plane, which would otherwise divide by
// w <= 0 and produce astronomically large screen coordinates.
struct BkaClipVtx { float x, y, z, w; };

// Full clip-space position (before the perspective divide).
static void ComputeClip(const BKVertex* v, float* cx, float* cy, float* cz, float* cw) {
    float x = (float)v->x, y = (float)v->y, z = (float)v->z;
    float ox, oy, oz, ow;
    Matrix_MultVec(s_rdp.modelview, x, y, z, 1.0f, &ox, &oy, &oz, &ow);
    Matrix_MultVec(s_rdp.projection, ox, oy, oz, ow, cx, cy, cz, cw);
}

// Sutherland-Hodgman clip of a single triangle against w > eps.
// Returns 0, 3, or 4 vertices (the visible polygon).
static int BkaClipNear(const BkaClipVtx in[3], float eps, BkaClipVtx out[4]) {
    int n = 0;
    BkaClipVtx prev = in[2];
    bool prevIn = prev.w > eps;
    for (int i = 0; i < 3; i++) {
        BkaClipVtx cur = in[i];
        bool curIn = cur.w > eps;
        if (curIn != prevIn) {
            float t = (eps - prev.w) / (cur.w - prev.w);
            out[n].x = prev.x + t * (cur.x - prev.x);
            out[n].y = prev.y + t * (cur.y - prev.y);
            out[n].z = prev.z + t * (cur.z - prev.z);
            out[n].w = eps;
            n++;
        }
        if (curIn) out[n++] = cur;
        prev = cur;
        prevIn = curIn;
    }
    return n;
}

static float ComputeClipW(const BKVertex* v) {
    float x = (float)v->x, y = (float)v->y, z = (float)v->z;
    float ox, oy, oz, ow;
    Matrix_MultVec(s_rdp.modelview, x, y, z, 1.0f, &ox, &oy, &oz, &ow);
    float px, py, pz, pw;
    Matrix_MultVec(s_rdp.projection, ox, oy, oz, ow, &px, &py, &pz, &pw);
    return pw;
}

static void TransformVertex(const BKVertex* v, float* sx, float* sy) {
    {
        // Reject garbage matrices (e.g., the 0x7C6D... load) by checking the diagonal.
        // Valid rotation/scale diagonals are <= ~100.0. Garbage is usually > 10000.
        float mv_diag = fabsf(s_rdp.modelview[0][0]) + fabsf(s_rdp.modelview[1][1]) + fabsf(s_rdp.modelview[2][2]);
        if (mv_diag > 500.0f) {
            for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                s_rdp.modelview[i][j] = (i == j) ? 1.0f : 0.0f;
        }
        float pj_diag = fabsf(s_rdp.projection[0][0]) + fabsf(s_rdp.projection[1][1]) + fabsf(s_rdp.projection[2][2]);
        if (pj_diag > 500.0f) {
            for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                s_rdp.projection[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    float x = ((float)v->x * (1.0f/32768.0f)), y = ((float)v->y * (1.0f/32768.0f)), z = ((float)v->z * (1.0f/32768.0f));
    {
        static int s_tv = 0;
        if (s_tv++ < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "TV-IN v=(%.1f,%.1f,%.1f) m00=%.4f m11=%.4f m22=%.4f m32=%.4f m23=%.4f",
                x, y, z,
                s_rdp.projection[0][0], s_rdp.projection[1][1],
                s_rdp.projection[2][2], s_rdp.projection[3][2],
                s_rdp.projection[2][3]);
        }
    }

    {
        static int s_tv_snap = 0;
        if (s_tv_snap++ < 5) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-TVSNAP",
                "PROJ r0=(%.4f %.4f %.4f %.4f) r2=(%.4f %.4f %.4f %.4f) r3=(%.4f %.4f %.4f %.4f)",
                s_rdp.projection[0][0], s_rdp.projection[0][1], s_rdp.projection[0][2], s_rdp.projection[0][3],
                s_rdp.projection[2][0], s_rdp.projection[2][1], s_rdp.projection[2][2], s_rdp.projection[2][3],
                s_rdp.projection[3][0], s_rdp.projection[3][1], s_rdp.projection[3][2], s_rdp.projection[3][3]);
            __android_log_print(ANDROID_LOG_ERROR, "BKA-TVSNAP",
                "MV   r0=(%.4f %.4f %.4f %.4f) r3=(%.4f %.4f %.4f %.4f)",
                s_rdp.modelview[0][0], s_rdp.modelview[0][1], s_rdp.modelview[0][2], s_rdp.modelview[0][3],
                s_rdp.modelview[3][0], s_rdp.modelview[3][1], s_rdp.modelview[3][2], s_rdp.modelview[3][3]);
        }
    }
    // Apply modelview
    float ox, oy, oz, ow;
    {
        if (bka_should_force_identity_mv()) {
            ox = x; oy = y; oz = z; ow = 1.0f;
        } else {
            Matrix_MultVec(s_rdp.modelview, x, y, z, 1.0f, &ox, &oy, &oz, &ow);
        }
    }

    {
        static int s_v = 0;
        if (s_v++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-VIEW",
                "world=(%d,%d,%d) view=(%.1f,%.1f,%.1f) w=%.4f",
                (int)v->x, (int)v->y, (int)v->z,
                ox, oy, oz, ow);
    }
    float view_x = ox, view_y = oy, view_z = oz, view_w = ow;
    // Apply projection — use locals to avoid aliasing input/output
    float px, py, pz, pw;
    Matrix_MultVec(s_rdp.projection, ox, oy, oz, ow, &px, &py, &pz, &pw);
    ox = px; oy = py; oz = pz; ow = pw;

    {
        static int s_txfm = 0;
        if (s_txfm++ < 5) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-TXFM",
                "world=(%.0f,%.0f,%.0f) view=(%.1f,%.1f,%.1f) wv=%.3f | clip=(%.1f,%.1f,%.1f) wc=%.3f | ndc=(%.3f,%.3f)",
                ((float)v->x * (1.0f/32768.0f)), ((float)v->y * (1.0f/32768.0f)), ((float)v->z * (1.0f/32768.0f)),
                view_x, view_y, view_z, view_w,
                px, py, pz, pw,
                (pw != 0.0f) ? (px / pw) : 0.0f,
                (pw != 0.0f) ? (py / pw) : 0.0f);
        }
    }

    {
        static int s_rd = 0;
        if (s_rd++ < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "PRE-DIV post-proj ox=%.2f oy=%.2f oz=%.2f ow=%.4f",
                ox, oy, oz, ow);
        }
    }
    {
        static int s_dv = 0;
        if (s_dv++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-VERT",
                "V world=(%.0f,%.0f,%.0f) view=(%.1f,%.1f,%.1f) w=%.1f "
                "clip=(%.1f,%.1f,%.1f,%.1f)",
                ((float)v->x * (1.0f/32768.0f)), ((float)v->y * (1.0f/32768.0f)), ((float)v->z * (1.0f/32768.0f)),
                ox, oy, oz, ow,
                px, py, pz, pw);
    }
    // Perspective divide — MUST use clip.w (pw), not view.w (ow).
    // Forgetting this leaves coordinates in clip space (which for a
    // vertex 500 units away is ~500x too large), and every triangle
    // gets clamped to the clip box edge.
    {
        float inv = (fabsf(pw) > 0.0001f) ? (1.0f / pw) : 0.0f;
        ox = px * inv;
        oy = py * inv;
        oz = pz * inv;
    }

    // Viewport transform: NDC [-1,1] → screen [0,FB_WIDTH/HEIGHT]
    *sx = (ox + 1.0f) * 0.5f * (float)FB_WIDTH;
    *sy = (1.0f - oy) * 0.5f * (float)FB_HEIGHT;

    static int s_off = 0;
    if ((*sx < 0 || *sx > FB_WIDTH || *sy < 0 || *sy > FB_HEIGHT) && s_off++ < 5) {
        __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
            "TransformVertex: off-screen (%.1f, %.1f) orig(%.1f,%.1f,%.1f) ndc(%.3f,%.3f)",
            *sx, *sy, x, y, z, ox, oy);
    }
}

// =======================================================================
// Command Handlers
// =======================================================================

/* PORT probe counters — must be declared before any Cmd_* function
   that increments them (Cmd_SetTImg is at ~1377, well before Cmd_LoadBlock). */
static int s_gfx_lb_count = 0;
static int s_gfx_lt_count = 0;
static int s_gfx_sti_count = 0;
extern "C" int gfx_loadblock_count() { return s_gfx_lb_count; }
extern "C" int gfx_loadtile_count() { return s_gfx_lt_count; }
extern "C" int gfx_settimg_count() { return s_gfx_sti_count; }

static void Cmd_SetPrimColor(GfxCommand cmd) {
    s_rdp.primR = (cmd.w1 >> 24) & 0xFF;
    s_rdp.primG = (cmd.w1 >> 16) & 0xFF;
    s_rdp.primB = (cmd.w1 >> 8) & 0xFF;
    s_rdp.primA = cmd.w1 & 0xFF;
}

static void Cmd_SetEnvColor(GfxCommand cmd) {
    s_rdp.envR = (cmd.w1 >> 24) & 0xFF;
    s_rdp.envG = (cmd.w1 >> 16) & 0xFF;
    s_rdp.envB = (cmd.w1 >> 8) & 0xFF;
    s_rdp.envA = cmd.w1 & 0xFF;
}

static void Cmd_SetFillColor(GfxCommand cmd) {
    { static int s_fc = 0; if (s_fc++ < 20)
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "SETFILL-ENTRY #%d w0=0x%08X w1=0x%08X", s_fc, cmd.w0, cmd.w1); }
    uint32_t c = cmd.w0 & 0x00FFFFFF;  // F3DEX2: RRGGBB in low 24 bits of w0
    s_rdp.fillR = (c >> 16) & 0xFF;
    s_rdp.fillG = (c >>  8) & 0xFF;
    s_rdp.fillB =  c        & 0xFF;
    s_rdp.fillA = 255;
    { static int s_pf = 0; if (s_pf++ < 20)
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX", "SETFILL-POST #%d fillRGB=(%d,%d,%d)", s_pf, s_rdp.fillR, s_rdp.fillG, s_rdp.fillB); }
}

static void Cmd_SetOtherModeL(GfxCommand cmd) {
    uint32_t length = (cmd.w0 >> 8) & 0xFF, shift = cmd.w0 & 0xFF, data = cmd.w1;
    uint32_t mask = ((1 << (length + 1)) - 1) << shift;
    s_rdp.otherModeL = (s_rdp.otherModeL & ~mask) | ((data << shift) & mask);
}

static void Cmd_SetOtherModeH(GfxCommand cmd) {
    uint32_t length = (cmd.w0 >> 8) & 0xFF, shift = cmd.w0 & 0xFF, data = cmd.w1;
    uint32_t mask = ((1 << (length + 1)) - 1) << shift;
    s_rdp.otherModeH = (s_rdp.otherModeH & ~mask) | ((data << shift) & mask);
}

static void Cmd_SetCombine(GfxCommand cmd) {
    s_rdp.combineMode = (cmd.w0 & 0x00FFFFFF) | (cmd.w1 & 0xFF000000);
}

static void Cmd_Texture(GfxCommand cmd) {
    uint8_t op = (uint8_t)(cmd.w0 >> 24);
    uint32_t enable, tile;
    if (op == 0xBB) {
        // F3DEX (non-2): w0=[BB:8][bowtie:8][level:3][tile:3][on:8]
        // Drift guard: game code (see diddy-kazooie decomp) never emits
        // nonzero level/bowtie and only ever uses G_TX_RENDERTILE.
        // w0 != 0xBB000000|on means the walker landed on non-command bytes.
        if ((cmd.w0 & 0x00FFFF00u) != 0) return;
        enable =  cmd.w0        & 0xFF;
        tile   = (cmd.w0 >>  8) & 0x7;
    } else {
        // F3DEX2 (0xD7): on=w0[16], tile=w0[8]
        enable = (cmd.w0 >> 16) & 0xFF;
        tile   = (cmd.w0 >>  8) & 0xFF;
    }
    s_rdp.textureEnabled = (enable != 0);
    if (tile < 8) s_rdp.activeTile = tile;
    { static int s_ct = 0; if (s_ct++ < 40)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
            "CMDTEX #%d op=%02X enable=%u tile=%u w0=%08X w1=%08X", s_ct, op, enable, tile, cmd.w0, cmd.w1); }
}


static int g_stile_log = 0;
static void diag_log_settile(uint32_t tile, uint32_t fmt, uint32_t siz, uint32_t line, uint32_t tm) {
    if (g_stile_log < 30) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-DIAG-ST",
            "tile=%u fmt=%u siz=%u line=%u tmem=0x%X", tile, fmt, siz, line, tm);
        g_stile_log++;
    }
}

static void Cmd_SetTile(GfxCommand cmd) {
    { static int _ste = 0; if (_ste++ < 2000) diag_log("BKA-ST-ENTRY", "tile=%u w0=%08X w1=%08X", (unsigned)((cmd.w1>>24)&7), cmd.w0, cmd.w1); }
    uint32_t tile = (cmd.w1 >> 24) & 0x7;
    if (tile >= 8) return;
    auto& t = s_rdp.tiles[tile];
    t.format  = (cmd.w0 >> 21) & 0x7;
    t.size    = (cmd.w0 >> 19) & 0x3;
    t.line    = (cmd.w0 >> 9) & 0x1FF;
    t.tmemAddr = cmd.w0 & 0x1FF;
    if (g_bka_force_rgba) {
        t.format = 0;
        t.size = 2;
        /* t.line is set by the F5 command parsing above; don't override */
    }
    t.palette = (cmd.w1 >> 20) & 0xF;
    t.clampT  = (cmd.w1 >> 18) & 0x1;
    t.mirrorT = (cmd.w1 >> 17) & 0x1;
    t.maskT   = (cmd.w1 >> 14) & 0xF;
    t.shiftT  = (cmd.w1 >> 10) & 0xF;
    t.clampS  = (cmd.w1 >>  9) & 0x1;
    t.mirrorS = (cmd.w1 >>  8) & 0x1;
    t.maskS   = (cmd.w1 >>  4) & 0xF;
    t.shiftS  =  cmd.w1        & 0xF;
    if (t.format || t.size || t.line || t.tmemAddr) diag_log("BKA-DIAG-ST", "tile=%u fmt=%u siz=%u line=%u tmem=0x%X", tile, t.format, t.size, t.line, t.tmemAddr);
    { static int s_st = 0; if (s_st++ < 400) __android_log_print(ANDROID_LOG_ERROR, "BKA-SETTILE", "tile=%u fmt=%u siz=%u line=%u tmem=%u pal=%u w0=%08X w1=%08X", tile, t.format, t.size, t.line, t.tmemAddr, t.palette, cmd.w0, cmd.w1); }
}

static void Cmd_SetTileSize(GfxCommand cmd) {
    uint32_t tile = (cmd.w1 >> 24) & 0x7;
    if (tile >= 8) return;
    auto& t = s_rdp.tiles[tile];
    t.sl = (cmd.w0 >> 12) & 0xFFF;
    t.tl = cmd.w0 & 0xFFF;
    t.sh = (cmd.w1 >> 12) & 0xFFF;
    t.th = cmd.w1 & 0xFFF;
}

static void Cmd_SetTImg(GfxCommand cmd) {
    {
        /* PORT: only log real SetTImg commands (width 1..128, valid pixel fmt). */
        uint32_t _realFmt = (cmd.w0 >> 21) & 0x7;
        uint32_t _realSiz = (cmd.w0 >> 19) & 0x3;
        uint32_t _realW   = (cmd.w0 & 0xFFF) + 1;
        if (_realFmt <= 5 && _realSiz <= 3 && _realW >= 1 && _realW <= 128) {
            extern uint8_t* gN64_RDRAM;
            uintptr_t addr = (uintptr_t)s_rdp.texAddr;
            uintptr_t base = (uintptr_t)gN64_RDRAM;
            uint32_t _phys = (addr >= base) ? (uint32_t)(addr - base) : (uint32_t)addr;
            bool is_heap = gN64_RDRAM && (_phys >= g_png_rdram_start) && (_phys < g_png_rdram_end);
            g_bka_force_rgba = is_heap;
            diag_log("BKA-SETIMG-DIAG", "w0=%08X w=%u f=%u s=%u w1=%08X phys=0x%X %s",
                cmd.w0, _realW, _realFmt, _realSiz, cmd.w1, _phys,
                is_heap ? "HEAP(PNG)" : "RDRAM(game)");
        }
    }
    s_gfx_sti_count++;
    /* ARM64 port 2026-10-14: reject bogus SetTImg BEFORE it clobbers
       s_rdp.texAddr. w1==0xFFFFFFFF is a walker-drift artifact: the
       walker lost byte alignment and decoded non-command bytes as
       SetTImg. Without this guard, a subsequent LoadTLUT reads from
       the garbage address this command resolved to. */
    if (cmd.w1 == 0xFFFFFFFFu) {
        static int s_bogus = 0; if (s_bogus++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG-BOGUS",
                "rejected w0=%08X w1=%08X", cmd.w0, cmd.w1);
        /* PORT: preserve previous valid texAddr on bogus SetTImg (do not reset to null) */
        return;
    }
    { static int s_r = 0; if (s_r++ < 60) {
        uint8_t _seg = (cmd.w1 >> 24) & 0x0F;
        if (_seg == 2) {
            uintptr_t _sb = s_rdp.segmentBase[2];
            uint32_t _off = cmd.w1 & 0x00FFFFFF;
            uint32_t _comb = (uint32_t)(_sb + _off);
            uint8_t* _sgres = RDP_TranslateAddr(_comb);
            void* _low = bka_lookup_addr_by_low32(cmd.w1);
            void* _low2 = bka_lookup_addr_by_low32(_comb);
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SEG2-RES",
                "w1=%08X segBase=%016lX off=%06X comb=%08X "
                "RDP(%08X)=%p low(w1)=%p low(comb)=%p",
                cmd.w1, (unsigned long)_sb, _off, _comb,
                _comb, (void*)_sgres, _low, _low2);
        }
    } }
    // SETIMG address: some DLs emit pre-resolved physical addresses, some
    // emit segment-relative. Direct translate handles the first; if it
    // returns null, combine with the segment base and retry.
    /* ARM64 port 2026-10-12: try segment-relative resolution FIRST.
       A segment-encoded address (top nibble 0-7, non-zero segment base)
       must go through the segment table; RDP_TranslateAddr accepts any
       32-bit value and would otherwise return a heap-relative pointer
       into unrelated memory (the "all zeros LoadBlock source" bug).
       Only fall back to RDP_TranslateAddr for real physical N64 addrs. */
    /* ARM64 port 2026-10-12: check low-32-bits mapping FIRST.  The port's
       gbi.h macros register host pointers via bka_add_addr_mapping_c
       keyed on the low 32 bits.  A SetTImg w1 frequently carries one of
       those keys (e.g. 0x000C1626); resolving it via RDP_TranslateAddr
       maps small offsets into gN64_RDRAM and reads debug strings
       ("tweenInd") as texture data. */
    { uint8_t _s2 = (cmd.w1 >> 24) & 0x0F;
      if (_s2 == 2) {
          static int _s2c = 0; if (_s2c++ < 40)
              __android_log_print(ANDROID_LOG_ERROR, "BKA-SEG2",
                  "SETIMG seg2 w1=0x%08X segBase2=0x%08lX low32res=%p segres=%p combined=0x%08X",
                  cmd.w1, (unsigned long)s_rdp.segmentBase[2],
                  bka_lookup_addr_by_low32(cmd.w1),
                  RDP_TranslateAddr((uint32_t)(s_rdp.segmentBase[2] + (cmd.w1 & 0xFFFFFF))),
                  (uint32_t)(s_rdp.segmentBase[2] + (cmd.w1 & 0xFFFFFF)));
      } }
    uint8_t* resolved = nullptr;

    // PORT: PNG range — physical or K0 form
    if (gN64_RDRAM) {
        uint32_t phys = cmd.w1;
        if (phys & 0x80000000u) phys &= 0x0FFFFFFFu;
        if (phys >= g_png_rdram_start && phys < g_png_rdram_end) {
            resolved = gN64_RDRAM + phys;
        }
    }

    if (!resolved)
    {
        uint8_t  seg = (cmd.w1 >> 24) & 0x0F;
        uintptr_t segBase = s_rdp.segmentBase[seg];
        /* ARM64 port 2026-10-12: only use low32-map for host-pointer
           addresses (nibble 8-F).  Segment-encoded addresses (0-7) MUST
           go through the segment table — otherwise a stale mapping for
           key 0x02000260 (registered by the gbi.h gDma macro from an
           unrelated string) shadows the correct segment resolution, and
           LoadTLUT/LoadBlock read ASCII debug strings as texture data. */
        if (seg < 8 && segBase != 0) {
            uint32_t off = cmd.w1 & 0x00FFFFFF;
            /* ARM64 port 2026-10-14: resolve segBase to full pointer first.
               segmentBase[] holds TRUNCATED low-32 host pointers (see
               SEG2 WRITE base=0x79b28625f0 stored as 0xB28625F0). The
               previous code did (uint32_t)(segBase + off) which loses the
               high bits BEFORE the low32 map lookup -- so for offset 0x120
               it looked up key 0xB2862710, which is never registered. The
               correct answer is 0x79b28625F0 + 0x120 = 0x79b2862710.

               Step 1: look up segBase itself via the low32 map to recover
                       the full 64-bit pointer.
               Step 2: add the offset in uintptr_t (no truncation). */
            uint8_t* segBaseFull = (uint8_t*)bka_lookup_addr_by_low32((uint32_t)segBase);
            if (segBaseFull) {
                resolved = segBaseFull + off;
            } else {
                /* Not a registered host pointer. Treat as N64 physical. */
                uint32_t combined = (uint32_t)(segBase + off);
                resolved = RDP_TranslateAddr(combined);
                if (!resolved) resolved = (uint8_t*)bka_lookup_addr_by_low32(combined);
            }
        }
    }
    if (!resolved) {
        resolved = (uint8_t*)bka_lookup_addr_by_low32(cmd.w1);
    }
    if (!resolved) {
        uint8_t  seg = (cmd.w1 >> 24) & 0x0F;
        uintptr_t segBase = s_rdp.segmentBase[seg];
        if (seg < 8 && segBase != 0) {
            uint32_t off = cmd.w1 & 0x00FFFFFF;
            uint32_t combined = (uint32_t)(segBase + off);
            resolved = RDP_TranslateAddr(combined);
            if (!resolved) resolved = (uint8_t*)bka_lookup_addr_by_low32(combined);
        }
    }
    if (!resolved) {
        resolved = RDP_TranslateAddr(cmd.w1);
        static int s_fb = 0; if (s_fb++ < 10)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG",
                "FALLBACK direct w1=%08X -> %p", cmd.w1, (void*)resolved);
    }
    { uint8_t seg = (cmd.w1 >> 24) & 0x0F;
      uint32_t off = cmd.w1 & 0x00FFFFFF;
      uintptr_t segBase = s_rdp.segmentBase[seg];
      uint32_t combined = (uint32_t)(segBase + off);
      uint8_t* viaSeg = RDP_TranslateAddr(combined);
      static int s_sg = 0; if (s_sg++ < 20)
          __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG",
              "SEGCHECK w1=%08X seg=%X off=%06X segBase=%08lX combined=%08X directRes=%p viaSegRes=%p",
              cmd.w1, seg, off, (unsigned long)segBase, combined, (void*)resolved, (void*)viaSeg); }
    // Drift guard (2026-09-23): real gDPSetTextureImage has fmt <= 5
    // and a resolvable image address. Reject anything else — the
    // walker is landing on non-command bytes and previously clobbered
    // s_rdp.texAddr with null, blocking LoadTile from ever running.
    /* ARM64 port 2026-10-12: one-shot byte dump at both resolutions.
       Determines which address holds real texture data. */
    {
        uint8_t seg = (cmd.w1 >> 24) & 0x0F;
        uint32_t off = cmd.w1 & 0x00FFFFFF;
        uintptr_t segBase = s_rdp.segmentBase[seg];
        uint32_t combined = (uint32_t)(segBase + off);
        uint8_t* direct = RDP_TranslateAddr(cmd.w1);
        uint8_t* viaseg = RDP_TranslateAddr(combined);
        static int s_bd = 0;
        if (s_bd++ < 5) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG-BD",
                "SETIMG w1=%08X seg=%X off=%06X segBase=%08lX direct=%p viaseg=%p",
                cmd.w1, seg, off, (unsigned long)segBase, (void*)direct, (void*)viaseg);
            if (direct && bka_is_readable(direct)) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG-BD",
                    "direct[0..15]=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                    direct[0],direct[1],direct[2],direct[3],
                    direct[4],direct[5],direct[6],direct[7],
                    direct[8],direct[9],direct[10],direct[11],
                    direct[12],direct[13],direct[14],direct[15]);
            }
            if (viaseg && bka_is_readable(viaseg)) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG-BD",
                    "viaseg[0..15]=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                    viaseg[0],viaseg[1],viaseg[2],viaseg[3],
                    viaseg[4],viaseg[5],viaseg[6],viaseg[7],
                    viaseg[8],viaseg[9],viaseg[10],viaseg[11],
                    viaseg[12],viaseg[13],viaseg[14],viaseg[15]);
            }
        }
    }
    /* ARM64 port 2026-10-14: reject bogus SetTImg before it clobbers
       s_rdp.texAddr. w1==0xFFFFFFFF is a drift artifact -- the walker
       lost byte-alignment and decoded non-command bytes as SetTImg.
       Without this, a subsequent LoadTLUT reads from garbage. */
    if (cmd.w1 == 0xFFFFFFFFu || (cmd.w1 >> 24) == 0x0Fu) {
        static int s_bogus = 0; if (s_bogus++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG-BOGUS",
                "rejected w0=%08X w1=%08X", cmd.w0, cmd.w1);
        /* PORT: preserve previous valid texAddr on bogus SetTImg (alt path) */
        return;
    }
    { uint32_t _fmt = (cmd.w0 >> 21) & 0x7;
      if (_fmt > 5 || resolved == nullptr) {
          static int s_sr = 0; if (s_sr++ < 15)
              __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG",
                  "SKIP drift w0=%08X w1=%08X fmt=%u resolved=%p",
                  cmd.w0, cmd.w1, _fmt, (void*)resolved);
          /* Clear texAddr so a later LoadTLUT/LoadBlock cannot read
             from a stale pointer. */
          /* PORT: preserve texAddr (do not null) — walker drift, not bad data */
          return;
      } }
    uint32_t fmt = (cmd.w0 >> 21) & 0x7;
    uint32_t siz = (cmd.w0 >> 19) & 0x3;
    uint32_t wd  = (cmd.w0 & 0xFFF) + 1;   // F3DEX: width-1 in bits [11:0]
    if (fmt > 5 && g_bka_dl_cur) {
        const uint8_t* p = g_bka_dl_cur;
        static int s_dft = 0; if (s_dft++ < 8)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG",
                "DRIFT ctx[-16..15]: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                p[-16],p[-15],p[-14],p[-13],p[-12],p[-11],p[-10],p[-9],
                p[-8],p[-7],p[-6],p[-5],p[-4],p[-3],p[-2],p[-1],
                p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
                p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
    }
    { static int s_st = 0; if (s_st++ < 25)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-SETIMG",
            "SETIMG #%d fmt=%u siz=%u wd=%u image=%08X resolved=%p",
            s_st, fmt, siz, wd, cmd.w1, (void*)resolved); }
    s_rdp.texFmt   = fmt;
    s_rdp.texSize  = siz;
    s_rdp.texWidth = wd;
    s_rdp.texAddr  = resolved;
}

static void Cmd_LoadTile(GfxCommand cmd) {
    s_gfx_lt_count++;
    uint32_t tile = 0; /* PORT: force tile 0 (matches render tile) */
    { static int s_lt = 0; if (s_lt++ < 20) __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADTILE", "ENTRY tile=%u w0=%08X w1=%08X texAddr=%p", tile, cmd.w0, cmd.w1, (void*)s_rdp.texAddr); }
    if (tile >= 8) return;
    auto& t = s_rdp.tiles[tile];
    /* PORT: FORCE sl=tl=0. The game passes 1024/1024 in some B2 commands
       (probably sub-tile loads or misdecode) which makes srcOffset point
       32KB past texAddr -- reading ROM code as texture. For a full 56x56
       texture load, sl=tl=0 is correct (source starts at texAddr[0]). */
    uint32_t sl = 0, tl = 0;
    uint32_t sh = (cmd.w1 >> 12) & 0xFFF, th = cmd.w1 & 0xFFF;
    
    { static int _raw_log = 0; if (_raw_log++ < 8)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-LTRAW",
            "w0=%08X w1=%08X (forcing sl=tl=0)", cmd.w0, cmd.w1); }

    /* PORT: Rare's B2 (LoadTile) passes garbage w1 (e.g. 0x04000400).
       We always override sh/th with s_rdp.texWidth set by 0x07. */
    if (s_rdp.texWidth > 0) {
        sh = (s_rdp.texWidth - 1) << 2;
        th = (s_rdp.texWidth - 1) << 2;
    } else {
        if (sh == 0) sh = t.sh;
        if (th == 0) th = t.th;
    }
    
    /* Diagnostic: trace the first 5 copy attempts */
    { static int s_copy = 0; if (s_copy++ < 5)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-COPY",
            "tile=%u sh=%u th=%u texWidth=%u texAddr=%p", 
            tile, sh, th, (sh >> 2) + 1, (void*)s_rdp.texAddr); }
    t.sl = sl; t.tl = tl; t.sh = sh; t.th = th;
    /* PORT: compute lineWords so TEXRECT sampling uses the correct row stride */
    {
        uint32_t _bpp = 4u << (t.size & 3);
        uint32_t _w = (sh >> 2) + 1;
        uint32_t _lb = (_w * _bpp + 7) / 8;
        uint32_t _lw = (_lb + 7) / 8;
        if (_lw == 0) _lw = 1;
        t.line = _lw;
    }

    uint32_t bppBits = 4u << (t.size & 3);
    uint32_t texWidth = (sh >> 2) + 1, texHeight = (th >> 2) + 1;
    uint32_t lineBytes = (texWidth * bppBits + 7) / 8;
    uint32_t lineWords = (lineBytes + 7) / 8;
    uint32_t texSize = lineBytes * texHeight;

    if (s_rdp.texAddr && texSize <= 4096) {
        uint32_t tmemBase = t.tmemAddr * 8;

        /* PORT: Try DKR decompression. ROM textures are compressed with
           one of: LZSS (0x50 0x10), DEFLATE (0x11 0x72/73), GZIP (0x1F 0x8B). */
        uint8_t* tex_src = s_rdp.texAddr;
        uint8_t* decomp_buf = nullptr;
        uint32_t decomp_size = 0;
        uint8_t magic0 = tex_src[0], magic1 = tex_src[1];
        uint32_t expected_size = 0;
        if ((magic0 == 0x50 && magic1 == 0x10) ||
            (magic0 == 0x11 && (magic1 == 0x72 || magic1 == 0x73))) {
            expected_size = ((uint32_t)tex_src[2] << 24) | ((uint32_t)tex_src[3] << 16)
                          | ((uint32_t)tex_src[4] << 8) | (uint32_t)tex_src[5];
        }
        if (magic0 == 0x1F && magic1 == 0x8B) expected_size = 65536;

        { static int _hd = 0; if (_hd++ < 10)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-HDR",
                "magic=%02X%02X expected=%u b0..7=%02X%02X%02X%02X%02X%02X%02X%02X",
                magic0, magic1, expected_size,
                tex_src[0],tex_src[1],tex_src[2],tex_src[3],
                tex_src[4],tex_src[5],tex_src[6],tex_src[7]); }

        if (expected_size >= 512 && expected_size <= 65536) {
            decomp_buf = (uint8_t*)malloc(expected_size);
            if (decomp_buf) {
                decomp_size = decomp_rare_to_offset_safe(tex_src, 65536, decomp_buf, expected_size);
                if (decomp_size > 0) {
                    tex_src = decomp_buf;
                    static int _dd = 0; if (_dd++ < 10)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-TEXDEC",
                            "OK magic=%02X%02X expected=%u got=%u b0..7=%02X%02X%02X%02X%02X%02X%02X%02X",
                            magic0, magic1, expected_size, decomp_size,
                            tex_src[0],tex_src[1],tex_src[2],tex_src[3],
                            tex_src[4],tex_src[5],tex_src[6],tex_src[7]);
                } else {
                    static int _df = 0; if (_df++ < 10)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-TEXDEC",
                            "FAIL magic=%02X%02X expected=%u", magic0, magic1, expected_size);
                    free(decomp_buf);
                    decomp_buf = nullptr;
                }
            }
        }
        uint32_t use_stride = decomp_buf ? lineBytes : (lineWords * 8);
        /* PORT: N64 RDRAM stores texture rows WORD-ALIGNED. For 56x56 CI4,
           a 28-byte row becomes a 32-byte padded row (lineWords*8).
           Using packed 28-byte stride reads row 0 padding + row 1 data,
           producing diagonal-streak noise. Use padded stride. */
        uint32_t srcLineStride = lineWords * 8;  // padded row stride (matches dstOffset)
        if (srcLineStride == 0) srcLineStride = lineBytes;
        if (srcLineStride == 0) srcLineStride = 1;
        for (uint32_t row = 0; row < texHeight; row++) {
            uint32_t srcOffset = row * use_stride;
            uint32_t dstOffset = tmemBase + row * t.line * 8;
            if (dstOffset + lineBytes <= 4096)
                memcpy(s_rdp.tmem + dstOffset, tex_src + srcOffset, lineBytes);
        { static int s_tdmp = 0; if (s_tdmp++ < 3) {
            FILE* f = fopen("/data/data/com.dkrwrapper/files/lt_dump.bin", "wb");
            if (f) {
                /* Header: tmemAddr, line, texW, texH, srcAddr, srcOff */
                uint32_t hdr[6] = {t.tmemAddr, t.line, texWidth, texHeight,
                                   (uint32_t)(uintptr_t)s_rdp.texAddr,
                                   (uint32_t)(uintptr_t)(s_rdp.texAddr + srcOffset)};
                fwrite(hdr, 4, 6, f);
                fwrite(s_rdp.tmem, 1, 4096, f);
                fclose(f);
            }
        } }
        }
        if (decomp_buf) free(decomp_buf);
    }
}

static void Cmd_LoadTLUT(GfxCommand cmd) {
    /* PORT: Early NULL check. The diagnostic block below dereferences
    if (s_rdp.texAddr == nullptr) { return; } // PORT: Early NULL guard
       s_rdp.texAddr[0], which crashes if a preceding SetTImg was bogus
       and set texAddr to nullptr. */
    if (s_rdp.texAddr == nullptr) {
        static int s_ltn = 0;
        if (s_ltn++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADTLUT",
                "SKIP early: texAddr=null");
        return;
    }

    { uint32_t _s2 = (uint32_t)s_rdp.segmentBase[2];
      if (_s2 == 0xB28625F0u || _s2 == 0xB553D480u || _s2 == 0xB27FB050u || _s2 == 0xB284E050u) {
          __android_log_print(ANDROID_LOG_ERROR, "BKA-LT-RARE",
              "seg2=%08X texAddr=%p first16=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
              _s2, (void*)s_rdp.texAddr,
              s_rdp.texAddr[0], s_rdp.texAddr[1], s_rdp.texAddr[2], s_rdp.texAddr[3],
              s_rdp.texAddr[4], s_rdp.texAddr[5], s_rdp.texAddr[6], s_rdp.texAddr[7],
              s_rdp.texAddr[8], s_rdp.texAddr[9], s_rdp.texAddr[10], s_rdp.texAddr[11],
              s_rdp.texAddr[12], s_rdp.texAddr[13], s_rdp.texAddr[14], s_rdp.texAddr[15]);
      } }

    // F3DEX1 _gDPLoadTLUTCmd: w1[24:26]=tile, w1[14:23]=count.
    // Destination TMEM address comes from that tile's descriptor (set by the
    // preceding gDPSetTile). Standard G_TX_LOADTILE (7) is used for the
    // real texture, and here the palette lands wherever the game placed it
    // (usually 256 words = byte 2048 for pal 0).
    uint32_t tile  = (cmd.w1 >> 24) & 0x7;
    uint32_t count = (cmd.w1 >> 14) & 0x3FF;
    if (tile >= 8) {
        static int s_badtile = 0; if (s_badtile++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADTLUT-BAD",
                "rejected w0=%08X w1=%08X tile=%u", cmd.w0, cmd.w1, tile);
        return;
    }
    auto& t = s_rdp.tiles[tile];
    /* ARM64 port 2026-10-12: F3DEX1 convention — CI4 palette bank N
       lives at TMEM word 256 + N*16.  The load tile's tmemAddr (usually
       256) is not the bank address; the render tile's palette field picks
       the bank.  Look at tile 0 (render tile) to find which bank to load
       into, and write there.  Without this, sampler reads word 256+pal*16
       while loader wrote word 256, and all CI4 palette lookups resolve to
       zero (the black RARE logo interior). */
    uint32_t render_pal = s_rdp.tiles[0].palette;
    uint32_t bank_offset = (render_pal < 16) ? (render_pal * 16 * 8) : 0;
    uint32_t tmemBase = 2048 + bank_offset;
    if (tmemBase >= 4096) tmemBase = 2048;
    if (!s_rdp.texAddr) {
        static int s_ltn = 0;
        if (s_ltn++ < 2000)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADTLUT",
                "SKIP tile=%u tmem=%u count=%u texAddr=null",
                tile, t.tmemAddr, count);
        return;
    }
    uint32_t bytes = (count + 1) * 2;
    if (bytes > 1024) bytes = 1024;
    static int s_first = 0;
    if (s_first++ == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-LT1",
            "FIRST LT: tile=%u tmem=%u tmemBase=%u count=%u texAddr=%p "
            "segBase[0..3]=%08lX %08lX %08lX %08lX "
            "bytes=%02X%02X%02X%02X%02X%02X%02X%02X",
            tile, t.tmemAddr, tmemBase, count, (void*)s_rdp.texAddr,
            (unsigned long)s_rdp.segmentBase[0],
            (unsigned long)s_rdp.segmentBase[1],
            (unsigned long)s_rdp.segmentBase[2],
            (unsigned long)s_rdp.segmentBase[3],
            s_rdp.texAddr[0],s_rdp.texAddr[1],s_rdp.texAddr[2],s_rdp.texAddr[3],
            s_rdp.texAddr[4],s_rdp.texAddr[5],s_rdp.texAddr[6],s_rdp.texAddr[7]);
    }
    memcpy(s_rdp.tmem + tmemBase, s_rdp.texAddr, bytes);
    { static int s_tdmp = 0; if (s_tdmp++ < 8) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-TLUTDUMP",
            "TLUTDUMP base=%u bytes=%u w256..263=%02X%02X%02X%02X%02X%02X%02X%02X w264..271=%02X%02X%02X%02X%02X%02X%02X%02X w272..279=%02X%02X%02X%02X%02X%02X%02X%02X w280..287=%02X%02X%02X%02X%02X%02X%02X%02X",
            tmemBase, bytes,
            s_rdp.tmem[2048],s_rdp.tmem[2049],s_rdp.tmem[2050],s_rdp.tmem[2051],s_rdp.tmem[2052],s_rdp.tmem[2053],s_rdp.tmem[2054],s_rdp.tmem[2055],
            s_rdp.tmem[2056],s_rdp.tmem[2057],s_rdp.tmem[2058],s_rdp.tmem[2059],s_rdp.tmem[2060],s_rdp.tmem[2061],s_rdp.tmem[2062],s_rdp.tmem[2063],
            s_rdp.tmem[2064],s_rdp.tmem[2065],s_rdp.tmem[2066],s_rdp.tmem[2067],s_rdp.tmem[2068],s_rdp.tmem[2069],s_rdp.tmem[2070],s_rdp.tmem[2071],
            s_rdp.tmem[2072],s_rdp.tmem[2073],s_rdp.tmem[2074],s_rdp.tmem[2075],s_rdp.tmem[2076],s_rdp.tmem[2077],s_rdp.tmem[2078],s_rdp.tmem[2079]);
    }}
    { static int s_lt = 0; if (s_lt++ < 2000)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADTLUT",
            "ENTRY tile=%u tmem=%u tmemBase=%u count=%u bytes=%u texAddr=%p "
            "src0..7=%02X%02X%02X%02X%02X%02X%02X%02X "
            "tmem[%u..%u]=%02X%02X%02X%02X%02X%02X%02X%02X "
            "pal=%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X%04X",
            tile, t.tmemAddr, tmemBase, count, bytes, (void*)s_rdp.texAddr,
            s_rdp.texAddr[0], s_rdp.texAddr[1], s_rdp.texAddr[2], s_rdp.texAddr[3],
            s_rdp.texAddr[4], s_rdp.texAddr[5], s_rdp.texAddr[6], s_rdp.texAddr[7],
            tmemBase, tmemBase+7,
            s_rdp.tmem[tmemBase+0], s_rdp.tmem[tmemBase+1],
            s_rdp.tmem[tmemBase+2], s_rdp.tmem[tmemBase+3],
            s_rdp.tmem[tmemBase+4], s_rdp.tmem[tmemBase+5],
            s_rdp.tmem[tmemBase+6], s_rdp.tmem[tmemBase+7],
            ((u16)s_rdp.tmem[(tmemBase+ 0)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+ 1)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+ 2)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+ 3)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+ 4)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+ 5)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+ 6)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+ 7)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+ 8)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+ 9)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+10)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+11)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+12)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+13)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+14)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+15)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+16)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+17)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+18)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+19)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+20)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+21)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+22)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+23)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+24)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+25)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+26)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+27)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+28)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+29)&0xFFF],
            ((u16)s_rdp.tmem[(tmemBase+30)&0xFFF]<<8)|s_rdp.tmem[(tmemBase+31)&0xFFF]); }

}

static void Cmd_LoadBlock(GfxCommand cmd) {
    s_gfx_lb_count++;
    /* Fix T: F3DEX LoadBlock.  Tile field is bits 16-23 of w0, NOT low 3.
     * Contiguous transfer; sh encodes 16-bit word count - 1. */
    uint32_t tile = (cmd.w1 >> 24) & 0x7;
    if (tile >= 8) return;
    auto& t = s_rdp.tiles[tile];
    uint32_t sl = (cmd.w0 >> 12) & 0xFFF, tl = cmd.w0 & 0xFFF;
    uint32_t sh = (cmd.w1 >> 12) & 0xFFF, dxt = cmd.w1 & 0xFFF;
    t.sl = sl; t.tl = tl;
    /* DO NOT overwrite t.sh, t.th, or t.line here! G_SETTILE handles that. */
    { static int s_lb = 0; if (s_lb++ < 2000) {
        extern uint8_t* gN64_RDRAM;
        uintptr_t addr = (uintptr_t)s_rdp.texAddr;
        uintptr_t base = (uintptr_t)gN64_RDRAM;
        bool is_heap = gN64_RDRAM && ((addr < base) || (addr >= base + 0x800000));
        if (0) diag_log("BKA-DIAG-LB", "");
    } }

    uint32_t texSizeBytes = (sh + 1) * 8; // G_LOADBLOCK fix: sh is 64-bit word count - 1
    if (texSizeBytes == 0) texSizeBytes = 1;
    if (texSizeBytes > 4096) {
        /* PORT: TMEM is 4KB. Game may send huge sizes due to walker drift
           or multi-chunk tricks. Clamp to prevent corruption. */
        texSizeBytes = 4096;
    }
    uint32_t tmemBase = t.tmemAddr * 8;

    /* ARM64 port 2026-10-13: scope the 2KB reject to ASCII-only.
       The guard was added to filter a misparse whose SetTImg resolved
       to a debug string ("ewPosX"); but it also rejects legitimate
       LoadBlock from model bins (RARE badge, code_59780.c:112). */
    if (0) { // Bypassed ASCII guard
        int ascii = 0;
        for (int _i = 0; _i < 8; _i++) {
            uint8_t _b = s_rdp.texAddr[_i];
            if ((_b >= 0x20 && _b < 0x7F) || _b == 0) ascii++;
        }
        if (ascii >= 7) {
            static int s_bad = 0;
            if (s_bad++ < 10)
                __android_log_print(ANDROID_LOG_WARN, "BKA-LOADBLK",
                    "REJECT ASCII 2K load: texAddr=%p bytes=%u tmemBase=%u "
                    "b0..7=%02X%02X%02X%02X%02X%02X%02X%02X",
                    (void*)s_rdp.texAddr, texSizeBytes, tmemBase,
                    s_rdp.texAddr[0],s_rdp.texAddr[1],s_rdp.texAddr[2],s_rdp.texAddr[3],
                    s_rdp.texAddr[4],s_rdp.texAddr[5],s_rdp.texAddr[6],s_rdp.texAddr[7]);
            return;
        } else {
            static int s_ok = 0;
            if (s_ok++ < 20)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADBLK",
                    "ALLOW 2K load: texAddr=%p bytes=%u tmemBase=%u "
                    "b0..7=%02X%02X%02X%02X%02X%02X%02X%02X",
                    (void*)s_rdp.texAddr, texSizeBytes, tmemBase,
                    s_rdp.texAddr[0],s_rdp.texAddr[1],s_rdp.texAddr[2],s_rdp.texAddr[3],
                    s_rdp.texAddr[4],s_rdp.texAddr[5],s_rdp.texAddr[6],s_rdp.texAddr[7]);
        }
    }
    if (s_rdp.texAddr && texSizeBytes > 0 &&
        texSizeBytes <= 4096 && tmemBase + texSizeBytes <= 4096) {
        memcpy(s_rdp.tmem + tmemBase, s_rdp.texAddr, texSizeBytes);
        static int s_lbcopy = 0;
        if (s_lbcopy++ < 2000) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADBLK",
                "COPY tile=%u tmemBase=%u bytes=%u src0..7=%02X%02X%02X%02X %02X%02X%02X%02X tmem0..7=%02X%02X%02X%02X %02X%02X%02X%02X",
                tile, tmemBase, texSizeBytes,
                s_rdp.texAddr[0],s_rdp.texAddr[1],s_rdp.texAddr[2],s_rdp.texAddr[3],
                s_rdp.texAddr[4],s_rdp.texAddr[5],s_rdp.texAddr[6],s_rdp.texAddr[7],
                s_rdp.tmem[tmemBase+0],s_rdp.tmem[tmemBase+1],s_rdp.tmem[tmemBase+2],s_rdp.tmem[tmemBase+3],
                s_rdp.tmem[tmemBase+4],s_rdp.tmem[tmemBase+5],s_rdp.tmem[tmemBase+6],s_rdp.tmem[tmemBase+7]);
        }
    } else {
        static int s_lbskip = 0;
        if (s_lbskip++ < 2000) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-LOADBLK",
                "SKIP tile=%u texAddr=%p bytes=%u tmemBase=%u",
                tile, (void*)s_rdp.texAddr, texSizeBytes, tmemBase);
        }
    }
}

// =======================================================================
// G_VTX - Load vertices into DMEM (F3DEX_GBI format)
// w0 = [G_VTX:8][v0:8][n:6][length:10]
// w1 = address of Vtx data in RDRAM
// =======================================================================
static int s_vtxCallCount = 0;
static void Cmd_Vtx(GfxCommand cmd) {
    s_vtxCallCount++;
    /* PORT: distinguish standard F3DEX1 gSPVertex from DKR's gSPVertexDKR.
       Standard: w0[15:0] = ((n-1)<<10) | (n*2), so (w0 & 0x3FF) == n*2.
       DKR:      w0[15:0] = ((n-1)<<3) | (v&6) | v0, so packed field != n*2. */
    uint32_t packed = cmd.w0 & 0xFFFF;
    uint32_t std_n = ((packed >> 10) & 0x3F) + 1;
    uint32_t std_check = packed & 0x3FF;
    uint32_t v0, n;
    bool is_dkr = (std_check != std_n * 2);
    if (is_dkr) {
        /* DKR gSPVertexDKR: w0 = [G_VTX:8][p3:8][p2:16]
         *   p3 = ((n-1)<<3) | (v&6) | v0
         *   p2 = ((n*8 + n) << 1) + 8 = 18n + 8
         * Emulator's previous decode (from p2 nibble + p3/2) is WRONG:
         *   w0=0x0410003E → real n=3, v0=0 (was decoded as n=8, v0=8)
         *   w0=0x04760116 → real n=15, v0=6 (was decoded as n=3, v0=59)
         * Wrong v0 sends vertices to slots triangles never reference →
         *   dmem[5], dmem[7] = empty → degenerate tri → 4096 px only. */
        uint32_t p3 = (cmd.w0 >> 16) & 0xFF;
        n  = ((p3 >> 3) & 0x1F) + 1;
        v0 = p3 & 0x07;
        static int _dkrlog = 0;
        if (_dkrlog++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-VTXDKR",
                "n=%u v0=%u w0=%08X w1=%08X", n, v0, cmd.w0, cmd.w1);
    } else {
        v0 = ((cmd.w0 >> 16) & 0xFF) / 2;
        n = std_n;
    }

    {
        static int _vdiag = 0;
        if (_vdiag++ < 8) {
            diag_log("BKA-VTX-DIAG",
                "w0=0x%08X w1=0x%08X is_dkr=%d v0=%u n=%u std_check=%u std_n=%u",
                cmd.w0, cmd.w1, (int)is_dkr, v0, n, std_check, std_n);
        }
    }

    uint32_t addr = cmd.w1;

    static int s_vtx_dump = 0;
    if (s_vtx_dump++ < 3) {
        extern const uint8_t* s_current_cmd;
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "Cmd_Vtx CALL #%d: v0=%u n=%u addr=0x%08X w0=0x%08X cur=%p cur+8=%p delta=%ld",
            s_vtxCallCount, v0, n, addr, cmd.w0,
            (void*)s_current_cmd, (void*)((uintptr_t)s_current_cmd + 8),
            (long)((intptr_t)0));  // placeholder, filled below
    }

    if (v0 >= DMEM_VERTEX_COUNT || v0 + n > DMEM_VERTEX_COUNT) {
        LOGW("Cmd_Vtx: v0=%u n=%u exceeds DMEM limit (%d)",
             v0, n, DMEM_VERTEX_COUNT);
        return;
    }

    /* Fix: same registry-first resolution as Cmd_Mtx. RDP_TranslateAddr
     * silently returns gN64_RDRAM+off for KSEG0 words, which is readable
     * but wrong when the game wrote via a host pointer registered by
     * bka_k0_to_phys. */
    uint8_t* src = (uint8_t*)bka_lookup_addr_by_low32(addr);
    if (!src) {
        uint32_t _sw_vtx = __builtin_bswap32(addr);
        src = (uint8_t*)bka_lookup_addr_by_low32(_sw_vtx);
    }
    if (!src) src = RDP_TranslateAddr(addr);
    /* VTX: fallback heap-pointer reconstruction if the above all miss. */
    {
        /* BKA-SRCNULL: RDP_TranslateAddr can return non-null sign-extended
         * garbage for heap addresses (e.g. RDP_TranslateAddr(0x5610B4B0) ->
         * 0xFFFFFFFFD610B4B0). This is non-null, so `&& !src` below blocks
         * prefix reconstruction. Null it out first. */
        if (src && !bka_is_readable(src)) src = NULL;
        int _ok = 0;
        if (src && bka_is_readable(src) &&
            bka_is_readable((void*)(src + 15))) {
            const uint8_t *_b = src;
            /* Vtx byte 6,7 = flag u16 (should be 0 or 0x80xx per N64 spec;
               mostly 0). Byte 12-15 = rgba. Signature: byte 6 must be 0x00 or 0x80
               AND byte 10-11 (t coord) usually not both 0xFF. Too loose; just
               check the readable pointer is non-NULL. */
            _ok = 1;
        }
        if (!_ok) {
            /* BKA-RDRAMFB: game emits w1 = low32(gN64_RDRAM + offset).
             * All observed VTXDKR addresses start with 0x55 (low32 of RDRAM host base).
             * Try reconstructing via gN64_RDRAM directly before prefix array. */
            {
                extern uint8_t* gN64_RDRAM;
                if (gN64_RDRAM) {
                    uint32_t rdram_low = (uint32_t)(uintptr_t)gN64_RDRAM;
                    if (addr >= rdram_low && addr < rdram_low + 0x04000000u) {
                        uint8_t* cand = gN64_RDRAM + (addr - rdram_low);
                        if (bka_is_readable(cand) &&
                            bka_is_readable(cand + (n * 16) - 1)) {
                            src = cand;
                            static int _rd = 0; if (_rd++ < 10)
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-RDRAMFB",
                                    "addr=0x%08X rdram_low=0x%08X off=0x%X src=%p OK",
                                    addr, rdram_low, addr - rdram_low, (void*)cand);
                        }
                    }
                }
            }
            /* PORT: gSPVertex writes (u32)v into w1 with no OS_K0_TO_PHYSICAL
             * wrap (NU6-06-0030-001G p.121; p.138). Real HW maps bits[27:0]
             * via the SP segment-0 table; on Android we must reconstruct the
             * 64-bit host pointer. The original filter `_hb < 0x20 || _hb > 0x3F`
             * rejects every real Android heap (0x7D-0x7F mmap, 0xEA-0xEB
             * compat heap), so it was dead code. Widen and try several
             * prefix arenas. */
            uint32_t _raw = addr;
            uint32_t _cands[2];
            _cands[0] = _raw;
            _cands[1] = ((_raw & 0xFFu) << 24) | ((_raw & 0xFF00u) << 8) |
                        ((_raw >> 8) & 0xFF00u) | ((_raw >> 24) & 0xFFu);
            static const uint64_t _prefixes[] = {
                0x0000000000ULL,   /* zero-extend (32-bit compat / low heap) */
                0x7D00000000ULL,   /* mmap-anon arena */
                0x7E00000000ULL,   /* mmap-anon arena */
                0x7F00000000ULL,   /* stack / mmap tail */
                0x7B00000000ULL,   /* linker / data arena */
            };
            /* BKA-RDRAMPFX: same fix for vertices — gN64_RDRAM >> 32 prefix
             * with raw addr (not adjusted). */
            {
                extern uint8_t* gN64_RDRAM;
                if (gN64_RDRAM && !src) {
                    uint64_t _rdram_pfx = (uint64_t)(uintptr_t)gN64_RDRAM & 0xFF00000000ULL;
                    uint64_t _c = _rdram_pfx | (uint64_t)addr;
                    if (bka_is_readable((void*)_c) &&
                        bka_is_readable((void*)((const uint8_t*)_c + (n * 16) - 1))) {
                        src = (uint8_t*)_c;
                        static int _rv = 0; if (_rv++ < 15)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-RDRAMPFX",
                                "addr=0x%08X pre0x%02llX src=%p VTX v0=%d n=%d",
                                addr, (unsigned long long)(_rdram_pfx >> 32),
                                (void*)_c, v0, n);
                    }
                }
            }
            /* BKA-RAWPX: try prefix | raw addr (not adjusted). Vertex heap
             * is at 0x7B..., not 0x6E... like matrix heap. Existing loop
             * only tries prefix | adj (0x7BD50F2934 for addr=0x550F2934),
             * which misses 0x7B550F2934. */
            if (!src) {
                extern uint8_t* gN64_RDRAM;
                uint64_t _dyn = gN64_RDRAM ? ((uint64_t)(uintptr_t)gN64_RDRAM & 0xFF00000000ULL) : 0;
                uint64_t _rawpx[6] = {
                    _dyn,
                    0x7B00000000ULL, 0x7D00000000ULL, 0x7E00000000ULL,
                    0x7F00000000ULL, 0x0000000000ULL,
                };
                for (int _i = 0; _i < 6 && !src; _i++) {
                    if (_rawpx[_i] == 0 && _i == 0) continue;  /* skip null dynamic */
                    uint64_t _c = _rawpx[_i] | (uint64_t)addr;
                    if (bka_is_readable((void*)_c) &&
                        bka_is_readable((void*)((const uint8_t*)_c + (n * 16) - 1))) {
                        src = (uint8_t*)_c;
                        static int _rr = 0; if (_rr++ < 20)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-RAWPX",
                                "addr=0x%08X pre0x%02llX src=%p v0=%d n=%d OK",
                                addr, (unsigned long long)(_rawpx[_i] >> 32),
                                (void*)_c, v0, n);
                    }
                }
                /* BKA-SEGTRY: addresses like 0x560FADF0 are segmented:
                 * segment_id = bits[27:24] (6), offset = bits[23:0] (0x0FADF0).
                 * Try 3 formulas: RDRAM+off24, segBase[segid]+off24,
                 * RDRAM+segBase[segid]+off24. Any readable becomes src. */
                if (!src) {
                    extern uint8_t* gN64_RDRAM;
                    uint32_t segid = (addr >> 24) & 0x0F;
                    uint32_t off24 = addr & 0x00FFFFFF;
                    uint32_t sb = s_rdp.segmentBase[segid];
                    /* Try 1: gN64_RDRAM + offset24 */
                    if (gN64_RDRAM && off24 < 0x04000000u) {
                        uint8_t* _c = gN64_RDRAM + off24;
                        if (bka_is_readable(_c) &&
                            bka_is_readable(_c + (n * 16) - 1)) {
                            src = _c;
                            static int _s1 = 0; if (_s1++ < 10)
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGTRY",
                                    "addr=0x%08X FIX1 RDRAM+0x%X src=%p", addr, off24, (void*)_c);
                        }
                    }
                    /* Try 2: segBase[segid] as host ptr + offset24 */
                    if (!src && sb) {
                        uint8_t* _c = (uint8_t*)(uintptr_t)sb + off24;
                        if (bka_is_readable(_c) &&
                            bka_is_readable(_c + (n * 16) - 1)) {
                            src = _c;
                            static int _s2 = 0; if (_s2++ < 10)
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGTRY",
                                    "addr=0x%08X FIX2 segBase[%d]+0x%X src=%p", addr, segid, off24, (void*)_c);
                        }
                    }
                    /* Try 3: gN64_RDRAM + segBase[segid] as offset + off24 */
                    if (!src && sb && gN64_RDRAM) {
                        uint8_t* _c = gN64_RDRAM + sb + off24;
                        if (bka_is_readable(_c) &&
                            bka_is_readable(_c + (n * 16) - 1)) {
                            src = _c;
                            static int _s3 = 0; if (_s3++ < 10)
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGTRY",
                                    "addr=0x%08X FIX3 RDRAM+segBase[%d]+0x%X src=%p", addr, segid, off24, (void*)_c);
                        }
                    }
                    /* Log what segBase values are for failing addresses */
                    static int _sl = 0; if (!src && _sl++ < 8)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGTRY",
                            "addr=0x%08X FAIL segid=%d off24=0x%X segBase[%d]=0x%08X",
                            addr, segid, off24, segid, sb);
                }
                static int _rrf = 0; if (_rrf++ < 5 && !src) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DEEP",
                        "addr=0x%08X n=%u testing all 256 prefixes + partial-read",
                        addr, n);
                    extern uint8_t* gN64_RDRAM;
                    uint32_t _rdlow = gN64_RDRAM ? (uint32_t)(uintptr_t)gN64_RDRAM : 0;
                    int _found16 = -1, _found_full = -1;
                    for (int _bt = 0; _bt <= 0xFF; _bt++) {
                        uint64_t _c = ((uint64_t)_bt << 32) | (uint64_t)addr;
                        if (!bka_is_readable((void*)_c)) continue;
                        if (_found16 < 0) _found16 = _bt;
                        if (bka_is_readable((void*)((const uint8_t*)_c + (n * 16) - 1))) {
                            _found_full = _bt;
                            break;
                        }
                    }
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DEEP",
                        "addr=0x%08X found16=%d found_full=%d rdlow=0x%08X",
                        addr, _found16, _found_full, _rdlow);
                    /* Also try: addr - rdlow as RDRAM offset (no 64MB clamp) */
                    if (gN64_RDRAM) {
                        uint32_t _off = (uint32_t)addr - _rdlow;
                        uint8_t* _cand = gN64_RDRAM + _off;
                        int _r16 = bka_is_readable(_cand) ? 1 : 0;
                        int _rfull = bka_is_readable(_cand + (n * 16) - 1) ? 1 : 0;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-DEEP",
                            "addr=0x%08X offcalc=0x%X cand=%p r16=%d rfull=%d",
                            addr, _off, (void*)_cand, _r16, _rfull);
                        if (_rfull && !src) {
                            src = _cand;
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-DEEP",
                                "FIXED via addr-rdlow offset");
                        }
                    }
                }
            }
            for (int _k = 0; _k < 2 && !src; _k++) {
                uint32_t _c = _cands[_k];
                uint8_t _hb = (_c >> 24) & 0xFF;
                if (_hb == 0x00 || _hb == 0xFF) continue;
                for (int _pk = 0;
                     _pk < (int)(sizeof(_prefixes)/sizeof(_prefixes[0]));
                     _pk++) {
                    uint64_t _p = _prefixes[_pk] | (uint64_t)_c;
                    if (!bka_is_readable((void*)_p)) continue;
                    if (!bka_is_readable((void*)((const uint8_t*)_p +
                                                 (n * 16) - 1))) continue;
                    const uint8_t *_b = (const uint8_t*)_p;
                    int _nz = 0;
                    for (int _i = 0; _i < 16; _i++) if (_b[_i]) { _nz = 1; break; }
                    if (!_nz) continue;
                    static int _lg = 0;
                    if (_lg++ < 20) diag_log("BKA-VTX-REBUILD",
                        "addr=0x%08X cand%d pre0x%02llX ptr=0x%llX "
                        "v0=%u n=%u b=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                        addr, _k,
                        (unsigned long long)(_prefixes[_pk] >> 32),
                        (unsigned long long)_p,
                        (unsigned)v0, (unsigned)n,
                        _b[0],_b[1],_b[2],_b[3],_b[4],_b[5],_b[6],_b[7],
                        _b[8],_b[9],_b[10],_b[11],_b[12],_b[13],_b[14],_b[15]);
                    src = (uint8_t*)_p;
                    break;
                }
            }
        }
    }
    /* Guard: validate the FULL n*16 byte source before any swap/read.
     * Previous check only tested 16 bytes; for n>1 the swap loop faults
     * on src+16.. at RSP_ProcessGfxTask+19384 (read_u16_be). Also covers
     * the case where fallback failed but left a non-NULL unreadable src. */
    if (!src ||
        !bka_is_readable(src) ||
        !bka_is_readable((void*)((const uint8_t*)src + (n * 16) - 1))) {
        static int s_ft = 0;
        if (s_ft++ < 200) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "VtxFAIL addr=0x%08X v0=%u n=%u w0=0x%08X src=%p rd1=%d rdN=%d",
                addr, v0, n, cmd.w0, (void*)src,
                src ? (int)bka_is_readable(src) : 0,
                src ? (int)bka_is_readable((void*)((const uint8_t*)src + (n * 16) - 1)) : 0);
        }
        return;
    }
    uint8_t* src_base = src;

    static int s_vtx_resolve_log = 0;
    if (s_vtx_resolve_log++ < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "VTXRESOLVE addr=0x%08X -> src=%p bytes: "
            "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            addr, (void*)src,
            src[0],src[1],src[2],src[3],src[4],src[5],src[6],src[7],
            src[8],src[9],src[10],src[11],src[12],src[13],src[14],src[15]);
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "VTXSEGS [0]=%08lX [1]=%08lX [2]=%08lX [3]=%08lX "
            "[4]=%08lX [5]=%08lX [6]=%08lX [7]=%08lX "
            "[8]=%08lX [9]=%08lX [A]=%08lX [B]=%08lX "
            "[C]=%08lX [D]=%08lX [E]=%08lX [F]=%08lX",
            (unsigned long)s_rdp.segmentBase[0x0],
            (unsigned long)s_rdp.segmentBase[0x1],
            (unsigned long)s_rdp.segmentBase[0x2],
            (unsigned long)s_rdp.segmentBase[0x3],
            (unsigned long)s_rdp.segmentBase[0x4],
            (unsigned long)s_rdp.segmentBase[0x5],
            (unsigned long)s_rdp.segmentBase[0x6],
            (unsigned long)s_rdp.segmentBase[0x7],
            (unsigned long)s_rdp.segmentBase[0x8],
            (unsigned long)s_rdp.segmentBase[0x9],
            (unsigned long)s_rdp.segmentBase[0xA],
            (unsigned long)s_rdp.segmentBase[0xB],
            (unsigned long)s_rdp.segmentBase[0xC],
            (unsigned long)s_rdp.segmentBase[0xD],
            (unsigned long)s_rdp.segmentBase[0xE],
            (unsigned long)s_rdp.segmentBase[0xF]);
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "RDRAM=%p max_offset=%08lX (alloc 0x%lX)",
            (void*)gN64_RDRAM,
            (unsigned long)(addr & 0x00FFFFFF),
            (unsigned long)0x04000000);
    }

    for (uint32_t i = 0; i < n; i++) {
        BKVertex* v = &s_rdp.dmem[v0 + i];
        bka_guard_write(v, sizeof(BKVertex), "Cmd_Vtx.dmem");
        { static int s_vtxwr = 0; if (s_vtxwr++ < 0) LOGV("VTXWR i=%u v=%p src=%p dmem=%p idx=%u", i, (void*)v, (void*)src, (void*)s_rdp.dmem, v0+i); }
        v->x = read_int16(src + 0);
        v->y = read_int16(src + 2);
        v->z = read_int16(src + 4);
        v->flag = (src[6] << 8) | src[7];
        v->s = read_int16(src + 8);
        v->t = read_int16(src + 10);
        { static int nv = 0; if (nv++ < 8)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-VTX",
                "raw=%02X%02X %02X%02X %02X%02X | cur x=%d y=%d z=%d | BE x=%d y=%d z=%d",
                src[0],src[1],src[2],src[3],src[4],src[5], (int)v->x,(int)v->y,(int)v->z,
                (int)(int16_t)((src[0]<<8)|src[1]), (int)(int16_t)((src[2]<<8)|src[3]),
                (int)(int16_t)((src[4]<<8)|src[5])); }
        v->r = src[12];
        v->g = src[13];
        v->b = src[14];
        v->a = src[15];
        src += 16;
    }

    /* BKA-SYNTH: fill ALL 256 dmem slots with a synthetic gradient.
     * Slot i = vertex at (i*128, 0, 0) in s10.2 format, so triangle
     * vi=(0,100,200) has positions (0,0), (12800,0), (25600,0) — a real
     * triangle. This proves whether delivery works independent of game
     * vertex data. If Chrome shows triangles -> game data is bug. If
     * black -> rasterizer/delivery is bug. */
    s_rdp.dmemVertexCount = 256;
    {
        extern BKVertex s_synth_vertices[256];
        static int _init = 0;
        if (!_init) {
            for (int _i = 0; _i < 256; _i++) {
                s_rdp.dmem[_i].x = (int16_t)(_i * 128);  /* spread across screen */
                s_rdp.dmem[_i].y = (int16_t)((_i % 16) * 1024);
                s_rdp.dmem[_i].z = 0;
                s_rdp.dmem[_i].flag = 0;
                s_rdp.dmem[_i].s = 0;
                s_rdp.dmem[_i].t = 0;
                s_rdp.dmem[_i].r = (uint8_t)(_i * 1);
                s_rdp.dmem[_i].g = (uint8_t)(255 - _i);
                s_rdp.dmem[_i].b = 128;
                s_rdp.dmem[_i].a = 255;
            }
            _init = 1;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SYNTH",
                "filled 256 dmem slots with gradient");
        }
    }
    {
        static int s_l = 0;
        if (s_l++ < 3) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "VtxOK addr=0x%08X v0=%u n=%u first8=%02X%02X%02X%02X %02X%02X%02X%02X",
                addr, v0, n,
                src_base[0],src_base[1],src_base[2],src_base[3],
                src_base[4],src_base[5],src_base[6],src_base[7]);
        }
    }

    static int s_vtx_hex = 0;
    if (s_vtx_hex++ < 3 && n >= 3 && src) {
        const uint8_t* raw = src_base;
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "VTXRAW %u bytes @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            n * 16, (const void*)raw,
            raw[0],raw[1],raw[2],raw[3],raw[4],raw[5],raw[6],raw[7],
            raw[8],raw[9],raw[10],raw[11],raw[12],raw[13],raw[14],raw[15]);
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "VTXDEC v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d)",
            s_rdp.dmem[v0].x, s_rdp.dmem[v0].y, s_rdp.dmem[v0].z,
            s_rdp.dmem[v0+1].x, s_rdp.dmem[v0+1].y, s_rdp.dmem[v0+1].z,
            s_rdp.dmem[v0+2].x, s_rdp.dmem[v0+2].y, s_rdp.dmem[v0+2].z);
    }

    __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
        "Cmd_Vtx: loaded %u vertices, dmemVertexCount=%d", n, s_rdp.dmemVertexCount);
}

// =======================================================================
// G_TRI1 - Draw 1 triangle
// w0 = [G_TRI1:8][v2:8][v1:8][v0:8]
// w1 = [flag:24][00000000]
// =======================================================================


/* Local copy of DKR Triangle (from structs.h) — 16 bytes each */
typedef struct DkrTriangle {
    u8  flags;
    u8  vi0;
    u8  vi1;
    u8  vi2;
    s16 uv0u, uv0v;
    s16 uv1u, uv1v;
    s16 uv2u, uv2v;
} DkrTriangle;

static void Cmd_Polygon(GfxCommand cmd) {
    { static int _pe = 0; if (_pe++ < 20)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-POLYENTER",
            "w0=0x%08X w1=0x%08X dmem=%d", cmd.w0, cmd.w1, s_rdp.dmemVertexCount); }
    /* Guard: op 0x05 = DKR G_TRIN. w1 points to a 16-byte vertex-triangle
     * descriptor. Drift-fed walker can hand us garbage (0xEBxxxxxx etc).
     * Resolve via registry, fall back to bswap, then readability-check
     * the full 16-byte payload before any deref. */
    {
        /* BKA-POLYSEG: DKR emits w1 like 0x560F3CD0 (segmented format,
         * segment_id=6, offset=0x0F3CD0). No prefix reconstruction works
         * for this (same as VTXDKR). The formula that works is
         * gN64_RDRAM + (w1 & 0x00FFFFFF) — segments 0-7 all map to RDRAM
         * from base 0. Try this FIRST before the fallback chain. */
        uint8_t* _poly_src = NULL;
        {
            extern uint8_t* gN64_RDRAM;
            uint32_t _off24 = cmd.w1 & 0x00FFFFFFu;
            uint32_t _segid = (cmd.w1 >> 24) & 0x0F;
            if (gN64_RDRAM && _off24 < 0x04000000u && _segid < 8) {
                uint8_t* _cand = gN64_RDRAM + _off24;
                if (bka_is_readable(_cand) &&
                    bka_is_readable(_cand + 15)) {
                    _poly_src = _cand;
                    static int _ps = 0; if (_ps++ < 15)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-POLYSEG",
                            "w1=0x%08X segid=%u off24=0x%X src=%p",
                            (unsigned)cmd.w1, _segid, _off24, (void*)_cand);
                }
            }
        }
        if (!_poly_src) _poly_src = (uint8_t*)bka_lookup_addr_by_low32(cmd.w1);
        if (!_poly_src) {
            uint32_t _sw = __builtin_bswap32(cmd.w1);
            _poly_src = (uint8_t*)bka_lookup_addr_by_low32(_sw);
        }
        if (!_poly_src) _poly_src = RDP_TranslateAddr(cmd.w1);
        if (!_poly_src || !bka_is_readable(_poly_src) ||
            !bka_is_readable((void*)(_poly_src + 15))) {
            static int _pf = 0;
            if (_pf++ < 30)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-POLYFAIL",
                    "w1=0x%08X src=%p rd1=%d rd16=%d — skipping",
                    cmd.w1, (void*)_poly_src,
                    _poly_src ? (int)bka_is_readable(_poly_src) : 0,
                    _poly_src ? (int)bka_is_readable((void*)(_poly_src + 15)) : 0);
            return;
        }
    }

    /* G_TRIN (DKR): w0[23:16] = ((numTris-1)<<4) | texEn; w1 = Triangle array ptr. */
    uint32_t numTris = (((cmd.w0 >> 20) & 0xF)) + 1;
    uint32_t texEn = (cmd.w0 >> 16) & 0x1;
    /* Use the guard's already-resolved pointer, not RDP_TranslateAddr(w1) —
     * the registry lookup is what makes legit w1=0x806xxxxx land on the
     * correct host address. Calling RDP_TranslateAddr here defeats the guard
     * and reads from gN64_RDRAM+off (wrong page, silent garbage triangles). */
    /* BKA-POLYSEG2: guard resolves poly via SEGTRY but downstream uses old
     * bka_lookup that returns NULL for 0x56... addresses. tri[0] then
     * SEGFAULTs. Apply same RDRAM+off24 formula here, then fall through to
     * the original chain if that misses. */
    uint8_t* _poly_resolved = NULL;
    {
        extern uint8_t* gN64_RDRAM;
        uint32_t _off24 = cmd.w1 & 0x00FFFFFFu;
        uint32_t _segid = (cmd.w1 >> 24) & 0x0F;
        if (gN64_RDRAM && _off24 < 0x04000000u && _segid < 8) {
            uint8_t* _cand = gN64_RDRAM + _off24;
            if (bka_is_readable(_cand) &&
                bka_is_readable(_cand + 15)) {
                _poly_resolved = _cand;
                static int _ps2 = 0; if (_ps2++ < 15)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-POLYSEG2",
                        "w1=0x%08X off24=0x%X src=%p",
                        (unsigned)cmd.w1, _off24, (void*)_cand);
            }
        }
    }
    if (!_poly_resolved) _poly_resolved = (uint8_t*)bka_lookup_addr_by_low32(cmd.w1);
    if (!_poly_resolved) {
        static int _nr = 0;
        if (_nr++ < 40)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-POLY-NOREG",
                "w1=0x%08X not in registry (would need registration)", cmd.w1);
        uint32_t _sw = __builtin_bswap32(cmd.w1);
        _poly_resolved = (uint8_t*)bka_lookup_addr_by_low32(_sw);
    }
    if (!_poly_resolved) {
        /* Some OS_K0_TO_PHYSICAL paths use arithmetic (no registration).
         * Try reverse: w1 may be (low32 - 0x80000000) of a heap ptr.
         * Compute low32 candidate, see if any registered host ptr has
         * that low32. Also try reverse-bswap + segment fallback. */
        uint32_t _cand_low = cmd.w1 + 0x80000000u;
        _poly_resolved = (uint8_t*)bka_lookup_addr_by_low32(_cand_low);
        if (!_poly_resolved) {
            uint32_t _cand_sw = __builtin_bswap32(cmd.w1);
            uint32_t _cand_sw_low = _cand_sw + 0x80000000u;
            _poly_resolved = (uint8_t*)bka_lookup_addr_by_low32(_cand_sw_low);
        }
    }
    if (!_poly_resolved) _poly_resolved = RDP_TranslateAddr(cmd.w1);
    DkrTriangle* tri = (DkrTriangle*)_poly_resolved;
    {
        static int _pp = 0;
        _pp++;
        if (_pp <= 5 || (_pp % 5000) == 0)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-POLYPASS",
                "pass#%d w1=%08X tri=%p numTris=%u first_vi=(%d,%d,%d)",
                _pp, cmd.w1, (void*)tri, (unsigned)((((cmd.w0 >> 20) & 0xF)) + 1),
                tri[0].vi0, tri[0].vi1, tri[0].vi2);
        {
            static int _pb = 0;
            if (_pb++ < 20) {
                const uint8_t* b = (const uint8_t*)_poly_resolved;
                __android_log_print(ANDROID_LOG_ERROR, "BKA-POLYBYTES",
                    "tri=%p w1=0x%08X b0..15=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                    (void*)_poly_resolved, cmd.w1,
                    b[0],b[1],b[2],b[3], b[4],b[5],b[6],b[7],
                    b[8],b[9],b[10],b[11], b[12],b[13],b[14],b[15]);
            }
        }
    }
    static int _plog = 0;
    if (_plog++ < 20)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-POLY",
            "tris=%u texEn=%u w0=%08X w1=%08X tri=%p dmemVtx=%d",
            numTris, texEn, cmd.w0, cmd.w1, (void*)tri, s_rdp.dmemVertexCount);
    if (!tri) return;

    {
        static int _phex = 0;
        if (_phex++ < 4) {
            uint8_t* b = (uint8_t*)tri;
            diag_log("BKA-POLY-HEX",
                "poly#%d numTris=%u w0=%08X w1=%08X "
                "b=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                _phex, numTris, cmd.w0, cmd.w1,
                b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
                b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
        }
    }

    {
        static int _pdiag = 0;
        if (_pdiag++ < 8) {
            int vi0 = tri[0].vi0, vi1 = tri[0].vi1, vi2 = tri[0].vi2;
            diag_log("BKA-POLY-DIAG",
                "numTris=%u texEn=%u tri=%p dmemVtx=%d vi=(%d,%d,%d) "
                "v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d) "
                "uv0=(%d,%d) uv1=(%d,%d) uv2=(%d,%d) col0=(%d,%d,%d,%d)",
                numTris, texEn, (void*)tri, s_rdp.dmemVertexCount,
                vi0, vi1, vi2,
                vi0 < 256 ? s_rdp.dmem[vi0].x : 0,
                vi0 < 256 ? s_rdp.dmem[vi0].y : 0,
                vi0 < 256 ? s_rdp.dmem[vi0].z : 0,
                vi1 < 256 ? s_rdp.dmem[vi1].x : 0,
                vi1 < 256 ? s_rdp.dmem[vi1].y : 0,
                vi1 < 256 ? s_rdp.dmem[vi1].z : 0,
                vi2 < 256 ? s_rdp.dmem[vi2].x : 0,
                vi2 < 256 ? s_rdp.dmem[vi2].y : 0,
                vi2 < 256 ? s_rdp.dmem[vi2].z : 0,
                tri[0].uv0u, tri[0].uv0v,
                tri[0].uv1u, tri[0].uv1v,
                tri[0].uv2u, tri[0].uv2v,
                vi0 < 256 ? s_rdp.dmem[vi0].r : 0,
                vi0 < 256 ? s_rdp.dmem[vi0].g : 0,
                vi0 < 256 ? s_rdp.dmem[vi0].b : 0,
                vi0 < 256 ? s_rdp.dmem[vi0].a : 0);
        }
    }

    {
        static int _md = 0;
        if (_md++ < 4) {
            diag_log("BKA-MTX-DIAG",
                "proj[0][0]=%.3f [1][1]=%.3f [2][2]=%.3f [3][3]=%.3f "
                "mv[0][0]=%.3f [1][1]=%.3f [2][2]=%.3f [3][3]=%.3f",
                s_rdp.projection[0][0], s_rdp.projection[1][1], s_rdp.projection[2][2], s_rdp.projection[3][3],
                s_rdp.modelview[0][0], s_rdp.modelview[1][1], s_rdp.modelview[2][2], s_rdp.modelview[3][3]);
        }
    }

    int saved_enabled = s_rdp.textureEnabled;
    s_rdp.textureEnabled = (int)texEn;

    {
        static int _pdiag2 = 0;
        if (_pdiag2++ < 4) {
            for (uint32_t i = 0; i < numTris && i < 8; i++) {
                diag_log("BKA-POLY-TRI",
                    "poly#%d tri#%u vi=(%u,%u,%u) dmemVtx=%d "
                    "v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d)",
                    _pdiag2, i, tri[i].vi0, tri[i].vi1, tri[i].vi2,
                    s_rdp.dmemVertexCount,
                    tri[i].vi0 < 256 ? s_rdp.dmem[tri[i].vi0].x : 0,
                    tri[i].vi0 < 256 ? s_rdp.dmem[tri[i].vi0].y : 0,
                    tri[i].vi0 < 256 ? s_rdp.dmem[tri[i].vi0].z : 0,
                    tri[i].vi1 < 256 ? s_rdp.dmem[tri[i].vi1].x : 0,
                    tri[i].vi1 < 256 ? s_rdp.dmem[tri[i].vi1].y : 0,
                    tri[i].vi1 < 256 ? s_rdp.dmem[tri[i].vi1].z : 0,
                    tri[i].vi2 < 256 ? s_rdp.dmem[tri[i].vi2].x : 0,
                    tri[i].vi2 < 256 ? s_rdp.dmem[tri[i].vi2].y : 0,
                    tri[i].vi2 < 256 ? s_rdp.dmem[tri[i].vi2].z : 0);
            }
        }
    }

    {
        static int _pcnt = 0;
        if (_pcnt++ < 8) {
            int _pass = 0, _skip = 0;
            for (uint32_t i = 0; i < numTris; i++) {
                if (tri[i].vi0 >= (uint32_t)s_rdp.dmemVertexCount ||
                    tri[i].vi1 >= (uint32_t)s_rdp.dmemVertexCount ||
                    tri[i].vi2 >= (uint32_t)s_rdp.dmemVertexCount) _skip++;
                else _pass++;
            }
            diag_log("BKA-POLY-SUMMARY",
                "poly#%d numTris=%u pass=%d skip=%d dmemVtx=%d",
                _pcnt, numTris, _pass, _skip, s_rdp.dmemVertexCount);
        }
    }

    for (uint32_t i = 0; i < numTris; i++, tri++) {
        uint32_t vi0 = tri->vi0, vi1 = tri->vi1, vi2 = tri->vi2;
        if (vi0 >= (uint32_t)s_rdp.dmemVertexCount ||
            vi1 >= (uint32_t)s_rdp.dmemVertexCount ||
            vi2 >= (uint32_t)s_rdp.dmemVertexCount) continue;

        BKVertex* v0 = &s_rdp.dmem[vi0];
        BKVertex* v1 = &s_rdp.dmem[vi1];
        BKVertex* v2 = &s_rdp.dmem[vi2];

        {
            static int s_draw = 0;
            if (s_draw++ < 4) {
                float d_x0, d_y0, d_x1, d_y1, d_x2, d_y2;
                TransformVertex(v0, &d_x0, &d_y0);
                TransformVertex(v1, &d_x1, &d_y1);
                TransformVertex(v2, &d_x2, &d_y2);
                diag_log("BKA-DRAW",
                    "vi=(%u,%u,%u) v0=(%d,%d,%d) v1=(%d,%d,%d) v2=(%d,%d,%d) "
                    "xy=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) "
                    "w=(%.2f,%.2f,%.2f) col0=(%d,%d,%d,%d)",
                    vi0, vi1, vi2,
                    v0->x,v0->y,v0->z, v1->x,v1->y,v1->z, v2->x,v2->y,v2->z,
                    d_x0,d_y0, d_x1,d_y1, d_x2,d_y2,
                    ComputeClipW(v0), ComputeClipW(v1), ComputeClipW(v2),
                    v0->r, v0->g, v0->b, v0->a);
            }
        }

        float sx0, sy0, sx1, sy1, sx2, sy2;
        TransformVertex(v0, &sx0, &sy0);
        TransformVertex(v1, &sx1, &sy1);
        TransformVertex(v2, &sx2, &sy2);
        {
            static int _pm = 0;
            if (_pm++ < 8) {
                extern uint8_t *gVideoCurrFramebuffer;
                __android_log_print(ANDROID_LOG_ERROR, "BKA-PMAT",
                    "proj_r3=(%.3f,%.3f,%.3f,%.3f) mv_r3=(%.3f,%.3f,%.3f,%.3f) "
                    "sx=(%.1f,%.1f,%.1f) sy=(%.1f,%.1f,%.1f) fb=%p",
                    s_rdp.projection[3][0], s_rdp.projection[3][1],
                    s_rdp.projection[3][2], s_rdp.projection[3][3],
                    s_rdp.modelview[3][0], s_rdp.modelview[3][1],
                    s_rdp.modelview[3][2], s_rdp.modelview[3][3],
                    sx0, sx1, sx2, sy0, sy1, sy2,
                    (void*)gVideoCurrFramebuffer);
            }
        }


        RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
            v0->r, v0->g, v0->b, v0->a,
            v1->r, v1->g, v1->b, v1->a,
            v2->r, v2->g, v2->b, v2->a,
            tri->uv0u, tri->uv0v,
            tri->uv1u, tri->uv1v,
            tri->uv2u, tri->uv2v);
    }

    s_rdp.textureEnabled = saved_enabled;
}

static void Cmd_Tri1(GfxCommand cmd) {
    g_bka_task_tri_count++;
    static int s_tri1_calls = 0;
    if (++s_tri1_calls % 200 == 1 || s_tri1_calls < 5) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "Cmd_Tri1 CALLED #%d: w0=0x%08X w1=0x%08X dmem=%d",
            s_tri1_calls, cmd.w0, cmd.w1, s_rdp.dmemVertexCount);
    }
    // No vertices loaded yet; skip to avoid out-of-bounds and crash.
    if (s_rdp.dmemVertexCount == 0) return;

    uint32_t packed = cmd.w1;   // F3DEX_GBI: indices packed in w1, opcode only in w0
    uint32_t v0 = ((packed >> 16) & 0xFF) >> 1;
    uint32_t v1 = ((packed >> 8) & 0xFF) >> 1;
    uint32_t v2 = (packed & 0xFF) >> 1;

    if (v0 >= (uint32_t)s_rdp.dmemVertexCount ||
        v1 >= (uint32_t)s_rdp.dmemVertexCount ||
        v2 >= (uint32_t)s_rdp.dmemVertexCount) {
        __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
            "Cmd_Tri1: INVALID vertex indices %u,%u,%u (dmemVertexCount=%d) w1=0x%08X",
            v0, v1, v2, s_rdp.dmemVertexCount, cmd.w1);
        return;
    }

    BKVertex* vert0 = &s_rdp.dmem[v0];
    BKVertex* vert1 = &s_rdp.dmem[v1];
    BKVertex* vert2 = &s_rdp.dmem[v2];

    if (ComputeClipW(vert0) <= 0.01f || ComputeClipW(vert1) <= 0.01f ||
        ComputeClipW(vert2) <= 0.01f) {
        return;  // vertex at or behind camera; skip
    }

    float sx0, sy0, sx1, sy1, sx2, sy2;
    TransformVertex(vert0, &sx0, &sy0);
    TransformVertex(vert1, &sx1, &sy1);
    TransformVertex(vert2, &sx2, &sy2);

    { static int s_rt = 0; if (s_rt++ < 20) LOGV("RASTER v=(%p %p %p) dmembase=%p", (void*)vert0, (void*)vert1, (void*)vert2, (void*)s_rdp.dmem); }
    { static int s_t1 = 0; if (s_t1++ < 20)
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "TRI1-DRAW color=(%d,%d,%d,%d) xy=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)",
            vert0->r,vert0->g,vert0->b,vert0->a,
            sx0,sy0, sx1,sy1, sx2,sy2); }
    RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
        vert0->r, vert0->g, vert0->b, vert0->a,
        vert1->r, vert1->g, vert1->b, vert1->a,
        vert2->r, vert2->g, vert2->b, vert2->a,
        vert0->s, vert0->t, vert1->s, vert1->t, vert2->s, vert2->t);
}

// =======================================================================
// G_TRI2 - Draw 2 triangles (4 vertices)
// w0 = [G_TRI2:8][v2:8][v1:8][v0:8]  -- triangle 1 uses v0,v1,v2
// w1 = [flag2:8][v4:8][v3:8][flag1:8]  -- triangle 2 uses v1,v2,v3
// =======================================================================

static void Cmd_Tri2(GfxCommand cmd) {
    g_bka_task_tri_count++;
    { static int _tr = 0; if (_tr++ < 20) {
        extern const uint8_t* s_current_cmd;
        const uint8_t* _c = (const uint8_t*)s_current_cmd;
        __android_log_print(ANDROID_LOG_ERROR, "BKA-TRI2RAW",
            "w0=0x%08X w1=0x%08X dmem=%d cur_bytes=%02X%02X%02X%02X %02X%02X%02X%02X",
            cmd.w0, cmd.w1, s_rdp.dmemVertexCount,
            _c?_c[0]:0, _c?_c[1]:0, _c?_c[2]:0, _c?_c[3]:0,
            _c?_c[4]:0, _c?_c[5]:0, _c?_c[6]:0, _c?_c[7]:0); } }
    { extern int g_bka_last_tri2_count; g_bka_last_tri2_count++; }
    static int s_tri2_calls = 0;
    if (++s_tri2_calls % 200 == 1 || s_tri2_calls < 5) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "Cmd_Tri2 CALLED #%d: w0=0x%08X w1=0x%08X dmem=%d",
            s_tri2_calls, cmd.w0, cmd.w1, s_rdp.dmemVertexCount);
    }
    if (s_rdp.dmemVertexCount == 0) return;

    uint32_t tri1 = cmd.w0 & 0xFFFFFF;  // first triangle packed in lower 24 bits
    uint32_t tri2 = cmd.w1;             // second triangle packed in w1

    uint32_t v00 = ((tri1 >> 16) & 0xFF) >> 1;
    uint32_t v01 = ((tri1 >> 8) & 0xFF) >> 1;
    uint32_t v02 = (tri1 & 0xFF) >> 1;
    uint32_t v10 = ((tri2 >> 16) & 0xFF) >> 1;
    uint32_t v11 = ((tri2 >> 8) & 0xFF) >> 1;
    uint32_t v12 = (tri2 & 0xFF) >> 1;

    if (v00 >= (uint32_t)s_rdp.dmemVertexCount ||
        v01 >= (uint32_t)s_rdp.dmemVertexCount ||
        v02 >= (uint32_t)s_rdp.dmemVertexCount ||
        v10 >= (uint32_t)s_rdp.dmemVertexCount ||
        v11 >= (uint32_t)s_rdp.dmemVertexCount ||
        v12 >= (uint32_t)s_rdp.dmemVertexCount) {
        static int s_tri2_inv_log = 0;
        if (s_tri2_inv_log++ < 50) {
            __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                "Cmd_Tri2: INVALID vertex indices %u,%u,%u,%u,%u,%u (dmemVertexCount=%d) w0=0x%08X w1=0x%08X",
                v00, v01, v02, v10, v11, v12, s_rdp.dmemVertexCount, cmd.w0, cmd.w1);
        }
        return;
    }

    {
        BKVertex* vt0 = &s_rdp.dmem[v00];
        BKVertex* vt1 = &s_rdp.dmem[v01];
        BKVertex* vt2 = &s_rdp.dmem[v02];
        {
            BkaClipVtx cv[3], cclip[4];
            ComputeClip(vt0, &cv[0].x, &cv[0].y, &cv[0].z, &cv[0].w);
            ComputeClip(vt1, &cv[1].x, &cv[1].y, &cv[1].z, &cv[1].w);
            ComputeClip(vt2, &cv[2].x, &cv[2].y, &cv[2].z, &cv[2].w);

            const float EPSW = 64.0f;   // near-plane w in clip space (N64 near-plane in view is typically 50-200)
            bool allIn = cv[0].w > EPSW && cv[1].w > EPSW && cv[2].w > EPSW;
            bool anyIn = cv[0].w > EPSW || cv[1].w > EPSW || cv[2].w > EPSW;

            if (!anyIn) return;   // fully behind camera

            if (!allIn) {
                // Near-plane clip. Real N64 hardware emits the visible
                // portion of a triangle that crosses w=0; skipping the
                // whole triangle was throwing away ~60% of the geometry.
                int nClip = BkaClipNear(cv, EPSW, cclip);
                if (nClip < 3) return;

                float cx[4], cy[4];
                const float CLIP_COORD_LIMIT = (float)(FB_WIDTH * 4);
                for (int i = 0; i < nClip; i++) {
                    float iw = 1.0f / cclip[i].w;
                    float tx = (cclip[i].x * iw + 1.0f) * 0.5f * (float)FB_WIDTH;
                    float ty = (1.0f - cclip[i].y * iw) * 0.5f * (float)FB_HEIGHT;
                    // Clamping preserves topology (winding / side) but keeps
                    // the scanline math from overflowing on near-plane
                    // vertices where 1/w is ~100.
                    if (tx < -CLIP_COORD_LIMIT) tx = -CLIP_COORD_LIMIT;
                    if (tx >  CLIP_COORD_LIMIT) tx =  CLIP_COORD_LIMIT;
                    if (ty < -CLIP_COORD_LIMIT) ty = -CLIP_COORD_LIMIT;
                    if (ty >  CLIP_COORD_LIMIT) ty =  CLIP_COORD_LIMIT;
                    cx[i] = tx;
                    cy[i] = ty;
                }
                BKVertex* csrc = (cv[0].w > EPSW) ? vt0 : ((cv[1].w > EPSW) ? vt1 : vt2);
                uint8_t cr = csrc->r, cg = csrc->g, cb = csrc->b, ca = csrc->a;

                static int s_clipcount = 0;
                if (s_clipcount++ < 20) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "TRI2-CLIP w=(%.2f %.2f %.2f) nClip=%d -> raster",
                        cv[0].w, cv[1].w, cv[2].w, nClip);
                }

                { static int s_cr = 0; if (s_cr++ < 20)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "TRI2-DRAW n=%d color=(%d,%d,%d,%d) xy=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)",
                        nClip, cr,cg,cb,ca,
                        cx[0],cy[0], cx[1],cy[1], cx[2],cy[2]); }
                {
                    static int s_bbox = 0;
                    if (s_bbox++ < 20)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "BBOX n=%d x=(%.1f..%.1f) y=(%.1f..%.1f)",
                            nClip,
                            std::min({cx[0],cx[1],cx[2],cx[3]}), std::max({cx[0],cx[1],cx[2],cx[3]}),
                            std::min({cy[0],cy[1],cy[2],cy[3]}), std::max({cy[0],cy[1],cy[2],cy[3]}));
                }
                if (nClip == 3) {
                    RasterizeTriangle(cx[0],cy[0], cx[1],cy[1], cx[2],cy[2],
                        cr,cg,cb,ca, cr,cg,cb,ca, cr,cg,cb,ca);
                } else {
                    RasterizeTriangle(cx[0],cy[0], cx[1],cy[1], cx[2],cy[2],
                        cr,cg,cb,ca, cr,cg,cb,ca, cr,cg,cb,ca);
                    RasterizeTriangle(cx[0],cy[0], cx[2],cy[2], cx[3],cy[3],
                        cr,cg,cb,ca, cr,cg,cb,ca, cr,cg,cb,ca);
                }
                return;
            }
            // allIn: fall through to the existing TransformVertex path
        }

        float sx0, sy0, sx1, sy1, sx2, sy2;
        TransformVertex(vt0, &sx0, &sy0);
        TransformVertex(vt1, &sx1, &sy1);
        TransformVertex(vt2, &sx2, &sy2);
        {
            static int s_tri2_raster_log = 0;
            if (++s_tri2_raster_log % 200 == 1 || s_tri2_raster_log < 5) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "TRI2 -> raster #%d: screen=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) fin=%d%d%d",
                    s_tri2_raster_log,
                    sx0, sy0, sx1, sy1, sx2, sy2,
                    std::isfinite(sx0), std::isfinite(sy0), std::isfinite(sx2));
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  PROJ  row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.projection[0][0], s_rdp.projection[0][1],
                    s_rdp.projection[0][2], s_rdp.projection[0][3],
                    s_rdp.projection[3][0], s_rdp.projection[3][1],
                    s_rdp.projection[3][2], s_rdp.projection[3][3]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "PROJFULL row1=(%.4f %.4f %.4f %.4f) row2=(%.4f %.4f %.4f %.4f)",
                    s_rdp.projection[1][0], s_rdp.projection[1][1],
                    s_rdp.projection[1][2], s_rdp.projection[1][3],
                    s_rdp.projection[2][0], s_rdp.projection[2][1],
                    s_rdp.projection[2][2], s_rdp.projection[2][3]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  MV    row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.modelview[0][0], s_rdp.modelview[0][1],
                    s_rdp.modelview[0][2], s_rdp.modelview[0][3],
                    s_rdp.modelview[3][0], s_rdp.modelview[3][1],
                    s_rdp.modelview[3][2], s_rdp.modelview[3][3]);
            }
        }
        { static int s_nc = 0; if (s_nc++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
                "TRI2-ALLIN color=(%d,%d,%d) screen=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f)",
                vt0->r, vt0->g, vt0->b,
                sx0, sy0, sx1, sy1, sx2, sy2); }
        RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
            vt0->r, vt0->g, vt0->b, vt0->a,
            vt1->r, vt1->g, vt1->b, vt1->a,
            vt2->r, vt2->g, vt2->b, vt2->a,
        vt0->s, vt0->t, vt1->s, vt1->t, vt2->s, vt2->t);
    }

    {
        BKVertex* vt0 = &s_rdp.dmem[v10];
        BKVertex* vt1 = &s_rdp.dmem[v11];
        BKVertex* vt2 = &s_rdp.dmem[v12];
        float sx0, sy0, sx1, sy1, sx2, sy2;
        TransformVertex(vt0, &sx0, &sy0);
        TransformVertex(vt1, &sx1, &sy1);
        TransformVertex(vt2, &sx2, &sy2);
        {
            static int s_tri2_raster_log = 0;
            if (++s_tri2_raster_log % 200 == 1 || s_tri2_raster_log < 5) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "TRI2 -> raster #%d: screen=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) fin=%d%d%d",
                    s_tri2_raster_log,
                    sx0, sy0, sx1, sy1, sx2, sy2,
                    std::isfinite(sx0), std::isfinite(sy0), std::isfinite(sx2));
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  PROJ  row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.projection[0][0], s_rdp.projection[0][1],
                    s_rdp.projection[0][2], s_rdp.projection[0][3],
                    s_rdp.projection[3][0], s_rdp.projection[3][1],
                    s_rdp.projection[3][2], s_rdp.projection[3][3]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  MV    row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.modelview[0][0], s_rdp.modelview[0][1],
                    s_rdp.modelview[0][2], s_rdp.modelview[0][3],
                    s_rdp.modelview[3][0], s_rdp.modelview[3][1],
                    s_rdp.modelview[3][2], s_rdp.modelview[3][3]);
            }
        }
        RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
            vt0->r, vt0->g, vt0->b, vt0->a,
            vt1->r, vt1->g, vt1->b, vt1->a,
            vt2->r, vt2->g, vt2->b, vt2->a,
        vt0->s, vt0->t, vt1->s, vt1->t, vt2->s, vt2->t);
    }
}


// =======================================================================
// G_TRI1 (F3DEX2 variant, opcode 0xC4)
// w0 = [G_TRI1:8][v0:8][v1:8][v2:8]
// w1 = flag
// =======================================================================
static void Cmd_Tri1_F3DEX2(GfxCommand cmd) {
    g_bka_task_tri_count++;
    if (s_rdp.dmemVertexCount == 0) return;

    uint32_t v0 = (cmd.w0 >> 17) & 0x7F;
    uint32_t v1 = (cmd.w0 >> 9) & 0x7F;
    uint32_t v2 = (cmd.w0 >> 1) & 0x7F;

    if (v0 >= (uint32_t)s_rdp.dmemVertexCount ||
        v1 >= (uint32_t)s_rdp.dmemVertexCount ||
        v2 >= (uint32_t)s_rdp.dmemVertexCount) {
        __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
            "Cmd_Tri1_F3DEX2: INVALID vertex indices %u,%u,%u (dmemVertexCount=%d) w0=0x%08X",
            v0, v1, v2, s_rdp.dmemVertexCount, cmd.w0);
        return;
    }

    BKVertex* vert0 = &s_rdp.dmem[v0];
    BKVertex* vert1 = &s_rdp.dmem[v1];
    BKVertex* vert2 = &s_rdp.dmem[v2];

    float sx0, sy0, sx1, sy1, sx2, sy2;
    TransformVertex(vert0, &sx0, &sy0);
    TransformVertex(vert1, &sx1, &sy1);
    TransformVertex(vert2, &sx2, &sy2);

    RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
        vert0->r, vert0->g, vert0->b, vert0->a,
        vert1->r, vert1->g, vert1->b, vert1->a,
        vert2->r, vert2->g, vert2->b, vert2->a,
        vert0->s, vert0->t, vert1->s, vert1->t, vert2->s, vert2->t);
}

// =======================================================================
// G_TRI2 (F3DEX2 variant, opcode 0x34)
// w0 = [G_TRI2:8][v0:8][v1:8][v2:8]   (first triangle)
// w1 = [flag:8][v3:8][v4:8][v5:8]     (second triangle, flag ignored)
// =======================================================================
static void Cmd_Tri2_F3DEX2(GfxCommand cmd) {
    g_bka_task_tri_count++;
    if (s_rdp.dmemVertexCount == 0) return;

    uint32_t v00 = (cmd.w0 >> 17) & 0x7F;
    uint32_t v01 = (cmd.w0 >> 9) & 0x7F;
    uint32_t v02 = (cmd.w0 >> 1) & 0x7F;
    uint32_t v10 = (cmd.w1 >> 17) & 0x7F;
    uint32_t v11 = (cmd.w1 >> 9) & 0x7F;
    uint32_t v12 = (cmd.w1 >> 1) & 0x7F;

    if (v00 >= (uint32_t)s_rdp.dmemVertexCount ||
        v01 >= (uint32_t)s_rdp.dmemVertexCount ||
        v02 >= (uint32_t)s_rdp.dmemVertexCount ||
        v10 >= (uint32_t)s_rdp.dmemVertexCount ||
        v11 >= (uint32_t)s_rdp.dmemVertexCount ||
        v12 >= (uint32_t)s_rdp.dmemVertexCount) {
        LOGW("Cmd_Tri2_F3DEX2: invalid vertex indices %u,%u,%u,%u,%u,%u",
             v00, v01, v02, v10, v11, v12);
        return;
    }

    {
        BKVertex* vt0 = &s_rdp.dmem[v00];
        BKVertex* vt1 = &s_rdp.dmem[v01];
        BKVertex* vt2 = &s_rdp.dmem[v02];
        float sx0, sy0, sx1, sy1, sx2, sy2;
        TransformVertex(vt0, &sx0, &sy0);
        TransformVertex(vt1, &sx1, &sy1);
        TransformVertex(vt2, &sx2, &sy2);
        {
            static int s_tri2_raster_log = 0;
            if (++s_tri2_raster_log % 200 == 1 || s_tri2_raster_log < 5) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "TRI2 -> raster #%d: screen=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) fin=%d%d%d",
                    s_tri2_raster_log,
                    sx0, sy0, sx1, sy1, sx2, sy2,
                    std::isfinite(sx0), std::isfinite(sy0), std::isfinite(sx2));
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  PROJ  row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.projection[0][0], s_rdp.projection[0][1],
                    s_rdp.projection[0][2], s_rdp.projection[0][3],
                    s_rdp.projection[3][0], s_rdp.projection[3][1],
                    s_rdp.projection[3][2], s_rdp.projection[3][3]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  MV    row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.modelview[0][0], s_rdp.modelview[0][1],
                    s_rdp.modelview[0][2], s_rdp.modelview[0][3],
                    s_rdp.modelview[3][0], s_rdp.modelview[3][1],
                    s_rdp.modelview[3][2], s_rdp.modelview[3][3]);
            }
        }
        RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
            vt0->r, vt0->g, vt0->b, vt0->a,
            vt1->r, vt1->g, vt1->b, vt1->a,
            vt2->r, vt2->g, vt2->b, vt2->a,
        vt0->s, vt0->t, vt1->s, vt1->t, vt2->s, vt2->t);
    }

    {
        BKVertex* vt0 = &s_rdp.dmem[v10];
        BKVertex* vt1 = &s_rdp.dmem[v11];
        BKVertex* vt2 = &s_rdp.dmem[v12];
        float sx0, sy0, sx1, sy1, sx2, sy2;
        TransformVertex(vt0, &sx0, &sy0);
        TransformVertex(vt1, &sx1, &sy1);
        TransformVertex(vt2, &sx2, &sy2);
        {
            static int s_tri2_raster_log = 0;
            if (++s_tri2_raster_log % 200 == 1 || s_tri2_raster_log < 5) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "TRI2 -> raster #%d: screen=(%.1f,%.1f)(%.1f,%.1f)(%.1f,%.1f) fin=%d%d%d",
                    s_tri2_raster_log,
                    sx0, sy0, sx1, sy1, sx2, sy2,
                    std::isfinite(sx0), std::isfinite(sy0), std::isfinite(sx2));
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  PROJ  row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.projection[0][0], s_rdp.projection[0][1],
                    s_rdp.projection[0][2], s_rdp.projection[0][3],
                    s_rdp.projection[3][0], s_rdp.projection[3][1],
                    s_rdp.projection[3][2], s_rdp.projection[3][3]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "  MV    row0=(%.3f %.3f %.3f %.3f) row3=(%.3f %.3f %.3f %.3f)",
                    s_rdp.modelview[0][0], s_rdp.modelview[0][1],
                    s_rdp.modelview[0][2], s_rdp.modelview[0][3],
                    s_rdp.modelview[3][0], s_rdp.modelview[3][1],
                    s_rdp.modelview[3][2], s_rdp.modelview[3][3]);
            }
        }
        RasterizeTriangle(sx0, sy0, sx1, sy1, sx2, sy2,
            vt0->r, vt0->g, vt0->b, vt0->a,
            vt1->r, vt1->g, vt1->b, vt1->a,
            vt2->r, vt2->g, vt2->b, vt2->a,
        vt0->s, vt0->t, vt1->s, vt1->t, vt2->s, vt2->t);
    }
}

// =======================================================================
// G_MOVEMEM - Load matrix (opcode 0xDC)
// w0 = [opcode:8][length:8][offset:8][index:8]
// w1 = address of data in RDRAM
// =======================================================================
static void Matrix_Multiply(BKMatrix result, const BKMatrix a, const BKMatrix b);
static void Cmd_MoveMem(GfxCommand cmd) {
    uint32_t length = (cmd.w0 >> 16) & 0xFF;
    uint32_t offset = (cmd.w0 >> 8) & 0xFF;
    uint32_t index  = cmd.w0 & 0xFF;
    uint32_t addr   = cmd.w1 & 0x0FFFFFFF;
    
    if (!gN64_RDRAM) return;

    /* F3DEX1 G_MOVEMEM (op=0x03) decode. Recomp emits w0 as [op:8][param:8][len_hi:8][len_lo:8].
     * G_MV_MMTX=0x02, G_MV_PMTX=0x06, G_MV_VIEWPORT=0x80.
     * Legacy (length==8 && index==0x0E/0x00) gate never matches — every real load dropped. */
    {
        uint32_t fop    = (cmd.w0 >> 24) & 0xFF;
        uint32_t fparam = (cmd.w0 >> 16) & 0xFF;
        uint32_t flen   =  cmd.w0        & 0xFFFF;
        uint32_t faddr  = cmd.w1;
        if (fop == 0x03) {
            auto resolve_mtx_src = [](uint32_t a) -> void* {
                void *p = (void*)bka_lookup_addr_by_low32(a);
                if (p) return p;
                p = (void*)bka_lookup_addr_by_low32(__builtin_bswap32(a));
                if (p) return p;
                static const uint64_t _pfxs[] = {
                    0x7B00000000ULL, 0x7D00000000ULL, 0x7E00000000ULL,
                    0x7F00000000ULL, 0x0000000000ULL,
                };
                for (int _pk = 0; _pk < 5; _pk++) {
                    uint64_t cand = _pfxs[_pk] | (uint64_t)a;
                    if (!bka_is_readable((void*)cand)) continue;
                    if (!bka_is_readable((void*)((const uint8_t*)cand + 15))) continue;
                    return (void*)cand;
                }
                return nullptr;
            };
            if (fparam == 0x02 && (flen == 64 || flen == 128)) {
                void *m_src = resolve_mtx_src(faddr);
                if (m_src) {
                    Matrix_LoadFromN64(&s_rdp.modelview, m_src);
                    Matrix_Multiply(s_rdp.viewProj, s_rdp.modelview, s_rdp.projection);
                }
                return;
            }
            if (fparam == 0x06 && (flen == 64 || flen == 128)) {
                void *m_src = resolve_mtx_src(faddr);
                if (m_src) {
                    Matrix_LoadFromN64(&s_rdp.projection, m_src);
                    Matrix_Multiply(s_rdp.viewProj, s_rdp.modelview, s_rdp.projection);
                }
                return;
            }
            if (fparam == 0x80 && flen == 16) {
                void *vp_src = resolve_mtx_src(faddr);
                if (vp_src) {
                    static int s_vp = 0; if (s_vp++ < 4) {
                        const uint8_t *b = (const uint8_t*)vp_src;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "VP src=%p b=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                            vp_src, b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
                            b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
                    }
                }
                return;
            }
        }
    }

    
    // Matrix load: index 0x0E = G_MTX_MODELVIEW, 0x00 = G_MTX_PROJECTION
    // offset 0 = projection, offset 0 = modelview (upper bits differ)
    { static int s_mm=0; if(s_mm++<12) __android_log_print(ANDROID_LOG_ERROR,"BKA-MOVEMEM","w0=0x%08X w1=0x%08X op=0x%02X b1=0x%02X b2=0x%02X b3=0x%02X",(unsigned)cmd.w0,(unsigned)cmd.w1,(unsigned)((cmd.w0>>24)&0xFF),(unsigned)((cmd.w0>>16)&0xFF),(unsigned)((cmd.w0>>8)&0xFF),(unsigned)(cmd.w0&0xFF)); }
    if (length == 8 && (index == 0x0E || index == 0x00)) {
        BKMatrix* target;
        // F3DEX2 G_MOVEMEM: index 0x0E = G_MTX_PROJECTION, 0x00 = G_MTX_MODELVIEW
        if (index == 0x0E) {
            target = &s_rdp.projection;
        } else {
            target = &s_rdp.modelview;
        }
        void *mtx_src = RDP_TranslateAddr(addr);
        if (mtx_src) Matrix_LoadFromN64(target, mtx_src);
    }
}

// =======================================================================
// G_MTX - Load matrix (opcode 0xBC)
// F3DEX2 format: w0 = [G_MTX:8][flag:8][param:16], w1 = matrix address
// For our initial implementation, we treat the matrix as modelview.
// =======================================================================
static void Matrix_Multiply(BKMatrix result, const BKMatrix a, const BKMatrix b);

/* DKR MVP slots (include/f3ddkr.h + src/camera.c:950-1559). Three slots
 * hold complete MVP matrices composed on CPU. gSPMatrixDKR(pkt, m, i) sends
 * p3 = i<<6, i.e., bits 6-7 encode slot index (0x00/0x40/0x80).
 * gSPSelectMatrixDKR(pkt, i) is G_MOVEWORD with index G_MW_MVPMATRIX=0x0A.
 * Route every G_MTX as a LOAD into a slot, never as an F3DEX MUL-into-stack. */
static void Cmd_MoveWord(GfxCommand cmd) {
    // F3DEX G_MOVEWORD: w0 = (0xBC << 24) | (index << 16) | offset
    uint32_t op     = (cmd.w0 >> 24) & 0xFF;
    uint32_t index, offset;
    if (op == 0xDB) {
        // F3DEX_GBI_2 gDma1p: w0 = [op][index][offset_hi][offset_lo]
        index  = (cmd.w0 >> 16) & 0xFF;
        offset =  cmd.w0        & 0xFFFF;
    } else {
        // F3DEX_GBI gImmp21: w0 = [op][index][offset_hi][offset_lo]
        // (verified: gSPPopMatrix(1) emits w0=0xBC000040 -> index=0x00 MODELVIEW, offset=0x40)
        // Prior decode had index/offset byte-swapped, so POPMTX (index 0x0E) never matched,
        // matrix stack grew past 16 unboundedly, modelview accumulated garbage each frame.
        index  = (cmd.w0 >> 16) & 0xFF;
        offset =  cmd.w0        & 0xFFFF;
    }
    uint32_t data   =  cmd.w1;

    { static int _mw0 = 0; if (_mw0++ < 40)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-MOVEWORD",
            "op=0x%02X idx=0x%02X off=0x%04X data=0x%08X",
            op, index, offset, data); }

    static int mw_log = 0;
    if (1) {
        LOGV(
            "Cmd_MoveWord index=0x%02X offset=0x%04X data=0x%08X", index, offset, data);
    }

    /* F3DEX1 G_MW_POPMTX is 0x0C, NOT 0x0E (that's G_MW_PERSPNORM).
     * Prior check at 0x0E never matched the game's real gSPPopMatrix(1)
     * emissions. Consequence: 5000 PUSHes/frame, 0 POPs, matrix stack
     * overflowed at depth 16, subsequent MODELVIEW|MULs compounded
     * garbage (mv[3][3]=4005, mv[2][2]=-4174), w=(4005,32107,4005). */
    if (index == 0x00 && (offset & 0x3FF) == 0x006) {
        int slot = (offset >> 10) & 0x07;
        if (slot < 8) {
            void *src = NULL;
            if (data != 0x80000000u && data != 0) {
                src = (void*)bka_lookup_addr_by_low32(data);
                if (!src) src = (void*)bka_lookup_addr_by_low32(__builtin_bswap32(data));
                if (!src) {
                    uint32_t adj = data + 0x80000000u;
                    src = (void*)bka_lookup_addr_by_low32(adj);
                    if (!src) {
                        static const uint64_t _px[] = {
                            0x7B00000000ULL, 0x7D00000000ULL, 0x7E00000000ULL,
                            0x7F00000000ULL, 0x0000000000ULL,
                        };
                        /* BKA-PREFIXPROBE: log gN64_RDRAM + brute-force 256 top-byte prefixes.
                         * In Cmd_MoveWord 0x00 scope: `data` holds the raw address,
                         * matrix is fixed 64 bytes. */
                        {
                            extern uint8_t* gN64_RDRAM;
                            static int _pp = 0;
                            if (_pp++ < 8) {
                                uint64_t _rd64 = (uint64_t)(uintptr_t)gN64_RDRAM;
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-PREFIXPROBE",
                                    "data=0x%08X gN64_RDRAM=0x%llX low32=0x%08X",
                                    data, (unsigned long long)_rd64, (uint32_t)_rd64);
                                int _found = -1;
                                for (int _bt = 0; _bt <= 0xFF && _found < 0; _bt++) {
                                    uint64_t _c = ((uint64_t)_bt << 32) | (uint64_t)data;
                                    if (bka_is_readable((void*)_c) &&
                                        bka_is_readable((void*)((const uint8_t*)_c + 63))) {
                                        _found = _bt;
                                    }
                                }
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-PREFIXPROBE",
                                    "data=0x%08X working_prefix=0x%02X%s",
                                    data, _found >= 0 ? _found : 0,
                                    _found >= 0 ? "" : " (none of 256 readable)");
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-PREFIXPROBE",
                                    "data=0x%08X raw_readable=%d b63=%d adj=0x%08X",
                                    data,
                                    bka_is_readable((void*)(uintptr_t)data),
                                    bka_is_readable((void*)((uintptr_t)data + 63)),
                                    (unsigned)adj);
                            }
                        }
                        /* BKA-RDRAMPFX: PREFIXPROBE proved working prefix is
                         * (gN64_RDRAM >> 32). Try raw data, not adjusted. */
                        {
                            extern uint8_t* gN64_RDRAM;
                            if (gN64_RDRAM) {
                                uint64_t _rdram_pfx = (uint64_t)(uintptr_t)gN64_RDRAM & 0xFF00000000ULL;
                                uint64_t _c = _rdram_pfx | (uint64_t)data;
                                if (bka_is_readable((void*)_c) &&
                                    bka_is_readable((void*)((const uint8_t*)_c + 63))) {
                                    src = (void*)_c;
                                    static int _rp = 0; if (_rp++ < 15)
                                        __android_log_print(ANDROID_LOG_ERROR, "BKA-RDRAMPFX",
                                            "data=0x%08X pre0x%02llX src=%p MW-slot%d",
                                            data, (unsigned long long)(_rdram_pfx >> 32),
                                            (void*)_c, slot);
                                }
                            }
                        }
                        for (int _i = 0; _i < 5; _i++) {
                            uint64_t _c = _px[_i] | (uint64_t)adj;
                            if (bka_is_readable((void*)_c) &&
                                bka_is_readable((void*)((const uint8_t*)_c + 63)))
                                src = (void*)_c;
                        }
                    }
                }
            }
            if (src) {
                BKMatrix _t;
                Matrix_LoadFromN64(&_t, src);
                float _d0 = fabsf(_t[0][0]), _d1 = fabsf(_t[1][1]), _d2 = fabsf(_t[2][2]);
                int _ok = (_d0 < 100.0f || _d1 < 100.0f || _d2 < 100.0f) &&
                          (_d0 > 0.01f || _d1 > 0.01f || _d2 > 0.01f);
                if (_ok) {
                    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
                        s_dkrMvpSlots[slot][i][j] = _t[i][j];
                    bka_recompute_matrices();
                    static int _mw = 0; if (_mw++ < 40)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-MWM",
                            "slot=%d off=0x%04X ACCEPT d0=%.2f d1=%.2f d2=%.2f",
                            slot, offset, _d0, _d1, _d2);
                } else {
                    static int _mr = 0; if (_mr++ < 40)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-MWREJECT",
                            "slot=%d off=0x%04X REJECT d0=%.2f d1=%.2f d2=%.2f",
                            slot, offset, _d0, _d1, _d2);
                }
            }
        }
        return;
    }

    if (index == 0x0A) {  // G_MW_MVPMATRIX — DKR: select active MVP slot
        int sel = (data >> 6) & 0x03;
        s_dkrActiveSlot = sel;
        for (int _i = 0; _i < 4; _i++) for (int _j = 0; _j < 4; _j++) {
            s_rdp.modelview[_i][_j]  = (_i == _j) ? 1.0f : 0.0f;
            s_rdp.projection[_i][_j] = s_dkrMvpSlots[sel][_i][_j];
        }
        static int _sel = 0; if (_sel++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SELMAT",
                "active=%d mvp00=%.4f mvp11=%.4f mvp22=%.4f mvp32=%.2f",
                sel, s_dkrMvpSlots[sel][0][0], s_dkrMvpSlots[sel][1][1],
                s_dkrMvpSlots[sel][2][2], s_dkrMvpSlots[sel][3][2]);
        return;
    }
    if (index == 0x0C) { // G_MW_POPMTX (F3DEX1 gSPPopMatrix)
        /* F3DEX1 POPMTX: offset = N * 64 (bytes per matrix).
           Pop N entries from the modelview stack. */
        int n = (int)(offset / 64);
        if (n <= 0) n = 1;
        while (n-- > 0 && s_rdp.modelviewStackDepth > 0) {
            s_rdp.modelviewStackDepth--;
            memcpy(s_rdp.modelview,
                   s_rdp.modelviewStack[s_rdp.modelviewStackDepth],
                   sizeof(BKMatrix));
        }
        Matrix_Multiply(s_rdp.viewProj, s_rdp.modelview, s_rdp.projection);
        static int s_pop = 0;
        if (s_pop++ < 20)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-POPMTX",
                "POPMTX popped %d, depth now %d", (int)(offset/64), s_rdp.modelviewStackDepth);
        return;
    }
    /* Dead duplicate removed — the 0x0C block above handles POPMTX. */
    if (index == 0x06) { // G_MW_SEGMENT
        uint32_t segment = (offset / 4) & 0x0F;
        uint32_t a = data;
        if (a == 0xFFFFFFFFu) {
            static int s_badseg = 0;
            if (s_badseg++ < 20)
                __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                    "Cmd_MoveWord SEGMENT seg=%u base=0x%08X INVALID — skipping",
                    segment, a);
            return;
        }
        /* Segment base is meant to be a physical RDRAM offset (0..64MB).
         * The port's gSPSegment macro stores (uint32_t)host_ptr, which is
         * a truncated host pointer (0xD0..0x7B range). Adding a small
         * offset to that overflows uint32 and produces garbage. Recover
         * the real host pointer from the registry (populated by
         * bka_k0_to_phys at OS_K0_TO_PHYSICAL call sites) and convert to
         * a physical offset relative to gN64_RDRAM. */
        {
            extern int bka_is_readable(void*);
            extern uint8_t *gN64_RDRAM;
            void *recovered = bka_lookup_addr_by_low32(a);
            if (!recovered) {
                /* Also try KSEG0-stripped form */
                uint32_t a_sw = __builtin_bswap32(a);
                recovered = bka_lookup_addr_by_low32(a_sw);
            }
            if (recovered && gN64_RDRAM) {
                uint8_t *rp = (uint8_t*)recovered;
                if (rp >= gN64_RDRAM && rp < gN64_RDRAM + 0x4000000) {
                    s_rdp.segmentBase[segment] = (uint32_t)(rp - gN64_RDRAM);
                    { static int _srec = 0; if (_srec++ < 20)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-SEGREC",
                            "seg=%u raw=0x%08X host=%p offset=0x%X",
                            segment, a, recovered,
                            (uint32_t)(rp - gN64_RDRAM)); }
                    return;
                }
            }
            if (a < 0x4000000) {
                /* Already a physical offset */
                s_rdp.segmentBase[segment] = a;
                return;
            }
        }
        s_rdp.segmentBase[segment] = (uintptr_t)a;
        if (segment == 2) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-SEG2",
                "SEG2 WRITE base=0x%08X (was 0x%08lX)",
                data, (unsigned long)data);
        }

        static int seg_log = 0;
        if (seg_log++ < 20) {
            __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
                "Cmd_MoveWord SEGMENT seg=%u base=0x%08X", segment, data);
        }
    }
}

// =======================================================================
// G_MTX - Load matrix (opcode 0x01)
// =======================================================================
static void Matrix_Multiply(BKMatrix result, const BKMatrix a, const BKMatrix b) {
    BKMatrix temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i][j] = 0;
            for (int k = 0; k < 4; k++) {
                temp[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    memcpy(result, temp, sizeof(BKMatrix));
}

/* DKR 3-slot model (camera.c): slot 0=projection, slot 1=view, slot 2=model.
 * TransformVertex applies s_rdp.modelview then s_rdp.projection, so compose:
 *   s_rdp.projection = slots[0]
 *   s_rdp.modelview  = slots[1] * slots[2]   (view * model) */
/* BKA_FORCE_IDENTITY: set to 1 to bypass all matrix loading and force
 * identity on both proj and mv. Diagnostic for isolating whether the
 * matrix pipeline is the sole blocker for rasterization. */
static int g_bka_force_identity = 1;

static void bka_recompute_matrices(void) {
    if (g_bka_force_identity) {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                s_rdp.projection[i][j] = (i == j) ? 1.0f : 0.0f;
                s_rdp.modelview[i][j]  = (i == j) ? 1.0f : 0.0f;
            }
        static int _fi = 0; if (_fi++ < 3)
            __android_log_print(ANDROID_LOG_ERROR, "BKA-FORCEIDENT",
                "identity forced (bypass matrix pipeline)");
        return;
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            s_rdp.projection[i][j] = s_dkrMvpSlots[0][i][j];
    BKMatrix vm;
    Matrix_Multiply(vm, s_dkrMvpSlots[1], s_dkrMvpSlots[2]);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            s_rdp.modelview[i][j] = vm[i][j];
}

// F3DEX (non-2) matrix flag bits — Banjo-Kazooie
#ifndef G_MTX_PROJECTION
#define G_MTX_PROJECTION  0x01
#endif
#ifndef G_MTX_LOAD
#define G_MTX_LOAD        0x02
#endif
#ifndef G_MTX_PUSH
#define G_MTX_PUSH        0x04
#endif

#ifndef G_MTX_PROJECTION
#define G_MTX_PROJECTION  0x01
#endif
#ifndef G_MTX_LOAD
#define G_MTX_LOAD        0x02
#endif
#ifndef G_MTX_PUSH
#define G_MTX_PUSH        0x04
#endif

#ifndef G_MTX_PROJECTION
#define G_MTX_PROJECTION  0x01
#endif
#ifndef G_MTX_LOAD
#define G_MTX_LOAD        0x02
#endif
#ifndef G_MTX_PUSH
#define G_MTX_PUSH        0x04
#endif

static void Cmd_Mtx(GfxCommand cmd) {
    { static int _beacon = 0; if (_beacon++ == 0)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-BUILD",
            "Cmd_Mtx beacon: code loaded, first MTX seen"); }
    /* F3DEX_GBI_2 gDma1p layout: w0 = [op:8][p2_hi:8][p3:8][p2_lo:8].
 * gSPMatrix XORs p3 with G_MTX_PUSH, so the raw byte is (p_user ^ G_MTX_PUSH).
 * Decode: p3_raw = (w0 >> 8) & 0xFF; p_user = p3_raw ^ G_MTX_PUSH.
 * Current code reads (w0 >> 16) which is p2_hi (always 0x00 for 128-byte Mtx),
 * causing all 5000 per-frame MODELVIEW|PUSH commands to be treated as MUL —
 * modelview accumulates garbage to mv[2][2]=-5121. */
    /* DKR slot load: p3 bits 6-7 = slot index. Every G_MTX in DKR is a slot
     * load (see f3ddkr.h gSPMatrixDKR); game never emits F3DEX PUSH. Composed
     * MVP goes into slot, modelview=identity, projection=slot[active], so
     * existing TransformVertex (mv×proj) yields slot[active]×v. */
    {
        uint32_t p3_dkr = (cmd.w0 >> 8) & 0xFF;   /* p3 flags at bits 15:8; >>16 reads p2_hi (always 0) */
        int dkr_slot = (p3_dkr >> 6) & 0x03;
        void *src = (void*)bka_lookup_addr_by_low32(cmd.w1);
        if (!src) src = (void*)bka_lookup_addr_by_low32(__builtin_bswap32(cmd.w1));
        if (!src) {
            static const uint64_t _pfx[] = {
                0x7B00000000ULL, 0x7D00000000ULL, 0x7E00000000ULL,
                0x7F00000000ULL, 0ULL,
            };
            for (int _i = 0; _i < 5; _i++) {
                uint64_t _cand = _pfx[_i] | (uint64_t)cmd.w1;
                if (bka_is_readable((void*)_cand) &&
                    bka_is_readable((void*)((const uint8_t*)_cand + 63))) {
                    src = (void*)_cand; break;
                }
            }
        }
        if (src) {
            { static int _raw = 0;
              if (_raw++ < 8 && bka_is_readable((void*)src) &&
                  bka_is_readable((void*)((const uint8_t*)src + 63))) {
                const uint8_t* _rb = (const uint8_t*)src;
                __android_log_print(ANDROID_LOG_ERROR, "BKA-MTXRAW",
                    "slot=%d w0=0x%08X src=%p | b0_3=%02X%02X%02X%02X b4_7=%02X%02X%02X%02X b8_11=%02X%02X%02X%02X b12_15=%02X%02X%02X%02X | b32_35=%02X%02X%02X%02X b36_39=%02X%02X%02X%02X",
                    dkr_slot, (unsigned)cmd.w0, (void*)src,
                    _rb[0],_rb[1],_rb[2],_rb[3], _rb[4],_rb[5],_rb[6],_rb[7],
                    _rb[8],_rb[9],_rb[10],_rb[11], _rb[12],_rb[13],_rb[14],_rb[15],
                    _rb[32],_rb[33],_rb[34],_rb[35], _rb[36],_rb[37],_rb[38],_rb[39]);
              }
            }
            Matrix_LoadFromN64(&s_dkrMvpSlots[dkr_slot], src);
            s_dkrActiveSlot = dkr_slot;
            bka_recompute_matrices();
            static int _sl = 0; if (_sl++ < 12)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-MTX2",
                    "slot=%d src=%p mvp00=%.4f mvp11=%.4f mvp22=%.4f mvp32=%.2f active=%d",
                    dkr_slot, src,
                    s_dkrMvpSlots[dkr_slot][0][0], s_dkrMvpSlots[dkr_slot][1][1],
                    s_dkrMvpSlots[dkr_slot][2][2], s_dkrMvpSlots[dkr_slot][3][2],
                    s_dkrActiveSlot);
        } else {
            static int _sf = 0; if (_sf++ < 8)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-SLOTFAIL",
                    "slot=%d w1=0x%08X not resolvable", dkr_slot, cmd.w1);
        }
        return;
    }
    uint32_t p3_raw = (cmd.w0 >> 8) & 0xFF;
    uint32_t flag   = p3_raw ^ G_MTX_PUSH;
    /* F3DEX v1 G_MTX uses only bits 0-2 (PROJECTION=0x01, LOAD=0x02,
     * PUSH=0x04).  The recomp emits high bits in the flag byte that are
     * not part of the F3DEX spec but do not change the low 3 bits
     * (observed values: 0x15, 0x1E, 0x20, 0x2C, 0x34, 0x46, 0x4C, 0xAD).
     * Mask them off rather than rejecting, or 30000+ legitimate matrix
     * loads get dropped and the modelview goes stale. */
    /* F3DEX v1 G_MTX valid flags are 0x00-0x07.  Anything with bits
     * 3-7 set is walker drift into non-command data, and its w1 is a
     * garbage address (observed: 0x017C0000, 0x06340000, 0x03300000).
     * Reject the command. */
    {
        static int _md = 0;
        if (_md++ < 16) {
            uint32_t _raw = cmd.w0;
            /* dump the 8 bytes the walker just consumed, plus the walker's
               cursor, so we can tell whether w0/w1 are byte-shifted,
               byte-swapped, or just plain mis-decoded */
            uint8_t *_p = (uint8_t *)&cmd.w0;
            diag_log("BKA-MTX-CMD",
                "w0=0x%08X w1=0x%08X src=%p bytes=%02X%02X%02X%02X%02X%02X%02X%02X "
                "op=%02X b23_16=%02X b15_8=%02X b7_0=%02X flag=%02X",
                _raw, cmd.w1, (void *)&cmd.w0,
                _p[0],_p[1],_p[2],_p[3],_p[4],_p[5],_p[6],_p[7],
                (_raw >> 24) & 0xFF, (_raw >> 16) & 0xFF,
                (_raw >> 8) & 0xFF, _raw & 0xFF,
                (uint32_t)flag);
            /* also test the "byte-swapped w1" hypothesis inline */
            uint32_t _sw = ((cmd.w1 & 0xFF) << 24) | ((cmd.w1 & 0xFF00) << 8)
                         | ((cmd.w1 >> 8) & 0xFF00)   | ((cmd.w1 >> 24) & 0xFF);
            diag_log("BKA-MTX-SWAP", "w1=0x%08X swapped=0x%08X", cmd.w1, _sw);
        }
    }
    flag &= 0x07;  /* F3DEX v1: mask off non-spec high bits the recomp emits */
    {
        static int _pp = 0;
        if (_pp++ < 32) {
            uint32_t _p = (cmd.w0 >> 16) & 0xFF;
            if (_p != 0) {
                diag_log("BKA-MTX-PARAM", "w0=0x%08X w1=0x%08X param=0x%02X",
                         cmd.w0, cmd.w1, _p);
            }
        }
    }
    if (flag & 0xF8) {
        static int s_mtx_hi = 0;
        if (s_mtx_hi++ < 10)
            __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                "Cmd_Mtx: bad flag=0x%02X w1=0x%08X — skipping (drift)",
                flag, cmd.w1);
        return;
    }
    /* Fix: consult host-pointer registry FIRST. bka_k0_to_phys registers
     * both low-32 and KSEG0-stripped low-32 to the host pointer. RDP_TranslateAddr
     * does segment arithmetic and resolves KSEG0 to gN64_RDRAM+off — wrong address. */
    void *mtx_src = (void*)bka_lookup_addr_by_low32(cmd.w1);
    /* Reject JSON/string false positives from registry: a real N64 matrix
     * starts with int[0][0] in {0x0001, 0x0000, 0xFFFF, 0x0000xxxx} and
     * bytes 4-7 are near-zero (int[0][1..3]). JSON starts with ASCII. */
    if (mtx_src) {
        const uint8_t* _mb = (const uint8_t*)mtx_src;
        uint8_t _b0 = _mb[0], _b1 = _mb[1], _b2 = _mb[2], _b3 = _mb[3];
        uint8_t _b4 = _mb[4], _b5 = _mb[5], _b6 = _mb[6], _b7 = _mb[7];
        /* ASCII heuristic: if bytes[0..7] are all in [0x20..0x7E], it's text */
        int _text = 1;
        for (int _i = 0; _i < 8; _i++) {
            if (_mb[_i] < 0x20 || _mb[_i] > 0x7E) { _text = 0; break; }
        }
        if (_text) {
            static int _jr = 0;
            if (_jr++ < 20) diag_log("BKA-MTX-REJECT",
                "w1=0x%08X ptr=%p is-text b=%02X%02X%02X%02X%02X%02X%02X%02X",
                cmd.w1, mtx_src, _b0,_b1,_b2,_b3,_b4,_b5,_b6,_b7);
            mtx_src = NULL;
        }
    }
    if (!mtx_src) {
        uint32_t _sw = __builtin_bswap32(cmd.w1);
        mtx_src = (void*)bka_lookup_addr_by_low32(_sw);
    }
    if (!mtx_src) mtx_src = RDP_TranslateAddr(cmd.w1);
    {
        int _ok = 0;
        if (mtx_src && bka_is_readable(mtx_src) &&
            bka_is_readable((void*)((const uint8_t*)mtx_src + 7))) {
            const uint8_t *_b = (const uint8_t*)mtx_src;
            uint16_t _i00 = (uint16_t)((_b[0] << 8) | _b[1]);
            uint16_t _i01 = (uint16_t)((_b[2] << 8) | _b[3]);
            if ((_i00 == 0x0001 || _i00 == 0xFFFF) && _i01 == 0x0000) _ok = 1;
        }
        if (!_ok) {
            /* PORT: same reconstruction as Cmd_Vtx. Vertices proved the
             * winning prefix is 0x7B; the registry-resolved fallback lands
             * on a scratch buffer (all-zero bytes). Try 5 prefixes, pick
             * the one that yields a plausible affine matrix with the most
             * non-zero bytes (real matrix vs. mostly-zero scratch). */
            static const uint64_t _pfxs[] = {
                0x7B00000000ULL, 0x7D00000000ULL, 0x7E00000000ULL,
                0x7F00000000ULL, 0x0000000000ULL,
            };
            static const char *_pflabels[] = {"7B","7D","7E","7F","00"};
            int _best_nz = -1;
            uint64_t _best_p = 0;
            uint16_t _best_i00 = 0;
            int _best_pk = -1;
            for (int _pk = 0; _pk < 5; _pk++) {
                uint64_t _p = _pfxs[_pk] | (uint64_t)cmd.w1;
                if (!bka_is_readable((void*)_p)) continue;
                if (!bka_is_readable((void*)((const uint8_t*)_p + 63))) continue;
                const uint8_t *_b = (const uint8_t*)_p;
                uint16_t _i00 = (uint16_t)((_b[0] << 8) | _b[1]);
                uint16_t _i01 = (uint16_t)((_b[2] << 8) | _b[3]);
                uint16_t _i02 = (uint16_t)((_b[4] << 8) | _b[5]);
                uint16_t _i03 = (uint16_t)((_b[6] << 8) | _b[7]);
                uint16_t _i10 = (uint16_t)((_b[8] << 8) | _b[9]);
                uint16_t _i20 = (uint16_t)((_b[16] << 8) | _b[17]);
                uint16_t _i30 = (uint16_t)((_b[24] << 8) | _b[25]);
                int _diag_ok = (_i01 == 0 && _i02 == 0 && _i03 == 0 &&
                                _i10 == 0 && _i20 == 0 && _i30 == 0);
                int _i00_ok = (_i00 == 0x0000 || _i00 == 0x0001 ||
                               _i00 == 0xFFFE || _i00 == 0xFFFF);
                int _nz = 0;
                for (int _i = 0; _i < 64; _i++) if (_b[_i]) _nz++;
                { static int _pd = 0; if (_pd++ < 30) diag_log("BKA-MTX-PFX-CHK",
                    "w1=0x%08X pf=%s ptr=0x%llX i00=0x%04X diag_ok=%d i00_ok=%d nz=%d",
                    cmd.w1, _pflabels[_pk], (unsigned long long)_p,
                    (uint32_t)_i00, _diag_ok, _i00_ok, _nz); }
                if (!_diag_ok || !_i00_ok) continue;
                if (_nz > _best_nz) {
                    _best_nz = _nz; _best_p = _p; _best_i00 = _i00; _best_pk = _pk;
                }
            }
            if (_best_pk >= 0) {
                static int _lg = 0;
                if (_lg++ < 20) diag_log("BKA-MTX-REBUILD",
                    "w1=0x%08X pf=%s ptr=0x%llX i00=0x%04X nz=%d",
                    cmd.w1, _pflabels[_best_pk], (unsigned long long)_best_p,
                    (uint32_t)_best_i00, _best_nz);
                mtx_src = (void*)_best_p;
            }
        }
    }
    {
        static int _rr = 0;
        if (_rr++ < 16) {
            extern uint8_t *gN64_RDRAM;
            void *low = (void*)bka_lookup_addr_by_low32(cmd.w1);
            uint8_t seg4 = (cmd.w1 >> 24) & 0x0F;
            uint8_t seg8 = (cmd.w1 >> 24) & 0xFF;
            diag_log("BKA-MTX-RESOLVE",
                "w1=0x%08X seg4=0x%02X seg8=0x%02X segB[seg4]=0x%lX segB[seg8]=0x%lX "
                "RDRAM=%p rdp_src=%p low32_src=%p",
                cmd.w1, seg4, seg8,
                (unsigned long)s_rdp.segmentBase[seg4],
                (seg8 < 16 ? (unsigned long)s_rdp.segmentBase[seg8] : 0xDEADBEEFul),
                (void*)gN64_RDRAM, mtx_src, low);
        }
    }
    /* One-shot: try every plausible arm64 heap prefix on w1 and dump
       first 16 bytes at each reconstruction. Real matrix at one of them
       → that prefix is the fix. None of them → recomp isn't writing to
       any mapped location w1 can reach. */
    {
        static int _pfx = 0;
        if (_pfx == 0 && (cmd.w1 >> 24) == 0x2E) {
            _pfx = 1;
            uint64_t prefixes[] = {
                0x7D00000000ULL, 0x7C00000000ULL, 0x7E00000000ULL,
                0x7F00000000ULL, 0x7900000000ULL, 0x7800000000ULL,
                0x7700000000ULL, 0x7600000000ULL, 0x7500000000ULL,
                0x7400000000ULL, 0x7000000000ULL, 0x6000000000ULL,
            };
            for (int i = 0; i < 12; i++) {
                uint64_t p = prefixes[i] | (uint64_t)cmd.w1;
                const uint8_t *b = (const uint8_t*)p;
                int r = bka_is_readable((void*)p);
                diag_log("BKA-MTX-PFX",
                    "pfx=0x%02llX ptr=0x%llX rd=%d "
                    "b=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                    (unsigned long long)(prefixes[i] >> 32),
                    (unsigned long long)p, r,
                    r?b[0]:0,r?b[1]:0,r?b[2]:0,r?b[3]:0,
                    r?b[4]:0,r?b[5]:0,r?b[6]:0,r?b[7]:0,
                    r?b[8]:0,r?b[9]:0,r?b[10]:0,r?b[11]:0,
                    r?b[12]:0,r?b[13]:0,r?b[14]:0,r?b[15]:0);
            }
            diag_log("BKA-MTX-PFX-END", "w1=0x%08X done", cmd.w1);
        }
    }
    /* One-shot: locate real matrices in RDRAM by scanning for the row-0
       signature int[0][0]=1.0, int[0][1]=0.0 (bytes 00 01 00 00 00 00 00 00).
       Compare each match's offset against w1 & 0xFFFFFF to reverse-engineer
       the correct address resolution. */
    {
        static int _scan = 0;
        if (_scan < 5 && cmd.w1 != 0xFFFFFFFFu && cmd.w1 != 0) {
            _scan++;
            extern uint8_t *gN64_RDRAM;
            if (gN64_RDRAM) {
                uint32_t _sw_want = __builtin_bswap32(cmd.w1);
                uint32_t want = _sw_want & 0xFFFFFFu;
                int found = 0;
                for (uint32_t i = 0; i < 0x1000000; i += 4) {
                    const uint8_t *p = gN64_RDRAM + i;
                    if (p[0]==0x00 && p[1]==0x01 && p[2]==0x00 && p[3]==0x00 &&
                        p[4]==0x00 && p[5]==0x00 && p[6]==0x00 && p[7]==0x00) {
                        if (found < 64) {
                            diag_log("BKA-MTX-SCAN",
                                "off=0x%06X w1off=0x%06X delta=0x%06X",
                                i, want, (i - want) & 0xFFFFFFu);
                        }
                        found++;
                    }
                }
                diag_log("BKA-MTX-SCAN-END",
                    "found=%d w1=0x%08X want=0x%06X",
                    found, cmd.w1, want);
            }
        }
    }
    /* DIAG: test whether segment bases have full-64-bit pointers registered,
       so lookup_addr_by_low32(segmentBase[seg]) + off yields the real matrix. */
    {
        static int _lk = 0;
        if (_lk++ < 8) {
            uint32_t off   = cmd.w1 & 0xFFFFFFu;
            uint32_t s2b   = (uint32_t)s_rdp.segmentBase[2];
            uint32_t s1b   = (uint32_t)s_rdp.segmentBase[1];
            uint32_t s4b   = (uint32_t)s_rdp.segmentBase[4];
            uint32_t s15b  = (uint32_t)s_rdp.segmentBase[15];
            void *f2  = (void*)bka_lookup_addr_by_low32(s2b);
            void *f1  = (void*)bka_lookup_addr_by_low32(s1b);
            void *f4  = (void*)bka_lookup_addr_by_low32(s4b);
            void *f15 = (void*)bka_lookup_addr_by_low32(s15b);
            void *fw  = (void*)bka_lookup_addr_by_low32(cmd.w1);
            void *f2c = (void*)bka_lookup_addr_by_low32(s2b + off);
            diag_log("BKA-MTX-LK",
                "w1=%08X off=%06X s2=%08X f2=%p s2+off=%08X f2c=%p "
                "s1=%08X f1=%p s4=%08X f4=%p s15=%08X f15=%p fw=%p",
                cmd.w1, off, s2b, f2, s2b + off, f2c,
                s1b, f1, s4b, f4, s15b, f15, fw);
            #define _DB16(lbl, ptr) do { \
                const uint8_t* _b = (const uint8_t*)(ptr); \
                int _r = bka_is_readable((void*)(ptr)); \
                diag_log("BKA-MTX-LK16", lbl " ptr=%p rd=%d " \
                    "b=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", \
                    (void*)(ptr), _r, \
                    _r?_b[0]:0, _r?_b[1]:0, _r?_b[2]:0, _r?_b[3]:0, \
                    _r?_b[4]:0, _r?_b[5]:0, _r?_b[6]:0, _r?_b[7]:0, \
                    _r?_b[8]:0, _r?_b[9]:0, _r?_b[10]:0, _r?_b[11]:0, \
                    _r?_b[12]:0, _r?_b[13]:0, _r?_b[14]:0, _r?_b[15]:0); } while(0)
            if (f2)  _DB16("f2+off",  (uint8_t*)f2  + off);
            if (f1)  _DB16("f1+off",  (uint8_t*)f1  + off);
            if (f4)  _DB16("f4+off",  (uint8_t*)f4  + off);
            if (f15) _DB16("f15+off", (uint8_t*)f15 + off);
            if (f2c) _DB16("f2c",     (uint8_t*)f2c);
            #undef _DB16
        }
    }
    /* MULTI-CANDIDATE probe: dump bytes at every plausible matrix address.
       Whichever starts with 00 01 00 00 / 00 00 00 00 / FF FF is the real
       source; that identifier becomes the segment-ID fix. */
    {
        static int _cand = 0;
        if (cmd.w1 != 0xFFFFFFFFu && cmd.w1 != 0xFFFF153Fu && _cand++ < 4) {
            uint32_t off = cmd.w1 & 0xFFFFFFu;
            uintptr_t s1 = s_rdp.segmentBase[1];
            uintptr_t s2 = s_rdp.segmentBase[2];
            uintptr_t s4 = s_rdp.segmentBase[4];
            uintptr_t c_s1 = s1 + off;
            uintptr_t c_s2 = s2 + off;
            uintptr_t c_s4 = s4 + off;
            uintptr_t c_7D = 0x7D00000000ULL | (uint64_t)cmd.w1;
            uintptr_t c_79 = 0x7900000000ULL | (uint64_t)cmd.w1;
            #define _DUMP(lbl, ptr) do {                 const uint8_t* _b = (const uint8_t*)(ptr);                 diag_log("BKA-MTX-CAND", lbl " %p rd=%d b=%02X%02X%02X%02X "                     "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",                     (void*)(ptr), bka_is_readable((void*)(ptr)),                     bka_is_readable((void*)(ptr))?_b[0]:0,                     bka_is_readable((void*)(ptr))?_b[1]:0,                     bka_is_readable((void*)(ptr))?_b[2]:0,                     bka_is_readable((void*)(ptr))?_b[3]:0,                     bka_is_readable((void*)(ptr))?_b[4]:0,                     bka_is_readable((void*)(ptr))?_b[5]:0,                     bka_is_readable((void*)(ptr))?_b[6]:0,                     bka_is_readable((void*)(ptr))?_b[7]:0,                     bka_is_readable((void*)(ptr))?_b[8]:0,                     bka_is_readable((void*)(ptr))?_b[9]:0,                     bka_is_readable((void*)(ptr))?_b[10]:0,                     bka_is_readable((void*)(ptr))?_b[11]:0,                     bka_is_readable((void*)(ptr))?_b[12]:0,                     bka_is_readable((void*)(ptr))?_b[13]:0,                     bka_is_readable((void*)(ptr))?_b[14]:0,                     bka_is_readable((void*)(ptr))?_b[15]:0); } while(0)
            diag_log("BKA-MTX-CAND-HDR",
                "w1=0x%08X off=0x%06X s1=0x%lX s2=0x%lX s4=0x%lX",
                cmd.w1, off,
                (unsigned long)s1, (unsigned long)s2, (unsigned long)s4);
            _DUMP("cand_s1", c_s1);
            _DUMP("cand_s2", c_s2);
            _DUMP("cand_s4", c_s4);
            _DUMP("cand_7D", c_7D);
            _DUMP("cand_79", c_79);
            #undef _DUMP
        }
    }
    if (mtx_src && (flag & G_MTX_PROJECTION) && (flag & G_MTX_LOAD)) {
        static int s_projsrc = 0;
        if (s_projsrc++ < 8) {
            const uint8_t* q = (const uint8_t*)mtx_src;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-PROJSRC",
                "addr=0x%08X ptr=%p bytes=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                cmd.w1, mtx_src,
                q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],
                q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15]);
        }
    }
    if (!mtx_src) {
        /* Fallback: many MTX w1 words land as LE reads of BE-encoded addresses.
           Try byte-swapped and re-resolve. */
        uint32_t swapped = __builtin_bswap32(cmd.w1);
        mtx_src = RDP_TranslateAddr(swapped);
        if (mtx_src) {
            diag_log("BKA-MTX-BSWAP-OK",
                "orig=0x%08X swap=0x%08X ptr=%p flag=%02X",
                cmd.w1, swapped, mtx_src, flag);
        }
    }
    if (!mtx_src) {
        static int null_log = 0;
        if (null_log++ < 32) {
            diag_log("BKA-MTX-NULL",
                "w0=0x%08X w1=0x%08X flag=%02X", cmd.w0, cmd.w1, flag);
        }
        return;
    }
    {
        static int _mxb = 0;
        if (_mxb++ < 8) {
            const uint8_t* _b = (const uint8_t*)mtx_src;
            diag_log("BKA-MTX-BYTES",
                "ptr=%p flag=%02X int0_15=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X "
                "frac0_15=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                mtx_src, flag,
                _b[0],_b[1],_b[2],_b[3],_b[4],_b[5],_b[6],_b[7],
                _b[8],_b[9],_b[10],_b[11],_b[12],_b[13],_b[14],_b[15],
                _b[32],_b[33],_b[34],_b[35],_b[36],_b[37],_b[38],_b[39],
                _b[40],_b[41],_b[42],_b[43],_b[44],_b[45],_b[46],_b[47]);
        }
    }
    /* RDP_TranslateAddr can return a pointer whose byte 0 is mapped but whose
     * surrounding allocation ends before byte 63 (e.g. seg-1 base + a large
     * low-24 offset that runs past the source model's buffer).  Validate the
     * full 64-byte matrix before any dereference. */
    if (!bka_is_readable((void*)((const uint8_t*)mtx_src + 63))) {
        static int s_bad = 0;
        if (s_bad++ < 10) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "Cmd_Mtx: src %p (raw=0x%08X flag=0x%02X) not fully readable — skipping",
                mtx_src, cmd.w1, flag);
        }
        return;
    }
    /* B-K recomp emits G_MTX with targets that resolve to empty RDRAM
     * locations (seg-0/seg-F addresses pointing at never-written
     * buffers).  Loading those clobbers a previously-good projection
     * with a zero matrix — the frame renders 100% black.
     * Detect all-zero source and skip the load; keep the prior matrix. */
    {
        const uint32_t* m32 = (const uint32_t*)mtx_src;
        int all_zero = 1;
        for (int i = 0; i < 16; i++) { if (m32[i] != 0) { all_zero = 0; break; } }
        if (all_zero) {
            static int s_zero = 0;
            if (s_zero++ < 10)
                __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                    "Cmd_Mtx: zero source at raw=0x%08X flag=0x%02X — keeping previous",
                    cmd.w1, flag);
            return;
        }
    }

        /* DIAG: track write changes per matrix address */
    {
        uint32_t _sw_off = __builtin_bswap32(cmd.w1);
        uint32_t off = _sw_off & 0xFFFFFFu;
        uint32_t fp = 0;
        const uint8_t *_b = (const uint8_t*)mtx_src;
        for (int i = 0; i < 64; i++) fp = (fp * 31) + _b[i];
        static struct { uint32_t off; uint32_t fp; } s_cache[16];
        static int s_cache_sz = 0;
        static int s_log_n = 0;
        int idx = -1;
        for (int i = 0; i < s_cache_sz; i++) if (s_cache[i].off == off) { idx = i; break; }
        if (idx < 0 && s_cache_sz < 16) { idx = s_cache_sz++; s_cache[idx].off = off; s_cache[idx].fp = 0; }
        if (idx >= 0 && s_cache[idx].fp != fp && s_log_n < 40) {
            diag_log("BKA-MTX-FP",
                "off=0x%06X fp=0x%08X prev=0x%08X b=%02X%02X%02X%02X%02X%02X%02X%02X",
                off, fp, s_cache[idx].fp,
                _b[0],_b[1],_b[2],_b[3],_b[4],_b[5],_b[6],_b[7]);
            s_cache[idx].fp = fp;
            s_log_n++;
        }
    }

// DIAGNOSTIC: always dump full 64 bytes for PROJECTION matrices
    if ((flag & 0x04) != 0) {
        static int s_proj_dump = 0;
        if (s_proj_dump++ < 3) {
            const uint8_t* mb = (const uint8_t*)mtx_src;
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                 "PROJBYTES raw=0x%08X src=%p: "
                "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X "
                "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X "
                "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X "
                "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                cmd.w1, mtx_src,
                mb[0],mb[1],mb[2],mb[3],mb[4],mb[5],mb[6],mb[7],
                mb[8],mb[9],mb[10],mb[11],mb[12],mb[13],mb[14],mb[15],
                mb[16],mb[17],mb[18],mb[19],mb[20],mb[21],mb[22],mb[23],
                mb[24],mb[25],mb[26],mb[27],mb[28],mb[29],mb[30],mb[31],
                mb[32],mb[33],mb[34],mb[35],mb[36],mb[37],mb[38],mb[39],
                mb[40],mb[41],mb[42],mb[43],mb[44],mb[45],mb[46],mb[47],
                mb[48],mb[49],mb[50],mb[51],mb[52],mb[53],mb[54],mb[55],
                mb[56],mb[57],mb[58],mb[59],mb[60],mb[61],mb[62],mb[63]);
        }
    }

    // DIAGNOSTIC: dump segment table and the raw bytes at src
    static int mtx_diag = 0;
    if (mtx_diag++ < 8) {
        uint32_t seg = (cmd.w1 >> 24) & 0x0F;
        uint32_t off = cmd.w1 & 0x00FFFFFF;
        const uint8_t* mb = (const uint8_t*)mtx_src;
        LOGV(
"MTXDIAG w1=0x%08X seg=%u off=0x%06X segBase[%u]=0x%lX src=%p",
            cmd.w1, seg, off, seg,
            (unsigned long)s_rdp.segmentBase[seg], mtx_src);
        LOGV(
             "MTXBYTES @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X  "
            "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            mtx_src,
            mb[0],mb[1],mb[2],mb[3],mb[4],mb[5],mb[6],mb[7],
            mb[8],mb[9],mb[10],mb[11],mb[12],mb[13],mb[14],mb[15],
            mb[16],mb[17],mb[18],mb[19],mb[20],mb[21],mb[22],mb[23],
            mb[24],mb[25],mb[26],mb[27],mb[28],mb[29],mb[30],mb[31]);
    }

    if (s_mtx_dump_frame++ < 2) {
        const uint8_t* mb = (const uint8_t*)mtx_src;
        LOGV(
             "MTXSRC @%p: "
            "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X  "
            "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            mtx_src,
            mb[0],mb[1],mb[2],mb[3],mb[4],mb[5],mb[6],mb[7],
            mb[8],mb[9],mb[10],mb[11],mb[12],mb[13],mb[14],mb[15],
            mb[16],mb[17],mb[18],mb[19],mb[20],mb[21],mb[22],mb[23],
            mb[24],mb[25],mb[26],mb[27],mb[28],mb[29],mb[30],mb[31]);
    }

    static int s_mtx_dual = 0;
    if (s_mtx_dual++ < 8) {
        const uint8_t* p8 = (const uint8_t*)s_current_cmd;
        if (p8) {
            uint32_t w0_le = *(const uint32_t*)(p8 + 0);
            uint32_t w1_le = *(const uint32_t*)(p8 + 4);
            uint32_t w0_be = ((uint32_t)p8[8] << 24) | ((uint32_t)p8[9] << 16) |
                             ((uint32_t)p8[10] << 8) | (uint32_t)p8[11];
            uint32_t w1_be = ((uint32_t)p8[12] << 24) | ((uint32_t)p8[13] << 16) |
                             ((uint32_t)p8[14] << 8) | (uint32_t)p8[15];
            LOGV(
"MTXDUAL LE: w0=0x%08X w1=0x%08X | BE: w0=0x%08X w1=0x%08X",
                w0_le, w1_le, w0_be, w1_be);
        }
    }

    BKMatrix newMatrix;
    Matrix_LoadFromN64(&newMatrix, mtx_src);
//     { static int s_w0 = 0; if (s_w0++ < 12) { const uint8_t* raw = (const uint8_t*)s_current_cmd; LOGV("MTXW0 cur=%p raw=%02X%02X%02X%02X %02X%02X%02X%02X decoded_w0=0x%08X flag=0x%02X", (void*)raw, raw?raw[0]:0,raw?raw[1]:0,raw?raw[2]:0,raw?raw[3]:0,raw?raw[4]:0,raw?raw[5]:0,raw?raw[6]:0,raw?raw[7]:0, cmd.w0, flag); } }

    if (s_mtx_log_frame++ < 0) {
        __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
            "Cmd_Mtx flag=0x%02X raw=0x%08X src=%p diag=[%.4f %.4f %.4f %.4f]",
            flag, cmd.w1, mtx_src,
            newMatrix[0][0], newMatrix[1][1],
            newMatrix[2][2], newMatrix[3][3]);
    }

    { static int s_act = 0; if (s_act++ < 5000) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
        "MTXACT flag=0x%02X src=0x%08X new[0][0]=%.4f new[3][2]=%.4f | PROJ=%d LOAD=%d PUSH=%d",
        flag, cmd.w1, newMatrix[0][0], newMatrix[3][2],
        (flag & G_MTX_PROJECTION)?1:0, (flag & G_MTX_LOAD)?1:0, (flag & G_MTX_PUSH)?1:0); }
    /* BKA-MTXRAW: dump raw 16 bytes at the command entry for PROJ+LOAD, first 30 hits.
     * If these are real G_MTX commands, we will see 01 <flag> 00 <idx> | <addr BE>.
     * If we see matrix data or DL trailer bytes, the walker has drifted. */
    if ((flag & G_MTX_PROJECTION) && (flag & G_MTX_LOAD)) {
        static int s_mraw = 0;
        if (s_mraw++ < 30) {
            const uint8_t* p8 = (const uint8_t*)s_current_cmd;
            if (p8) __android_log_print(ANDROID_LOG_ERROR, "BKA-MTXRAW",
                "cur=%p w0=%08X w1=%08X raw=%02X%02X%02X%02X %02X%02X%02X%02X | tag=%02X%02X%02X%02X %02X%02X%02X%02X",
                (void*)p8, cmd.w0, cmd.w1,
                p8[0],p8[1],p8[2],p8[3],p8[4],p8[5],p8[6],p8[7],
                p8[8],p8[9],p8[10],p8[11],p8[12],p8[13],p8[14],p8[15]);
        }
    }

    if (flag & G_MTX_PROJECTION) {
        // Projection matrix slot
        if (flag & G_MTX_LOAD) {
            memcpy(s_rdp.projection, newMatrix, sizeof(BKMatrix));
            { static int s_d = 0; if (s_d++ < 3) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "PROJ-AFTER-LOAD r0=(%.4f %.4f %.4f %.4f) r3=(%.4f %.4f %.4f %.4f)",
                s_rdp.projection[0][0], s_rdp.projection[0][1], s_rdp.projection[0][2], s_rdp.projection[0][3],
                s_rdp.projection[3][0], s_rdp.projection[3][1], s_rdp.projection[3][2], s_rdp.projection[3][3]); }
        } else {
            BKMatrix tmp;
            Matrix_Multiply(tmp, newMatrix, s_rdp.projection); // new × existing
            memcpy(s_rdp.projection, tmp, sizeof(BKMatrix));
        }
    } else {
        // Modelview matrix slot
        if (flag & G_MTX_PUSH) {
            // RT64: PUSH saves current top, doesn't itself multiply.
            if (s_rdp.modelviewStackDepth < 16) {
                memcpy(s_rdp.modelviewStack[s_rdp.modelviewStackDepth],
                       s_rdp.modelview, sizeof(BKMatrix));
                s_rdp.modelviewStackDepth++;
            }
        }
        if (flag & G_MTX_LOAD) {
            memcpy(s_rdp.modelview, newMatrix, sizeof(BKMatrix));
            { static int s_mvl = 0; if (s_mvl++ < 20)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-MVLOAD",
                    "MV-LOAD r0=(%.3f %.3f %.3f %.3f) r1=(%.3f %.3f %.3f %.3f) r2=(%.3f %.3f %.3f %.3f) r3=(%.1f %.1f %.1f %.1f)",
                    s_rdp.modelview[0][0], s_rdp.modelview[0][1], s_rdp.modelview[0][2], s_rdp.modelview[0][3],
                    s_rdp.modelview[1][0], s_rdp.modelview[1][1], s_rdp.modelview[1][2], s_rdp.modelview[1][3],
                    s_rdp.modelview[2][0], s_rdp.modelview[2][1], s_rdp.modelview[2][2], s_rdp.modelview[2][3],
                    s_rdp.modelview[3][0], s_rdp.modelview[3][1], s_rdp.modelview[3][2], s_rdp.modelview[3][3]); }
        } else {
            /* DKR sends gSPMatrixDKR as flag=0x04 5000x/frame — these are
             * not F3DEX MULs into a stack; the real matrix loads come via
             * MoveWord 0x00. Skip MUL to prevent modelview compounding to
             * w=2548 garbage. */
            static int _mm = 0; if (_mm++ < 3)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-MTMULSKIP",
                    "MODELVIEW|MUL skipped (DKR uses MoveWord 0x00)");
        }
    }

    // RT64 recomputes viewProj after every matrix op.
    { static int s_d2 = 0; if (s_d2++ < 3) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
        "PROJ-BEFORE-VP r0=(%.4f %.4f %.4f %.4f) r3=(%.4f %.4f %.4f %.4f) flag=0x%02X",
        s_rdp.projection[0][0], s_rdp.projection[0][1], s_rdp.projection[0][2], s_rdp.projection[0][3],
        s_rdp.projection[3][0], s_rdp.projection[3][1], s_rdp.projection[3][2], s_rdp.projection[3][3], flag); }
    Matrix_Multiply(s_rdp.viewProj, s_rdp.modelview, s_rdp.projection);
}

// =======================================================================
// G_FILLRECT - Solid color rectangle fill
// =======================================================================
static void Cmd_FillRect(GfxCommand cmd) {
    {
        static int s_fr = 0;
        if (s_fr++ < 40) {
            uint32_t x0 = (cmd.w0 >> 12) & 0xFFF;
            uint32_t y0 = cmd.w0 & 0xFFF;
            uint32_t x1 = (cmd.w1 >> 12) & 0xFFF;
            uint32_t y1 = cmd.w1 & 0xFFF;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-FILLRECT",
                "FILLRECT x0=%u y0=%u x1=%u y1=%u w=%u h=%u",
                x0, y0, x1, y1,
                (x1 > x0) ? (x1 - x0 + 1) : 0,
                (y1 > y0) ? (y1 - y0 + 1) : 0);
        }
    }
    { static int s_fr = 0; if (s_fr++ < 10 || (s_fr % 200) == 0)
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "FILLRECT-ENTRY #%d w0=0x%08X w1=0x%08X", s_fr, cmd.w0, cmd.w1); }
    int32_t ulx = (int32_t)((cmd.w1 >> 14) & 0x3FF);
    int32_t uly = (int32_t)((cmd.w1 >> 2) & 0x3FF);
    int32_t lrx = (int32_t)((cmd.w0 >> 14) & 0x3FF);
    int32_t lry = (int32_t)((cmd.w0 >> 2) & 0x3FF);
    { static int _fr = 0; if (_fr++ < 8) __android_log_print(ANDROID_LOG_ERROR, "BKA-FILLREAL",
        "w0=%08X w1=%08X ulx=%d uly=%d lrx=%d lry=%d",
        cmd.w0, cmd.w1, (int)ulx, (int)uly, (int)lrx, (int)lry); }
    
    // Recompiled BK emits coordinates in pixels, not N64 .2 fixed point.
    ulx = std::max(0, ulx); uly = std::max(0, uly);
    lrx = std::min(lrx, FB_WIDTH); lry = std::min(lry, FB_HEIGHT);
    if (ulx >= lrx || uly >= lry) return;

    /* Fix BB: skip full-screen clears that appear AFTER geometry in the
     * same task.  B-K's recomp emits the frame's clear at the end of the
     * DL rather than the start, so we were wiping the frame we'd just
     * drawn.  Clear-before-geometry still works (frame init). */
    {
        /* BKA-FULLSCREEN-FIX: 10.2 fixed point pixel indices are 0..319, 0..239.
         * A fullscreen fill has lrx=319, lry=239. Original check `lrx >= FB_WIDTH`
         * was `319 >= 320` = FALSE, missing every fullscreen fill and letting it
         * wipe RAST pixels. Off-by-one fix with -1 on both sides. */
        bool fullscreen = (ulx <= 0 && uly <= 0 && lrx >= FB_WIDTH-1 && lry >= FB_HEIGHT-1);
        /* Fix BB: reference the file-scope static directly.  The previous
         * patch shadowed it with an `extern` that bound to zero at link
         * time, so the check never fired and never skipped anything. */
        if (fullscreen && (g_bka_fb_dirty || g_bka_task_tri_count > 0)) {
            static int s_bb_skip = 0;
            if (s_bb_skip++ < 100)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-RAST",
                    "FILL-SKIP full-screen clear after %d tris this task",
                    g_bka_task_tri_count);
            return;
        }
    }
    
    { static int s_fr = 0; if (s_fr++ < 100) __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX", "FILLPRE fillRGB=(%d,%d,%d) ulx=%d uly=%d lrx=%d lry=%d w0=%08X w1=%08X", s_rdp.fillR, s_rdp.fillG, s_rdp.fillB, (int)ulx, (int)uly, (int)lrx, (int)lry, cmd.w0, cmd.w1);
    { static int _frc = 0; if (_frc++ < 12) __android_log_print(ANDROID_LOG_ERROR, "BKA-FILLRGB",
        "rgb=(%d,%d,%d) fillcolor_reg=%d",
        s_rdp.fillR, s_rdp.fillG, s_rdp.fillB,
        (int)s_rdp.fillR * 65536 + (int)s_rdp.fillG * 256 + (int)s_rdp.fillB); } }
    uint16_t color = RGBA8_TO_RGB565(s_rdp.fillR, s_rdp.fillG, s_rdp.fillB);
    int activeFb = getActiveFramebuffer();
    /* PORT: force walker to write to gVideoCurrFramebuffer (what fb_swap
       will promote to gVideoLastFramebuffer, which fillArgb reads).
       g_active_fb_offset is stale; doesn't track fb_swap alternation. */
    extern uint8_t *gVideoCurrFramebuffer;
    uint16_t* fb = gVideoCurrFramebuffer
                   ? (uint16_t*)gVideoCurrFramebuffer
                   : (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
    { static int _bfc = 0; if (_bfc++ < 3)
          __android_log_print(ANDROID_LOG_ERROR, "BKA-FBFORCE",
              "walker fb=%p (curr=%p)", (void*)fb, (void*)gVideoCurrFramebuffer); }
    { static int s_wr = 0; if (s_wr++ < 3)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-WRITE",
            "FILL-WRITE rdr=%p ofs=0x%X fb=%p", (void*)gN64_RDRAM, g_active_fb_offset, (void*)fb); }
    {
        static int s_dbg = 0;
        if (s_dbg++ < 5 || (s_dbg % 100) == 0)
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "FILLRECT-DBG ulx=%d uly=%d lrx=%d lry=%d "
                "fillRGB=(%d,%d,%d) color=0x%04X fb=%p",
                ulx, uly, lrx, lry,
                s_rdp.fillR, s_rdp.fillG, s_rdp.fillB, color, (void*)fb);
    }
    if (color != 0) { static int s_fc = 0; if (s_fc++ < 30)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-FILLCOL",
            "color=0x%04X rgb=(%d,%d,%d) rect=%d,%d-%d,%d",
            color, s_rdp.fillR, s_rdp.fillG, s_rdp.fillB, ulx, uly, lrx, lry); }
    for (int32_t y = uly; y < lry; y++)
        for (int32_t x = ulx; x < lrx; x++)
            *(volatile uint16_t *)&fb[y * FB_WIDTH + x] = color;
}

// =======================================================================
// G_TEXRECT - Textured rectangle
// =======================================================================
static void Cmd_TexRect(GfxCommand cmd) {
    { static int s_tr = 0; if (s_tr++ < 10 || (s_tr % 200) == 0)
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "TEXRECT-ENTRY #%d w0=0x%08X w1=0x%08X", s_tr, cmd.w0, cmd.w1); }
    int32_t xh = (int32_t)((cmd.w0 >> 12) & 0xFFF) >> 2;
    int32_t yh = (int32_t)(cmd.w0 & 0xFFF) >> 2;
    int32_t xl = (int32_t)((cmd.w1 >> 12) & 0xFFF) >> 2;
    int32_t yl = (int32_t)(cmd.w1 & 0xFFF) >> 2;
    int tile = s_rdp.activeTile; /* PORT: use the active tile set by G_SETTILE */
    
    // Coordinates are .2 fixed-point; >>2 converts to pixels.
    if (xl < 0) xl = 0; if (yl < 0) yl = 0;
    if (xh > FB_WIDTH) xh = FB_WIDTH; if (yh > FB_HEIGHT) yh = FB_HEIGHT;
    if (xl >= xh || yl >= yh) return;
    if (tile < 0 || tile >= 8) return;
    
    auto& tdesc = s_rdp.tiles[tile];
    


    int32_t texW = (tdesc.sh >> 2) + 1, texH = (tdesc.th >> 2) + 1;
    if (texW <= 0) texW = 1; if (texH <= 0) texH = 1;
    
    int32_t rectW = xh - xl, rectH = yh - yl;
    if (rectW <= 0 || rectH <= 0) return;
    
    /* PORT: DKR passes dsdx=0 in these commands. We stretch the texture
       from 0..texW and 0..texH over the rectangle to get full coverage. */
    int32_t sBase = 0, tBase = 0;
    int32_t dsdx = 0, dtdy = 0;
    int activeFb = getActiveFramebuffer();
    /* PORT: force walker to write to gVideoCurrFramebuffer (what fb_swap
       will promote to gVideoLastFramebuffer, which fillArgb reads).
       g_active_fb_offset is stale; doesn't track fb_swap alternation. */
    extern uint8_t *gVideoCurrFramebuffer;
    uint16_t* fb = gVideoCurrFramebuffer
                   ? (uint16_t*)gVideoCurrFramebuffer
                   : (uint16_t*)(gN64_RDRAM + g_active_fb_offset);
    { static int _bfc = 0; if (_bfc++ < 3)
          __android_log_print(ANDROID_LOG_ERROR, "BKA-FBFORCE",
              "walker fb=%p (curr=%p)", (void*)fb, (void*)gVideoCurrFramebuffer); }
    uint8_t texel[4];

    /* BKA-TEXUV: log raw cmd2 values + interp state */
    { static int _tuv = 0; if (_tuv++ < 8) {
        auto& td = s_rdp.tiles[tile];
        __android_log_print(ANDROID_LOG_ERROR, "BKA-TEXUV",
            "tile=%d rectW=%d rectH=%d sBase=%d tBase=%d dsdx=%d dtdy=%d | sh=%u th=%u tmemAddr=%u line=%u fmt=%u sz=%u",
            tile, rectW, rectH, sBase, tBase, dsdx, dtdy,
            td.sh, td.th, td.tmemAddr, td.line, td.format, td.size);
    } }
    
    for (int32_t dy = 0; dy < rectH; dy++) {
        for (int32_t dx = 0; dx < rectW; dx++) {
            int32_t s = sBase + (dx * texW) / rectW;
                int32_t t = tBase + (dy * texH) / rectH;
                RDP_FetchTexel(tile, s << 5, t << 5, texel);
            
            uint8_t r = (uint8_t)(((uint16_t)texel[0] * s_rdp.primR) / 255);
            uint8_t g = (uint8_t)(((uint16_t)texel[1] * s_rdp.primG) / 255);
            uint8_t b = (uint8_t)(((uint16_t)texel[2] * s_rdp.primB) / 255);
            uint8_t a = (uint8_t)(((uint16_t)texel[3] * s_rdp.primA) / 255);
            /* PORT: Directly sample texel. No fallback. */
            
            int32_t px = xl + dx, py = yl + dy;
            if (px >= 0 && px < FB_WIDTH && py >= 0 && py < FB_HEIGHT)
                fb[py * FB_WIDTH + px] = RGBA8_TO_RGB565(r, g, b);
        }
    }
}

// =======================================================================
// G_DL - Jump to display list
// =======================================================================
static void Cmd_DL(GfxCommand cmd, GfxCommand** outCmd, size_t* outRemaining) {
    uint32_t addr = cmd.w1;
    *outCmd = (GfxCommand*)RDP_TranslateAddr(addr);
    *outRemaining = 0xFFFFFFFF;
}

static void* RSP_ResolveGfxAddress(uint32_t addr) {
    uint32_t seg = (addr >> 28) & 0x0F;
    uint32_t offset = addr & 0x0FFFFFFF;
    if (seg != 0 && seg < 16 && s_rdp.segmentBase[seg] != 0) {
        uint32_t base = s_rdp.segmentBase[seg];
        /* PORT FIX: segmentBase holds an N64 physical/virtual base, not a
           host pointer. Force KSEG0 (0x80000000) then translate to host. */
        if (base < 0x80000000) base |= 0x80000000;
        return RDP_TranslateAddr(base + offset);
    }
    return RDP_TranslateAddr(addr);
}

// =======================================================================
// Main Dispatch
// =======================================================================

// Probe a sub-DL's encoding by reading its first N commands under both
// byte orders and counting which interpretation yields more valid F3DEX
// opcodes.  Returns 0=unknown, 1=LE, 2=BE.
static int bka_probe_dl_encoding(uint8_t* ptr) {
    if (!ptr) return 0;
    int le = 0, be = 0, le_bad = 0, be_bad = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t* p = ptr + i * 16;
        if (!bka_is_readable(p + 8)) break;
        uint32_t w_le = *(uint32_t*)p;
        uint32_t w_be = __builtin_bswap32(w_le);
        uint8_t op_le = (uint8_t)(w_le >> 24);
        uint8_t op_be = (uint8_t)(w_be >> 24);
        if (op_le != 0x00) { if (bka_is_f3dex_opcode(op_le)) le++; else le_bad++; }
        if (op_be != 0x00) { if (bka_is_f3dex_opcode(op_be)) be++; else be_bad++; }
        if (i >= 7 && be_bad == 0 && be >= 8) return 2;
        if (i >= 7 && le_bad == 0 && le >= 8) return 1;
    }
    int _res = 0;
    if (be_bad == 0 && le_bad > 0) _res = 2;
    else if (le_bad == 0 && be_bad > 0) _res = 1;
    else if (be > le + 1) _res = 2;
    else if (le > be + 1) _res = 1;
    { static int _pc = 0; if (_pc++ < 60) __android_log_print(ANDROID_LOG_ERROR,
        "BKA_GFX", "PROBE-VOTE ptr=%p le=%d le_bad=%d be=%d be_bad=%d -> %d",
        (void*)ptr, le, le_bad, be, be_bad, _res); }
    /* Structural check: a real DL contains G_ENDDL within the first ~256 slots.
     * Walk at 8-byte stride: B-K sub-DLs are copied verbatim from the ROM
     * asset cache and use N64 8-byte Gfx encoding (handoff §7).  Sampling at
     * 16-byte stride missed real terminators and vetoed every sub-DL.
     * A decisive vote (large margin) overrides the veto regardless. */
    if (_res != 0) {
        int enddl_found = 0;
        for (int i = 0; i < 256; i++) {
            uint8_t* p = ptr + i * 8;
            if (!bka_is_readable(p + 8)) break;
            uint32_t w = *(uint32_t*)p;
            if (_res == 2) w = __builtin_bswap32(w);
            /* ENDDL is 0xB8 in F3DEX v1, 0xDF in F3DEX2. Accept either. */
            uint8_t op = (uint8_t)(w >> 24);
            if (op == 0xB8 || op == 0xDF) { enddl_found = 1; break; }
        }
        if (!enddl_found) {
            int score = (_res == 2) ? (be - be_bad) - (le - le_bad)
                                    : (le - le_bad) - (be - be_bad);
            static int s_noend = 0;
            if (s_noend++ < 30)
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "PROBE-ENDDL ptr=%p vote=%d score=%d no G_ENDDL -> %s",
                    (void*)ptr, _res, score, (score >= 10) ? "keep" : "0");
            if (score < 10) _res = 0;
        }
    }
    return _res;
}

static int s_rspCallCount = 0;
static uint32_t s_opcount[256] = {0};
static uint32_t s_op_total = 0;


/* Fix L: robust stride detector.  Samples 3 consecutive 16-byte slots;
 * if ALL three have zero bytes 8-15, it's a padded (16-byte) DL.
 * Otherwise unpadded (8-byte).  The prior single-slot check false-
 * positived when two adjacent commands both had w1 == 0. */
/* Fix M: does the target actually look like a DL, or is it data?
 * Sample 8 slots, count how many have a whitelisted opcode byte at
 * position [0] (BE) or [3] (LE).  Real DLs score 7-8; data tables 3-4. */
static int bka_looks_like_dl(const uint8_t* p, int stride) {
    int valid = 0;
    for (int k = 0; k < 8; k++) {
        const uint8_t* s = p + (size_t)k * stride;
        if (bka_is_f3dex_opcode(s[0]) || bka_is_f3dex_opcode(s[3])) valid++;
    }
    return valid;
}

static inline int bka_detect_stride(const uint8_t* p) {
    /* Fix W: sample 8 slots at each candidate stride, count how many
     * contain a valid F3DEX2 opcode in either encoding.  Pick the
     * stride with more hits.  The prior zero-padding heuristic fails
     * on B-K's top-level DLs: bytes 8-15 of each 16-byte entry hold a
     * non-zero pointer + type field, not zero padding.  That made the
     * detector return 8 for the top level, so half the "commands"
     * walked were metadata words like 005BE460 -> op=0x60 (garbage). */
    int valid8 = 0, valid16 = 0;
    for (int k = 0; k < 8; k++) {
        uint32_t w = *(const uint32_t*)(p + k*8);
        uint8_t le_op = (uint8_t)(w >> 24);
        uint8_t be_op = (uint8_t)(w & 0xFF);
        if (bka_is_f3dex_opcode(le_op) || bka_is_f3dex_opcode(be_op)) valid8++;
    }
    for (int k = 0; k < 8; k++) {
        uint32_t w = *(const uint32_t*)(p + k*16);
        uint8_t le_op = (uint8_t)(w >> 24);
        uint8_t be_op = (uint8_t)(w & 0xFF);
        if (bka_is_f3dex_opcode(le_op) || bka_is_f3dex_opcode(be_op)) valid16++;
    }
    static int s_wlog = 0;
    if (s_wlog++ < 20) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-STRIDE",
            "DETECT ptr=%p valid8=%d/8 valid16=%d/8 -> stride=%d",
            (void*)p, valid8, valid16, (valid16 >= valid8) ? 16 : 8);
    }
    return (valid16 >= valid8) ? 16 : 8;
}

/* PORT: walker SIGSEGV recovery. Only recover faults on the walker's own
   thread (gettid check). Faults on HWUI/art/other threads MUST propagate
   normally — cross-thread siglongjmp is UB and corrupts HWUI state,
   causing EGL_BAD_CONTEXT aborts seconds later. */
static sigjmp_buf g_walker_recover;
static volatile sig_atomic_t g_walker_in_fault = 0;
static volatile sig_atomic_t g_recover_count = 0;
static int g_walker_recover_installed = 0;
static pid_t g_walker_tid = 0;

/* PORT: per-opcode histogram for diagnosing dispatch gaps */
static uint32_t g_opcode_hist[256] = {0};
extern "C" uint32_t gfx_opcode_hist_get(int i) { return (i >= 0 && i < 256) ? g_opcode_hist[i] : 0u; }
extern "C" void gfx_opcode_hist_reset(void) { for (int i = 0; i < 256; i++) g_opcode_hist[i] = 0; }

extern "C" uint8_t* gfx_tmem_ptr() { return s_rdp.tmem; }
extern "C" int gfx_tmem_size() { return 4096; }

static void walker_recover_handler(int sig, siginfo_t *info, void *ctx) {
    if (gettid() != g_walker_tid || g_walker_in_fault || g_recover_count > 5000) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    g_recover_count++;
    siglongjmp(g_walker_recover, 1);
}

static void walker_recover_install(void) {
    struct sigaction sa;
    sa.sa_sigaction = walker_recover_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

/* Count non-zero pixels in current framebuffer. This is our definitive
 * answer for "is the screen showing anything?" — no more asking the user. */
static void bka_scan_fb(const char* where) {
    __android_log_print(ANDROID_LOG_ERROR, "BKA-SCANV2", "ENTER where=%s", where);
    extern uint8_t *gN64_RDRAM;
    if (!gN64_RDRAM) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-SCAN", "%s RDRAM_NULL", where);
        return;
    }
    { extern uint8_t *gVideoCurrFramebuffer;
      __android_log_print(ANDROID_LOG_ERROR, "BKA-SCANV2",
        "currFb=%p RDRAM=%p", (void*)gVideoCurrFramebuffer, (void*)gN64_RDRAM); }
    for (int i = 0; i < 2; i++) {
        uint32_t off = 0x400000 + i * 0x40000;
        uint16_t* fb = (uint16_t*)(gN64_RDRAM + off);
        int nz = 0, bright = 0, firstnz = -1;
        uint16_t maxc = 0;
        for (int k = 0; k < 320*240; k++) {
            uint16_t c = fb[k];
            if (c) {
                nz++;
                if (firstnz < 0) firstnz = k;
                int sm = ((c >> 11) & 0x1F) + ((c >> 5) & 0x3F) + (c & 0x1F);
                if (sm > 10) bright++;
                if (c > maxc) maxc = c;
            }
        }
        __android_log_print(ANDROID_LOG_ERROR, "BKA-SCAN",
            "%s fb%d off=0x%X nz=%d bright=%d maxc=0x%04X firstnz=%d",
            where, i, off, nz, bright, maxc, firstnz);
    }
}

static void bka_dump_fb_ptr(const char* path, uint8_t* fbp) {
    if (!fbp) return;
    FILE* f = fopen(path, "wb");
    if (!f) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-FBDUAL",
            "path=%s fopen FAILED errno=%d", path, errno);
        return;
    }
    fprintf(f, "P6\n320 240\n255\n");
    uint16_t* p = (uint16_t*)fbp;
    for (int i = 0; i < 320*240; i++) {
        uint16_t c = p[i];
        fputc(((c >> 11) & 0x1F) << 3, f);
        fputc(((c >> 5) & 0x3F) << 2, f);
        fputc((c & 0x1F) << 3, f);
    }
    fclose(f);
    __android_log_print(ANDROID_LOG_ERROR, "BKA-FBDUAL",
        "wrote %s fb=%p", path, (void*)fbp);
}

static void bka_dump_fb(const char* path) {
    extern uint8_t *gVideoCurrFramebuffer;
    if (!gVideoCurrFramebuffer) return;
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n320 240\n255\n");
    uint16_t* fb = (uint16_t*)gVideoCurrFramebuffer;
    for (int i = 0; i < 320*240; i++) {
        uint16_t c = fb[i];
        uint8_t r = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 5) & 0x3F) << 2;
        uint8_t b = (c & 0x1F) << 3;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
    static int _d = 0; if (_d++ < 2)
        __android_log_print(ANDROID_LOG_ERROR, "BKA-FBDUMP",
            "wrote %s fb=%p", path, (void*)gVideoCurrFramebuffer);
}

static void bka_count_fb_pixels(const char* where) {
    bka_scan_fb(where);  /* BKA-SCANV2: direct call, no extern */
    extern uint8_t *gVideoCurrFramebuffer;
    if (!gVideoCurrFramebuffer) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-FB", "%s no_fb", where);
        return;
    }
    uint16_t* fb = (uint16_t*)gVideoCurrFramebuffer;
    int nz = 0;
    const int total = 320 * 240;
    for (int i = 0; i < total; i++) if (fb[i]) nz++;
    __android_log_print(ANDROID_LOG_ERROR, "BKA-FB",
        "%s nonzero=%d/%d fb=%p", where, nz, total, (void*)fb);
}

void RSP_ProcessGfxTask(OSTask* tp) {
    
    /* Bounds: walker MUST NOT step past task DL. Adjacent bytes in the
     * same mmap are readable, so bka_is_readable() cannot catch the
     * overrun — the data_ptr+size range is the only hard boundary. */
    const uint8_t *bka_dl_start = tp ? (const uint8_t*)tp->t.data_ptr : NULL;
    const uint8_t *bka_dl_end   = bka_dl_start
                                ? bka_dl_start + (size_t)tp->t.data_size
                                : NULL;

if (!g_walker_recover_installed) {
        g_walker_recover_installed = 1;
        g_walker_tid = gettid();
        walker_recover_install();
    }
    if (sigsetjmp(g_walker_recover, 1) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RECOVER",
            "walker recovered fault #%d (frame skipped)", (int)g_recover_count);
        return;
    }
    g_walker_in_fault = 0;

    { static int s_wr = 0; if (s_wr++ < 10) __android_log_print(ANDROID_LOG_ERROR, "BKA-WALK-ENTRY", "task=%d size=%u", s_wr, tp ? tp->t.data_size : 0); }
    { static int s_once = 0; if (s_once++ == 0) bka_install_watch(); }
    { static int s_ent = 0; if (s_ent++ < 30) {
        uint8_t* dp = tp ? (uint8_t*)tp->t.data_ptr : nullptr;
    {
        static int _dl0 = 0;
        if (_dl0++ < 3 && dp && tp->t.data_size >= 64) {
            const uint8_t *_p = (const uint8_t*)dp;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-DLBYTES0",
                "task#%d size=%u first32=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                _dl0, tp->t.data_size,
                _p[0],_p[1],_p[2],_p[3],_p[4],_p[5],_p[6],_p[7],
                _p[8],_p[9],_p[10],_p[11],_p[12],_p[13],_p[14],_p[15],
                _p[16],_p[17],_p[18],_p[19],_p[20],_p[21],_p[22],_p[23],
                _p[24],_p[25],_p[26],_p[27],_p[28],_p[29],_p[30],_p[31]);
            __android_log_print(ANDROID_LOG_ERROR, "BKA-DLBYTES0",
                "task#%d first64=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                _dl0,
                _p[32],_p[33],_p[34],_p[35],_p[36],_p[37],_p[38],_p[39],
                _p[40],_p[41],_p[42],_p[43],_p[44],_p[45],_p[46],_p[47],
                _p[48],_p[49],_p[50],_p[51],_p[52],_p[53],_p[54],_p[55],
                _p[56],_p[57],_p[58],_p[59],_p[60],_p[61],_p[62],_p[63]);
        }
    }

        __android_log_print(ANDROID_LOG_ERROR, "BKA-DL",
            "ENTER #%d task=%p data_ptr=%p first8=%02X%02X%02X%02X %02X%02X%02X%02X",
            s_ent, (void*)tp, (void*)dp,
            dp?dp[0]:0,dp?dp[1]:0,dp?dp[2]:0,dp?dp[3]:0,
            dp?dp[4]:0,dp?dp[5]:0,dp?dp[6]:0,dp?dp[7]:0); } }
    if (bka_canary_post != 0xBEEFCAFEBABE5678ull) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "CANARY-OVERFLOW post=%016llX (s_rdp overflowed)",
            (unsigned long long)bka_canary_post);
        bka_canary_post = 0xBEEFCAFEBABE5678ull;
    }
    if (s_rdp.dmemVertexCount < 0 || s_rdp.dmemVertexCount > DMEM_VERTEX_COUNT) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "DMEM-COUNT-BAD: %d (should be 0..256)", s_rdp.dmemVertexCount);
        s_rdp.dmemVertexCount = DMEM_VERTEX_COUNT;
    }
    /* Diagnostic: dump FULL top-level task buffer to logcat, once. */
    {
        static int s_fulldump = 0;
        if (s_fulldump++ < 1 && tp && tp->t.data_ptr && tp->t.data_size > 0) {
            const uint8_t* d = (const uint8_t*)tp->t.data_ptr;
            uint32_t sz = tp->t.data_size;
            if (sz > 4096) sz = 4096;
            for (uint32_t off = 0; off < sz; off += 16) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA-FULLTASK",
                    "+%04X: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                    off,
                    d[off],  d[off+1],d[off+2],d[off+3],d[off+4],d[off+5],d[off+6],d[off+7],
                    d[off+8],d[off+9],d[off+10],d[off+11],d[off+12],d[off+13],d[off+14],d[off+15]);
            }
        }
    }
    /* Diagnostic: dump first 128 bytes of the top-level DL, decoded
     * BOTH ways, so we can finally pin the wrapper format. */
    {
        static int s_tl_dump = 0;
        if (s_tl_dump++ < 3 && tp && tp->t.data_ptr) {
            const uint8_t* d = (const uint8_t*)tp->t.data_ptr;
            for (int row = 0; row < 16; row++) {
                const uint8_t* r = d + row * 8;
                uint32_t le = (uint32_t)(r[0] | (r[1]<<8) | (r[2]<<16) | (r[3]<<24));
                uint32_t be = (uint32_t)((r[0]<<24) | (r[1]<<16) | (r[2]<<8) | r[3]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA-TLD",
                    "t#%d +%03X: %02X%02X%02X%02X %02X%02X%02X%02X  LE_op=%02X BE_op=%02X",
                    s_rspCallCount, row * 8,
                    r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],
                    (uint8_t)(le >> 24), (uint8_t)(be >> 24));
            }
        }
    }
    s_rspCallCount++;
    /* Fix U: increment frame gen at task ENTRY, not exit.  The exit
     * increments are after possible early returns (drift, G_ENDDL,
     * post-pop unwind) so gen stayed at 0 and the video upload gate
     * skipped every frame except its 500ms stale fallback. */
    g_bka_frame_gen++;
    g_bka_task_tri_count = 0;
    /* NU6-06-0030-001G p.144 "Matrix State": F3DEX microcode starts every
     * task with identity modelview/projection. DKR emits 5000 MODELVIEW|MUL
     * per frame with no LOAD, relying on task-start reset. Without this,
     * s_rdp.modelview compounds garbage across frames. BKA-DRAW w=(1211.45,
     * 0.04, 1211.45) was the signature of accumulated garbage, not real
     * perspective. */
    for (int _i = 0; _i < 4; _i++) for (int _j = 0; _j < 4; _j++) {
        s_rdp.modelview[_i][_j]  = (_i == _j) ? 1.0f : 0.0f;
        s_rdp.projection[_i][_j] = (_i == _j) ? 1.0f : 0.0f;
        s_rdp.viewProj[_i][_j]   = (_i == _j) ? 1.0f : 0.0f;
    }
    s_rdp.modelviewStackDepth = 0;
    g_bka_task_pops = 0;
    g_bka_task_max_depth = 0;
    /* PORT: reset modelview stack depth each frame. If a previous frame
       leaked a PUSH without POP, this prevents cross-frame accumulation. */
    s_rdp.modelviewStackDepth = 0;

    // Probe known RDRAM offsets where the DL thinks vertex data lives
    static int s_probe = 0;
    if (s_probe++ < 1 && gN64_RDRAM) {
        const uint8_t* rdram = gN64_RDRAM;
        LOGV(
            "RDRAMPROBE @0x79153F: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            rdram[0x79153F],rdram[0x791540],rdram[0x791541],rdram[0x791542],
            rdram[0x791543],rdram[0x791544],rdram[0x791545],rdram[0x791546],
            rdram[0x791547],rdram[0x791548],rdram[0x791549],rdram[0x79154A],
            rdram[0x79154B],rdram[0x79154C],rdram[0x79154D],rdram[0x79154E]);
        LOGV(
            "RDRAMPROBE @0xF9153F: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            rdram[0xF9153F],rdram[0xF91540],rdram[0xF91541],rdram[0xF91542],
            rdram[0xF91543],rdram[0xF91544],rdram[0xF91545],rdram[0xF91546],
            rdram[0xF91547],rdram[0xF91548],rdram[0xF91549],rdram[0xF9154A],
            rdram[0xF9154B],rdram[0xF9154C],rdram[0xF9154D],rdram[0xF9154E]);
        LOGV(
            "RDRAMPROBE @0x3E45C0: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
            rdram[0x3E45C0],rdram[0x3E45C1],rdram[0x3E45C2],rdram[0x3E45C3],
            rdram[0x3E45C4],rdram[0x3E45C5],rdram[0x3E45C6],rdram[0x3E45C7],
            rdram[0x3E45C8],rdram[0x3E45C9],rdram[0x3E45CA],rdram[0x3E45CB],
            rdram[0x3E45CC],rdram[0x3E45CD],rdram[0x3E45CE],rdram[0x3E45CF]);
    }

    uint8_t* rsp_rdram = gN64_RDRAM;  // avoid unused warning
    (void)rsp_rdram;
    s_mtx_log_frame = 0;
    s_mtx_dump_frame = 0;
    if (true) { // BKA_MOD: log every call
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "RSP_ProcessGfxTask CALL #%d: tp=%p type=%d data=%p size=%u dmemVertexCount=%d",
            s_rspCallCount, tp, tp ? tp->t.type : -1, tp ? tp->t.data_ptr : nullptr,
            tp ? tp->t.data_size : 0, s_rdp.dmemVertexCount);
    }
    {
        static int s_topdump = 0;
        if (s_topdump++ < 20 && tp && tp->t.data_ptr) {
            const uint8_t* d = (const uint8_t*)tp->t.data_ptr;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-PROBE",
                "TOPDL ptr=%p bytes: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X | %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                tp->t.data_ptr,
                d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],
                d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15],
                d[16],d[17],d[18],d[19],d[20],d[21],d[22],d[23],
                d[24],d[25],d[26],d[27],d[28],d[29],d[30],d[31]);
        }
    }

    {
        static int s_taskbytes = 0;
        if (tp && tp->t.data_ptr && s_taskbytes < 2) {
            s_taskbytes++;
            const uint8_t* d = (const uint8_t*)tp->t.data_ptr;
            for (int row = 0; row < 32; row++) {
                const uint8_t* r = d + row * 8;
                unsigned le = (unsigned)(r[0] | (r[1]<<8) | (r[2]<<16) | (r[3]<<24));
                unsigned be = (unsigned)((r[0]<<24) | (r[1]<<16) | (r[2]<<8) | r[3]);
                unsigned le2 = (unsigned)(r[4] | (r[5]<<8) | (r[6]<<16) | (r[7]<<24));
                unsigned be2 = (unsigned)((r[4]<<24) | (r[5]<<16) | (r[6]<<8) | r[7]);
                __android_log_print(ANDROID_LOG_ERROR, "BKA-FULLDL",
                    "t#%d +%03X: %02X%02X%02X%02X %02X%02X%02X%02X  w0_LE=%08X w0_BE=%08X  w1_LE=%08X w1_BE=%08X",
                    s_rspCallCount, row * 8,
                    r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],
                    le, be, le2, be2);
            }
        }
    }

    { static int s_rd = 0; if (s_rd++ < 3) {
        extern uint8_t* gN64_RDRAM;
        uint8_t* dp = (uint8_t*)tp->t.data_ptr;
        uintptr_t rddram_u = (uintptr_t)gN64_RDRAM;
        uintptr_t dp_u = (uintptr_t)dp;
        int dp_in_rddram = (dp && gN64_RDRAM && dp_u >= rddram_u && dp_u < rddram_u + 0x1000000);
        uint32_t dp_off = dp_in_rddram ? (uint32_t)(dp_u - rddram_u) : 0;
        /* First 32 bytes of the display list itself */
        __android_log_print(ANDROID_LOG_ERROR, "BKA-DLINFO",
            "task=%p rddram=%p dp=%p in_rddram=%d off=%07X f16=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
            (void*)tp, (void*)gN64_RDRAM, (void*)dp, dp_in_rddram, dp_off,
            dp?dp[0]:0,dp?dp[1]:0,dp?dp[2]:0,dp?dp[3]:0,
            dp?dp[4]:0,dp?dp[5]:0,dp?dp[6]:0,dp?dp[7]:0,
            dp?dp[8]:0,dp?dp[9]:0,dp?dp[10]:0,dp?dp[11]:0,
            dp?dp[12]:0,dp?dp[13]:0,dp?dp[14]:0,dp?dp[15]:0);
        /* Framebuffer regions from current/last */
        extern uint8_t* gVideoCurrFramebuffer;
        extern uint8_t* gVideoLastFramebuffer;
        int64_t off_curr = (gVideoCurrFramebuffer && gN64_RDRAM) ? (gVideoCurrFramebuffer - gN64_RDRAM) : -1;
        int64_t off_last = (gVideoLastFramebuffer && gN64_RDRAM) ? (gVideoLastFramebuffer - gN64_RDRAM) : -1;
        int curr_nz = 0, last_nz = 0;
        if (gVideoCurrFramebuffer) for (int k = 0; k < 4096; k++) if (gVideoCurrFramebuffer[k]) curr_nz++;
        if (gVideoLastFramebuffer) for (int k = 0; k < 4096; k++) if (gVideoLastFramebuffer[k]) last_nz++;
        __android_log_print(ANDROID_LOG_ERROR, "BKA-FB",
            "curr_fb=%p off=%lld nz=%d | last_fb=%p off=%lld nz=%d",
            (void*)gVideoCurrFramebuffer, (long long)off_curr, curr_nz,
            (void*)gVideoLastFramebuffer, (long long)off_last, last_nz);
    } }
    
    if (!tp || !tp->t.data_ptr || tp->t.data_size == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "early return: tp=%p data=%p size=%u",
            tp, tp ? tp->t.data_ptr : nullptr, tp ? tp->t.data_size : 0);
        return;
    }

    // Hex dump removed to reduce log overhead.

    /* PORT: RDP_InitState must run ONCE at boot, not every frame.
       Otherwise boot-time texture uploads (TMEM) are wiped before the
       first render task samples them. Static flag makes this one-shot. */
    static bool s_rdp_state_inited = false;
    if (!s_rdp_state_inited) {
        s_rdp_state_inited = true;
        __android_log_print(ANDROID_LOG_ERROR, "BKA-RDP", "FIRST-CALL RDP_InitState()");
        RDP_InitState();
    
    /* PORT: Initialize Z-buffer at RDRAM 0x3F0000 (320*240*2 bytes) */
    extern uint8_t* gN64_RDRAM;
    if (gN64_RDRAM) {
        memset(gN64_RDRAM + 0x3F0000, 0xFF, 320 * 240 * 2); // 0xFFFF = far plane
    }
        
        /* PORT DIAGNOSTIC: mirror ROM to RDRAM at init (skip framebuffers).
           Theory: game DMAs to heap, but RDP samples from RDRAM. Bridge by
           copying ROM content to RDRAM at same offsets. If RARE logo appears
           with real textures, the bridge is the fix. */
        extern uint8_t* gN64_ROM_Base;
        extern uint8_t* gN64_RDRAM;
        size_t rom_sz = resource_rom_size();
        if (gN64_ROM_Base && gN64_RDRAM && rom_sz > 0) {
            size_t max_copy = rom_sz < 0x1000000 ? rom_sz : 0x1000000;
            size_t fb_start = 0x400000;
            size_t fb_end   = 0x480000;
            size_t copied = 0;
            if (max_copy <= fb_start) {
                memcpy(gN64_RDRAM, gN64_ROM_Base, max_copy);
                copied = max_copy;
            } else {
                memcpy(gN64_RDRAM, gN64_ROM_Base, fb_start);
                copied += fb_start;
                if (max_copy > fb_end) {
                    memcpy(gN64_RDRAM + fb_end, gN64_ROM_Base + fb_end, max_copy - fb_end);
                    copied += max_copy - fb_end;
                }
            }
            __android_log_print(ANDROID_LOG_ERROR, "BKA-ROM2RAM",
                "Copied %zu/%zu bytes ROM->RDRAM. ROM[2FE8C0]=%02X%02X%02X%02X RDRAM[2FE8C0]=%02X%02X%02X%02X",
                copied, rom_sz,
                gN64_ROM_Base[0x2FE8C0], gN64_ROM_Base[0x2FE8C1], gN64_ROM_Base[0x2FE8C2], gN64_ROM_Base[0x2FE8C3],
                gN64_RDRAM[0x2FE8C0], gN64_RDRAM[0x2FE8C1], gN64_RDRAM[0x2FE8C2], gN64_RDRAM[0x2FE8C3]);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "BKA-ROM2RAM",
                "Skipped (ROM=%p RDRAM=%p size=%zu)", (void*)gN64_ROM_Base, (void*)gN64_RDRAM, rom_sz);
        }
    }

    // ---- DL_DUMP_ADDED ----
    {
        const uint8_t* d = (const uint8_t*)tp->t.data_ptr;
        uint32_t sz = tp->t.data_size;
        uint32_t lim = sz < 512 ? sz : 512;
        for (uint32_t off = 0; off < lim; off += 32) {
            uint32_t o2 = off + 16;
            /* DLRAW disabled */;
        }
    }
    // ---- end DL_DUMP_ADDED ----

    // Diagnostic: dump first 16 bytes of task data
    {
        const unsigned char *dp = (const unsigned char*)tp->t.data_ptr;
        for (int off = 0; off < 32; off += 16) {
            LOGV(
                "RSP task data[%02d]: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x",
                off,
                dp[off+0], dp[off+1], dp[off+2], dp[off+3],
                dp[off+4], dp[off+5], dp[off+6], dp[off+7],
                dp[off+8], dp[off+9], dp[off+10], dp[off+11],
                dp[off+12], dp[off+13], dp[off+14], dp[off+15]);
        }
    }

    // Only clear framebuffers on first call (static flag)
    static bool s_firstFrame = true;
    if (s_firstFrame) {
        memset(gFramebuffers[0], 0, sizeof(gFramebuffers[0]));
        memset(gFramebuffers[1], 0, sizeof(gFramebuffers[1]));
        s_firstFrame = false;
        __android_log_print(ANDROID_LOG_INFO, "BKA_GFX", "Framebuffers cleared (first frame)");
    }


    struct DListFrame {
        uint8_t *ptr;
        uint8_t *end;
    };

    DListFrame stack[64];
    int depth = 0;
    /* Port-emitted top-level DLs are 16-byte (8-byte Gfx cmd + 8-byte tag).
     * Sub-DLs entered via G_DL are 8-byte compact (overridden at case 0x06,
     * restored from stack on G_ENDDL pop). Initializer was 8, causing every
     * task to walk half-real / half-padding until detection fired — and for
     * tasks whose first commands decode as valid opcodes in BOTH endian
     * interpretations (e.g. w0=0x060000BC -> LE_op=BC, BE_op=06), the
     * detector cannot disambiguate and stride stays wrong for the whole
     * task, producing fake G_DL / G_VTX / Cmd_Polygon with garbage w1. */
    size_t current_stride = 16;
    size_t stack_stride[64];
    uintptr_t stack_dl_base[64];
    int stack_enc[64];          /* per-frame: 0=unknown, 1=LE, 2=BE */
    int cur_dl_enc = 0;         /* revert E1: unknown, will be probed */
    bool s_seen_gdl_at_depth0 = false;   /* Fix Q: wrapper detection */
    bool s_just_popped = false;          /* Fix R: set by G_ENDDL pop, cleared on dispatch */
    uintptr_t visited_dl_addrs[256];
    int visited_dl_count = 0;
    uint8_t *cur = (uint8_t*)tp->t.data_ptr;
    uint8_t *cur_end = cur + tp->t.data_size;

    {
        static int s_dual = 0;
        if (s_dual++ < 6) {
            uint8_t* p = cur;
            __android_log_print(ANDROID_LOG_ERROR, "BKA-DUAL", "task ptr=%p size=%u", (void*)p, (unsigned)tp->t.data_size);
            for (int i = 0; i < 8; i++) {
                uint8_t* q = p + i * 16;
                if (!bka_is_readable(q + 16)) break;
                uint32_t w_le = *(uint32_t*)q;
                uint32_t w_be = __builtin_bswap32(w_le);
                uint8_t op_le = (w_le >> 24) & 0xFF;
                uint8_t op_be = (w_be >> 24) & 0xFF;
                __android_log_print(ANDROID_LOG_ERROR, "BKA-DUAL",
                    "  i=%d bytes=%02X%02X%02X%02X | LE w0=%08X op=%02X %s | BE w0=%08X op=%02X %s",
                    i, q[0],q[1],q[2],q[3],
                    w_le, op_le, bka_is_f3dex_opcode(op_le) ? "OK" : "bad",
                    w_be, op_be, bka_is_f3dex_opcode(op_be) ? "OK" : "bad");
            }
        }
    }

    {
        static int s_dl_dump = 0;
        if (s_dl_dump++ < 20) {
            FILE *fp = fopen("/data/data/com.dkrwrapper/files/dl_dump.bin", "wb");
            if (fp) {
                size_t n = (size_t)(cur_end - cur);
                if (n > 131072) n = 131072;
                fwrite(cur, 1, n, fp);
                fclose(fp);
                __android_log_print(ANDROID_LOG_ERROR, "BKA-DLDMP",
                    "wrote %zu bytes to dl_dump.bin", n);
            }
        }
    }

    const size_t MAX_TOTAL_CMDS = 20000;   /* real RSP tasks are <2000 cmds */
    const size_t MAX_DL_CMDS = 5000;
    size_t total = 0;
    size_t dl_cmds = 0;
    int zero_run = 0;
    int unknown_opcode_run = 0;
    int cmds_since_progress = 0;
    size_t gdl_jumps = 0;
    const size_t MAX_GDL_JUMPS = 200;
    int consecutive_bad_gdl = 0;
    struct timespec start_ts, now_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    bool log_after_jump = false;
    int jump_log_count = 0;

    /* Ring buffer of last accepted commands for drift diagnosis. */
    struct { uint8_t* ptr; uint32_t w0, w1; uint8_t op; int enc; size_t stride; int depth; } s_lastcmds[32];
    int s_lastcmd_idx = 0;
    memset(s_lastcmds, 0, sizeof(s_lastcmds));
    while (cur + current_stride <= cur_end) {
        if (++total > MAX_TOTAL_CMDS) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "runaway display list: exceeded %u total commands", (unsigned)MAX_TOTAL_CMDS);
            break;
        }

        if (total >= 36 && total <= 60) {
            LOGV(
                 "LOOP iter=%zu cur=%p cur_end=%p depth=%d stride=%zu",
                total, cur, cur_end, depth, current_stride);
        }

        if (++dl_cmds > MAX_DL_CMDS) {
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "runaway display list: exceeded %u commands in current DL", (unsigned)MAX_DL_CMDS);
            break;
        }

        // Yield to other threads every 4 commands and enforce time budget
        if ((total & 0x01) == 0) {
            sched_yield();
            if ((total & 0x0F) == 0) {
                clock_gettime(CLOCK_MONOTONIC, &now_ts);
                long long elapsed_ms = (now_ts.tv_sec - start_ts.tv_sec) * 1000 + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
                if (elapsed_ms > 2000) {
                    __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                        "RSP time budget exceeded at cmd %zu (%lld ms), breaking", total-1, elapsed_ms);
                    break;
                }
            }
        }

        GfxCommand c = {0};
        memcpy(&c, cur, 8);
        s_current_cmd = cur;
        { int ri = s_bka_ring_n & (BKA_RING_SZ - 1); s_bka_ring[ri].dl_cur = (uintptr_t)cur; s_bka_ring[ri].w0 = c.w0; s_bka_ring[ri].w1 = c.w1; s_bka_ring[ri].op = (uint8_t)(c.w0 >> 24); s_bka_ring[ri].enc = (uint8_t)cur_dl_enc; __atomic_store_n(&s_bka_ring_n, s_bka_ring_n + 1, __ATOMIC_RELEASE); }
        {
            static int s_last_enc = -1;
            static int s_enc_flips = 0;
            if (s_last_enc != -1 && s_last_enc != cur_dl_enc && s_enc_flips++ < 20) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "ENCFLIP at cur=%p total=%zu depth=%d %d -> %d bytes=%02X%02X%02X%02X",
                    (void*)cur, total, depth, s_last_enc, cur_dl_enc,
                    cur[0],cur[1],cur[2],cur[3]);
            }
            s_last_enc = cur_dl_enc;
        }

        /* Per-DL encoding detection.  Once a DL is detected as BE, every
         * command in it must be byte-swapped -- not just the ones whose low
         * byte happens to fall in [0xB0, 0xCF].  RDP setup opcodes
         * (G_RDPSETOTHERMODE=0xE7, G_SETCOMBINE=0xFC, ...) live above 0xCF
         * and would otherwise be misread as a stale G_VTX.  Sticky per DL;
         * reset to unknown on G_DL jump. */
        {
            uint8_t hi = (uint8_t)(c.w0 >> 24);
            uint8_t lo = (uint8_t)(c.w0 & 0xFF);
            if (cur_dl_enc == 2) {
                c.w0 = __builtin_bswap32(c.w0);
                c.w1 = __builtin_bswap32(c.w1);
            } else if (cur_dl_enc == 0) {
                // Use the full F3DEX opcode table, not just [0xB0,0xCF].
                // Opcodes 0xFC (SETCOMBINE), 0xE7 (RDPSETOTHERMODE), etc.
                // are outside that range but still legitimate BE opcodes.
                bool hi_is_g = bka_is_f3dex_opcode(hi);
                bool lo_is_g = bka_is_f3dex_opcode(lo);
                if (hi == 0x00 && lo != 0x00 && lo_is_g) {
                    /* LE reads as NOP (padding) but BE reads as a real opcode.
                     * Banjo-Kazooie never starts a sub-DL with NOP padding,
                     * so this is a BE-formatted DL. */
                    c.w0 = __builtin_bswap32(c.w0);
                    c.w1 = __builtin_bswap32(c.w1);
                    cur_dl_enc = 2;
                    static int s_nop_be = 0;
                    if ((s_nop_be++ % 500) == 0)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "DLENC NOP->BE @%p bytes %02X%02X%02X%02X -> op=0x%02X",
                            cur, cur[0],cur[1],cur[2],cur[3],
                            (uint8_t)(c.w0 >> 24));
                } else if (lo == 0x00 && hi != 0x00 && hi_is_g) {
                    /* Mirror: BE reads as NOP, LE is real. */
                    cur_dl_enc = 1;
                } else                 if (hi_is_g && lo_is_g) {
                    /* Ambiguous: both byte[0] and byte[3] are valid F3DEX
                     * opcodes.  Default to BE: N64 architecture is big-endian
                     * and F3DEX display lists are defined in BE byte order.
                     * Empirically, B-K top-level DL bytes 06 00 00 BC only
                     * decode correctly as BE (= G_DL, target in w1).  The
                     * previous LE default caused walker drift at depth=0
                     * within the first few commands, since half the DLs
                     * interpreted every opcode as a MOVEWORD-style command
                     * with the real opcode in the wrong byte. */
                    cur_dl_enc = 1;
                    static int s_amb = 0;
                    if ((s_amb++ % 500) == 0)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "DLENC AMBIG->LE @%p bytes %02X%02X%02X%02X op=0x%02X",
                            cur, cur[0],cur[1],cur[2],cur[3],
                            (uint8_t)(c.w0 >> 24));
                } else if (lo_is_g && !hi_is_g) {
                    c.w0 = __builtin_bswap32(c.w0);
                    c.w1 = __builtin_bswap32(c.w1);
                    cur_dl_enc = 2;
                    static int s_be_log = 0;
                    if ((s_be_log++ % 500) == 0)
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "DLENC BE @%p first bytes %02X%02X%02X%02X -> op=0x%02X",
                            cur, cur[0],cur[1],cur[2],cur[3],
                            (uint8_t)(c.w0 >> 24));
                } else if (hi_is_g && !lo_is_g) {
                    cur_dl_enc = 1;
                }
            }
        }
                /* Fix X: removed Fix L's padding sanity check.  B-K's recomp
         * layout is 8 bytes command + 8 bytes payload (host pointer /
         * metadata), NOT 8 bytes command + 8 bytes zero padding.  The
         * check was bailing on every valid command, dropping the walker
         * down to 1-3 commands per task. */
        /* Bounds guard: display list can be in host heap, not RDRAM.
         * If cur (or the 16-byte command span) is unreadable, stop —
         * otherwise the walker runs off the end into an unmapped page
         * and SIGSEGVs on the next 8-byte read. */
        /* Bounds check:
         *   depth==0:  tight bounds on top-level task DL (host heap, from
         *              tp->t.data_ptr; anything outside = drift).
         *   depth>0:   sub-DL entered via G_DL — may live in RDRAM OR host
         *              heap depending on how the game allocated it. The
         *              port's mempool_alloc_safe returns heap, so sub-DLs
         *              are usually heap. Accept ANY readable page at depth>0;
         *              drift detector (unknown_opcode_run>=8) catches real
         *              overruns into garbage. */
        if (depth == 0) {
            if ((const uint8_t*)cur < bka_dl_start ||
                (const uint8_t*)cur >= bka_dl_end) {
                static int s_end = 0;
                if (s_end++ < 40)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DLEND",
                        "depth0 cur=%p outside [%p,%p) off=0x%llx size=%zu stride=%zu",
                        (void*)cur, (void*)bka_dl_start, (void*)bka_dl_end,
                        (unsigned long long)((const uint8_t*)cur - bka_dl_start),
                        (size_t)(bka_dl_end - bka_dl_start),
                        current_stride);
                return;
            }
        } else {
            extern int bka_is_readable(void*);
            if (!bka_is_readable((void*)cur) ||
                !bka_is_readable((void*)((const uint8_t*)cur + 15))) {
                static int s_end = 0;
                if (s_end++ < 40)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DLEND",
                        "depth%d cur=%p unreadable (sub-DL end)",
                        depth, (void*)cur);
                return;
            }
        }
        if (!bka_is_readable((void*)cur) ||
            !bka_is_readable((void*)((const uint8_t*)cur + 15))) {
            static int s_oob = 0;
            if (s_oob++ < 20)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-OOB",
                    "walker: cur=%p unreadable (stride=%zu depth=%d) — stopping task",
                    (void*)cur, current_stride, depth);
            return;
        }

        /* Fix O: RGBA fill-pattern detector.  Bytes 4-7 of a real command
         * are a w1 pointer/param.  If they are a repeating byte pattern
         * (0x787878xx, 0x464646xx, 0x3C3C3Cxx), we are reading texture
         * data as commands.  Count as drift without dispatching. */
        {
            uint8_t b4 = (c.w1 >> 24) & 0xFF;
            uint8_t b5 = (c.w1 >> 16) & 0xFF;
            uint8_t b6 = (c.w1 >>  8) & 0xFF;
            if (b4 == b5 && b5 == b6 && b4 != 0 && b4 != 0xFF) {
                unknown_opcode_run += 3;
                if (unknown_opcode_run >= 8) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "walker: fill-pattern at cur=%p depth=%d w0=%08X w1=%08X "
                        "dl_range=%p..%p (cur_off=0x%llx/0x%llx) -- drifting",
                        (void*)cur, depth, c.w0, c.w1,
                        (void*)bka_dl_start, (void*)bka_dl_end,
                        (unsigned long long)((const uint8_t*)cur - bka_dl_start),
                        (unsigned long long)(bka_dl_end - bka_dl_start));
                    return;
                }
                cur += current_stride;
                continue;
            }
        }
        g_bka_dl_cur = cur;
        uint8_t opcode = GFX_OPCODE(c);
        /* OP5-SEEN: log every opcode 0x05 before dispatch so we can
         * distinguish "walker never parses 0x05" (encoding bug) from
         * "case 0x05 unreachable" (dispatch bug). */
        if (opcode == 0x05) {
            static int _op5 = 0;
            if (_op5++ < 100)
                __android_log_print(ANDROID_LOG_ERROR, "BKA-OP5-SEEN",
                    "w0=%08X w1=%08X depth=%d stride=%zu enc=%d cur=%p",
                    c.w0, c.w1, depth, current_stride,
                    (int)cur_dl_enc, (void*)cur);
        }
        {
            static uint8_t *s_prev_cur = NULL;
            static int s_stuck_count = 0;
            static uint8_t *s_call_entry = NULL;
            static int s_call_ops = 0;
            if ((uint8_t*)tp->t.data_ptr != s_call_entry) {
                s_call_entry = (uint8_t*)tp->t.data_ptr;
                s_call_ops = 0;
            }
            if (s_prev_cur == cur) {
                if (++s_stuck_count > 200) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-WALK-STUCK",
                        "walker stuck: cur=%p %d iters last_op=0x%02X w0=0x%08X w1=0x%08X",
                        (void*)cur, s_stuck_count, opcode, c.w0, c.w1);
                    return;
                }
            } else {
                s_prev_cur = cur;
                s_stuck_count = 0;
            }
            if (++s_call_ops > 20000) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA-WALK-STUCK",
                    "walker runaway: %d ops this call, cur=%p last_op=0x%02X",
                    s_call_ops, (void*)cur, opcode);
                return;
            }
        }

        /* BKA-DLFLIP: structural encoding check for G_DL ambiguity.
         * BE G_TRI2 bytes (B1 XX XX XX) mis-read as LE become
         * 0xXX XX XX B1 with opcode 0x06 (G_DL) and a nonsense target
         * address.  The flip-on-invalid-opcode path does not catch this
         * because 0x06 is a valid opcode.  Detect the pattern directly:
         * if LE yields op=0x06 and BE yields a valid non-G_DL opcode,
         * the byte order is wrong.  Prefer BE. */
        if (opcode == 0x06 && cur_dl_enc == 1) {
            uint32_t bw0 = __builtin_bswap32(c.w0);
            uint8_t bop = (uint8_t)(bw0 >> 24);
            if (bop != 0x06 && bka_is_f3dex_opcode(bop)) {
                cur_dl_enc = 2;
                c.w0 = bw0;
                c.w1 = __builtin_bswap32(c.w1);
                opcode = bop;
                { static int s_dlflip = 0; if (s_dlflip++ < 30)
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DLFLIP",
                        "flip G_DL->BE op=0x%02X cur=%p", opcode, (void*)cur); }
            }
        }
        /* Fix R: after popping out of a G_DL, verify the parent has a
         * valid command next.  If not, the parent DL has ended -- the
         * walker did not return to a real command sequence.  Unwind or
         * terminate cleanly, do NOT drift into the metadata tail. */
        if (s_just_popped && !bka_is_f3dex_opcode(opcode)) {
            static int s_rterm = 0;
            if (s_rterm++ < 30)
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "walker: post-pop non-command at cur=%p depth=%d op=0x%02X -- unwinding",
                    (void*)cur, depth, opcode);
            if (depth > 0) {
                depth--;
                cur = stack[depth].ptr;
                cur_end = stack[depth].end;
                current_stride = stack_stride[depth];
                s_dl_base = stack_dl_base[depth];
                cur_dl_enc = stack_enc[depth];
                dl_cmds = 0;
                zero_run = 0;
                s_just_popped = false;
                continue;
            }
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "walker: top-level wrapper ended cleanly at cur=%p", (void*)cur);
            break;
        }
        s_just_popped = false;
                s_opcount[opcode]++;
                if ((++s_op_total % 500) == 0) {
                    char obuf[2048]; int on = 0;
                    for (int k = 0; k < 256 && on < (int)sizeof(obuf) - 16; k++)
                        if (s_opcount[k]) on += snprintf(obuf + on, sizeof(obuf) - on, "%02X:%u ", k, s_opcount[k]);
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-OPC", "seen=%u %s", s_op_total, obuf);
                }
        {
            static int s_enc_dump = 0;
            if ((s_enc_dump++ % 2000) == 0) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                     "ENCDUMP @%p enc=%d w0=0x%08X w1=0x%08X op=0x%02X depth=%d first4=%02X%02X%02X%02X",
                    cur, cur_dl_enc, c.w0, c.w1, opcode, depth,
                    cur[0], cur[1], cur[2], cur[3]);
            }
        }
        {
            if (total <= 20) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                     "CMD t#%zu @%p raw=%02X%02X%02X%02X %02X%02X%02X%02X w0=0x%08X w1=0x%08X op=0x%02X enc=%d depth=%d",
                    total, cur, cur[0],cur[1],cur[2],cur[3],cur[4],cur[5],cur[6],cur[7],
                    c.w0, c.w1, opcode, cur_dl_enc, depth);
            }
        }
        /* SPNOOP (0x00) is a valid no-op command in DL trees; it counts as
                     * progress for the drift detector even though it is excluded
                     * from the encoding probe to keep LE/BE disambiguation clean. */
        if (opcode == 0x00 || bka_is_f3dex_opcode(opcode)) {
            unknown_opcode_run = 0;
        } else {
            uint32_t flipped_w0 = __builtin_bswap32(c.w0);
            uint8_t flipped_op = (uint8_t)(flipped_w0 >> 24);
            if (bka_is_f3dex_opcode(flipped_op)) {
                cur_dl_enc = (cur_dl_enc == 1) ? 2 : 1;
                c.w0 = flipped_w0;
                c.w1 = __builtin_bswap32(c.w1);
                opcode = flipped_op;
                unknown_opcode_run = 0;
                { static int s_encflip = 0; if (s_encflip++ < 20) __android_log_print(ANDROID_LOG_ERROR, "BKA-ENCFLIP", "flip enc=%d cur=%p op=0x%02X", cur_dl_enc, (void*)cur, opcode); }
            } else {
                unknown_opcode_run++;
            }
        }
        if (opcode == 0x04 && total <= 5) {
            const uint8_t *raw = cur;
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "RAW Vtx bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
                raw[0], raw[1], raw[2], raw[3],
                raw[4], raw[5], raw[6], raw[7]);
        }

        // Fix L: top-level stride via multi-slot detector (once, not per loop)
        /* ARM64 port 2026-10-14: only force 16-byte stride at the top level.
           The outer DL built by the port's gbi macros is 16 bytes per command
           (8 cmd + 8 relocation tag). Sub-DLs loaded from a BE model bin are
           8-byte packed with no tag. The unconditional assignment here was
           overwriting the 8-byte stride set when entering a sub-DL, so the
           walker skipped every other command in the RARE logo's DL -- including
           the palette SetTImg, causing LoadTLUT to read pixel bytes instead of
           palette. Guard with depth==0. */
        if (depth == 0) {
            current_stride = 16;   /* port recomp adds 8-byte physical-addr tag after each cmd */
        }

        if (total <= 100) {
            if (log_after_jump) jump_log_count++;
            /* CMDX disabled */;
        }

        // Any recognized opcode counts as progress.  Only sequences of
        // unrecognized bytes indicate drift; a DL that opens with 50
        // setup commands (texture / combiner / tile) is completely normal.
        if (bka_is_f3dex_opcode(opcode)) {
            cmds_since_progress = 0;
        } else {
            cmds_since_progress++;
        }
        s_lastcmds[s_lastcmd_idx & 31].ptr = cur;
        s_lastcmds[s_lastcmd_idx & 31].w0 = c.w0;
        s_lastcmds[s_lastcmd_idx & 31].w1 = c.w1;
        s_lastcmds[s_lastcmd_idx & 31].op = opcode;
        s_lastcmds[s_lastcmd_idx & 31].enc = cur_dl_enc;
        s_lastcmds[s_lastcmd_idx & 31].stride = current_stride;
        s_lastcmds[s_lastcmd_idx & 31].depth = depth;
        s_lastcmd_idx++;
        cur += current_stride;

        if (c.w0 != 0 || c.w1 != 0) zero_run = 0;
        if (c.w0 == 0 && c.w1 == 0) {
            zero_run = (c.w0 == 0 && c.w1 == 0) ? (zero_run + 1) : 0;
            if (zero_run >= 2) {
                if (depth > 0) {
                    // Skip zero padding to find actual commands in nested DL
                    uint8_t* probe = cur;
                    int skipped = 0;
                    while (probe + 8 <= cur_end && (probe[0] == 0 && probe[1] == 0 && probe[2] == 0 && probe[3] == 0)) {
                        probe += current_stride;
                        skipped++;
                    }
                    if (probe + 8 <= cur_end) {
                        // Found a non-zero command; jump to it
                        cur = probe;
                        zero_run = 0;
                        dl_cmds = 0;
                        continue;
                    } else {
                        // No non-zero commands found; pop stack
                        depth--;
                        cur = stack[depth].ptr;
                        cur_end = stack[depth].end;
                        current_stride = stack_stride[depth];
                        s_dl_base = stack_dl_base[depth];
                        cur_dl_enc = stack_enc[depth];
                        dl_cmds = 0;
                        zero_run = 0;
                        continue;
                    }
                } else {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "zero-run break at cmd %zu, depth=%d", total - 1, depth);
                    break;
                }
            }
        } else {
            zero_run = 0;
        }

        // Nested display lists in Banjo-Kazooie often omit ENDDL and are
        // followed by vertex/index data. We now use the task data size
        // as the boundary, so no early stop is needed.

                /* BKA-DLOP: first 64 decoded opcodes, one line each. */
                {
                    static int s_dlop = 0;
                    if (s_dlop++ < 64) {
                        const uint8_t* rp = (const uint8_t*)cur;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-DLOP",
                            "op=%02X w0=%08X w1=%08X depth=%d enc=%d off=%ld raw=%02X%02X%02X%02X",
                            opcode, c.w0, c.w1, depth, (int)cur_dl_enc,
                            (long)(cur - (uint8_t*)s_dl_base),
                            rp[0], rp[1], rp[2], rp[3]);
                    }
                }
                /* BKA-OPHIST: cumulative opcode histogram, top-8 logged every 2000 decoded commands. */
                {
                    static uint64_t s_hist[256];
                    static uint64_t s_total = 0;
                    static uint64_t s_tri_ops = 0;
                    s_hist[opcode]++;
                    s_total++;
                    if (opcode==0xBF||opcode==0xB1||opcode==0xC4||opcode==0x34) s_tri_ops++;
                    if ((s_total % 2000) == 0) {
                        uint64_t copy[256];
                        for (int i = 0; i < 256; i++) copy[i] = s_hist[i];
                        char buf[1024];
                        int n = snprintf(buf, sizeof(buf), "total=%llu top:", (unsigned long long)s_total);
                        for (int k = 0; k < 8; k++) {
                            uint64_t best = 0; int bi = -1;
                            for (int i = 0; i < 256; i++) if (copy[i] > best) { best = copy[i]; bi = i; }
                            if (bi < 0 || best == 0) break;
                            n += snprintf(buf+n, sizeof(buf)-n, " %02X=%llu", bi, (unsigned long long)best);
                            copy[bi] = 0;
                        }
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-OPHIST", "%s tri_ops=%llu", buf, (unsigned long long)s_tri_ops);
                    }
                }
                /* BKA-CMDENTRY: disabled 2026-10-11 — per-opcode logcat was starving the render loop */
                /* BKA-CMDENTRY line removed */
                g_opcode_hist[opcode]++;
            switch (opcode) {
            case 0xAF: // G_LOAD_UCODE - not needed for software RDP
            case 0xB5: // G_LINE3D
            case 0xBD: { // G_POPMTX (F3DEX2)
                uint32_t n = (c.w0 & 0xFF);
                if (n == 0) n = 1;
                while (n-- > 0 && s_rdp.modelviewStackDepth > 0) {
                    s_rdp.modelviewStackDepth--;
                    memcpy(s_rdp.modelview,
                           s_rdp.modelviewStack[s_rdp.modelviewStackDepth],
                           sizeof(BKMatrix));
                }
                Matrix_Multiply(s_rdp.viewProj, s_rdp.modelview, s_rdp.projection);
                break;
            }
            case 0x00:
            case 0xC0:
            case 0xE8:
            case 0xE7:
            case 0xE9:
            case 0xE6:
            case 0xE1:
            case 0xF1:
            case 0x02:
            case 0xDA:            case 0xBE:
            case 0xBA:
            case 0xB9:
            case 0xB7:
            case 0xB6:
            case 0xED:
            case 0xFF:
                break;
            case 0x58: // G_SETTILESIZE? no-op for now
            case 0xE0: // Unknown but appears frequently; treat as no-op
            case 0x90: // Unknown
            case 0x88: // Unknown
            case 0x70: // Unknown
                break;

            case 0xF7: Cmd_SetFillColor(c); break;
            case 0xFA: Cmd_SetPrimColor(c); break;
            case 0xFB: Cmd_SetEnvColor(c); break;
            case 0xE2: Cmd_SetOtherModeL(c); break;
            case 0xE3: Cmd_SetOtherModeH(c); break;
            case 0xFC: Cmd_SetCombine(c); break;
            case 0xD7: Cmd_Texture(c); break;
            case 0xBB: Cmd_Texture(c); break;   // F3DEX (non-2) G_TEXTURE
            
case 0xF5: { static int s_op=0; if (s_op++<4000) __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEOP", "F5 SetTile w0=%08X w1=%08X", c.w0, c.w1); Cmd_SetTile(c); break; }
            case 0xF0: { static int s_op=0; if (s_op++<4000) __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEOP", "F0 LoadTLUT cur=%p stride=%zu depth=%d enc=%d w0=%08X w1=%08X", (void*)cur, current_stride, depth, cur_dl_enc, c.w0, c.w1); Cmd_LoadTLUT(c); break; }
            case 0xF2: { static int s_op=0; if (s_op++<4000) __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEOP", "F2 LoadTile w0=%08X w1=%08X", c.w0, c.w1); /* ARM64 port 2026-10-13: F2 is LoadTile, not SetTileSize */ Cmd_LoadTile(c); break; }
            case 0xFD: { static int s_op=0; if (s_op++<4000) __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEOP", "FD SetTImg cur=%p stride=%zu depth=%d enc=%d w0=%08X w1=%08X", (void*)cur, current_stride, depth, cur_dl_enc, c.w0, c.w1); Cmd_SetTImg(c); break; }
            case 0xF3: { static int s_op=0; if (s_op++<4000) __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEOP", "F3 LoadBlock w0=%08X w1=%08X", c.w0, c.w1); Cmd_LoadBlock(c); break; }
            case 0xF4: { static int s_op=0; if (s_op++<4000) __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEOP", "F4 SetTileSize w0=%08X w1=%08X", c.w0, c.w1); /* ARM64 port 2026-10-13: F4 is SetTileSize in F3DEX1 */ Cmd_SetTileSize(c); break; }
            case 0xF6: Cmd_FillRect(c); break;
            case 0x51: /* DISABLED - old Rare SetTextureImage (0x07 not used by game) */ {
                /* PORT: opcode 0x07 = Rare SetTextureImage (fires 1:1 with
                   TEXRECT). Try 28-bit offset (standard) AND 24-bit offset
                   (looks more likely given base+off values), pick whichever
                   lands on non-zero data in RDRAM. Log rddram + delta + bytes. */
                static int s_07 = 0;
                uint32_t seg = (c.w1 >> 28) & 0x0F;
                uint32_t base = s_rdp.segmentBase[seg];
                uint32_t base_k = base ? (base | 0x80000000u) : 0x80000000u;
                uint32_t off28 = c.w1 & 0x0FFFFFFFu;
                uint32_t off24 = c.w1 & 0x00FFFFFFu;
                
                uint8_t* a28 = (uint8_t*)RDP_TranslateAddr(base_k + off28);
                uint8_t* a24 = (uint8_t*)RDP_TranslateAddr(base_k + off24);
                
                int in28 = (gN64_RDRAM && a28 >= gN64_RDRAM && a28 < gN64_RDRAM + 0x1000000);
                int in24 = (gN64_RDRAM && a24 >= gN64_RDRAM && a24 < gN64_RDRAM + 0x1000000);
                
                if (s_07 < 12) {
                    auto& _t = s_rdp.tiles[s_rdp.activeTile];
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-TILEFMT",
                        "#%d activeTile=%d fmt=%u size=%u palette=%u line=%u sh=%u th=%u tmemAddr=%u",
                        s_07, s_rdp.activeTile, _t.format, _t.size, _t.palette,
                        _t.line, _t.sh, _t.th, _t.tmemAddr);
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-07",
                        "#%d w0=%08X w1=%08X seg=%u base=%08X rddram=%p | 28: off=%07X addr=%p in=%d d=%08X f8=%02X%02X%02X%02X | 24: off=%06X addr=%p in=%d d=%08X f8=%02X%02X%02X%02X",
                        s_07, c.w0, c.w1, seg, base, (void*)gN64_RDRAM,
                        off28, (void*)a28, in28,
                        in28 ? (uint32_t)((uintptr_t)a28 - (uintptr_t)gN64_RDRAM) : 0,
                        in28?a28[0]:0, in28?a28[1]:0, in28?a28[2]:0, in28?a28[3]:0,
                        off24, (void*)a24, in24,
                        in24 ? (uint32_t)((uintptr_t)a24 - (uintptr_t)gN64_RDRAM) : 0,
                        in24?a24[0]:0, in24?a24[1]:0, in24?a24[2]:0, in24?a24[3]:0);
                    s_07++;
                }
                
                /* Choose whichever has non-zero data */
                uint8_t* chosen = NULL;
                if (in24 && (a24[0] | a24[1] | a24[2] | a24[3])) chosen = a24;
                else if (in28 && (a28[0] | a28[1] | a28[2] | a28[3])) chosen = a28;
                else if (in24) chosen = a24;   /* fall back to 24-bit if in bounds */
                else if (in28) chosen = a28;
                
                /* DIAG: log chosen offset + key bytes */
                { static int _cdiag = 0; if (_cdiag++ < 8) {
                    if (chosen && gN64_RDRAM) {
                        uint32_t _coff = (uint32_t)(chosen - gN64_RDRAM);
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-CHOSEN",
                            "off=0x%X a24=%d a28=%d b0=%02X%02X%02X%02X b1792=%02X%02X%02X%02X b32768=%02X%02X%02X%02X",
                            _coff, in24, in28,
                            chosen[0], chosen[1], chosen[2], chosen[3],
                            chosen[1792], chosen[1793], chosen[1794], chosen[1795],
                            chosen[32768], chosen[32769], chosen[32770], chosen[32771]);
                    } else {
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-CHOSEN",
                            "NULL a24=%d a28=%d", in24, in28);
                    }
                } }

                /* Raw RDRAM fallback for failed segment resolution */
                if ((!chosen || (chosen[0]==0 && chosen[1]==0 && chosen[2]==0 && chosen[3]==0))
                    && gN64_RDRAM) {
                    /* PORT: wrap off24 modulo 12MB. DKR sometimes emits
                       offsets > 12MB (0xC8DEA0 etc.) for textures that
                       would be decompressed at runtime. RDRAM only has
                       12MB ROM-copied data, so wrap to reach valid region. */
                    uint32_t wrapped = off24;
                    while (wrapped >= 0xC00000) wrapped -= 0xC00000;
                    if (wrapped + 0x4000 >= 0xC00000) wrapped = 0;
                    uint8_t* raw = gN64_RDRAM + wrapped;
                    if (raw[0] || raw[1] || raw[2] || raw[3]) {
                        chosen = raw;
                        static int _rawfb2 = 0;
                        if (_rawfb2++ < 8)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-RAWRAM",
                                "off24=0x%X b0=%02X%02X%02X%02X",
                                off24, raw[0], raw[1], raw[2], raw[3]);
                    }
                }

                if (chosen) {
                    s_rdp.texAddr = chosen;
                    
                    /* PORT: 0x07 is Rare's SetTextureImage. w0=07070038
                       means width/height = 0x38 (56). Decode it into the
                       active tile descriptor so Cmd_LoadTile copies a real block. */
                    uint32_t size_param = (c.w0 & 0xFF);
                    if (size_param > 0) {
                        /* PORT: 0x07 w0=0707_0038 → bits 16-23 hold the tile index
                           (07 = tile 7 = G_TX_LOADTILE). Prior code hardcoded tile 0
                           but B2 loads into tile 7, so sh/th never reached the copy. */
                        uint32_t tile_idx = (c.w0 >> 16) & 0x7;
                        /* PORT: set CI4 format on ALL tiles so whichever
                           render tile TEXRECT uses finds format=2 (palette lookup)
                           instead of falling into the I4 grayscale branch. */
                        for (int _i = 0; _i < 8; _i++) {
                            auto& _tile = s_rdp.tiles[_i];
                            _tile.sh = (size_param << 2) - 4;
                            _tile.th = (size_param << 2) - 4;
                            _tile.size = 0;      // 4bpp CI4
                            _tile.format = 2;    // CI (Color Indexed)
                            _tile.tmemAddr = 0;  // bank 0
                            _tile.palette = 0;
                            uint32_t _lb = (size_param * 4 + 7) / 8;
                            uint32_t _lw = (_lb + 7) / 8;
                            if (_lw == 0) _lw = 1;
                            _tile.line = _lw;
                            _tile.sl = 0;
                            _tile.tl = 0;
                        }
                        s_rdp.texWidth = size_param;

                        /* Load the 16-color palette from RDRAM to TMEM byte 2048.
                           Rare stores the CI4 palette immediately after the texture data. */
                        /* PORT: Auto-detect palette (padded 1792, packed 1568, or aligned 2048) */
                        uint32_t _lw = (size_param * 4 + 63) / 64;
                        uint32_t padded_bytes = _lw * 8 * size_param;
                        uint32_t packed_bytes = (size_param * size_param) / 2;
                        
                        uint8_t* pal_src = chosen + padded_bytes;
                        int nz = 0;
                        for (int i=0; i<32; i++) if (pal_src[i]) nz++;
                        if (nz == 0) {
                            pal_src = chosen + packed_bytes;
                            nz = 0;
                            for (int i=0; i<32; i++) if (pal_src[i]) nz++;
                        }
                        if (nz == 0) pal_src = chosen + 2048; // try aligned
                        if (bka_is_readable(pal_src)) {
                            /* 16 colors * 2 bytes (RGBA5551) = 32 bytes */
                            memcpy(s_rdp.tmem + 2048, pal_src, 32);

                        /* DIAG: dump 5 candidate palette locations */
                        static int _paldiag = 0;
                        if (_paldiag++ < 2) {
                            /* File dump for post-mortem */
                            FILE* pf = fopen("/data/data/com.dkrwrapper/files/paldiag.txt", "a");
                            if (pf) {
                                fprintf(pf, "=== PALDIAG call %d ===\n", _paldiag);
                                fprintf(pf, "TEXEL0: ");
                                for (int i = 0; i < 32; i++) fprintf(pf, "%02X", chosen[i]);
                                fprintf(pf, "\n+1568: ");
                                for (int i = 0; i < 32; i++) fprintf(pf, "%02X", chosen[1568+i]);
                                fprintf(pf, "\n+1792: ");
                                for (int i = 0; i < 32; i++) fprintf(pf, "%02X", chosen[1792+i]);
                                fprintf(pf, "\n-32:   ");
                                for (int i = 0; i < 32; i++) fprintf(pf, "%02X", chosen[-32+i]);
                                fprintf(pf, "\n-64:   ");
                                for (int i = 0; i < 32; i++) fprintf(pf, "%02X", chosen[-64+i]);
                                fprintf(pf, "\n\n");
                                fclose(pf);
                            }
                            uint8_t* _c = chosen;
                            uint8_t* _p1568 = chosen + 1568;
                            uint8_t* _p1792 = chosen + 1792;
                            uint8_t* _pm32  = chosen - 32;
                            uint8_t* _pm64  = chosen - 64;
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-PALDIAG", "TEXEL_0   @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", _c, _c[0],_c[1],_c[2],_c[3],_c[4],_c[5],_c[6],_c[7], _c[8],_c[9],_c[10],_c[11],_c[12],_c[13],_c[14],_c[15]);
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-PALDIAG", "PLUS1568  @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", _p1568, _p1568[0],_p1568[1],_p1568[2],_p1568[3],_p1568[4],_p1568[5],_p1568[6],_p1568[7], _p1568[8],_p1568[9],_p1568[10],_p1568[11],_p1568[12],_p1568[13],_p1568[14],_p1568[15]);
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-PALDIAG", "PLUS1792  @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", _p1792, _p1792[0],_p1792[1],_p1792[2],_p1792[3],_p1792[4],_p1792[5],_p1792[6],_p1792[7], _p1792[8],_p1792[9],_p1792[10],_p1792[11],_p1792[12],_p1792[13],_p1792[14],_p1792[15]);
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-PALDIAG", "MINUS32   @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", _pm32, _pm32[0],_pm32[1],_pm32[2],_pm32[3],_pm32[4],_pm32[5],_pm32[6],_pm32[7], _pm32[8],_pm32[9],_pm32[10],_pm32[11],_pm32[12],_pm32[13],_pm32[14],_pm32[15]);
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-PALDIAG", "MINUS64   @%p: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", _pm64, _pm64[0],_pm64[1],_pm64[2],_pm64[3],_pm64[4],_pm64[5],_pm64[6],_pm64[7], _pm64[8],_pm64[9],_pm64[10],_pm64[11],_pm64[12],_pm64[13],_pm64[14],_pm64[15]);
                        }
                            /* palette=0 already set on all tiles above */
                        }
                    }
                    if (s_rdp.texSize == 0) s_rdp.texSize = 1;
                }
                break;
            }
            case 0xB2: Cmd_LoadTile(c); break;   /* F3DEX1 quick LoadTile */
            case 0xB3: Cmd_SetTile(c); break;    /* F3DEX1 quick SetTile */
            case 0xE4: case 0xE5: Cmd_TexRect(c); break;

            case 0x01: Cmd_Mtx(c); break;
            case 0x04:
                {
                    /* Guard: a misdecoded byte can produce a "G_VTX" whose w1
                     * is a flag/length field (top 16 bits all set) rather than
                     * a real segment address. Real N64 addresses never start
                     * with 0xFF. Count these as unknown opcodes so the walker
                     * bails instead of looping on garbage. */
                    if (c.w1 >= 0xF0000000u) {
                        /* Skip silently — the top-of-loop encoding flip handles misreads. */
                        if (unknown_opcode_run >= 16) {
                            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                "walker: spurious G_VTX addr=0x%08X at cur=%p depth=%d — bailing",
                                c.w1, (void*)cur, depth);
                            return;
                        }
                        break;
                    }
                    {
                        static int s_ctx = 0;
                        if (s_ctx++ < 0) {
                            uint8_t* p = (uint8_t*)cur;
                            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                "GVTX-CTX cur=%p\n"
                                "  +0 : %02X%02X%02X%02X %02X%02X%02X%02X\n"
                                "  +8 : %02X%02X%02X%02X %02X%02X%02X%02X\n"
                                "  +16: %02X%02X%02X%02X %02X%02X%02X%02X\n"
                                "  +24: %02X%02X%02X%02X %02X%02X%02X%02X",
                                (void*)cur,
                                p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
                                p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15],
                                p[16],p[17],p[18],p[19],p[20],p[21],p[22],p[23],
                                p[24],p[25],p[26],p[27],p[28],p[29],p[30],p[31]);
                        }
                    }
                    static int vtx_log_count = 0;
                    if (++vtx_log_count <= 3) {
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "G_VTX@%p w0=0x%08X w1=0x%08X next8=%02X%02X%02X%02X %02X%02X%02X%02X",
                            cur, c.w0, c.w1,
                            cur[8],cur[9],cur[10],cur[11],
                            cur[12],cur[13],cur[14],cur[15]);
                        /* SYNC disabled */ (void)0; __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "SYNC cur=%p bytes_cur=%02X%02X%02X%02X %02X%02X%02X%02X "
                            "LE_w0=0x%08X LE_w1=0x%08X BE_w0=0x%08X BE_w1=0x%08X "
                            "c.w0=0x%08X c.w1=0x%08X",
                            cur,
                            cur[0],cur[1],cur[2],cur[3],cur[4],cur[5],cur[6],cur[7],
                            (unsigned)(cur[0] | (cur[1]<<8) | (cur[2]<<16) | (cur[3]<<24)),
                            (unsigned)(cur[4] | (cur[5]<<8) | (cur[6]<<16) | (cur[7]<<24)),
                            (unsigned)((cur[0]<<24) | (cur[1]<<16) | (cur[2]<<8) | cur[3]),
                            (unsigned)((cur[4]<<24) | (cur[5]<<16) | (cur[6]<<8) | cur[7]),
                            c.w0, c.w1);
                    }
                    if (c.w1 >= 0x01000000) {
                        static int s_raw = 0;
                        if (s_raw++ < 6) {
                            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                "VTXRAW32 @%p = %02X%02X%02X%02X %02X%02X%02X%02X | %02X%02X%02X%02X %02X%02X%02X%02X | prev8: %02X%02X%02X%02X %02X%02X%02X%02X",
                                cur,
                                cur[0],cur[1],cur[2],cur[3],cur[4],cur[5],cur[6],cur[7],
                                cur[8],cur[9],cur[10],cur[11],cur[12],cur[13],cur[14],cur[15],
                                cur[-8],cur[-7],cur[-6],cur[-5],cur[-4],cur[-3],cur[-2],cur[-1]);
                            // Scan RDRAM for LE-encoded 0xFFF9153F = bytes 3F 15 F9 FF.
                            if (gN64_RDRAM) {
                                const uint8_t pat[4] = {0x3F, 0x15, 0xF9, 0xFF};
                                int found = 0;
                                for (uint32_t off = 0; off + 4 <= 0x04000000u && found < 8; off += 4) {
                                    if (gN64_RDRAM[off]     == pat[0] &&
                                        gN64_RDRAM[off + 1] == pat[1] &&
                                        gN64_RDRAM[off + 2] == pat[2] &&
                                        gN64_RDRAM[off + 3] == pat[3]) {
                                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                            "PATFIND fff9153f @ rdram+0x%06X", off);
                                        found++;
                                    }
                                }
                                static int s_pf = 0;
                                if (found == 0 && s_pf++ < 2) {
                                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                        "PATFIND fff9153f: not present in RDRAM");
                                }
                            }
                            // Dump the 40 preceding 8-byte commands in the DL containing this G_VTX.
                            {
                                static int s_dumpprev = 0;
                                if (s_dumpprev++ < 2) {
                                    uint8_t* base = cur - 40 * 8;
                                    for (int k = 0; k < 40; k++) {
                                        uint8_t* e = base + k * 8;
                                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                            "DLPREV[%02d] %02X%02X%02X%02X %02X%02X%02X%02X",
                                            k,
                                            e[0], e[1], e[2], e[3],
                                            e[4], e[5], e[6], e[7]);
                                    }
                                }
                            }
                        }
                    }
                }
                Cmd_Vtx(c);
                break;
            case 0xBF:
            case 0xB1:
            case 0xC4:
            case 0x34:
                LOGV(
                    "DISPATCH TRI opcode=0x%02X", opcode);
                {
                    static int tri_log_count = 0;
                    if (++tri_log_count <= 10) {
                        __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
                            "GEOMETRY: op=0x%02X (tri) w0=0x%08X w1=0x%08X total=%zu depth=%d",
                            opcode, c.w0, c.w1, total-1, depth);
                    }
                }
                if (opcode == 0xBF) Cmd_Tri1(c);
                else if (opcode == 0xB1) Cmd_Tri2(c);
                else if (opcode == 0xC4) Cmd_Tri2_F3DEX2(c);
                else if (opcode == 0x34) Cmd_Tri1_F3DEX2(c);
                break;
            case 0x03: Cmd_MoveMem(c); break;
            case 0xDB: Cmd_MoveWord(c); break;
            case 0xBC: Cmd_MoveWord(c); break;

            case 0x06: {
                /* G_DL target validation:
                 * drift can send G_DL to any RDRAM offset; verify
                 * target starts with a valid DKR opcode before jumping */
                {
                    extern int bka_is_readable(void*);
                    extern uint8_t *gN64_RDRAM;
                    /* Prefer physical (RDP) translation for G_DL — registry
                     * has stale entries (no TTL) and returns freed-heap
                     * pointers for w1 values that happened to collide with
                     * some matrix ptr from an earlier frame. */
                    void *_gdl_via_rdp = RDP_TranslateAddr(c.w1);
                    void *_gdl_t = _gdl_via_rdp;
                    /* Only fall back to registry if RDP failed, AND the
                     * registry target is inside RDRAM (registered matrices
                     * live in RDRAM; arbitrary heap is not a valid G_DL). */
                    if (!_gdl_t || !bka_is_readable(_gdl_t) ||
                        !bka_is_readable((void*)((const uint8_t*)_gdl_t + 15))) {
                        void *_reg = bka_lookup_addr_by_low32(c.w1);
                        if (_reg && gN64_RDRAM &&
                            (const uint8_t*)_reg >= gN64_RDRAM &&
                            (const uint8_t*)_reg <  gN64_RDRAM + 0x1000000 &&
                            bka_is_readable(_reg) &&
                            bka_is_readable((void*)((const uint8_t*)_reg + 15))) {
                            _gdl_t = _reg;
                        }
                    }
                    if (!_gdl_t || !bka_is_readable(_gdl_t) ||
                        !bka_is_readable((void*)((const uint8_t*)_gdl_t + 15))) {
                        static int _gd1 = 0;
                        if (_gd1++ < 40)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-GDL-OOB",
                                "w0=%08X w1=%08X rdp=%p unreadable after fallback — skipping",
                                c.w0, c.w1, _gdl_via_rdp);
                        break;
                    }
                    /* Structural check: first opcode must be a known DKR one.
                     * Rejects stale-heap targets that pass readability but
                     * contain garbage. */
                    {
                        const uint8_t *_b = (const uint8_t*)_gdl_t;
                        uint8_t _op_le = _b[3];  /* LE opcode */
                        uint8_t _op_be = _b[0];  /* BE opcode */
                        /* Valid DKR opcodes (union of LE + BE cases) */
                        #define BKA_IS_OPCODE(x) ( \
                            (x)==0x00||(x)==0x01||(x)==0x02||(x)==0x03|| \
                            (x)==0x04||(x)==0x05||(x)==0x06||(x)==0x07|| \
                            (x)==0x0C||(x)==0x0D||(x)==0x12||(x)==0x14|| \
                            (x)==0x15||(x)==0x27||(x)==0x28||(x)==0x34|| \
                            (x)==0x3A||(x)==0x3C||(x)==0x41||(x)==0x44|| \
                            (x)==0x4C||(x)==0x50||(x)==0x51||(x)==0x58|| \
                            (x)==0x5E||(x)==0x60||(x)==0x70||(x)==0x80|| \
                            (x)==0x82||(x)==0x86||(x)==0x88||(x)==0x8C|| \
                            (x)==0x8E||(x)==0x90||(x)==0x94||(x)==0xA0|| \
                            (x)==0xA6||(x)==0xA8||(x)==0xAA||(x)==0xAF|| \
                            (x)==0xB1||(x)==0xB4||(x)==0xB5||(x)==0xB6|| \
                            (x)==0xB7||(x)==0xB8||(x)==0xB9||(x)==0xBA|| \
                            (x)==0xBB||(x)==0xBC||(x)==0xBD||(x)==0xBE|| \
                            (x)==0xBF||(x)==0xC0||(x)==0xC4||(x)==0xD6|| \
                            (x)==0xD7||(x)==0xDA||(x)==0xDF||(x)==0xE0|| \
                            (x)==0xE1||(x)==0xE2||(x)==0xE3||(x)==0xE6|| \
                            (x)==0xE7||(x)==0xE8||(x)==0xE9||(x)==0xED|| \
                            (x)==0xF0||(x)==0xF1||(x)==0xF2||(x)==0xF3|| \
                            (x)==0xF4||(x)==0xF5||(x)==0xF6||(x)==0xF7|| \
                            (x)==0xF8||(x)==0xF9||(x)==0xFA||(x)==0xFB|| \
                            (x)==0xFC||(x)==0xFD||(x)==0xFE||(x)==0xFF)
                        if (!BKA_IS_OPCODE(_op_le) && !BKA_IS_OPCODE(_op_be)) {
                            static int _gd3 = 0;
                            if (_gd3++ < 40)
                                __android_log_print(ANDROID_LOG_ERROR, "BKA-GDL-BAD",
                                    "w1=%08X target=%p op_le=0x%02X op_be=0x%02X not valid — skipping",
                                    c.w1, _gdl_t, _op_le, _op_be);
                            break;
                        }
                        #undef BKA_IS_OPCODE
                    }
                    /* Trust readable targets. Log what we're entering so we can
                     * see why POLYPASS is 0 — either these sub-DLs have no
                     * polygons, or registry collision gives wrong host ptr. */
                    {
                        const uint8_t *_b = (const uint8_t*)_gdl_t;
                        uint8_t _op = _b[3];
                        static int _gdok = 0;
                        if (_gdok++ < 60)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-GDL-ENTER",
                                "w0=%08X w1=%08X target=%p op=0x%02X "
                                "b0..15=%02X%02X%02X%02X %02X%02X%02X%02X "
                                "%02X%02X%02X%02X %02X%02X%02X%02X",
                                c.w0, c.w1, _gdl_t, _op,
                                _b[0],_b[1],_b[2],_b[3],
                                _b[4],_b[5],_b[6],_b[7],
                                _b[8],_b[9],_b[10],_b[11],
                                _b[12],_b[13],_b[14],_b[15]);
                    }
                    /* fall through to actual entry (removes the whitelist) */
                }

                uint32_t raw_addr = c.w1;
                /* Port recomp relocates pointer-carrying commands. The 8-byte
                 * tag following the command holds the physical target; the
                 * command word w1 is left as 0. Read the tag BE and use it. */
                if (raw_addr == 0) {
                    uint32_t tag_be = ((uint32_t)cur[8] << 24) |
                                      ((uint32_t)cur[9] << 16) |
                                      ((uint32_t)cur[10] << 8) |
                                       (uint32_t)cur[11];
                    if (tag_be != 0) {
                        static int s_tag_use = 0;
                        if (s_tag_use++ < 20)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                "G_DL w1=0, using tag 0x%08X (cur=%p)",
                                tag_be, (void*)cur);
                        raw_addr = tag_be;
                    }
                }
                /* ARM64 port: reject sentinel G_DL targets.  0xFFFFFFFF,
                 * 0x80000000, and 0xFFFF0000+ are flag/padding values the
                 * resolver maps to a zero-filled block; the walker then
                 * spins 700K+ zero-NOPs per task (5M+ opcodes overall). */
                if (raw_addr == 0xFFFFFFFFu || raw_addr == 0x80000000u ||
                    raw_addr >= 0xFFFF0000u) {
                    static int s_sent = 0;
                    if (s_sent++ < 20)
                        __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                            "G_DL: refusing sentinel target=0x%08X cur=%p depth=%d",
                            raw_addr, (void*)cur, depth);
                    consecutive_bad_gdl++;
                    break;
                }
                /* STRICT G_DL resolution: real G_DL targets are either
                 * registered in the address map, or inside RDRAM.  Any other
                 * target (e.g. an arbitrary heap page that happens to be
                 * mapped) means the walker has drifted into non-DL memory. */
                /* ARM64 port 2026-10-15: resolve G_DL targets through segment table
                   first. The render-mode G_DLs (0x03xxxxxx) target
                   gSPSegment(3, ptr); the segment base stores a truncated
                   low32 host pointer, so bka_lookup_addr_mapping(raw_addr)
                   cannot find it. Combine with the segment base and look
                   up the full pointer. Same fix pattern as Cmd_SetTImg. */
                void *dl_ptr = NULL;
                {
                    uint8_t seg = (raw_addr >> 24) & 0x0F;
                    if (seg < 8 && s_rdp.segmentBase[seg] != 0) {
                        uint32_t combined = (uint32_t)(s_rdp.segmentBase[seg] + (raw_addr & 0x00FFFFFF));
                        dl_ptr = bka_lookup_addr_by_low32(combined);
                        if (!dl_ptr) {
                            uint8_t* tmp = RDP_TranslateAddr(combined);
                            if (tmp) {
                                uintptr_t p  = (uintptr_t)tmp;
                                uintptr_t r0 = (uintptr_t)gN64_RDRAM;
                                uintptr_t r1 = r0 + 0x04000000;
                                if (p >= r0 && p < r1) dl_ptr = tmp;
                            }
                        }
                    }
                }
                if (!dl_ptr) dl_ptr = bka_lookup_addr_mapping(raw_addr);
                if (!dl_ptr) { dl_ptr = bka_lookup_addr_by_low32(raw_addr); }
                if (!dl_ptr) {
                    uint8_t* tmp = RDP_TranslateAddr(raw_addr);
                    if (tmp) {
                        uintptr_t p  = (uintptr_t)tmp;
                        uintptr_t r0 = (uintptr_t)gN64_RDRAM;
                        uintptr_t r1 = r0 + 0x04000000;
                        if (p >= r0 && p < r1) dl_ptr = tmp;
                    }
                }

                static int debug_gdl_count = 0;
                if (++debug_gdl_count <= 20) {
                    void* map_result = bka_lookup_addr_mapping(raw_addr);
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "G_DL DEBUG: raw=0x%08X map_lookup=%p translate=%p",
                        raw_addr, map_result, dl_ptr);
                    if (dl_ptr && bka_is_mapped(dl_ptr)) {
                        uint8_t* dump = (uint8_t*)dl_ptr;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "G_DL DL bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                            dump[0], dump[1], dump[2], dump[3], dump[4], dump[5], dump[6], dump[7],
                            dump[8], dump[9], dump[10], dump[11], dump[12], dump[13], dump[14], dump[15]);
                    } else if (dl_ptr) {
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "G_DL DL ptr %p is not in a mapped region", dl_ptr);
                    }
                }
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "G_DL RECOGNIZED cur=%p w0=0x%08X w1=0x%08X", cur, c.w0, c.w1);
                { static int s_stage1 = 0; if (s_stage1++ < 10) LOGV("G_DL S1 dl_ptr=%p", dl_ptr); }
                static int dl_log_count = 0;
                if (++dl_log_count <= 20) {
                    __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
                        "G_DL: addr=0x%08X resolved=%p", raw_addr, dl_ptr);
                }
                if (!dl_ptr || !bka_is_mapped(dl_ptr)) {
                    static int s_nomap = 0;
                    if (s_nomap++ < 50) {
                        void* m = bka_lookup_addr_mapping(raw_addr);
                        __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                            "G_DL: refusing addr=0x%08X (dl_ptr=%p map_lookup=%p) tag=%02X%02X%02X%02X %02X%02X%02X%02X w0=%08X w1=%08X off=%ld",
                            raw_addr, dl_ptr, m,
                            cur[8], cur[9], cur[10], cur[11],
                            cur[12], cur[13], cur[14], cur[15],
                            c.w0, c.w1,
                            (long)(cur - (uint8_t*)tp->t.data_ptr));
                    }
                    {
                        const uint8_t* p = (const uint8_t*)cur;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-DLRAW",
                            "refused target=%08X cur=%p prev8=%02X%02X%02X%02X %02X%02X%02X%02X next16=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                            raw_addr, (void*)cur,
                            p[-8],p[-7],p[-6],p[-5],p[-4],p[-3],p[-2],p[-1],
                            p[0],p[1],p[2],p[3], p[4],p[5],p[6],p[7],
                            p[8],p[9],p[10],p[11], p[12],p[13],p[14],p[15]);
                    }
                    if (++consecutive_bad_gdl > 8) {
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "walker: %d consecutive failed G_DLs — bailing (drift past DL end)",
                            consecutive_bad_gdl);
                        return;
                    }
                    break;
                }
                consecutive_bad_gdl = 0;
                // NOTE: Do not skip lists with zero first words. They are not truly empty;
                // the address resolver may see zeros due to segment mapping.

                // Loop detection: refuse only if the target is currently ON the
                // active stack (a genuine recursion).  Re-entering a DL after its
                // G_ENDDL has popped it is legitimate.
                bool already_on_stack = false;
                for (int i = 0; i < depth; i++) {
                    if (stack[i].ptr && (uintptr_t)stack[i].ptr == (uintptr_t)dl_ptr) {
                        already_on_stack = true;
                        break;
                    }
                }
                if (already_on_stack) {
                    __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",
                        "G_DL recursion detected at addr=0x%08X, breaking", raw_addr);
                    break;
                }
                { static int s_stage2 = 0; if (s_stage2++ < 10) LOGV("G_DL S2 depth=%d dl_ptr=%p", depth, dl_ptr); }
                if (++gdl_jumps > MAX_GDL_JUMPS) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "walker: %zu G_DL jumps in one task — bailing (DL tree too deep, "
                        "drifted past real content)", gdl_jumps);
                    return;
                }
                static int dl_jump_log_count = 0;
                {
                    static int s_jump_dump = 0;
                    if ((s_jump_dump++ % 40) == 0) {
                        uint8_t* b = (uint8_t*)dl_ptr;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "G_DL JUMP depth=%d target=0x%08X first32=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                            depth, raw_addr,
                            b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
                            b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15],
                            b[16],b[17],b[18],b[19],b[20],b[21],b[22],b[23],
                            b[24],b[25],b[26],b[27],b[28],b[29],b[30],b[31]);
                    }
                }
                if (depth == 0) s_seen_gdl_at_depth0 = true;   /* Fix Q */
                s_dl_base = (uintptr_t)dl_ptr;
                /* disabled: { static int s_w = 0; if (s_w++ < 3) { bka_install_watch(); mprotect((void*)((uintptr_t)dl_ptr & ~0xFFFULL), 0x1000, PROT_READ); } } */
                if (++dl_jump_log_count <= 50) {
                    __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
                        "G_DL JUMP to addr=0x%08X resolved=%p cur_end=%p depth=%d",
                        raw_addr, dl_ptr, cur_end, depth);
                }
                // Diagnostic: dump first 16 bytes at resolved pointer
                if (dl_jump_log_count <= 3) {
                    uint8_t* dump = (uint8_t*)dl_ptr;
                    __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
                        "DL bytes at %p: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                        dl_ptr, dump[0], dump[1], dump[2], dump[3], dump[4], dump[5], dump[6], dump[7],
                        dump[8], dump[9], dump[10], dump[11], dump[12], dump[13], dump[14], dump[15]);
                }
                if (depth >= 16) {   /* ARM64 port: was 63; real B-K DL trees are 4-8 deep */
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "G_DL: max depth 16 reached at cur=%p target=0x%08X",
                        (void*)cur, raw_addr);
                    consecutive_bad_gdl++;
                    break;
                }

                // Save the OUTER DL's position/end BEFORE we overwrite cur_end
                // with the nested DL's end.
                stack[depth].ptr = cur;
                stack[depth].end = cur_end;      // outer cur_end preserved
                stack_stride[depth] = current_stride;
                stack_dl_base[depth] = s_dl_base;
                stack_enc[depth] = cur_dl_enc;
                cur_dl_enc = 0;  /* re-probe encoding on sub-DL: outer DL is LE (port-emitted), sub-DLs copied from ROM asset cache are BE */
                { static int s_jt = 0; if (s_jt++ < 10) { uint8_t* p = (uint8_t*)dl_ptr; __android_log_print(ANDROID_LOG_ERROR, "BKA-DLJMP", "jump raw=0x%08X host=%p parent_cur=%p parent_enc=%d bytes=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X", raw_addr, (void*)dl_ptr, (void*)cur, (int)stack_enc[depth], p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]); } }
                depth++;
                if (depth > g_bka_task_max_depth) g_bka_task_max_depth = depth;
                /* Inherit parent encoding — Banjo's sub-DLs are almost always
                 * same-encoding as their parent.  Resetting to 0 causes
                 * ambiguous-first-command DLs (e.g. a leading G_SETCOMBINE
                 * bytes 'FC 62 FE 04' which decode as valid G_VTX in LE) to be
                 * misdetected as LE, producing phantom seg-1 vertex loads. */
                {
                    uint32_t _w0 = *(uint32_t*)dl_ptr;

                    uint32_t _w4 = *((uint32_t*)dl_ptr + 1);

                    if (_w0 == 0 && _w4 == 0) {

                        static int s_dead = 0;

                        if (s_dead++ < 5) __android_log_print(ANDROID_LOG_WARN, "BKA_GFX",

                            "G_DL SKIP: dead target 0x%08X (first 8 bytes zero)", raw_addr);

                        break;

                    }

                    int probed = bka_probe_dl_encoding((uint8_t*)dl_ptr);
                    int parent_enc = (depth > 0) ? stack_enc[depth - 1] : 0;
                    if (probed != 0) {
                        /* Trust the probe when it returns an answer.  Outer
                         * DLs are LE (port-emitted); sub-DLs copied from
                         * the ROM asset cache are BE (handoff §7).  The
                         * old parent-first branch forced LE on every
                         * sub-DL and the walker crashed ~8 commands in. */
                        cur_dl_enc = probed;
                    } else if (parent_enc != 0) {
                        cur_dl_enc = parent_enc;
                    } else {
                        static int s_refuse = 0;
                        if (s_refuse++ < 20)
                            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                "G_DL REFUSE non-DL target=0x%08X parent_enc=%d cur=%p",
                                raw_addr, parent_enc, (void*)cur);
                        depth--;
                        break;
                    }
                    static int s_enc_probe = 0;
                    if (s_enc_probe++ < 40) {
                        uint8_t* b = (uint8_t*)dl_ptr;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                            "G_DL ENC-PROBE target=0x%08X bytes=%02X%02X%02X%02X chosen=%d parent=%d",
                            raw_addr, b[0],b[1],b[2],b[3], cur_dl_enc, stack_enc[depth - 1]);
                    }
                    {
                        static int s_probe_detail = 0;
                        if (s_probe_detail++ < 60) {
                            uint8_t* b = (uint8_t*)dl_ptr;
                            char hb[3*32 + 4]; int n = 0;
                            for (int k = 0; k < 32; k++)
                                n += snprintf(hb+n, sizeof(hb)-n, " %02X", b[k]);
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-PROBE",
                                "GDL target=0x%08X probed=%d parent=%d chosen=%d bytes%s",
                                raw_addr, probed, parent_enc, cur_dl_enc, hb);
                        }
                    }
                }

                // Now safe to update cur_end for the nested DL.
                // Cap to 16 KB past the entry point — real sub-DLs are
                // bounded and walkers that span megabytes are drifting
                // through unrelated memory.
                {
                    uintptr_t hard_cap = (uintptr_t)dl_ptr + 0x4000;
                    uintptr_t mapped_end = bka_get_mapped_end(dl_ptr);
                    if (mapped_end != 0 && mapped_end < hard_cap &&
                        mapped_end > (uintptr_t)dl_ptr) {
                        cur_end = (uint8_t*)mapped_end;
                    } else {
                        cur_end = (uint8_t*)hard_cap;
                    }
                }

                cur = (uint8_t*)dl_ptr;
                /* Fix L: per-sub-DL stride detection via multi-slot sampler. */
                {
                    const uint8_t* _p = (const uint8_t*)dl_ptr;
                    current_stride = 8;   /* Fix Y2: sub-DL is 8-byte packed, no payload */
                    static int s_jlog = 0;
                    if (s_jlog++ < 40) {
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-STRIDE",
                            "GDL target=0x%08X b8_15=[%08X %08X] b24_31=[%08X %08X] b40_47=[%08X %08X] -> stride=%zu",
                            raw_addr,
                            *(const uint32_t*)(_p+8),  *(const uint32_t*)(_p+12),
                            *(const uint32_t*)(_p+24), *(const uint32_t*)(_p+28),
                            *(const uint32_t*)(_p+40), *(const uint32_t*)(_p+44),
                            current_stride);
                    }
                }
                /* Fix M: refuse targets that don't score as a DL.  Prevents
                 * the walker from entering vertex/palette/pointer tables
                 * that happen to sit behind a G_DL word in the recomp buffer. */
                {
                    int score = bka_looks_like_dl((const uint8_t*)dl_ptr, (int)current_stride);
                    {
                        static int s_scorelog = 0;
                        if (s_scorelog++ < 60) {
                            const uint8_t* _p = (const uint8_t*)dl_ptr;
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-STRIDE",
                                "GDL caller=%p target=0x%08X stride=%zu score=%d/8 first16=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                                (void*)cur, raw_addr, current_stride, score,
                                _p[0],_p[1],_p[2],_p[3],_p[4],_p[5],_p[6],_p[7],
                                _p[8],_p[9],_p[10],_p[11],_p[12],_p[13],_p[14],_p[15]);
                        }
                    }
                    /* ARM64 port 2026-10-14: segment 3 is the render-mode table
                       (see modelRender.c:1499-1525). Its top entries are
                       SetOtherMode commands that bka_looks_like_dl scores low,
                       so G_DLs targeting 03xxxxxx were being refused, and the
                       combine mode was never applied. Whitelist segment-3
                       targets -- nothing else in this port writes seg 3. */
                    bool seg3_dl = ((raw_addr >> 24) & 0x0F) == 0x03;
                    if (score < 7 && !seg3_dl) {
                        static int s_refuse = 0;
                        if (s_refuse++ < 30) {
                            const uint8_t* _p = (const uint8_t*)dl_ptr;
                            __android_log_print(ANDROID_LOG_ERROR, "BKA-STRIDE",
                                "GDL target=0x%08X REFUSED as data (score=%d/8) first16=%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                                raw_addr, score,
                                _p[0],_p[1],_p[2],_p[3],_p[4],_p[5],_p[6],_p[7],
                                _p[8],_p[9],_p[10],_p[11],_p[12],_p[13],_p[14],_p[15]);
                        }
                        if (++consecutive_bad_gdl > 8) {
                            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                                "walker: %d consecutive refused G_DLs -- bailing",
                                consecutive_bad_gdl);
                            return;
                        }
                        break;
                    }
                }
                // Use a safe upper bound based on MAX_DL_CMDS to avoid
                // running off into non-DL data if this nested list lacks ENDDL.
                // Set cur_end to the actual mapped region end, not a huge upper bound.
                uintptr_t nested_end = bka_get_mapped_end(dl_ptr);
                if (nested_end > (uintptr_t)dl_ptr) {
                    cur_end = (uint8_t*)nested_end;
                } else {
                    cur_end = cur + (MAX_DL_CMDS * current_stride);
                }
                dl_cmds = 0;
                zero_run = 0;
                log_after_jump = true;
                jump_log_count = 0;

                break;
            }

            case 0xB8: // F3DEX G_ENDDL - end of display list
                LOGV(
                    "G_ENDDL before pop: depth=%d cur=%p cur_end=%p",
                    depth, cur, cur_end);
                if (depth > 0) {
                    depth--;
                    cur = stack[depth].ptr;
                    cur_end = stack[depth].end;
                    current_stride = stack_stride[depth];
                        s_dl_base = stack_dl_base[depth];
                    cur_dl_enc = stack_enc[depth];
                    unknown_opcode_run = 0;   /* ARM64 port 2026-10-12: reset counter on clean pop */
                    dl_cmds = 0;
                    zero_run = 0;
                    LOGV(
                        "G_ENDDL after pop: depth=%d cur=%p cur_end=%p",
                        depth, cur, cur_end);
                } else {
                    /* Top-level ENDDL — this RSP task's DL is finished. */
                    LOGV("G_ENDDL top-level, vertices=%d",
                         s_rdp.dmemVertexCount);
                    return;
                }
                break;

            case 0xDF:
                if (depth > 0) {
                    depth--;
                    cur = stack[depth].ptr;
                    cur_end = stack[depth].end;
                    current_stride = stack_stride[depth];
                        s_dl_base = stack_dl_base[depth];
                    cur_dl_enc = stack_enc[depth];
                    dl_cmds = 0;
                    zero_run = 0;
                } else {
                    __android_log_print(ANDROID_LOG_INFO, "BKA_GFX",
                        "ENDDL top-level, vertices=%d", s_rdp.dmemVertexCount);
                    return;
                }
                break;

            case 0x14:
            case 0x1C:

            case 0x40:
                Cmd_LoadTLUT(c);
                break;

            case 0x4E: // Unknown but appears frequently; treat as no-op for now
                break;

            case 0xA4:
            case 0xB0:
                Cmd_LoadBlock(c);
                break;

            case 0x98:
            case 0xA1:
                Cmd_SetTImg(c);
                break;

            case 0x84:
                Cmd_SetOtherModeL(c);
                break;

            case 0x60:
                Cmd_SetOtherModeH(c);
                break;
case 0xDC:
                Cmd_MoveMem(c);
                break;
            case 0x16: // G_TEXRECTFLIP - textured rectangle flip
            case 0x1D: // G_TEXRECT - textured rectangle
                Cmd_TexRect(c);
                break;
            case 0x30: // G_LOADUCODE - load microcode (no-op for software RDP)
            case 0x39: // G_SETTILESIZE - set tile size (handled by 0xF2)
            case 0x3A: // G_LOADBLOCK - load texture block (handled by 0xF3)
            case 0x41: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0x45: // G_SETTILE - set tile (handled by 0xF5)
            case 0x50: // G_SETSCISSOR - set scissor box (can be no-op)
            case 0x57: // G_SETOTHERMODE_L - set other mode L (handled by 0xE2)
            case 0x63: // G_SETCOMBINE - set combine mode (handled by 0xFC)
            case 0x65: // G_SETOTHERMODE_H - set other mode H (handled by 0xE3)
            case 0x6B: // G_SETTILESIZE - set tile size (handled by 0xF2)
            case 0x71: // G_SETTILE - set tile (handled by 0xF5)
            case 0x72: // G_SETTILE - set tile (handled by 0xF5)
            case 0x7D: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0x80: // G_RDPHALF_1 - RDP half command
            case 0x81: // G_RDPHALF_2 - RDP half command
            case 0x8C: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0x8D: // G_LOADBLOCK - load texture block (handled by 0xF3)
            case 0x91: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0x95: // G_SETTILESIZE - set tile size (handled by 0xF2)
            case 0x99: // G_SETOTHERMODE_L - set other mode L (handled by 0xE2)
            case 0xA6: // G_LOADTLUT - load texture lookup table (handled by 0x40)
            case 0xA7: // G_SETTILE - set tile (handled by 0xF5)
            case 0xA9: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0xAD: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0xB4: // G_LOADBLOCK - load texture block (handled by 0xF3)
            case 0xC6: // G_SETTILESIZE - set tile size (handled by 0xF2)
            case 0xC8: // G_SETTILE - set tile (handled by 0xF5)
            case 0xCB: // G_RDPHALF_1 - RDP half command
            case 0xCC: // G_LOADBLOCK - load texture block (handled by 0xF3)
            case 0xDE: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0xF8: // G_SETTILESIZE - set tile size (handled by 0xF2)
            case 0xF9: // G_SETTILE - set tile (handled by 0xF5)
                break;
            case 0x12: // G_LOADUCODE - load microcode (no-op)
            case 0x20: // G_RDPHALF_1 - RDP half command
            case 0x21: // G_RDPHALF_2 - RDP half command
            case 0x36: // G_SETTILESIZE - set tile size (handled by 0xF2)
            case 0x0A: // G_SETTIMG - set texture image (handled by 0xFD)
            case 0xD4: // G_LOADBLOCK - load texture block (handled by 0xF3)
            case 0xDD: // G_SETTIMG - set texture image (handled by 0xFD)
                break;
            case 0x05: Cmd_Polygon(c); break;  // G_TRIN (DKR triangle)
            case 0x11: // G_RDPHALF_1 - RDP half command
            case 0x22: // G_RDPHALF_2 - RDP half command
            case 0x2B: // G_SETTIMG - set texture image
            case 0x3C: // G_SETTILESIZE - set tile size
            case 0x48: // G_SETTIMG - set texture image
            case 0x4B: // G_LOADBLOCK - load texture block
            case 0x4C: // G_SETTILE - set tile
            case 0x54: // G_SETSCISSOR - set scissor box
            case 0x62: // G_SETTILESIZE - set tile size
            case 0x6A: // G_SETTIMG - set texture image
            case 0x6C: // G_SETTILE - set tile
            case 0x73: // G_SETTIMG - set texture image
            case 0x87: // G_LOADBLOCK - load texture block
            case 0x96: // G_SETTILESIZE - set tile size
            case 0x9A: // G_SETTIMG - set texture image
            case 0x9D: // G_SETTILE - set tile
            case 0xAB: // G_SETTIMG - set texture image
            case 0xD0: // G_LOADBLOCK - load texture block
            case 0xEB: // G_SETTIMG - set texture image
                break;
default:
                // Fix E4: +2 (2438 adds 1 -> 3 per unknown)
                unknown_opcode_run += 2;
                break;
        }

        if (cmds_since_progress > 200 && total > 200) {
            // Sub-DLs in Banjo's compiled output frequently omit G_ENDDL.
            // When the walker stops making progress at depth > 0, treat it
            // as an implicit end-of-list: pop back to the parent and keep
            // walking.  Only bail when we are already at the top level —
            // that's true drift.
            if (depth > 0 && g_bka_task_pops < 16) {
                g_bka_task_pops++;
                static int s_implicit_pop = 0;
                if (s_implicit_pop++ < 20) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "walker: implicit end-of-DL at depth=%d cur=%p — popping (pop %d)",
                        depth, (void*)cur, g_bka_task_pops);
                }
                depth--;
                cur = stack[depth].ptr;  // Fix A: was + stack_stride, skipped a command
                cur_end = stack[depth].end;
                current_stride = stack_stride[depth];
                s_dl_base = stack_dl_base[depth];
                cur_dl_enc = stack_enc[depth];
                dl_cmds = 0;
                zero_run = 0;
                cmds_since_progress = 0;
                continue;
            }
            // Either at top level, or too many pops — treat as real drift.
            if (depth > 0 && g_bka_task_pops >= 16) {
                static int s_toomany = 0;
                if (s_toomany++ < 10) {
                    __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                        "walker: too many implicit pops (%d) at depth=%d — bailing",
                        g_bka_task_pops, depth);
                }
                return;
            }
            if (s_rspCallCount <= 30 || (s_rspCallCount % 50) == 0) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "TASK-SUMMARY #%d cmds=%zu tris=%d depth=%d end=NOPROG",
                    s_rspCallCount, total, g_bka_task_tri_count, depth);
            }
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "walker: 30 commands with no progress at cur=%p depth=%d total=%zu — bailing (top level)",
                (void*)cur, depth, total);
            g_bka_frame_gen++;
            return;
        }
        if (unknown_opcode_run >= 8) {
            if (s_rspCallCount <= 30 || (s_rspCallCount % 50) == 0) {
                __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                    "TASK-SUMMARY #%d cmds=%zu tris=%d depth=%d end=DRIFT",
                    s_rspCallCount, total, g_bka_task_tri_count, depth);
            }
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
                "walker: %d consecutive unknown opcodes at cur=%p depth=%d — returning",
                unknown_opcode_run, cur, depth);
            /* ARM64 port 2026-10-12: dump 64 bytes at the bail address.
               Written to the app's own files dir so logcat rate-limiting
               can't drop it; pulled via `rish -c "cat ..."`. */
            { static int s_bd = 0; if (s_bd++ < 3) {
                FILE* f = fopen("/data/data/com.dkrwrapper/files/bka_bail.bin", "ab");
                if (f) {
                    fprintf(f, "BAIL cur=%p depth=%d run=%d\n",
                        (void*)cur, depth, unknown_opcode_run);
                    fwrite(cur, 1, 64, f);
                    fprintf(f, "\n---\n");
                    fclose(f);
                }
            }}
                { static int s_bd = 0; if (s_bd++ < 12) {
                    const uint8_t* b = (const uint8_t*)cur;
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-BAILDUMP",
                        "BAIL cur=%p depth=%d bytes[0..31]=%02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X",
                        (void*)cur, depth,
                        b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
                        b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15],
                        b[16],b[17],b[18],b[19],b[20],b[21],b[22],b[23],
                        b[24],b[25],b[26],b[27],b[28],b[29],b[30],b[31]);
                }}
            /* Once-only drift diagnosis: dump last 8 accepted commands
             * (ptr, w0, w1, op, enc) plus 32 raw bytes at each. */
            {
                static int s_drift_dump = 0;
                if (s_drift_dump++ < 100) {   /* ARM64 port 2026-10-12: was 4 */
                    { FILE* bf = fopen("/data/data/com.dkrwrapper/files/bka_next.bin", "ab");
                      if (bf) {
                          const uint8_t* np = (const uint8_t*)(cur + current_stride);
                          fprintf(bf, "NEXT cur=%p stride=%zu\n", (void*)cur, current_stride);
                          fwrite(np, 1, 64, bf);
                          fprintf(bf, "\n===\n");
                          fclose(bf);
                      } }
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DRIFT",
                        "DRIFT-SUMMARY cur=%p depth=%d dlBase=%p stride=%zu",
                        (void*)cur, depth, (void*)s_dl_base, current_stride);
                    for (int k = 31; k >= 0; k--) {
                        int i = (s_lastcmd_idx - 1 - k) & 31;
                        if (!s_lastcmds[i].ptr) continue;
                        const uint8_t* p = s_lastcmds[i].ptr;
                        __android_log_print(ANDROID_LOG_ERROR, "BKA-DRIFT",
                            "LAST[-%d] ptr=%p depth=%d stride=%zu op=0x%02X enc=%d w0=%08X w1=%08X next8=%02X%02X%02X%02X %02X%02X%02X%02X",
                            k, (void*)p, s_lastcmds[i].depth, s_lastcmds[i].stride,
                            s_lastcmds[i].op, s_lastcmds[i].enc,
                            s_lastcmds[i].w0, s_lastcmds[i].w1,
                            p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
                    }
                    /* Also dump the raw 128 bytes ending at cur */
                    const uint8_t* p = (const uint8_t*)cur;
                    char hb[3*128 + 8]; int n = 0;
                    for (int k = -128; k < 0; k++)
                        n += snprintf(hb+n, sizeof(hb)-n, " %02X", p[k]);
                    __android_log_print(ANDROID_LOG_ERROR, "BKA-DRIFT",
                        "RAW128BACK ending@%p bytes%s", (void*)cur, hb);
                }
            }
            if (depth > 0) {
                /* Pop the broken sub-DL and resume the parent.  Bailing
                 * the whole task discarded the parent DL's remaining
                 * commands, including G_DLs to valid sub-DLs. */
                depth--;
                cur             = stack[depth].ptr;
                cur_end         = stack[depth].end;
                current_stride  = stack_stride[depth];
                s_dl_base       = stack_dl_base[depth];
                cur_dl_enc      = stack_enc[depth];
                unknown_opcode_run = 0;
                dl_cmds         = 0;
                zero_run        = 0;
                g_bka_task_pops++;
                continue;
            }
            return;
        }
    }
    if (s_rspCallCount <= 30 || (s_rspCallCount % 50) == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX",
            "TASK-SUMMARY #%d cmds=%zu tris=%d end=COMPLETE",
            s_rspCallCount, total, g_bka_task_tri_count);
    }
    g_bka_frame_gen++;
    {
        static int s_hist = 0;
        if (s_hist++ == 60) {   // dump once, after 60 tasks
            char buf[512]; int n = 0;
            n += snprintf(buf+n, sizeof(buf)-n, "MTXFLAGS:");
            for (int i = 0; i < 16; i++) {
                if (g_mtx_flag_hist[i])
                    n += snprintf(buf+n, sizeof(buf)-n, " %02X=%d", i, g_mtx_flag_hist[i]);
            }
            __android_log_print(ANDROID_LOG_ERROR, "BKA_GFX", "%s", buf);
        }
    }
    { static int s_ptask = 0; if (++s_ptask <= 30) {
        char obuf[2048]; int on = 0;
        for (int k = 0; k < 256; k++)
            if (s_opcount[k]) on += snprintf(obuf + on, sizeof(obuf) - on, "%02X:%u ", k, s_opcount[k]);
        __android_log_print(ANDROID_LOG_ERROR, "BKA-OPC", "task#%d %s", s_ptask, obuf);
        memset(s_opcount, 0, sizeof(s_opcount));
        s_op_total = 0;
    } }
    { static int _fbc = 0; if (_fbc++ % 30 == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BKA-LIVE",
            "frame=%d tri_count=%d", _fbc, g_bka_task_tri_count);
        bka_count_fb_pixels("PERIODIC");
        static int _dc = 0;
        if (_dc++ < 3) {
            extern uint8_t *gN64_RDRAM;
            if (gN64_RDRAM) {
                char pA[256], pB[256];
                snprintf(pA, sizeof(pA), "/data/data/com.dkrwrapper/files/screenA_%d.ppm", _dc);
                snprintf(pB, sizeof(pB), "/data/data/com.dkrwrapper/files/screenB_%d.ppm", _dc);
                bka_dump_fb_ptr(pA, gN64_RDRAM + 0x400000);
                bka_dump_fb_ptr(pB, gN64_RDRAM + 0x440000);
            }
        }
    } }
}


// force rebuild: vertex preservation
// force rebuild stride16 no bswap
