#include "DiscordPresence.h"

#include "DiscordIpcClient.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "sys.h"
#include "sf64level.h"
#include "sf64player.h"

extern "C" {
extern GameState gGameState;
extern LevelId gCurrentLevel;
extern s32 gPlayerNum;
extern Player* gPlayer;
extern s32 gHitCount;
extern s32 gTotalHits;
extern s16 gLifeCount[4];
extern s32 gBombCount[4];
extern LaserStrength gLaserStrength[4];
extern u8 gGoldRingCount[4];
extern bool gVersusMode;
s32 Play_GetMaxShields(void);
}

namespace Starship {
namespace {

using Clock = std::chrono::steady_clock;

struct PresenceSnapshot {
    std::string details;
    std::string state;
    std::string largeImage;
    std::string smallImage;
    std::string largeText;
    std::string smallText;
    std::string activityKey;
};

constexpr auto RETRY_INTERVAL = std::chrono::seconds(5);
constexpr auto REFRESH_INTERVAL = std::chrono::seconds(15);
constexpr bool ENABLE_DISCORD_ASSETS = false;

DiscordIpcClient sClient;
Clock::time_point sNextConnectAttempt;
Clock::time_point sNextRefresh;
std::string sLastActivityJson;
std::string sLastActivityKey;
int64_t sActivityStartUnixSeconds = 0;
bool sInitialized = false;
bool sLoggedDisabled = false;
bool sLoggedUnavailable = false;
std::string sClientId;

std::string JsonEscape(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

void AppendJsonString(std::string& json, const char* key, const std::string& value) {
    if (value.empty()) {
        return;
    }
    json += ",\"";
    json += key;
    json += "\":\"";
    json += JsonEscape(value);
    json += "\"";
}

const char* LevelName(LevelId level) {
    switch (level) {
        case LEVEL_CORNERIA:
            return "Corneria";
        case LEVEL_METEO:
            return "Meteo";
        case LEVEL_SECTOR_X:
            return "Sector X";
        case LEVEL_AREA_6:
            return "Area 6";
        case LEVEL_SECTOR_Y:
            return "Sector Y";
        case LEVEL_VENOM_1:
            return "Venom 1";
        case LEVEL_SOLAR:
            return "Solar";
        case LEVEL_ZONESS:
            return "Zoness";
        case LEVEL_VENOM_ANDROSS:
            return "Andross";
        case LEVEL_TRAINING:
            return "Training";
        case LEVEL_MACBETH:
            return "Macbeth";
        case LEVEL_TITANIA:
            return "Titania";
        case LEVEL_AQUAS:
            return "Aquas";
        case LEVEL_FORTUNA:
            return "Fortuna";
        case LEVEL_KATINA:
            return "Katina";
        case LEVEL_BOLSE:
            return "Bolse";
        case LEVEL_SECTOR_Z:
            return "Sector Z";
        case LEVEL_VENOM_2:
            return "Venom 2";
        case LEVEL_VERSUS:
            return "Versus";
        case LEVEL_WARP_ZONE:
            return "Warp Zone";
        default:
            return "Unknown Level";
    }
}

const char* LaserName(LaserStrength laser) {
    switch (laser) {
        case LASERS_SINGLE:
            return "Single";
        case LASERS_TWIN:
            return "Twin";
        case LASERS_HYPER:
            return "Hyper";
        case LASERS_UNK_3:
            return "Ultra";
        default:
            return "Unknown";
    }
}

const char* FormName(PlayerForm form) {
    switch (form) {
        case FORM_ARWING:
            return "Arwing";
        case FORM_LANDMASTER:
            return "Landmaster";
        case FORM_BLUE_MARINE:
            return "Blue Marine";
        case FORM_ON_FOOT:
            return "On Foot";
        default:
            return "Vehicle";
    }
}

const char* FormAsset(PlayerForm form) {
    switch (form) {
        case FORM_ARWING:
            return "arwing";
        case FORM_LANDMASTER:
            return "landmaster";
        case FORM_BLUE_MARINE:
            return "blue_marine";
        default:
            return "starship";
    }
}

int ClampPlayerIndex() {
    return std::clamp(gPlayerNum, 0, 3);
}

bool HasGameplayHudState() {
    return gPlayer != nullptr;
}

PresenceSnapshot BuildGameplaySnapshot() {
    const int playerIndex = ClampPlayerIndex();
    const Player& player = gPlayer[playerIndex];
    const int maxShields = std::max(1, Play_GetMaxShields());
    const int shields = std::clamp(player.shields, 0, maxShields);
    const int lives = std::clamp(static_cast<int>(gLifeCount[playerIndex]), 0, 99);
    const int bombs = std::clamp(gBombCount[playerIndex], 0, 99);
    const int rings = std::clamp(static_cast<int>(gGoldRingCount[0]), 0, 99);
    const int hits = std::max(0, gHitCount);
    const char* levelName = LevelName(gCurrentLevel);
    const char* formName = FormName(player.form);

    PresenceSnapshot snapshot;
    snapshot.details = std::string(levelName) + " | Hits " + std::to_string(hits);
    snapshot.state = "Lives " + std::to_string(lives) + " | HP " + std::to_string(shields) + "/" +
                     std::to_string(maxShields) + " | Bombs " + std::to_string(bombs) + " | Laser " +
                     LaserName(gLaserStrength[playerIndex]) + " | Rings " + std::to_string(rings);
    snapshot.largeImage = "starship";
    snapshot.largeText = levelName;
    snapshot.smallImage = FormAsset(player.form);
    snapshot.smallText = formName;
    snapshot.activityKey = std::string("play:") + levelName;
    return snapshot;
}

PresenceSnapshot BuildMenuSnapshot(std::string details, std::string state, std::string key) {
    PresenceSnapshot snapshot;
    snapshot.details = std::move(details);
    snapshot.state = std::move(state);
    snapshot.largeImage = "starship";
    snapshot.largeText = "Starship";
    snapshot.activityKey = std::move(key);
    return snapshot;
}

PresenceSnapshot BuildSnapshot() {
    switch (gGameState) {
        case GSTATE_TITLE:
            return BuildMenuSnapshot("At the Title Screen", "Preparing for launch", "title");
        case GSTATE_MENU:
            return BuildMenuSnapshot("In the Main Menu", "Configuring mission", "menu");
        case GSTATE_MAP:
            return BuildMenuSnapshot("On the Lylat Map", "Total Hits " + std::to_string(std::max(0, gTotalHits)), "map");
        case GSTATE_GAME_OVER:
            return BuildMenuSnapshot("Game Over", "Total Hits " + std::to_string(std::max(0, gTotalHits)), "game_over");
        case GSTATE_VS_INIT:
            return BuildMenuSnapshot("Starting Versus", "Preparing match", "versus_init");
        case GSTATE_ENDING:
            return BuildMenuSnapshot("Ending", "Total Hits " + std::to_string(std::max(0, gTotalHits)), "ending");
        case GSTATE_PLAY:
            if (!HasGameplayHudState()) {
                return BuildMenuSnapshot("Loading Mission", "Preparing HUD", "play_loading");
            }
            if (gVersusMode) {
                PresenceSnapshot snapshot = BuildGameplaySnapshot();
                snapshot.details = std::string("Versus | ") + LevelName(gCurrentLevel);
                snapshot.activityKey = std::string("versus:") + LevelName(gCurrentLevel);
                return snapshot;
            }
            return BuildGameplaySnapshot();
        default:
            return BuildMenuSnapshot("Starting Starship", "Preparing for launch", "boot");
    }
}

std::string BuildActivityJson(const PresenceSnapshot& snapshot) {
    std::string json = "{\"type\":0";
    AppendJsonString(json, "details", snapshot.details);
    AppendJsonString(json, "state", snapshot.state);
    json += ",\"timestamps\":{\"start\":";
    json += std::to_string(sActivityStartUnixSeconds);
    json += "}";

    if (!snapshot.largeImage.empty() || !snapshot.smallImage.empty()) {
        if (!ENABLE_DISCORD_ASSETS) {
            json += "}";
            return json;
        }

        json += ",\"assets\":{";
        bool needsComma = false;
        auto appendAsset = [&](const char* key, const std::string& value) {
            if (value.empty()) {
                return;
            }
            if (needsComma) {
                json += ",";
            }
            needsComma = true;
            json += "\"";
            json += key;
            json += "\":\"";
            json += JsonEscape(value);
            json += "\"";
        };
        appendAsset("large_image", snapshot.largeImage);
        appendAsset("large_text", snapshot.largeText);
        appendAsset("small_image", snapshot.smallImage);
        appendAsset("small_text", snapshot.smallText);
        json += "}";
    }

    json += "}";
    return json;
}

int64_t CurrentUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool EnsureConnected(Clock::time_point now) {
#if defined(STARSHIP_ENABLE_DISCORD_PRESENCE)
    if (sClientId.empty()) {
        return false;
    }
    if (sClient.IsConnected()) {
        return true;
    }
    if (now < sNextConnectAttempt) {
        return false;
    }

    sNextConnectAttempt = now + RETRY_INTERVAL;
    if (sClient.Connect(sClientId.c_str())) {
        sLoggedUnavailable = false;
        return true;
    }

    if (!sLoggedUnavailable) {
        SPDLOG_INFO("Discord Rich Presence unavailable; will retry while Starship is running");
        sLoggedUnavailable = true;
    }
    return false;
#else
    return false;
#endif
}

std::string ResolveClientId() {
#if defined(STARSHIP_ENABLE_DISCORD_PRESENCE)
    const char* envClientId = getenv("STARSHIP_DISCORD_CLIENT_ID");
    if ((envClientId != nullptr) && (envClientId[0] != '\0')) {
        return envClientId;
    }

    return STARSHIP_DISCORD_CLIENT_ID;
#else
    return {};
#endif
}

} // namespace

void DiscordPresence_Init() {
#if defined(STARSHIP_ENABLE_DISCORD_PRESENCE)
    if (sInitialized) {
        return;
    }
    sClientId = ResolveClientId();
    if (sClientId.empty()) {
        if (!sLoggedDisabled) {
            SPDLOG_INFO("Discord Rich Presence disabled: STARSHIP_DISCORD_CLIENT_ID is unset");
            sLoggedDisabled = true;
        }
        return;
    }

    sInitialized = true;
    sActivityStartUnixSeconds = CurrentUnixSeconds();
    sNextConnectAttempt = Clock::time_point {};
    sNextRefresh = Clock::time_point {};
    SPDLOG_INFO("Discord Rich Presence enabled");
#else
    if (!sLoggedDisabled) {
        SPDLOG_INFO("Discord Rich Presence disabled: STARSHIP_DISCORD_CLIENT_ID is unset");
        sLoggedDisabled = true;
    }
#endif
}

void DiscordPresence_Update() {
#if defined(STARSHIP_ENABLE_DISCORD_PRESENCE)
    if (!sInitialized) {
        return;
    }

    const Clock::time_point now = Clock::now();
    PresenceSnapshot snapshot = BuildSnapshot();
    if (snapshot.activityKey != sLastActivityKey) {
        sLastActivityKey = snapshot.activityKey;
        sActivityStartUnixSeconds = CurrentUnixSeconds();
        sLastActivityJson.clear();
        sNextRefresh = Clock::time_point {};
    }

    const std::string activityJson = BuildActivityJson(snapshot);
    const bool shouldPublish = (activityJson != sLastActivityJson) || (now >= sNextRefresh);

    if (!shouldPublish) {
        sClient.Pump();
        return;
    }

    if (!EnsureConnected(now)) {
        return;
    }

    const bool contentChanged = activityJson != sLastActivityJson;
    if (sClient.SetActivity(activityJson)) {
        if (contentChanged) {
            SPDLOG_INFO("Discord Rich Presence published: {} / {}", snapshot.details, snapshot.state);
        }
        sLastActivityJson = activityJson;
        sNextRefresh = now + REFRESH_INTERVAL;
    } else {
        sNextConnectAttempt = now + RETRY_INTERVAL;
    }
#endif
}

void DiscordPresence_Shutdown() {
#if defined(STARSHIP_ENABLE_DISCORD_PRESENCE)
    if (sClient.IsConnected()) {
        sClient.SetActivity("null");
    }
    sClient.Disconnect();
    sInitialized = false;
    sLastActivityJson.clear();
    sLastActivityKey.clear();
    sClientId.clear();
#endif
}

} // namespace Starship
