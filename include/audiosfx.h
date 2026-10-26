#ifndef _AUDIOSFX_H_
#define _AUDIOSFX_H_
/* Stub - not present in repo. Pulls in real libaudio.h for AL* types,
 * then adds SoundHandle which is DKR-specific (not in libaudio.h).
 */
#include "PR/libaudio.h"
typedef s32 SoundHandle;

/* sndp_set_param() / sndp event types -- full N64 SDK ALSndpEvent enum.
 * Used by src/audio*.c, src/audio_spatial.c, src/audio_vehicle.c, src/game_ui.c.
 */
typedef enum {
    AL_SNDP_PITCH_EVT = 1,
    AL_SNDP_VOL_EVT,
    AL_SNDP_PAN_EVT,
    AL_SNDP_FX_EVT,
    AL_SNDP_PRIORITY_EVT,
    AL_SNDP_STATE_EVT,
    AL_SNDP_API_EVT,
    AL_SNDP_DECAY_EVT,
    AL_SNDP_END_EVT,
    AL_SNDP_GROUP_VOL_EVT,
    AL_SNDP_PLAY_EVT,
    AL_SNDP_RELEASE_EVT,
    AL_SNDP_RELEASE_NEXT_EVT,
    AL_SNDP_RETRIGGER_EVT,
    AL_SNDP_STOP_EVT,
} ALSndpEvent;

/* Non-event constants */
#ifndef AL_SNDP_GROUP_VOLUME_MAX
#define AL_SNDP_GROUP_VOLUME_MAX 0x7FFFFFFF
#endif
#ifndef AL_SNDP_PRIORITY_DEFAULT
#define AL_SNDP_PRIORITY_DEFAULT 64
#endif

#endif
