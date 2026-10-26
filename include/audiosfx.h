#ifndef _AUDIOSFX_H_
#define _AUDIOSFX_H_
/* Stub - not present in repo. Pulls in real libaudio.h for AL* types,
 * then adds SoundHandle which is DKR-specific (not in libaudio.h).
 */
#include "PR/libaudio.h"
typedef s32 SoundHandle;

/* sndp_set_param() event types -- used by src/audio_spatial.c and src/audio_vehicle.c.
 * Standard N64 SDK ALSndpEvent values.
 */
#ifndef AL_SNDP_PITCH_EVT
typedef enum {
    AL_SNDP_PITCH_EVT = 1,
    AL_SNDP_VOL_EVT,
    AL_SNDP_PAN_EVT,
    AL_SNDP_FX_EVT,
} ALSndpEvent;
#endif

#endif
