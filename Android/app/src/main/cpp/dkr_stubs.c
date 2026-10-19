/* dkr_stubs.c — link stubs for DKR port.
 * Audio, OS, PFS, GU, RSP blobs. Silent/no-op for first boot. */
#include <stdint.h>
#include <string.h>
#include <strings.h>

/* RSP microcode blobs — unused, software renderer */
uint8_t rspbootTextStart[0];
uint8_t rspbootTextEnd[0];
uint8_t rspF3DDKRXbusStart[0];
uint8_t rspF3DDKRFifoStart[0];
uint8_t rspF3DDKRDataXbusStart[0];
uint8_t rspF3DDKRDataFifoStart[0];
uint8_t aspMainDataStart[0];
uint8_t aspMainTextStart[0];

/* ROM/linker symbols — populated by wrapper at runtime */
uint8_t __ROM_END[0];
uint8_t __ASSETS_LUT_START[0];
uint8_t __ASSETS_LUT_END[0];
uint8_t D_B0000578[16];
uint8_t gzip_inflate_block[4096];
uint8_t align16[16];

/* Video modes — software renderer ignores */
static uint8_t _vi_dummy[64];
void *osViModeTable[17];
void *osViModeNtscLan1 = _vi_dummy;
void *osViModeNtscLpn1 = _vi_dummy;
void *osViModePalLan1  = _vi_dummy;
void *osViModePalLpn1  = _vi_dummy;
void *osViModeMpalLan1 = _vi_dummy;
void *osViModeMpalLpn1 = _vi_dummy;

/* Audio — silent boot */
uint8_t alGlobals[2048];
void alInit(void) {}
void alClose(void) {}
void alAudioFrame(void) {}
void alCSPNew(void) {}
int32_t alCSPGetChlVol(void)    { return 0; }
int32_t alCSPGetFadeIn(void)    { return 0; }
int32_t alCSPGetState(void)     { return 0; }
void alCSPSetChlPan(void) {}
void alCSPSetChlVol(void) {}
void alCSPSetFadeIn(void) {}
void alCSeqNew(void) {}
void alFxReverbSet(void) {}
void alSeqChOff(void) {}
void alSeqChOn(void) {}
void alSeqFileNew(void) {}
int32_t alSeqpGetChlFXMix(void) { return 0; }
void alSynAddPlayer(void) {}
void alSynAllocVoice(void) {}
void alSynFreeVoice(void) {}
void alSynSetFXMix(void) {}
void alSynSetPan(void) {}
void alSynSetPitch(void) {}
void alSynSetVol(void) {}
void alSynStartVoice(void) {}
void alSynStopVoice(void) {}

/* Scheduler */
void osCreateScheduler(void) {}
void osScAddClient(void) {}
void *osScGetCmdQ(void)       { return 0; }
void *osScGetInterruptQ(void) { return 0; }
void osDpSetNextBuffer(void) {}
void osSetTime(uint64_t t)    { (void)t; }

/* PFS — Controller Pak, not on Android */
int32_t osPfsAllocateFile(void)  { return -1; }
int32_t osPfsChecker(void)       { return -1; }
int32_t osPfsDeleteFile(void)    { return -1; }
int32_t osPfsFileState(void)     { return -1; }
int32_t osPfsFindFile(void)      { return -1; }
int32_t osPfsFreeBlocks(void)    { return -1; }
int32_t osPfsIsPlug(void)        { return -1; }
int32_t osPfsNumFiles(void)      { return -1; }
int32_t osPfsReFormat(void)      { return -1; }
int32_t osPfsReadWriteFile(void) { return -1; }

/* Internal OS */
void *__osGetActiveQueue(void) { return 0; }
int32_t __osSpSetStatus(void)  { return 0; }

/* GU — matrices (wrong values but defined) */
void _guMtxF2L(void) {}
void _guRotateF(void) {}
void _guTranslateF(void) {}
void guPerspectiveF(void) {}

/* Game functions from asm — no C fallback */
void mempool_alloc(void) {}
void mempool_alloc_fixed(void) {}
void mempool_alloc_pool(void) {}
void mempool_alloc_safe(void) {}
void mempool_free(void) {}
void mempool_free_queue_clear(void) {}
void mempool_free_timer(void) {}
void mempool_init_main(void) {}
void mempool_locked_set(void) {}
void mempool_locked_unset(void) {}
void mempool_new_sub(void) {}
void obj_shade_fast(void) {}
int32_t obj_animate(void) { return 0; }
void func_80000450(void) {}
void func_80063A90(void) {}
void func_80065A80(void) {}
void calc_dynamic_lighting_for_object_2(void) {}
void set_voice_limit(void) {}
void set_stereo_pan_mode(void) {}

/* Misc */
int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }

/* Framebuffer dims — wrapper reads these */
int32_t gFramebufferWidth  = 320;
int32_t gFramebufferHeight = 240;
