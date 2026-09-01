#pragma once

#include "global.h"
#include "port/hooks/impl/EventSystem.h"

DEFINE_EVENT(DisplayPreUpdateEvent);
DEFINE_EVENT(DisplayPostUpdateEvent);
// Fires inside Display_Update's camera (LookAt) matrix scope, after all world geometry — for
// overlays that must land in world space.
DEFINE_EVENT(DisplayWorldOverlayEvent);

DEFINE_EVENT(GamePreUpdateEvent);
DEFINE_EVENT(GamePostUpdateEvent);

DEFINE_EVENT(PlayUpdateEvent);

DEFINE_EVENT(PlayerPreUpdateEvent, Player* player;);
DEFINE_EVENT(PlayerPostUpdateEvent, Player* player;);

DEFINE_EVENT(DrawRadarHUDEvent);
DEFINE_EVENT(DrawRadarMarkArwingEvent, s32 colorIdx;);
DEFINE_EVENT(DrawRadarMarkWolfenEvent);
DEFINE_EVENT(DrawBoostGaugeHUDEvent);
DEFINE_EVENT(DrawBombCounterHUDEvent);
DEFINE_EVENT(DrawIncomingMsgHUDEvent);
DEFINE_EVENT(PreSetupRadioMsgEvent, s32* radioRedBox;);
DEFINE_EVENT(DrawGoldRingsHUDEvent);
DEFINE_EVENT(DrawLivesCounterHUDEvent);
DEFINE_EVENT(DrawTrainingRingPassCountHUDEvent);
DEFINE_EVENT(DrawEdgeArrowsHUDEvent);
DEFINE_EVENT(DrawBossHealthHUDEvent);

DEFINE_EVENT(DrawGlobalHUDPreEvent);
DEFINE_EVENT(DrawGlobalHUDPostEvent);