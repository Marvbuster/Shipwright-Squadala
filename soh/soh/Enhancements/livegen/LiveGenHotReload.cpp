#include "LiveGenHotReload.h"
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

namespace LiveGen {

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

    // 1. Add the new archive (or re-add if already present)
    //    DON'T unload resources yet — that crashes if we're currently in the scene!
    //    Just add the archive. When we transition to a NEW scene load,
    //    SoH will pick up the overriding archive because it was added LAST.
    auto archiveMgr = resMgr->GetArchiveManager();
    if (archiveMgr) {
        archiveMgr->AddArchive(o2rPath);
        SPDLOG_INFO("LiveGen: Hot-reload — added archive: {}", o2rPath);
    }

    SPDLOG_INFO("LiveGen: Hot-reload ready! Walk through a door to enter the dungeon.");
    return true;
}

} // namespace LiveGen
