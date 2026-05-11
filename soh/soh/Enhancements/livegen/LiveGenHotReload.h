#pragma once

#include <string>
#include <cstdint>

namespace LiveGen {

// What kind of debug scene is currently overriding the vanilla scene load.
// DUNGEON         — full 3-room Squadala dungeon (chests/actors/decorations).
// MESH_LAB        — empty single-room sandbox for iterating on custom geometry.
// CUSTOM_DUNGEON  — the simple "custom dungeon" target the entry-portal warps
//                   into. Distinct namespace so loading it never evicts the
//                   3-room debug or any mesh lab.
enum class DebugMode {
    OFF,
    DUNGEON,
    MESH_LAB,
    CUSTOM_DUNGEON,
};

bool HotReloadDungeon(const std::string& o2rPath);
bool HotReloadMeshLab(const std::string& o2rPath);
bool HotReloadCustomDungeon(const std::string& o2rPath);

void WarpToEntrance(int16_t entranceIndex);

void SetDebugMode(DebugMode mode);
DebugMode GetDebugMode();

// Legacy predicate kept for the broader call sites (door cleanup, scene
// redirect predicate). True for any non-OFF mode.
bool IsDebugRoomActive();

// Returns the namespace prefix (e.g. "scenes/squadala") for the active mode,
// or "" when OFF. Used by the scene-load redirect.
const char* GetDebugScenePath();

// Door_Warp1-based portal flow. Spawns the iconic blue light pillar a few
// units in front of Link, swings the camera onto it via OnePointCutscene,
// plays the puzzle-solved chime, and snapshots his current location so the
// exit portal (auto-spawned inside the dungeon) can warp him back to it.
void SpawnEntryPortal();
void SpawnExitPortal(float x, float y, float z);

} // namespace LiveGen
