#pragma once

#include "sf64audio_provisional.h"
#include "public/bridge/audiobridge.h"
#include <stdbool.h>

#define AUDIO_SPATIAL_DEBUG_MAX_SOURCES STARSHIP_SPATIAL_AUDIO_MAX_SOURCES

// Snapshot of one live positioned SFX, refreshed each audio tick for the debug visualizer.
typedef struct AudioSpatialDebugSource {
    u32 instanceId;
    u32 sfxId;
    f32 worldX;
    f32 worldY;
    f32 worldZ;
    u8 hasWorldPos;
    u8 onObjectBus;
} AudioSpatialDebugSource;

void AudioSpatial_SetSourceListenerRelativePosition(f32* sfxSource, f32 x, f32 y, f32 z);
void AudioSpatial_ClearSourceListenerRelativePosition(f32* sfxSource);
bool AudioSpatial_GetSourceListenerRelativePosition(f32* sfxSource, f32* x, f32* y, f32* z);
void AudioSpatial_SetSourceWorldPosition(f32* sfxSource, f32 x, f32 y, f32 z);
bool AudioSpatial_GetSourceWorldPosition(f32* sfxSource, f32* x, f32* y, f32* z);
bool AudioSpatial_TrySubmitNoteObjectPcm(s32 noteIndex, NoteSubEu* noteSub, u16 dmemSrc, s32 frameCount);
u32 AudioSpatialDebug_GetSnapshot(AudioSpatialDebugSource* out, u32 maxCount);
