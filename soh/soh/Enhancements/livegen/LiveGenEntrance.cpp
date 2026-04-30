#include "LiveGenEntrance.h"
#include "soh/Notification/Notification.h"
#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
extern PlayState* gPlayState;
}

namespace LiveGen {

EntranceManager& EntranceManager::Instance() {
    static EntranceManager instance;
    return instance;
}

void EntranceManager::Activate(int16_t targetEntrance, const std::string& dungeonName) {
    mTargetEntrance = targetEntrance;
    mDungeonName = dungeonName;
    mActive = true;
    SPDLOG_INFO("LiveGen: Portal ACTIVE — next door → {} (entrance {:#06x})", dungeonName, targetEntrance);
}

void EntranceManager::Deactivate() {
    mActive = false;
    SPDLOG_INFO("LiveGen: Portal deactivated");
}

bool EntranceManager::IsActive() const {
    return mActive;
}

int16_t EntranceManager::ProcessEntrance(int16_t originalEntrance) {
    if (!mActive) {
        return originalEntrance;
    }

    // One-shot: redirect this one transition, then deactivate
    mActive = false;
    SPDLOG_INFO("LiveGen: PORTAL! Redirecting {:#06x} → {:#06x}", originalEntrance, mTargetEntrance);

    // Show dungeon name notification
    Notification::Emit({
        .prefix = "Entering",
        .prefixColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
        .message = mDungeonName,
        .messageColor = ImVec4(0.2f, 1.0f, 0.4f, 1.0f),
        .remainingTime = 5.0f,
    });

    return mTargetEntrance;
}

} // namespace LiveGen
