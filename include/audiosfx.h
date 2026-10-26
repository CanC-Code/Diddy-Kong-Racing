#ifndef _AUDIOSFX_H_
#define _AUDIOSFX_H_
/* Stub - not present in repo. Pulls in real libaudio.h for AL* types,
 * then adds SoundHandle which is DKR-specific (not in libaudio.h).
 */
#include "PR/libaudio.h"
typedef s32 SoundHandle;
#endif
