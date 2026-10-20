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
void alCSPSetFadeIn(ALCSPlayer *sp, void *ft, f32 t) { (void)sp; (void)ft; (void)t; }
void alCSeqNew(ALCSeq *sq, u8 *p) { (void)sq; (void)p; }
void alFxReverbSet(void *fx, void *rv) { (void)fx; (void)rv; }
void alSeqChOff(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; }
void alSeqChOn(ALSeqPlayer *sp, u8 ch) { (void)sp; (void)ch; }
void alSeqFileNew(ALSeqFile *fl, u8 *b) { (void)fl; (void)b; }
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
void _guMtxF2L(void) {}
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
