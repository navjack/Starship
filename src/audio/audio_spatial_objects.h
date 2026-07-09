#pragma once

#include "sf64audio_provisional.h"
#include <stdbool.h>

void AudioSpatial_SetSourceShipRelativePosition(f32* sfxSource, f32 x, f32 y, f32 z);
void AudioSpatial_ClearSourceShipRelativePosition(f32* sfxSource);
bool AudioSpatial_GetSourceShipRelativePosition(f32* sfxSource, f32* x, f32* y, f32* z);
bool AudioSpatial_TrySubmitNoteObjectPcm(s32 noteIndex, NoteSubEu* noteSub, u16 dmemSrc, s32 frameCount);
