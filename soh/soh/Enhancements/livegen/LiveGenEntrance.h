#pragma once

#include <cstdint>
#include <atomic>

namespace LiveGen {

/**
 * Manages the entrance override for generated dungeons.
 * When active, the next door transition redirects to our dungeon scene.
 */
class EntranceManager {
  public:
    static EntranceManager& Instance();

    // Activate: next door transition goes to our dungeon
    void Activate(int16_t targetEntrance);

    // Deactivate: doors work normally again
    void Deactivate();

    // Check if active
    bool IsActive() const;

    // Called from the entrance system — returns overridden entrance or original
    int16_t ProcessEntrance(int16_t originalEntrance);

  private:
    std::atomic<bool> mActive{false};
    int16_t mTargetEntrance = 0;
};

} // namespace LiveGen
