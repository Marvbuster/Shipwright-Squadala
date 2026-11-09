// LiveGen scene decorations — extra DLs drawn per-frame with custom transforms.
// Each entry is anchored to a single room and gated by curRoom/prevRoom so it
// only appears while its anchor room is on screen. Mario lives inside the
// chest (see LiveGenItemRegistry.cpp), not here.

#include "LiveGenHotReload.h"
#include "global.h"

#include <spdlog/spdlog.h>
#include <chrono>
#include <cstring>

// Use ResourceMgr_LoadGfxByName directly to resolve __OTR__ paths.
// We can't rely on the gSPDisplayList wrapper because in C++ scope the macro
// from gbi.h expands first, bypassing GbiWrap.cpp's interception.
extern "C" Gfx* ResourceMgr_LoadGfxByName(const char* path);

// Rotation tied to wall-clock — gameplayFrames in SoH ticks at render rate
// (interpolated), which made it run ~10× faster than expected. Wall-clock
// gives stable real-time rotation regardless of render FPS.
// 0x10000 (full turn) per 16384 ms → ~16 sec/rotation. Step = 4 per ms.
constexpr u32 ROT_STEP_PER_MS = 4;

struct RoomDecoration {
    f32 x_min, x_max;   // world X-bounds — only drawn when Link's x is inside
    f32 x, y, z;        // world-space pivot
    s16 rot_y_offset;   // baked-in rotation offset added to the wall-clock spin
    bool spin;          // true → wall-clock Y-rotation; false → static (rot_y_offset only)
    const char* dl_path;
};

// TODO(deco-registry, M8+ scope): drop the hard-coded ranges below. The
// LLM-authored dungeons won't have a fixed 3-room layout, so this needs
// to be data-driven — either emitted as a layout table at build time
// or expressed as honest in-engine actors whose room/cull semantics the
// vanilla actor system already handles for us. See docs/BACKLOG.md
// "Dynamic Deco-Registry".
//
// For the M7 debug scene we filter by Link's world-X rather than the
// engine's curRoom/prevRoom — curRoom flips to the destination room
// before Link has actually crossed En_Holl, which caused the Room-0
// pizza to appear in Room 1 for one frame and the Room-1 cupcake to
// blink out on the return trip. Link's world.pos.x is monotonic with
// the physical motion, so checking it directly is jitter-free.
//
// Ranges overlap by the door-peek-through width: as Link approaches the
// shared wall from Room 0 (e.g. x ≈ 400) the cupcake is already visible
// through the open door, and the pizza stays visible when peeking back
// from just inside Room 1 (e.g. x ≈ 800).
constexpr RoomDecoration DECORATIONS[] = {
    // Room 0 — pizza (60° X-tilt baked into the mesh, half-turn Y so the
    // top faces the camera at t=0). World X extends a bit past the shared
    // wall at x=+600 so Link can still glance back at the pizza from just
    // inside Room 1.
    {-1800.0f,  900.0f,    0.0f, 80.0f, 0.0f, (s16)0x8000, true,
     "__OTR__scenes/squadala/pizza_DL"},
    // Room 1 — cupcake. Mesh already upright. World X starts a bit before
    // the shared wall so the cupcake peeks through the doorway as Link
    // approaches from Room 0.
    {300.0f, 2800.0f, 1200.0f, 80.0f, 0.0f, 0, true,
     "__OTR__scenes/squadala/cupcake_DL"},
    // Room 2 — mouse. Sits on the floor at the room's centre. Static
    // (no spin) — a mouse twirling in mid-air would read as a glitch.
    // Animation comes with M11 (skeletal pipeline).
    {-2800.0f, -300.0f, -1200.0f, -100.0f, 0.0f, 0, false,
     "__OTR__scenes/squadala/mouse_DL"},
};
constexpr size_t NUM_DECORATIONS = sizeof(DECORATIONS) / sizeof(DECORATIONS[0]);

extern "C" void LiveGen_DrawSpinningMario(PlayState* play) {
    if (play == nullptr || play->state.gfxCtx == nullptr) {
        return;
    }

    // Mesh-lab-only decorations branch off first — they're cheaper and
    // independent of the 3-room dungeon's decoration set. Currently just
    // the vanilla Forest Temple ladder DL placed in the two-level lab.
    if (LiveGen::GetDebugMode() == LiveGen::DebugMode::MESH_LAB) {
        const char* labName = LiveGen::GetCurrentLabName();
        if (labName && std::strcmp(labName, "Two Level") == 0) {
            Gfx* ladderDL = ResourceMgr_LoadGfxByName(
                "__OTR__objects/object_mori_objects/gMoriHashigoLadderDL");
            if (ladderDL) {
                OPEN_DISPS(play->state.gfxCtx);
                Matrix_Push();
                // Position against the platform's south face. Platform
                // top is at y=+100, main floor at y=-100. Ladder DL sits
                // flush with the wall plane (z=-200) and bottom on the
                // main floor (y=-100). Center on x=-400 (middle of the
                // platform's south face, x∈[-700,-100]).
                Matrix_Translate(-400.0f, -100.0f, -200.0f, MTXMODE_NEW);
                gSPMatrix(POLY_OPA_DISP++,
                          Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                          G_MTX_MODELVIEW | G_MTX_LOAD);
                gSPDisplayList(POLY_OPA_DISP++, ladderDL);
                Matrix_Pop();
                CLOSE_DISPS(play->state.gfxCtx);
            }
        }
        return;
    }

    // The remaining decorations (Pizza/Cupcake/Mouse) are dungeon-only.
    if (LiveGen::GetDebugMode() != LiveGen::DebugMode::DUNGEON) {
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

    // Tie rotation to wall-clock — independent of any frame/tick counter.
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count();
    s16 base_rot = (s16)((ms * ROT_STEP_PER_MS) & 0xFFFF);

    OPEN_DISPS(play->state.gfxCtx);

    Player* player = GET_PLAYER(play);
    f32 linkX = player ? player->actor.world.pos.x : 0.0f;

    for (size_t i = 0; i < NUM_DECORATIONS; ++i) {
        const RoomDecoration& deco = DECORATIONS[i];
        if (linkX < deco.x_min || linkX > deco.x_max) {
            continue;
        }

        Gfx* dl = ResourceMgr_LoadGfxByName(deco.dl_path);
        if (dl == nullptr) {
            continue;
        }

        s16 rot_y = deco.spin ? (s16)(base_rot + deco.rot_y_offset)
                              : deco.rot_y_offset;

        Matrix_Push();
        Matrix_Translate(deco.x, deco.y, deco.z, MTXMODE_NEW);
        // Matrix_RotateY takes RADIANS — Matrix_RotateZYX accepts the s16
        // binary-angle representation and does the conversion internally.
        Matrix_RotateZYX(0, rot_y, 0, MTXMODE_APPLY);

        gSPMatrix(POLY_OPA_DISP++,
                  Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dl);

        Matrix_Pop();
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
