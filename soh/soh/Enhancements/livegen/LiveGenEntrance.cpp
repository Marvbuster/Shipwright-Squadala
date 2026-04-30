#include "LiveGenEntrance.h"
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

void EntranceManager::Activate(int16_t targetEntrance) {
    mTargetEntrance = targetEntrance;
    mActive = true;
    SPDLOG_INFO("LiveGen: Portal ACTIVE — next door → entrance {:#06x}", targetEntrance);
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
    return mTargetEntrance;
}

} // namespace LiveGen
