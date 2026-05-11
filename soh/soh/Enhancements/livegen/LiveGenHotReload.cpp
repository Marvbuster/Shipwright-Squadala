#include "LiveGenHotReload.h"
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include <cmath>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
extern PlayState* gPlayState;
extern SaveContext gSaveContext;
// Not declared in functions.h — only available via z_scene.c. Forward
// here so we can pre-load OBJECT_WARP1 before spawning Door_Warp1.
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);
}

// Door_Warp1 params — copy the constant instead of pulling in the actor's
// private header. WARP_DUNGEON_CHILD (=0) is the blue child-boss-clear warp:
// runs the float-up cutscene + white fade automatically, as long as we set
// the magic cutscene index 0xFFEF and a sane nextEntranceIndex.
#define LIVEGEN_WARP_DUNGEON_CHILD       0
// Magic value the warp's transition code checks before firing the post-
// cutscene scene change. Vanilla sets this on entry to boss scenes; we
// set it ourselves so the cutscene fires anywhere we spawn the warp.
#define LIVEGEN_NEXT_CUTSCENE_BLUE_WARP  0xFFEF
// Boss-clear BGM (NA_BGM_BOSS_CLEAR) — that swelling orchestral pad that
// plays while you're floating into the post-boss warp.
#define LIVEGEN_NA_BGM_BOSS_CLEAR        0x21
#define LIVEGEN_OBJECT_WARP1             0x48

namespace LiveGen {

static DebugMode sDebugMode = DebugMode::OFF;

void SetDebugMode(DebugMode mode) {
    sDebugMode = mode;
    SPDLOG_INFO("LiveGen: Debug mode = {}", static_cast<int>(mode));
}

DebugMode GetDebugMode() {
    return sDebugMode;
}

bool IsDebugRoomActive() {
    return sDebugMode != DebugMode::OFF;
}

const char* GetDebugScenePath() {
    switch (sDebugMode) {
        case DebugMode::DUNGEON:         return "scenes/squadala/dungeon_scene";
        case DebugMode::MESH_LAB:        return "scenes/squadala_mesh_lab/dungeon_scene";
        case DebugMode::CUSTOM_DUNGEON:  return "scenes/squadala_custom/dungeon_scene";
        default:                         return "";
    }
}

bool HotReloadCustomDungeon(const std::string& o2rPath) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) return false;
    auto resMgr = ctx->GetResourceManager();
    if (!resMgr) return false;

    auto archiveMgr = resMgr->GetArchiveManager();
    if (archiveMgr) {
        archiveMgr->AddArchive(o2rPath);
        SPDLOG_INFO("LiveGen: Hot-reload — added custom-dungeon archive: {}", o2rPath);
    }
    // Same no-evict policy as HotReloadMeshLab — eviction races the graphics
    // thread and crashes. AddArchive alone is enough on first load.
    return true;
}

// Evict + preload a list of resource paths so the next scene load picks
// up the archive contents instead of the cached vanilla data.
static void EvictAndPreload(const std::shared_ptr<Ship::ResourceManager>& resMgr,
                            const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        resMgr->UnloadResource(path);
    }
    for (const auto& path : paths) {
        auto resource = resMgr->LoadResourceProcess(path);
        SPDLOG_INFO("LiveGen: Preloaded {} — ok={}", path, resource != nullptr);
    }
}

bool HotReloadDungeon(const std::string& o2rPath) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) {
        SPDLOG_ERROR("LiveGen: No Ship context for hot reload");
        return false;
    }
    auto resMgr = ctx->GetResourceManager();
    if (!resMgr) {
        SPDLOG_ERROR("LiveGen: No ResourceManager for hot reload");
        return false;
    }

    auto archiveMgr = resMgr->GetArchiveManager();
    if (archiveMgr) {
        archiveMgr->AddArchive(o2rPath);
        SPDLOG_INFO("LiveGen: Hot-reload — added archive: {}", o2rPath);
    }

    // Squadala paths live entirely under `scenes/squadala/` — no overlap with
    // vanilla, so eviction is only needed for hot-reload (rebuilding the .o2r
    // between Debug-Room clicks); first-click is always a cache miss.
    EvictAndPreload(resMgr, {
        "scenes/squadala/dungeon_scene",
        "scenes/squadala/collision",
        "scenes/squadala/room_0",
        "scenes/squadala/room_1",
        "scenes/squadala/room0_DL",
        "scenes/squadala/room0_Vtx",
        "scenes/squadala/room1_DL",
        "scenes/squadala/room1_Vtx",
        "scenes/squadala/mario_DL",
        "scenes/squadala/mario_Vtx",
        "scenes/squadala/sonic_DL",
        "scenes/squadala/sonic_Vtx",
        "scenes/squadala/hydrant_DL",
        "scenes/squadala/hydrant_Vtx",
        "scenes/squadala/pizza_DL",
        "scenes/squadala/pizza_Vtx",
        "scenes/squadala/cupcake_DL",
        "scenes/squadala/cupcake_Vtx",
        "scenes/squadala/mouse_DL",
        "scenes/squadala/mouse_Vtx",
    });

    return true;
}

bool HotReloadMeshLab(const std::string& o2rPath) {
    auto ctx = Ship::Context::GetInstance();
    if (!ctx) return false;
    auto resMgr = ctx->GetResourceManager();
    if (!resMgr) return false;

    auto archiveMgr = resMgr->GetArchiveManager();
    if (archiveMgr) {
        archiveMgr->AddArchive(o2rPath);
        SPDLOG_INFO("LiveGen: Hot-reload — added mesh-lab archive: {}", o2rPath);
    }

    // NOTE: we deliberately do NOT evict + preload the lab resources here.
    // Eviction tears the resource pointers out from under the graphics
    // thread, which may already be mid-frame processing DL commands
    // (G_VTX, G_SETTIMG, G_BRANCH_LIST, …) that resolve through
    // gSegments[] indirection. Freeing the backing data leaves dangling
    // pointers in the interpreter's working set → "Unhandled OP 0x17"
    // (garbage interpreted as F3DEX2) → SIGSEGV in SETTIMG/SETCIMG.
    //
    // Trade-off: rebuilding the lab .o2r mid-session won't take effect
    // until SoH is restarted. The flag-reset block below still gives a
    // fresh in-game state on every Load Lab click (chests respawn, door
    // re-locks via its temp switch flag), which is what the user actually
    // needs from "Load Lab" in normal use.

    // Lab chests must respawn on every hot-reload, otherwise the player
    // gets into a dead state: chest's persistent treasure_flag stays set
    // → chest looks already opened → can't grab the key inside → can't
    // unlock the locked door (its switch flag is temp and resets, but
    // without a key the door is unusable). Flags 10-13 are the four lab
    // chests (maze v1 small/big = 10/11, maze_complex small/big = 12/13).
    //
    // Two stores hold the chest bits: the in-play actorCtx flags (set when
    // the chest opens) and the persistent save-context flags. The scene
    // transition fires Play_SaveSceneFlags which copies actorCtx → save
    // context, overwriting anything we clear here in just one of them.
    // Clear BOTH so the round-trip (clear here → transition saves → scene
    // init reloads) lands on the cleared state.
    constexpr int32_t LAB_SCENE_ID = 0;
    constexpr uint32_t LAB_CHEST_MASK =
        (1u << 10) | (1u << 11) | (1u << 12) | (1u << 13);
    gSaveContext.sceneFlags[LAB_SCENE_ID].chest &= ~LAB_CHEST_MASK;
    if (gPlayState) {
        gPlayState->actorCtx.flags.chest &= ~LAB_CHEST_MASK;
    }

    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Portal warp (Door_Warp1-based)
// ──────────────────────────────────────────────────────────────────────

namespace {

struct PreDungeonState {
    bool valid = false;
    int16_t entranceIndex = 0;
    Vec3f playerPos = {0, 0, 0};
    int16_t playerRotY = 0;
    int32_t roomNum = 0;
};

PreDungeonState sPreDungeon;
Actor* sActiveWarp = nullptr;
bool sPortalIsEntry = false;
bool sExitPortalSpawnedThisScene = false;
bool sPendingPositionRestore = false;

// Trigger radius around the warp's centre — empirical, roughly two Link-
// widths so the player has to walk INTO the pillar (not just brush past).
constexpr float kPortalTriggerRadiusSq = 60.0f * 60.0f;

void DespawnActiveWarp() {
    if (sActiveWarp && sActiveWarp->update != nullptr) {
        Actor_Kill(sActiveWarp);
    }
    sActiveWarp = nullptr;
}

Actor* SpawnWarpActor(PlayState* play, float x, float y, float z) {
    // Door_Warp1's drawFunc dereferences gWarpCrystalDL / gDoorWarpDL etc.
    // from OBJECT_WARP1's segment. Without that object loaded in the room's
    // object bank, the segment is NULL and the graphics thread blows up on
    // the first draw call. Spawn the object up front so it's resident
    // before Actor_Spawn pokes the actor's draw path.
    if (Object_GetIndex(&play->objectCtx, LIVEGEN_OBJECT_WARP1) < 0) {
        Object_Spawn(&play->objectCtx, LIVEGEN_OBJECT_WARP1);
    }
    Actor* warp = Actor_Spawn(&play->actorCtx, play, ACTOR_DOOR_WARP1,
                              x, y, z, 0, 0, 0, LIVEGEN_WARP_DUNGEON_CHILD);
    if (!warp) {
        SPDLOG_ERROR("LiveGen: Door_Warp1 spawn failed");
        return nullptr;
    }
    // OnePointCutscene swings the camera onto the warp on first spawn. The
    // actor's own NA_SE_EV_WARP_HOLE ambient loop replaces our earlier
    // one-shot chime — same audio you hear standing in a boss-clear warp.
    OnePointCutscene_Attention(play, warp);
    return warp;
}

void OnPortalProximity() {
    // No-op: with the WARP_DUNGEON_CHILD vanilla cutscene driving the
    // entry, the actor itself detects player proximity, halts actors,
    // floats Link up, and fires the transition. The transition uses the
    // play->nextEntranceIndex we preset in SpawnEntryPortal, so we don't
    // need to override anything here.
    //
    // Kept around so the exit-portal flow can plug in later if needed.
    if (!sActiveWarp || !gPlayState) return;
    if (sActiveWarp->update == nullptr) {
        sActiveWarp = nullptr;
        return;
    }
}

void MaybeSpawnExitPortal() {
    // Auto-spawn DISABLED — the user wanted to land in the custom dungeon
    // and explore without immediately seeing another portal. Use the
    // public SpawnExitPortal(x, y, z) helper when an explicit exit is
    // needed (e.g. after a boss/goal trigger).
}

void OnSceneInitForPortal(int16_t /*sceneNum*/) {
    sExitPortalSpawnedThisScene = false;
    sActiveWarp = nullptr;  // any prev-scene warp died with the scene
}

void OnTransitionEndForPortal(int16_t /*sceneNum*/) {
    if (!sPendingPositionRestore || !sPreDungeon.valid || !gPlayState) return;
    sPendingPositionRestore = false;
    Player* player = GET_PLAYER(gPlayState);
    if (player) {
        player->actor.world.pos = sPreDungeon.playerPos;
        player->actor.shape.rot.y = sPreDungeon.playerRotY;
        SPDLOG_INFO("LiveGen: Restored pre-dungeon position");
    }
    sPreDungeon.valid = false;  // one-shot, don't restore again
}

}  // anonymous namespace

// Visible to LiveGen namespace so the ShipInit registration below can
// reference it via LiveGen::RegisterPortalHooks.
void RegisterPortalHooks() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
        MaybeSpawnExitPortal();
        OnPortalProximity();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        OnSceneInitForPortal);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnTransitionEnd>(
        OnTransitionEndForPortal);
}

// Look around Link for an open spot at `kPortalDist` away. Tries up to 12
// candidate directions (front first, then symmetric side offsets, then
// behind) and accepts the first one where:
//   - a line-test from Link's chest height to the candidate doesn't hit
//     a wall (so the warp isn't BEHIND a partition)
//   - the floor below the candidate is reachable (so it's not floating
//     in a doorway / off the room geometry)
// Falls back to Link's own position if nothing works.
static Vec3f FindClearPortalSpot(PlayState* play, Vec3f playerPos, int16_t playerRotY) {
    constexpr float kPortalDist = 200.0f;
    constexpr float kEyeHeight  = 40.0f;
    // Direction offsets in BAM (Binary Angular Measure). Tries front,
    // then ±30°, ±60°, ±90°, ±120°, ±150°, then directly behind.
    static const int16_t kAngleOffsetsBam[] = {
        0,
        +0x1555, -0x1555,   // ±30°
        +0x2AAA, -0x2AAA,   // ±60°
        +0x4000, -0x4000,   // ±90°
        +0x5555, -0x5555,   // ±120°
        +0x6AAA, -0x6AAA,   // ±150°
        +0x8000,            // 180° (directly behind)
    };

    Vec3f startEye = {playerPos.x, playerPos.y + kEyeHeight, playerPos.z};

    for (int16_t offset : kAngleOffsetsBam) {
        int16_t yaw = (int16_t)(playerRotY + offset);
        float rad = (float)yaw * (3.14159265f / 0x8000);
        Vec3f candidate = {
            playerPos.x + sinf(rad) * kPortalDist,
            playerPos.y + kEyeHeight,
            playerPos.z + cosf(rad) * kPortalDist,
        };

        // Line test from Link's eye to the candidate. chkWall=1 → blocked
        // by walls; chkOneFace=1 → only front-face hits count.
        Vec3f hitPos;
        CollisionPoly* hitPoly = nullptr;
        int32_t bgId = 0;
        s32 hit = BgCheck_EntityLineTest1(&play->colCtx, &startEye, &candidate,
                                          &hitPos, &hitPoly, 1, 0, 0, 1, &bgId);
        if (hit) continue;

        // Drop the candidate to the floor — cast straight down from a
        // bit above the eye line. EntityRaycastFloor1 returns BGCHECK_Y_MIN
        // (very negative) when no floor is hit.
        Vec3f dropFrom = {candidate.x, candidate.y + 200.0f, candidate.z};
        CollisionPoly* floorPoly = nullptr;
        f32 floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &floorPoly, &dropFrom);
        if (floorY < -10000.0f) continue;   // no floor → skip

        return Vec3f{candidate.x, floorY, candidate.z};
    }

    // No clear spot found — fall back to Link's own position. The OnePoint
    // camera will swing onto Link himself, less dramatic but never wrong.
    SPDLOG_WARN("LiveGen: no clear portal spot found, spawning at Link's feet");
    return playerPos;
}

void SpawnEntryPortal() {
    if (!gPlayState) {
        SPDLOG_ERROR("LiveGen: SpawnEntryPortal: no PlayState");
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (!player) return;

    // Snapshot before-state so the exit portal can warp Link back to here.
    sPreDungeon.valid = true;
    sPreDungeon.entranceIndex = gSaveContext.entranceIndex;
    sPreDungeon.playerPos = player->actor.world.pos;
    sPreDungeon.playerRotY = player->actor.shape.rot.y;
    sPreDungeon.roomNum = gPlayState->roomCtx.curRoom.num;

    Vec3f spot = FindClearPortalSpot(gPlayState, player->actor.world.pos,
                                      player->actor.shape.rot.y);
    Actor* warp = SpawnWarpActor(gPlayState, spot.x, spot.y, spot.z);
    if (!warp) return;
    sActiveWarp = warp;
    sPortalIsEntry = true;

    // Activate the full boss-clear cutscene flow. Vanilla Door_Warp1 only
    // fires its transition when these two magic state bits are set:
    //   * gSaveContext.nextCutsceneIndex == 0xFFEF
    //   * play->nextEntranceIndex points at where we want to land
    // For our (non-boss) scene the actor's hardcoded sceneNum chain doesn't
    // overwrite nextEntranceIndex, so the value we set here sticks.
    // CUSTOM_DUNGEON mode is set NOW so the post-cutscene scene-load redirect
    // (z_play_otr.cpp:GetDebugScenePath) returns scenes/squadala_custom/...
    SetDebugMode(DebugMode::CUSTOM_DUNGEON);
    gSaveContext.nextCutsceneIndex = LIVEGEN_NEXT_CUTSCENE_BLUE_WARP;
    gPlayState->nextEntranceIndex = 0;

    // Queue the post-boss BGM so the cutscene plays over that swelling
    // orchestral pad instead of leftover scene music.
    Audio_QueueSeqCmd(0x00 << 24 | LIVEGEN_NA_BGM_BOSS_CLEAR);
    SPDLOG_INFO("LiveGen: Entry portal armed (full boss-clear cutscene)");
}

void SpawnExitPortal(float x, float y, float z) {
    if (!gPlayState) return;
    Actor* warp = SpawnWarpActor(gPlayState, x, y, z);
    if (warp) {
        sActiveWarp = warp;
        sPortalIsEntry = false;
        sExitPortalSpawnedThisScene = true;
    }
}

void WarpToEntrance(int16_t entranceIndex) {
    if (!gPlayState) {
        SPDLOG_ERROR("LiveGen: No PlayState for warp");
        return;
    }

    gPlayState->roomCtx.prevRoom.segment = NULL;
    gPlayState->roomCtx.curRoom.segment = NULL;

    gPlayState->nextEntranceIndex = entranceIndex;
    gPlayState->transitionTrigger = TRANS_TRIGGER_START;
    gPlayState->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
    SPDLOG_INFO("LiveGen: Warping to entrance {:#06x} (rooms cleared)", entranceIndex);
}

} // namespace LiveGen

static RegisterShipInitFunc liveGenPortalInit(LiveGen::RegisterPortalHooks, {});
extern "C" bool LiveGen_IsDebugRoomActive() {
    return LiveGen::IsDebugRoomActive();
}

extern "C" const char* LiveGen_GetDebugScenePath() {
    return LiveGen::GetDebugScenePath();
}

extern "C" int LiveGen_GetDebugMode() {
    return static_cast<int>(LiveGen::GetDebugMode());
}
