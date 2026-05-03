#pragma once

#include <string>
#include <cstdint>

namespace LiveGen {

/**
 * Hot-reload a dungeon .o2r at runtime.
 * Adds the archive and evicts cached resources so the next
 * scene load picks up the overriding data.
 */
bool HotReloadDungeon(const std::string& o2rPath);

/**
 * Warp the player to a specific entrance immediately.
 * Triggers a fade-to-black transition.
 */
void WarpToEntrance(int16_t entranceIndex);

/**
 * Debug room active state — blocks room loads for rooms != 0.
 */
void SetDebugRoomActive(bool active);
bool IsDebugRoomActive();

} // namespace LiveGen
