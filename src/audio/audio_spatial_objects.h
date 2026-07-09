#pragma once

#include "sf64audio_provisional.h"
#include <stdbool.h>

bool AudioSpatial_TrySubmitNoteObjectPcm(s32 noteIndex, NoteSubEu* noteSub, u16 dmemSrc, s32 frameCount);
