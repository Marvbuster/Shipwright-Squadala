/**
 * C bridge for LiveGen entrance override.
 * Called from z_play.c (C code) to intercept door transitions.
 */

#include "LiveGenEntrance.h"
#include "LiveGenHotReload.h"
#include <cstdint>

extern "C" {

int16_t LiveGen_ProcessEntrance(int16_t entrance) {
    bool wasActive = LiveGen::EntranceManager::Instance().IsActive();
    int16_t result = LiveGen::EntranceManager::Instance().ProcessEntrance(entrance);
    // Mode flip happens HERE, not at the "Enter Dungeon" button click, so the
    // lab scene (which the player is still in) doesn't start rendering
    // dungeon decorations before the actual transition fires. Only triggers
    // when the portal redirected an entrance (active + result differs).
    if (wasActive && result != entrance) {
        LiveGen::SetDebugMode(LiveGen::DebugMode::CUSTOM_DUNGEON);
    }
    return result;
}

}
