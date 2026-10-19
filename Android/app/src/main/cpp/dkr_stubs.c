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
uint8_t __ROM_END[0];
uint8_t __ASSETS_LUT_START[0];
uint8_t __ASSETS_LUT_END[0];
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
ALGlobals *alGlobals = 0;
void alInit(ALGlobals *g, ALSynConfig *c) { (void)g; (void)c; }
void alClose(ALGlobals *g) { (void)g; }
Acmd *alAudioFrame(Acmd *cl, s32 *l, s16 *ob, s32 ol) { (void)cl; (void)l; (void)ob; (void)ol; return 0; }
void alCSPNew(ALCSPlayer *sp, ALSeqpConfig *cfg) { (void)sp; (void)cfg; }
u8 alCSPGetChlVol(ALCSPlayer *sp, u8 ch) { (void)sp; (void)ch; return 0; }
s32 alCSPGetFadeIn(ALCSPlayer *sp) { (void)sp; return 0; }
s32 alCSPGetState(ALCSPlayer *sp) { (void)sp; return 0; }
void alCSPSetChlPan(ALCSPlayer *sp, u8 ch, ALPan p) { (void)sp; (void)ch; (void)p; }
void alCSPSetChlVol(ALCSPlayer *sp, u8 ch, u8 v) { (void)sp; (void)ch; (void)v; }
void alCSPSetFadeIn(ALCSPlayer *sp, ALFadeInType ft, f32 t) { (void)sp; (void)ft; (void)t; }
void alCSeqNew(ALCSeq *sq, u8 *p) { (void)sq; (void)p; }
void alFxReverbSet(ALFx *fx, ALFxReverb *rv) { (void)fx; (void)rv; }
void alSeqChOff(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; }
void alSeqChOn(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; }
void alSeqFileNew(ALSeqFile *fl, u8 *b) { (void)fl; (void)b; }
u8 alSeqpGetChlFXMix(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; return 0; }
void alSynAddPlayer(ALSynth *sy, ALPlayer *pl) { (void)sy; (void)pl; }
void alSynAllocVoice(ALSynth *sy, ALVoice *v, ALVoiceConfig *cfg) { (void)sy; (void)v; (void)cfg; }
void alSynFreeVoice(ALSynth *sy, ALVoice *v) { (void)sy; (void)v; }
void alSynSetFXMix(ALSynth *sy, ALVoice *v, u8 mx) { (void)sy; (void)v; (void)mx; }
void alSynSetPan(ALSynth *sy, ALVoice *v, ALPan p) { (void)sy; (void)v; (void)p; }
void alSynSetPitch(ALSynth *sy, ALVoice *v, f32 p) { (void)sy; (void)v; (void)p; }
void alSynSetVol(ALSynth *sy, ALVoice *v, s16 vol, f32 t) { (void)sy; (void)v; (void)vol; (void)t; }
void alSynStartVoice(ALSynth *sy, ALVoice *v, ALWaveTable *w) { (void)sy; (void)v; (void)w; }
void alSynStopVoice(ALSynth *sy, ALVoice *v) { (void)sy; (void)v; }

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
