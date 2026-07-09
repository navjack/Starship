#include "audio_spatial_objects.h"

#include "audio/mixer.h"
#include "audioseq_cmd.h"
#include "macros.h"
#include "public/bridge/audiobridge.h"

#include <stdbool.h>
#include <stddef.h>

#define SPATIAL_OBJECT_PCM_MAX_FRAMES 1024

extern SfxBankEntry sSfxBanks[5][20];
extern ActiveSfx sActiveSfx[5][8];
extern u8 sChannelsPerBank[5][5];
extern u8 sSfxChannelLayout;
extern SfxChannelState sSfxChannelState[16];
extern f32 gDefaultSfxSource[3];
extern s32 sAudioFrameCounter;

static s16 sSpatialObjectPcmScratch[SPATIAL_OBJECT_PCM_MAX_FRAMES];

static bool AudioSpatial_GetSfxChannelIndexForNote(s32 noteIndex, u8* channelIndex) {
    Note* note;
    SequenceLayer* layer;
    SequenceChannel* channel;
    u8 i;

    if ((channelIndex == NULL) || (noteIndex < 0) || (noteIndex >= gNumNotes) || (gNotes == NULL)) {
        return false;
    }

    note = &gNotes[noteIndex];
    layer = note->playbackState.parentLayer;
    if ((layer == NULL) || (layer == NO_LAYER)) {
        return false;
    }

    channel = layer->channel;
    if ((channel == NULL) || !IS_SEQUENCE_CHANNEL_VALID(channel) || !channel->is_sfx ||
        (channel->seqPlayer != &gSeqPlayers[SEQ_PLAYER_SFX])) {
        return false;
    }

    for (i = 0; i < SEQ_NUM_CHANNELS; i++) {
        if (gSeqPlayers[SEQ_PLAYER_SFX].channels[i] == channel) {
            *channelIndex = i;
            return true;
        }
    }

    return false;
}

static bool AudioSpatial_FindSourceForSfxChannel(u8 channelIndex, StarshipSpatialAudioSource* source) {
    u8 bankId;
    u8 i;

    if ((source == NULL) || (channelIndex >= ARRAY_COUNT(sSfxChannelState))) {
        return false;
    }

    for (bankId = 0; bankId < ARRAY_COUNT(sSfxBanks); bankId++) {
        for (i = 0; i < sChannelsPerBank[sSfxChannelLayout][bankId]; i++) {
            u8 entryIndex = sActiveSfx[bankId][i].entryIndex;
            SfxBankEntry* entry;
            SfxChannelState* channelState;

            if (entryIndex == 0xFF) {
                continue;
            }

            entry = &sSfxBanks[bankId][entryIndex];
            if ((entry->state == 0) || (entry->channelIndex != channelIndex)) {
                continue;
            }

            if ((entry->xPos == NULL) || (entry->xPos == gDefaultSfxSource)) {
                return false;
            }

            channelState = &sSfxChannelState[channelIndex];
            source->sfxId = entry->sfxId;
            source->bankId = bankId;
            source->channelIndex = channelIndex;
            source->token = entry->token;
            source->isDefaultSource = false;
            source->x = *entry->xPos;
            source->y = (entry->yPos != NULL) ? *entry->yPos : 0.0f;
            source->z = (entry->zPos != NULL) ? *entry->zPos : 0.0f;
            source->distance = entry->distance;
            source->gain = channelState->volMod;
            source->pan = channelState->pan;
            source->reverb = channelState->reverb;
            return true;
        }
    }

    return false;
}

static float AudioSpatial_GetObjectPcmGain(const NoteSubEu* noteSub) {
    u16 maxPanVol;
    float gain;

    if (noteSub == NULL) {
        return 0.0f;
    }

    maxPanVol = noteSub->panVolLeft;
    if (noteSub->panVolRight > maxPanVol) {
        maxPanVol = noteSub->panVolRight;
    }
    if (noteSub->panVolCenter > maxPanVol) {
        maxPanVol = noteSub->panVolCenter;
    }
    if (noteSub->panVolRLeft > maxPanVol) {
        maxPanVol = noteSub->panVolRLeft;
    }
    if (noteSub->panVolRRight > maxPanVol) {
        maxPanVol = noteSub->panVolRRight;
    }

    gain = maxPanVol * (1.0f / 4095.999f);
    if (gain < 0.0f) {
        gain = 0.0f;
    } else if (gain > 4.0f) {
        gain = 4.0f;
    }
    return gain;
}

bool AudioSpatial_TrySubmitNoteObjectPcm(s32 noteIndex, NoteSubEu* noteSub, u16 dmemSrc, s32 frameCount) {
    u8 channelIndex;
    StarshipSpatialAudioObjectPcm objectPcm = { 0 };

    if ((frameCount <= 0) || (frameCount > SPATIAL_OBJECT_PCM_MAX_FRAMES) ||
        !AudioPlayerSupportsSpatialAudioObjectPcm()) {
        return false;
    }

    if (!AudioSpatial_GetSfxChannelIndexForNote(noteIndex, &channelIndex)) {
        return false;
    }

    if (!AudioSpatial_FindSourceForSfxChannel(channelIndex, &objectPcm.source)) {
        return false;
    }

    objectPcm.sampleGain = AudioSpatial_GetObjectPcmGain(noteSub);
    if (objectPcm.sampleGain <= 0.0f) {
        return false;
    }

    AudioMixer_CopyDmemS16(dmemSrc, sSpatialObjectPcmScratch, frameCount);
    objectPcm.frameId = sAudioFrameCounter;
    objectPcm.frameCount = frameCount;
    objectPcm.samples = sSpatialObjectPcmScratch;
    return AudioPlayerSubmitSpatialAudioObjectPcm(&objectPcm);
}
