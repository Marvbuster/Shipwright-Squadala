/**
 * C bridge for LiveGen entrance override.
 * Called from z_play.c (C code) to intercept door transitions.
 */

#include "LiveGenEntrance.h"
#include <cstdint>

extern "C" {

int16_t LiveGen_ProcessEntrance(int16_t entrance) {
    return LiveGen::EntranceManager::Instance().ProcessEntrance(entrance);
}

}
