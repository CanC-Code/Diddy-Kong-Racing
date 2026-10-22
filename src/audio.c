#include <android/log.h>
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "audio.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "memory.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "asset_enums.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "asset_loading.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "audio_spatial.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "audiomgr.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "audiosfx.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "libultra/src/audio/seqchannel.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "sched.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#include "types.h"
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/* ARM64 port: byteswap an array of u32 in place (N64 BE -> host LE).
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Used at every asset_load boundary where a table of u32 offsets/sizes is
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * loaded from ROM. Matches the OTR pattern: swap per-structure at use site. */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
static void dkr_bswap_u32_buf(u32 *buf, u32 count) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    for (u32 i = 0; i < count; i++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        buf[i] = __builtin_bswap32(buf[i]);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/************ .data ************/
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALCSPlayer *gMusicPlayer = NULL;  // Official Name: tuneSeqPlayer
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALCSPlayer *gJinglePlayer = NULL; // Official Name: ambientSeqPlayer
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gMusicBaseVolume = 127;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 sfxRelativeVolume = 127;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gCanPlayMusic = TRUE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gCanPlayJingle = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gBlockMusicChange = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 audioPrevCount = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
f32 sMusicFadeVolume = 1.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gMusicSliderVolume = 256;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gDelayedSoundsCount = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gMusicNextSeqID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gJingleNextSeqID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED s32 D_800DC664 = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED s32 D_800DC668 = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gGlobalMusicVolume = 256; // This is never not 256...
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gBlockVoiceLimitChange = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/*******************************/
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/************ .bss ************/
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
// The audio heap is located at the start of the BSS section.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gAudioHeapStack[AUDIO_HEAP_SIZE];
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALHeap gALHeap;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALSeqFile *gSequenceTable;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void *gMusicSequenceData;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void *gJingleSequenceData;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gCurrentSequenceID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gCurrentJingleID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gMusicTempo;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u32 *gSeqLengthTable;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALBankFile *gSequenceBank;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALBankFile *gSoundBank; // Official Name: sfxBankPtr
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
SoundData *gSoundTable; // Official Name: sfxIndex
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
MusicData *gSeqSoundTable;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gSoundCount; // Official Name: maxSound
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 gSeqSoundCount;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u32 gSoundTableSize; // Official Name: sfxIndexSize
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u32 gSeqSoundTableSize;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s16 sMusicTempo;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
f32 gMusicAnimationTick;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 sMusicDelayTimer;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 sMusicDelayLength;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gMusicPlaying;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gJinglePlaying;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
DelayedSound gDelayedSounds[8];
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALCSeq gMusicSequence;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALCSeq gJingleSequence;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gSkipResetChannels; // Stored and used by a single function, but redundant.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 gAudioVolumeSetting;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u32 gDynamicMusicChannelMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
SoundHandle gGlobalSoundMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
SoundHandle gSpatialSoundMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
SoundHandle gRacerSoundMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/******************************/
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Allocate memory for all the audio systems, including sequence data, sound data and heaps.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Afterwards, set up the audio thread and start it.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void audio_init(OSSched *sc) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    (void)sc;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    __android_log_print(ANDROID_LOG_ERROR, "DKR-AUDIO",
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        "audio_init STUBBED — audio subsystem disabled (un-stub after rendering works)");
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Depending on whether or not the audio volume is set to normal and the argument is false, reset sound effect channel
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * volumes.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_volume_reset(u8 skipReset) {
#ifdef PORT
    /* Audio synthesis not wired up; sndp_set_group_volume crashes on NULL synthesis state. */
    (void)skipReset;
    return;
#endif
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gAudioVolumeSetting == VOLUME_NORMAL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gSkipResetChannels = skipReset;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (gSkipResetChannels == FALSE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            gGlobalMusicVolume = 256;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_volume_set(gMusicBaseVolume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            // Effectively sets all volumes to the max (AL_SNDP_GROUP_VOLUME_MAX)
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(0, gGlobalMusicVolume * 128 - 1);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(1, gGlobalMusicVolume * 128 - 1);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(2, gGlobalMusicVolume * 128 - 1);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(4, gGlobalMusicVolume * 128 - 1);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Changes the volume of each sound channel depending on what value is passed through.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amSetMuteMode
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_volume_change(s32 behaviour) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    switch (behaviour) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        case VOLUME_LOWER: // Mute most sound effects and half the volume of music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(0, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(1, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(2, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(4, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            alCSPSetVol(gMusicPlayer, (s16) (gMusicBaseVolume * gMusicSliderVolume >> 2));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            alCSPSetVol(gJinglePlayer, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            break;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        case VOLUME_LOWER_AMBIENT: // Mute the ambient channel, making course elements stop making noise.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(0, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(1, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(2, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(4, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            break;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        case VOLUME_UNK03:
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(0, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(1, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(2, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(4, 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            break;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        default: // Restore sound back to normal.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(0, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(1, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(2, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_group_volume(4, AL_SNDP_GROUP_VOLUME_MAX);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            alCSPSetVol(gMusicPlayer, (s16) (gMusicBaseVolume * gMusicSliderVolume));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            alCSPSetVol(gJinglePlayer, (s16) (sndp_get_global_volume() * sfxRelativeVolume));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            break;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gAudioVolumeSetting = behaviour;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Prevents changing of background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_change_off(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gBlockMusicChange = TRUE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Allows changing of background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_change_on(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gBlockMusicChange = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Queue a new music sequence to play if not blocked.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Stops any playing existing music beforehand.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTunePlay
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_play(u8 seqID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gBlockMusicChange == FALSE && gMusicSliderVolume != 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gCurrentSequenceID = seqID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicBaseVolume = 127;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (gCanPlayMusic) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_sequence_start(gCurrentSequenceID, gMusicPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicTempo = alCSPGetTempo(gMusicPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        audioPrevCount = osGetCount();
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicPlaying = TRUE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gDynamicMusicChannelMask = MUSIC_CHAN_MASK_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Update the background music voice limit if not prevented from doing so.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneVoiceLimit
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_voicelimit_set(u8 voiceLimit) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gBlockVoiceLimitChange == FALSE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        set_voice_limit(gMusicPlayer, voiceLimit);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Prevent the background music voice limit from being changed.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_voicelimit_change_off(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gBlockVoiceLimitChange = TRUE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Allow the background music voice limit to be changed.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_voicelimit_change_on(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gBlockVoiceLimitChange = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Update the jingle players voice limit.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_jingle_voicelimit_set(u8 voiceLimit) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    set_voice_limit(gJinglePlayer, voiceLimit);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void func_80000C68(u8 arg0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    func_80063A90(gMusicPlayer, arg0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Enables the timer to start fading in or out the background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Give it a positive number to fade in, otherwise, give it a negative one to fade out.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_fade(s32 time) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sMusicDelayTimer = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sMusicDelayLength = (time * 60) >> 8;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Sets the background music volume back to normal.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_volume_reset(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sMusicDelayTimer = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sMusicDelayLength = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sMusicFadeVolume = 1.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    music_volume_set(gMusicBaseVolume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Run every frame, this handles the transitions in and out of music sequences.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * If there's something in the queue, then begin to play that.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Additionally, it also handles the delayed audio queue, counting down and playing any sounds.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amAudioTick
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_update_queue(u8 updateRate) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 i;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 j;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (sMusicDelayLength > 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicDelayTimer += updateRate;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicFadeVolume = ((f32) sMusicDelayTimer) / ((f32) sMusicDelayLength);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (sMusicFadeVolume > 1.0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sMusicDelayTimer = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sMusicDelayLength = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sMusicFadeVolume = 1.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        music_volume_set(gMusicBaseVolume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else if (sMusicDelayLength < 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicDelayTimer -= updateRate;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicFadeVolume = 1.0f - ((f32) sMusicDelayTimer) / ((f32) sMusicDelayLength);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (sMusicFadeVolume < 0.0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sMusicDelayTimer = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sMusicDelayLength = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sMusicFadeVolume = 0.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        music_volume_set(gMusicBaseVolume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gDelayedSoundsCount > 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        for (i = 0; i < gDelayedSoundsCount;) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            gDelayedSounds[i].timer -= updateRate;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            if (gDelayedSounds[i].timer <= 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                j = i;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                sound_play(gDelayedSounds[i].soundId, gDelayedSounds[i].handlePtr);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                gDelayedSoundsCount -= 1;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                while (j < gDelayedSoundsCount) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    gDelayedSounds[i].soundId = gDelayedSounds[i + 1].soundId;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    gDelayedSounds[i].timer = gDelayedSounds[i + 1].timer;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    gDelayedSounds[i].handlePtr = gDelayedSounds[i + 1].handlePtr;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    j++;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                j++;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                i++;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    music_sequence_init(gMusicPlayer, gMusicSequenceData, &gMusicNextSeqID, &gMusicSequence);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    music_sequence_init(gJinglePlayer, gJingleSequenceData, &gJingleNextSeqID, &gJingleSequence);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (sMusicTempo == -1 && gMusicPlayer->target) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicTempo = 60000000 / alCSPGetTempo(gMusicPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Add a sound to a queue to play after a set time has passed.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Delay time is in seconds. (1.0f = 1 second)
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_play_delayed(u16 soundId, SoundHandle *handlePtr, f32 delayTime) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gDelayedSoundsCount < 8) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gDelayedSounds[gDelayedSoundsCount].soundId = soundId;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gDelayedSounds[gDelayedSoundsCount].handlePtr = handlePtr;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gDelayedSounds[gDelayedSoundsCount].timer = delayTime * 60.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gDelayedSoundsCount++;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Clear the delayed sound queue.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_clear_delayed(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gDelayedSoundsCount = 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the channel mask of the music player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u16 music_channel_get_mask(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gMusicPlayer->chanMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Sets the channels in the sequence on or off based on the channel mask given.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneSetChlMask
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_dynamic_set(u16 channelMask) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u32 i;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gMusicNextSeqID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gDynamicMusicChannelMask = channelMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicPlayer->chanMask = channelMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        for (i = 0; i != AUDIO_CHANNELS; i++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            if (channelMask & (1 << i)) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                music_channel_on(i);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                music_channel_off(i);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Mute the sequence channel, preventing it from playing any sound.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneMuteChl
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_channel_off(u8 channel) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel < AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alSeqChOff(gMusicPlayer, channel);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return true if the given channel is currently active.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 music_channel_active(s32 channel) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return (gMusicPlayer->chanMask & (1 << channel)) == 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Unmute the sequence channel so it can play sound.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneUnmuteChl
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_channel_on(u8 channel) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel < AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alSeqChOn(gMusicPlayer, channel);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the panning level of the given channel for the music player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_channel_pan_set(u8 channel, ALPan pan) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel < AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetChlPan(gMusicPlayer, channel, pan);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the volume of the given channel for the music player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneSetChlVolume
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_channel_volume_set(u8 channel, u8 volume) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel < AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetChlVol(gMusicPlayer, channel, volume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the volume of the given channel in the music player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED u8 music_channel_volume(u8 channel) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel >= AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return alCSPGetChlVol(gMusicPlayer, channel);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set this channel to fade in over time.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_channel_fade_set(u8 channel, ALPan fade) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel < AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetFadeIn(gMusicPlayer, channel, fade);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the fade volume of the given channel.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_channel_fade(u8 channel) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (channel >= AUDIO_CHANNELS) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return alCSPGetFadeIn(gMusicPlayer, channel);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Resets all audio channels for the music player to the default state.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * This is being enabled, centre panning and at normal volume.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneResetChls
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_channel_reset_all(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u32 channel;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gBlockMusicChange == FALSE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        for (channel = 0; channel < AUDIO_CHANNELS; channel++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_channel_on(channel);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_channel_fade_set(channel, 127);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_channel_volume_set(channel, 127);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED u8 func_80001358(u8 chan1, u8 chan2, s32 arg2) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u8 val_1f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u8 vol;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 updatedVol;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (chan1 != 100) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        val_1f = arg2 + alCSPGetChlVol(gMusicPlayer, chan1);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (val_1f > 127) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            val_1f = 127;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetChlVol(gMusicPlayer, chan1, val_1f);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (chan2 != 100) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        updatedVol = alCSPGetChlVol(gMusicPlayer, chan2);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        vol = (updatedVol > arg2) ? updatedVol - arg2 : 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetChlVol(gMusicPlayer, chan2, vol);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return vol;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return 127 - val_1f; //!@bug: This could be uninitialized!
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Retrieves the FX mix levels of all audio channels.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void music_get_fx_mix_all_channels(u8 *channelFXMix) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 channelIdx;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    for (channelIdx = 0; channelIdx < gMusicPlayer->maxChannels; channelIdx++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        channelFXMix[channelIdx] = alSeqpGetChlFXMix((ALSeqPlayer *) gMusicPlayer, channelIdx);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Multiplies the current tempo of the background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Since it calls music_tempo and multiplies it by the result, calling this repeatedly can recursively change the
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * music's speed.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneScaleTempo
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_tempo_set_relative(f32 tempo) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    music_tempo_set((s32) ((f32) (u32) (music_tempo() & 0xFF) * tempo));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the tempo of the current playing background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official name: amTuneSetTempoBPM
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_tempo_set(s32 tempo) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (tempo != 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        f32 inv_tempo = (1.0f / tempo);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetTempo(gMusicPlayer, (s32) (inv_tempo * 60000000.0f));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicTempo = tempo;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the tempo of the current playing background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official name: amTuneGetTempoBPM
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s16 music_tempo(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return sMusicTempo;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Returns true if background music is currently playing.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_is_playing(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return (alCSPGetState(gMusicPlayer) == AL_PLAYING);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Counts up using the internal timer.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Loops itself round, so the final result will return 0.0f - 1.0f.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
f32 music_animation_fraction(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    f32 tmp;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u32 cnt = osGetCount();
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if ((u32) audioPrevCount < cnt) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicAnimationTick += (f32) (cnt - audioPrevCount) / 46875.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicAnimationTick += (f32) ((cnt - audioPrevCount) - 1) / 46875.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gMusicPlaying == FALSE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sMusicTempo = 182;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    for (tmp = 120000.0f / (f32) sMusicTempo; tmp < gMusicAnimationTick; gMusicAnimationTick -= tmp) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        ;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    audioPrevCount = (s32) cnt;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gMusicAnimationTick / tmp;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Writes the music and sound tempo, as well as the volume to the arguments.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void sound_get_properties(u8 poolID, u8 *tempo, u8 *volume, u8 *reverb) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    *tempo = gSeqSoundTable[poolID].tempo;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    *volume = gSeqSoundTable[poolID].volume;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    *reverb = gSeqSoundTable[poolID].reverb;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Play a jingle, but only if there isn't one playing already.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official NAme: amAmbientPlay
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_jingle_play_safe(u8 jingleID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (music_jingle_playing() == SEQUENCE_NONE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        music_sequence_start(gCurrentJingleID = jingleID, gJinglePlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gJinglePlaying = TRUE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Sets the tempo for the jingle player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amAmbientSetTempoBPM
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_jingle_tempo_set(s32 tempo) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    f32 inv_tempo = (1.0f / tempo);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alCSPSetTempo(gJinglePlayer, (s32) (inv_tempo * 60000000.0f));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Stops the background music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneStop
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_stop(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gBlockMusicChange == FALSE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        music_sequence_stop(gMusicPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set background music to play or not.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * If the setting changed, then either stop or start music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void music_enabled_set(u8 setting) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (setting != gCanPlayMusic) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gCanPlayMusic = setting;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (setting) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_play(gCurrentSequenceID);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_stop();
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return whether background music can be played.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_can_play(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gCanPlayMusic;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Stops the currently playing jingle.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amAmbientStop
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_jingle_stop(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (music_jingle_playing() == SEQUENCE_NONE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gCurrentJingleID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        music_sequence_stop(gJinglePlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the currently playing music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneGetSeqNo
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_current_sequence(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gCurrentSequenceID != SEQUENCE_NONE && gMusicPlayer->state == AL_PLAYING) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return gCurrentSequenceID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the next music sequence to be played if there is one.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Otherwise, return what's currently playing.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED u8 music_next(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gMusicNextSeqID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return gMusicNextSeqID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return gCurrentSequenceID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the currently playing jingle.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amAmbientGetSeqNo
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_jingle_current(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gCurrentJingleID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the volume of the music.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Update music volume with this new setting.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneSetVolume
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_volume_set(u8 volume) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    f32 normalized_vol;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gMusicBaseVolume = volume;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    normalized_vol = gMusicSliderVolume * gMusicBaseVolume * sMusicFadeVolume;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alCSPSetVol(gMusicPlayer, (s16) ((s32) (gGlobalMusicVolume * normalized_vol) >> 8));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the user configured music volume.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Update music volume with this new setting.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneSetGlobalVolume
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_volume_config_set(u32 slider_val) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    f32 normalized_vol;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    slider_val = (slider_val <= 256) ? slider_val : 256;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gMusicSliderVolume = slider_val;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    normalized_vol = gMusicSliderVolume * gMusicBaseVolume * sMusicFadeVolume;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alCSPSetVol(gMusicPlayer, (s16) ((s32) (gGlobalMusicVolume * normalized_vol) >> 8));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the baseline music volume, unaffected by user config.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneGetVolume
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_volume(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gMusicBaseVolume;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the music volume set by the player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
s32 music_volume_config(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gMusicSliderVolume;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the volume for the jingle player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * The jingle player scales with sfx volume rather than music volume.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amAmbientSetVolume
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_jingle_volume_set(u8 arg0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sfxRelativeVolume = arg0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alCSPSetVol(gJinglePlayer, (s16) (sndp_get_global_volume() * sfxRelativeVolume));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the panning level for every channel in the jingle player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amAmbientSetPan
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_jingle_pan_set(ALPan pan) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u32 iChan;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    for (iChan = 0; iChan < AUDIO_CHANNELS; iChan++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetChlPan(gJinglePlayer, iChan, pan);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Plays a sequence just once, allowing it to coexist with the music if necessary.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Examples include getting silver coins, challenge keys, or getting the locked message.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amDittyPlay
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_jingle_play(u8 seqID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gCanPlayJingle = TRUE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    music_sequence_start(gCurrentJingleID = seqID, gJinglePlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * If there's a jingle playing, return that, otherwise, return 0.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amDittyPlaying
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u32 music_jingle_playing(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gCurrentJingleID && gCanPlayJingle && (gJinglePlayer->state == AL_PLAYING)) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return gCurrentJingleID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    gCanPlayJingle = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Sets the volume for every sound channel.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Tries to set up to 64, regardless of if there are 64 sound channels or not.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * !@bug: This can cause an out of bounds array index.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void sound_channel_volume_all(u16 volume) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u32 i;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    for (i = 0; i < 64; i++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_set_group_volume(i, volume << 8);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the audible distance of the sound effect.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u16 sound_distance(u16 soundId) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundId > gSoundCount) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gSoundTable[soundId].range;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Add the requested sound to the queue and update the mask to show that this sound is playing at that source.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * If no soundmask is provided, then instead use the global mask.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amSndPlay
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_play(u16 soundID, SoundHandle *handlePtr) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    f32 pitch;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 soundBite;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundID > gSoundCount) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (handlePtr != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            *handlePtr = NULL;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        stubbed_printf("amSndPlay: Illegal sound effects table index\n");
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    soundBite = gSoundTable[soundID].soundBite;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundBite == NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (handlePtr != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            *handlePtr = NULL;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    pitch = gSoundTable[soundID].pitch / 100.0f;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (handlePtr != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_play_with_priority(gSoundBank->bankArray[0], soundBite, gSoundTable[soundID].priority, handlePtr);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (*handlePtr != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_param(*handlePtr, AL_SNDP_VOL_EVT, gSoundTable[soundID].volume * 256);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_param(*handlePtr, AL_SNDP_PITCH_EVT, *((u32 *) &pitch));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        handlePtr = &gGlobalSoundMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_play_with_priority(gSoundBank->bankArray[0], soundBite, gSoundTable[soundID].priority, &gGlobalSoundMask);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (*handlePtr != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_param(*handlePtr, AL_SNDP_VOL_EVT, gSoundTable[soundID].volume * 256);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sndp_set_param(*handlePtr, AL_SNDP_PITCH_EVT, *((u32 *) &pitch));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Creates a spatial audio reference, then plays a sound.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * This then makes the audio pan around in 3D space.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * If it is not given a mask, then it will use the global mask.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_play_spatial(u16 soundID, f32 x, f32 y, f32 z, SoundHandle *handlePtr) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (handlePtr == NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        handlePtr = &gSpatialSoundMask;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    sound_play(soundID, handlePtr);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (*handlePtr != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        audspat_calculate_echo(*handlePtr, x, y, z);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amSndPlayDirect
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_play_direct(u16 soundID, SoundHandle *handlePtr) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundID <= 0 || sound_count() < soundID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        stubbed_printf("amSndPlayDirect: Somebody tried to play illegal sound %d\n", soundID);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (handlePtr) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            *handlePtr = NULL;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (handlePtr) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_play(gSoundBank->bankArray[0], (s16) soundID, handlePtr);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_play(gSoundBank->bankArray[0], (s16) soundID, &gRacerSoundMask);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Set the volume of the sound relative to the baseline volume of the sound ID.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amSndSetVol
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_volume_set_relative(u16 soundID, SoundHandle soundHandle, u8 volume) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 newVolume = ((s32) (gSoundTable[soundID].volume * (volume / 127.0f))) * 256;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundHandle) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_set_param(soundHandle, AL_SNDP_VOL_EVT, newVolume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Updates the volume of the given sound mask.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void sound_volume_set(SoundHandle soundHandle, u8 arg1) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundHandle != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_set_param(soundHandle, AL_SNDP_VOL_EVT, arg1 * 256);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Updates the pitch of the given sound mask.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official name: amSndSetPitchDirect
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void sound_pitch_set(SoundHandle soundHandle, u32 pitch) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    u32 *pitchAddr = &pitch;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundHandle != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        sndp_set_param(soundHandle, AL_SNDP_PITCH_EVT, *pitchAddr);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return the number of playable sounds in the audio table.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official name: amGetSfxCount
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u16 sound_count(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gSoundBank->bankArray[0]->instArray[0]->soundCount;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Return mumber of playable sequences in the table.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 music_sequence_count(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gSequenceTable->seqCount;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Writes the sound table address, size and element count into the arguments.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amGetSfxSettings
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_table_properties(SoundData **table, s32 *size, s32 *count) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (table != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *table = gSoundTable;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (size != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *size = gSoundTableSize;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (count != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *count = gSoundCount;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Writes the music sound table address, size and element count into the arguments.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED void music_table_properties(MusicData **table, s32 *size, s32 *count) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (table != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *table = gSeqSoundTable;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (size != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *size = gSeqSoundTableSize;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (count != NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *count = gSeqSoundCount;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Returns true if the given soundID is looped.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amSoundIsLooped
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
u8 sound_is_looped(u16 soundID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (soundID <= 0 || gSoundBank->bankArray[0]->instArray[0]->soundCount < soundID) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return 0;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return ((u32) (1 + gSoundBank->bankArray[0]->instArray[0]->soundArray[soundID - 1]->envelope->decayTime) == 0);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Allocate space, then initialise a sequence player for audio playback.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
ALCSPlayer *sound_seqplayer_init(s32 maxVoices, s32 maxEvents) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    ALCSPlayer *cseqp;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    ALSeqpConfig config;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.maxVoices = maxVoices;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.maxEvents = maxEvents;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.voiceLimit = maxVoices; // this member doesn't exist in other versions of ALSeqpConfig
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.maxChannels = AUDIO_CHANNELS;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.heap = &gALHeap;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.initOsc = NULL;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.updateOsc = NULL;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    config.stopOsc = NULL;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    cseqp = (ALCSPlayer *) alHeapAlloc(&gALHeap, 1, sizeof(ALCSPlayer));
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alCSPNew(cseqp, &config);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alCSPSetBank(cseqp, gSequenceBank->bankArray[0]);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    cseqp->unk36 = 127;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return cseqp;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Stop the current sequence then set the parameters for the next sequence.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_sequence_start(u8 seqID, ALCSPlayer *seqPlayer) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#ifdef PORT
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gSequenceTable == NULL || seqPlayer == NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#endif
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    music_sequence_stop(seqPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (seqID < gSequenceTable->seqCount) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (seqPlayer == gMusicPlayer) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            gMusicNextSeqID = seqID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            gJingleNextSeqID = seqID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        stubbed_printf("Invalid midi sequence index\n");
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * If the sequence player is currently inactive, start a new sequence with the current properties.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_sequence_init(ALCSPlayer *seqp, void *sequence, u8 *seqID, ALCSeq *seq) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    s32 i;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if ((alCSPGetState(seqp) == AL_STOPPED) && (*seqID != 0)) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        asset_load(ASSET_AUDIO, sequence,
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                   gSequenceTable->seqArray[*seqID].offset - asset_rom_offset(ASSET_AUDIO, 0),
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                   (s32) gSeqLengthTable[*seqID]);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSeqNew(seq, sequence);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPSetSeq(seqp, seq);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPPlay(seqp);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        if (seqp == gMusicPlayer) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_volume_set(gSeqSoundTable[*seqID].volume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            if (gSeqSoundTable[*seqID].tempo != 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                music_tempo_set(gSeqSoundTable[*seqID].tempo);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                sMusicTempo = -1;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            sound_reverb_set(gSeqSoundTable[*seqID].reverb);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            gCurrentSequenceID = *seqID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            if (gDynamicMusicChannelMask != MUSIC_CHAN_MASK_NONE) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                for (i = 0; i < AUDIO_CHANNELS; i++) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    if ((1 << i) & gDynamicMusicChannelMask) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                        music_channel_on(i);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                        music_channel_off(i);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        } else {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            music_jingle_volume_set(gSeqSoundTable[*seqID].volume);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            if (gSeqSoundTable[*seqID].tempo != 0) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
                sound_jingle_tempo_set(gSeqSoundTable[*seqID].tempo);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
            gCurrentJingleID = *seqID;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        *seqID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Stops the current playing sequence for the given sequence player.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void music_sequence_stop(ALCSPlayer *seqPlayer) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#ifdef PORT
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (seqPlayer == NULL) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        return;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
#endif
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    if (gMusicPlayer == seqPlayer && gMusicPlaying) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPStop(seqPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicPlaying = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gCurrentSequenceID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gMusicNextSeqID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    } else if (gJinglePlayer == seqPlayer && gJinglePlaying) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        alCSPStop(seqPlayer);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gJinglePlaying = FALSE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
        gJingleNextSeqID = SEQUENCE_NONE;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    }
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Enable or disable special audio effects.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * This includes reverb and echo.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Official Name: amTuneSetReverbOnOff
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
void sound_reverb_set(u8 setting) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    alFxReverbSet(setting);
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif

#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
/**
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 * Returns whether or not reverb is currently enabled.
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
 */
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
UNUSED u8 sound_reverb_enabled(void) {
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
    return gSeqSoundTable[gCurrentSequenceID].reverb;
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
}
#ifdef PORT
    if (gSequenceTable == NULL) {
        return;
    }
#endif
