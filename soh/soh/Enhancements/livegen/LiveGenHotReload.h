#pragma once

#include <string>

namespace LiveGen {

/**
 * Hot-reload a dungeon .o2r at runtime.
 * Unloads cached Deku Tree resources, adds the new archive,
 * so the next scene transition loads fresh data.
 */
bool HotReloadDungeon(const std::string& o2rPath);

} // namespace LiveGen
