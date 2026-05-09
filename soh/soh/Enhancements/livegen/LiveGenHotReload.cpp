#include "LiveGenHotReload.h"
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

extern "C" {
#include "z64.h"
extern PlayState* gPlayState;
}

namespace LiveGen {

static bool sDebugRoomActive = false;

void SetDebugRoomActive(bool active) {
    sDebugRoomActive = active;
    SPDLOG_INFO("LiveGen: Debug room active = {}", active);
}

bool IsDebugRoomActive() {
    return sDebugRoomActive;
}

bool HotReloadDungeon(const std::string& o2rPath) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) {
        SPDLOG_ERROR("LiveGen: No Ship context for hot reload");
        return false;
    }

    auto resMgr = ctx->GetResourceManager();
    if (!resMgr) {
        SPDLOG_ERROR("LiveGen: No ResourceManager for hot reload");
        return false;
    }

    // 1. Add the new archive
    auto archiveMgr = resMgr->GetArchiveManager();
    if (archiveMgr) {
        archiveMgr->AddArchive(o2rPath);
        SPDLOG_INFO("LiveGen: Hot-reload — added archive: {}", o2rPath);
    }

    // 2. Evict + preload all our scene/room/asset resources. Squadala paths
    //    live entirely under `scenes/squadala/` — no overlap with vanilla, so
    //    eviction is only needed for hot-reload (rebuilding the .o2r between
    //    Debug-Room clicks); first-click is always a cache miss.
    std::vector<std::string> paths = {
        "scenes/squadala/dungeon_scene",
        "scenes/squadala/collision",
        "scenes/squadala/room_0",
        "scenes/squadala/room_1",
        "scenes/squadala/room0_DL",
        "scenes/squadala/room0_Vtx",
        "scenes/squadala/room1_DL",
        "scenes/squadala/room1_Vtx",
        "scenes/squadala/mario_DL",
        "scenes/squadala/mario_Vtx",
        "scenes/squadala/pizza_DL",
        "scenes/squadala/pizza_Vtx",
    };

    for (const auto& path : paths) {
        resMgr->UnloadResource(path);
    }
    for (const auto& path : paths) {
        auto resource = resMgr->LoadResourceProcess(path);
        SPDLOG_INFO("LiveGen: Preloaded {} — ok={}", path, resource != nullptr);
    }

    return true;
}

void WarpToEntrance(int16_t entranceIndex) {
    if (!gPlayState) {
        SPDLOG_ERROR("LiveGen: No PlayState for warp");
        return;
    }

    gPlayState->roomCtx.prevRoom.segment = NULL;
    gPlayState->roomCtx.curRoom.segment = NULL;

    gPlayState->nextEntranceIndex = entranceIndex;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
    SPDLOG_INFO("LiveGen: Warping to entrance {:#06x} (rooms cleared)", entranceIndex);
}

} // namespace LiveGen

// C bridge for z_scene_otr.cpp
extern "C" bool LiveGen_IsDebugRoomActive() {
    return LiveGen::IsDebugRoomActive();
}
