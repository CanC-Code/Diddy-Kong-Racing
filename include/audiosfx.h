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


/* Additional AL_SNDP_* constants used by src/audio*.c and src/game_ui.c. */
#ifndef AL_SNDP_API_EVT
#define AL_SNDP_API_EVT 0
#endif
#ifndef AL_SNDP_DECAY_EVT
#define AL_SNDP_DECAY_EVT 0
#endif
#ifndef AL_SNDP_END_EVT
#define AL_SNDP_END_EVT 0
#endif
#ifndef AL_SNDP_GROUP_VOLUME_MAX
#define AL_SNDP_GROUP_VOLUME_MAX 0x7FFFFFFF
#endif
#ifndef AL_SNDP_GROUP_VOL_EVT
#define AL_SNDP_GROUP_VOL_EVT 0
#endif
#ifndef AL_SNDP_PLAY_EVT
#define AL_SNDP_PLAY_EVT 0
#endif
#ifndef AL_SNDP_PRIORITY_DEFAULT
#define AL_SNDP_PRIORITY_DEFAULT 0
#endif
#ifndef AL_SNDP_RELEASE_EVT
#define AL_SNDP_RELEASE_EVT 0
#endif
#ifndef AL_SNDP_RELEASE_NEXT_EVT
#define AL_SNDP_RELEASE_NEXT_EVT 0
#endif
#ifndef AL_SNDP_RETRIGGER_EVT
#define AL_SNDP_RETRIGGER_EVT 0
#endif
#ifndef AL_SNDP_STOP_EVT
#define AL_SNDP_STOP_EVT 0
#endif

#endif
