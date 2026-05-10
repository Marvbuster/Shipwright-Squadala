// LiveGen scene decorations — extra DLs drawn per-frame with custom transforms.
// Currently: a spinning Pizza showcase above the chest in the debug room.
// (Mario lives inside the chest now — see LiveGenItemRegistry.cpp.)

#include "LiveGenHotReload.h"
#include "global.h"

#include <spdlog/spdlog.h>
#include <chrono>

// Use ResourceMgr_LoadGfxByName directly to resolve __OTR__ paths.
// We can't rely on the gSPDisplayList wrapper because in C++ scope the macro
// from gbi.h expands first, bypassing GbiWrap.cpp's interception.
extern "C" Gfx* ResourceMgr_LoadGfxByName(const char* path);

// Rotation tied to wall-clock — gameplayFrames in SoH ticks at render rate
// (interpolated), which made it run ~10× faster than expected. Wall-clock
// gives stable real-time rotation regardless of render FPS.
// 0x10000 (full turn) per 16384 ms → ~16 sec/rotation. Step = 4 per ms.
constexpr u32 ROT_STEP_PER_MS = 4;

// Pizza floats above the chest (chest sits at world Y ≈ -100). Y=80 keeps it
// just above the chest top so it reads as part of the chest showcase, not a
// distant ornament.
constexpr f32 PIZZA_X = 0.0f;
constexpr f32 PIZZA_Y = 80.0f;
constexpr f32 PIZZA_Z = 0.0f;
// Half-turn offset on the Y rotation so the pizza's top side (not its
// underside) is what faces the camera at t=0.
constexpr s16 PIZZA_ROT_OFFSET_Y = (s16)0x8000;

// Path to the Pizza DL resource in our .o2r — resolved via GbiWrap __OTR__ mechanism.
// The model already has its 60° X-tilt baked in by mesh_to_dl.py, so we only
// apply translate + Y-spin here.
static const char* PIZZA_DL_PATH = "__OTR__scenes/squadala/pizza_DL";

// The pizza decoration is anchored to Room 0 (where the Mario chest lives).
// Drawing it in Room 1/2 leaves a phantom spinning pizza floating in the
// adjacent room when curRoom culls Room 0's mesh.
constexpr s8 PIZZA_ROOM = 0;

extern "C" void LiveGen_DrawSpinningMario(PlayState* play) {
    if (!LiveGen::IsDebugRoomActive()) {
        return;
    }
    if (play == nullptr || play->state.gfxCtx == nullptr) {
        return;
    }
    // Don't draw during scene transitions — current room not initialised
    // yet. We accept either segment being non-null because mid-transition
    // the engine briefly leaves curRoom.segment = NULL while loading the
    // new room, even though prevRoom (still rendered) is fully ready.
    if (play->roomCtx.curRoom.segment == NULL &&
        play->roomCtx.prevRoom.segment == NULL) {
        return;
    }
    // Only draw the pizza in its anchor room. We accept prevRoom too so
    // it stays visible during room transitions when curRoom has flipped
    // to the room being loaded but Room 0 is still on screen as the
    // outgoing side.
    if (play->roomCtx.curRoom.num != PIZZA_ROOM &&
        play->roomCtx.prevRoom.num != PIZZA_ROOM) {
        return;
    }

    // Tie rotation to wall-clock — independent of any frame/tick counter.
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count();
    s16 rotation_y = (s16)(((ms * ROT_STEP_PER_MS) & 0xFFFF) + PIZZA_ROT_OFFSET_Y);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(PIZZA_X, PIZZA_Y, PIZZA_Z, MTXMODE_NEW);
    // Matrix_RotateY takes RADIANS (f32) — passing an s16 binary angle directly
    // would interpret 31772 as 31772 radians (~5000 full turns). Use the s16-aware
    // Matrix_RotateZYX, which expects binary angles and does the conversion.
    Matrix_RotateZYX(0, rotation_y, 0, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++,
              Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
              G_MTX_MODELVIEW | G_MTX_LOAD);
    // Resolve __OTR__ path manually — the gSPDisplayList macro doesn't intercept in C++
    Gfx* dl = ResourceMgr_LoadGfxByName(PIZZA_DL_PATH);
    if (dl != nullptr) {
        gSPDisplayList(POLY_OPA_DISP++, dl);
    }

    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}
