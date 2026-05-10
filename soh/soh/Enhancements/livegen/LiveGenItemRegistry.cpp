// LiveGen custom item registry — registers Mario as a "chest content" item
// in the vanilla MOD_NONE table at an unused slot, plus a custom textbox.
//
// Strategy:
//   1. Pick an unused getItemId in the MOD_NONE table (0x7E — empty
//      GET_ITEM_NONE placeholder slot between GI_TEXT_0 (0x7D) and GI_MAX
//      (0x84) in z64item.h, also fits En_Box's 7-bit getItemId field).
//   2. Pick a textId far outside any vanilla/randomizer range (0xE000+).
//   3. Use ItemTableManager::SetItemEntry to overwrite the placeholder
//      (AddItemEntry silently fails on key collision).
//   4. Custom drawFunc renders Mario's DL.
//   5. OnOpenText hook builds a custom message for our textId.
//   6. itemId stays ITEM_NONE (no inventory side-effect). Vanilla normally
//      skips the slow kneel-and-pull cutscene for ITEM_NONE entries; we
//      force it back on via VB_PLAY_SLOW_CHEST_CS, identifying our chest
//      by the GI_LIVEGEN_MARIO marker in its params.

#include "soh/OTRGlobals.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Box/z_en_box.h"
extern PlayState* gPlayState;
}

extern "C" Gfx* ResourceMgr_LoadGfxByName(const char* path);
// OTRGlobals.h hides these behind #ifndef __cplusplus, so re-declare for C++.
extern "C" GetItemEntry ItemTable_Retrieve(int16_t getItemID);

static constexpr uint8_t  GI_LIVEGEN_MARIO   = 0x7E;     // unused in z64item.h, fits 7-bit chest params
static constexpr uint16_t TEXT_LIVEGEN_MARIO = 0xE000;   // safely above all vanilla/rando textIds
static constexpr uint8_t  GI_LIVEGEN_SONIC   = 0x7F;     // next free slot in the same gap
static constexpr uint16_t TEXT_LIVEGEN_SONIC = 0xE001;
// 0x7D — last chest-encodable slot (params field is only 7 bits). Replaces
// GI_TEXT_0, an unused "show text 0, no model" vanilla placeholder.
static constexpr uint8_t  GI_LIVEGEN_HYDRANT   = 0x7D;
static constexpr uint16_t TEXT_LIVEGEN_HYDRANT = 0xE002;

static const char* MARIO_DL_PATH   = "__OTR__scenes/squadala/mario_DL";
static const char* SONIC_DL_PATH   = "__OTR__scenes/squadala/sonic_DL";
static const char* HYDRANT_DL_PATH = "__OTR__scenes/squadala/hydrant_DL";

// Helper: returns true if `gi` is one of our custom chest GIs.
static inline bool LiveGen_IsCustomGi(uint8_t gi) {
    return gi == GI_LIVEGEN_MARIO || gi == GI_LIVEGEN_SONIC || gi == GI_LIVEGEN_HYDRANT;
}

// Custom drawFunc — file-scope extern "C" because OPEN_DISPS expands to a
// local declaration of FrameInterpolation_Record* without extern "C", which
// in C++ would otherwise produce a name-mangled symbol the linker can't find.
//
// Player_DrawGetItemImpl already pushed the translate/rotate/scale matrix —
// we only bind it and push our DL.
extern "C" void LiveGen_DrawMarioItem(PlayState* play, GetItemEntry* /*entry*/) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx* dl = ResourceMgr_LoadGfxByName(MARIO_DL_PATH);
    if (dl != nullptr) {
        // Mario's geometry is sized for a standalone scene showcase (scale 200
        // in build_box_room.py). For the GetItem display above Link's head we
        // need it down to roughly the size of a Heart Piece icon — 0.3× on top
        // of Player_DrawGetItemImpl's existing 0.2× brings it into range.
        Matrix_Scale(0.3f, 0.3f, 0.3f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++,
                  Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dl);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void LiveGen_DrawSonicItem(PlayState* play, GetItemEntry* /*entry*/) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx* dl = ResourceMgr_LoadGfxByName(SONIC_DL_PATH);
    if (dl != nullptr) {
        // Sonic's source mesh is taller than Mario's at the same parse-time
        // scale, so we shrink the GetItem display so the two chest rewards
        // read at roughly the same on-screen size (Mario is 0.3, Sonic
        // tuned-down to 0.216 ≈ Mario × 0.72).
        Matrix_Scale(0.216f, 0.216f, 0.216f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++,
                  Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dl);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void LiveGen_DrawHydrantItem(PlayState* play, GetItemEntry* /*entry*/) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx* dl = ResourceMgr_LoadGfxByName(HYDRANT_DL_PATH);
    if (dl != nullptr) {
        // Hydrant verts span Y ~-74..+74 at parse-time (scale 14000); after
        // the 0.3× draw scale the top sticks out of the GetItem icon
        // window. The translate runs BEFORE the scale in matrix-build
        // order, so it ends up applied to scaled vertices — i.e. -12 here
        // shifts the hydrant 12 visible units down.
        Matrix_Translate(0.0f, -12.0f, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(0.3f, 0.3f, 0.3f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++,
                  Matrix_NewMtx(play->state.gfxCtx, __FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, dl);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

// OnActorUpdate hook for ACTOR_EN_BOX — fires every frame after the chest's
// own update. EnBox calls Actor_OfferGetItemNearby with the negative getItemId,
// which sets player->getItemId but NOT player->getItemEntry. Player_DrawGetItemImpl
// reads the drawFunc from player->getItemEntry, so without this hook our drawFunc
// never runs and vanilla GetItem_Draw renders the placeholder Heart Piece object.
//
// We intervene whenever the player's pending getItem matches our id (positive
// or negative — Player code flips the sign mid-flow) and our chest is the
// originator (params marker). Setting player->getItemEntry every frame is
// idempotent; once Player resets its getItemId after the item is taken, we
// stop matching and leave the player alone.
extern "C" void LiveGen_EnBoxUpdate(void* actor) {
    EnBox* box = static_cast<EnBox*>(actor);
    int16_t chest_giid = (box->dyna.actor.params >> 5) & 0x7F;
    if (!LiveGen_IsCustomGi(chest_giid)) {
        return;
    }
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    if (player->getItemId == -chest_giid || player->getItemId == chest_giid) {
        // Mirror player->getItemId's sign onto the entry's getItemId field so
        // the (id != entry.getItemId) check in z_player.c:7318 / 14133 always
        // sees a match and uses our entry directly — no fallback lookup with
        // potentially negative key. We don't touch player->getItemId itself:
        //   - keeping it negative pre-A-press is required so vanilla doesn't
        //     hit the auto-pickup branch at z_player.c:7312 (getItemId > 0
        //     means "freestanding item walked over"), which would consume the
        //     chest without ever opening it.
        //   - vanilla func_8083A434 flips both id and entry.getItemId to
        //     positive once the chest open flow starts, and our hook re-fires
        //     with the now-positive id and writes a positive entry.getItemId.
        GetItemEntry entry = ItemTable_Retrieve(chest_giid);
        entry.getItemId = player->getItemId;
        player->getItemEntry = entry;
    }
}

// OnOpenText hook for our custom textId — builds DE/EN/FR message inline
// and tells the message system to use the font buffer instead of looking
// up vanilla message_data_static.
static void LiveGen_BuildMarioMessage(uint16_t* /*textId*/, bool* loadFromMessageTable) {
    CustomMessage msg(
        "You found %rMario%w in the chest!^Squadala!",
        "Du hast %rMario%w in der Truhe gefunden!^Squadala!",
        "Vous avez trouvé %rMario%w dans le coffre !^Squadala !",
        TEXTBOX_TYPE_BLUE);
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static void LiveGen_BuildSonicMessage(uint16_t* /*textId*/, bool* loadFromMessageTable) {
    CustomMessage msg(
        "You found %rSonic%w in the chest!^Gotta go fast!",
        "Du hast %rSonic%w in der Truhe gefunden!^Gotta go fast!",
        "Vous avez trouvé %rSonic%w dans le coffre !^Gotta go fast !",
        TEXTBOX_TYPE_BLUE);
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static void LiveGen_BuildHydrantMessage(uint16_t* /*textId*/, bool* loadFromMessageTable) {
    CustomMessage msg(
        "You found a %rFire Hydrant%w!^Stay frosty.",
        "Du hast einen %rHydranten%w gefunden!^Stay frosty.",
        "Vous avez trouvé une %rBouche d'incendie%w !^Stay frosty.",
        TEXTBOX_TYPE_BLUE);
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

static void LiveGen_RegisterItem(uint8_t giId, uint16_t textId,
                                   void (*drawFunc)(PlayState*, GetItemEntry*),
                                   void (*messageBuilder)(uint16_t*, bool*),
                                   const char* label) {
    GetItemEntry entry = GET_ITEM(
        ITEM_NONE,                  // no inventory side-effect
        OBJECT_GI_HEARTS,           // placeholder (drawFunc overrides)
        GID_HEART_PIECE,            // placeholder (drawFunc overrides)
        textId,                     // handled by the per-item OnOpenText hook
        0x80,                       // standard "wait for animation"
        CHEST_ANIM_LONG,            // big-chest kneel animation
        ITEM_CATEGORY_MAJOR,
        MOD_NONE,
        giId);
    entry.drawFunc = drawFunc;

    bool ok = ItemTableManager::Instance->SetItemEntry(MOD_NONE, giId, entry);
    SPDLOG_INFO("LiveGen: registered {} item at GI=0x{:X} textId=0x{:X} (ok={})",
                label, giId, textId, ok);

    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        textId, messageBuilder);
}

static void LiveGen_RegisterItems() {
    LiveGen_RegisterItem(GI_LIVEGEN_MARIO, TEXT_LIVEGEN_MARIO,
                         LiveGen_DrawMarioItem, LiveGen_BuildMarioMessage, "Mario");
    LiveGen_RegisterItem(GI_LIVEGEN_SONIC, TEXT_LIVEGEN_SONIC,
                         LiveGen_DrawSonicItem, LiveGen_BuildSonicMessage, "Sonic");
    LiveGen_RegisterItem(GI_LIVEGEN_HYDRANT, TEXT_LIVEGEN_HYDRANT,
                         LiveGen_DrawHydrantItem, LiveGen_BuildHydrantMessage, "Hydrant");

    // Single OnActorUpdate hook handles all our chest-content IDs (gated on
    // EnBox params); vanilla En_Box only sets player->getItemId, not the
    // full entry, so Player_DrawGetItemImpl needs us to patch the entry
    // every frame to pick up our drawFunc.
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorUpdate>(
        ACTOR_EN_BOX, LiveGen_EnBoxUpdate);

    // Force the slow kneel-and-pull cutscene for our chests. Vanilla's
    // vanillaPlaySlowChestCS check requires itemId != ITEM_NONE; since we
    // keep itemId = ITEM_NONE, we override the decision here, gated on the
    // chest's getItemId marker so vanilla chests are unaffected.
    REGISTER_VB_SHOULD(VB_PLAY_SLOW_CHEST_CS, {
        EnBox* chest = va_arg(args, EnBox*);
        if (chest) {
            uint8_t gi = (chest->dyna.actor.params >> 5) & 0x7F;
            if (LiveGen_IsCustomGi(gi)) {
                *should = true;
            }
        }
    });
}

// Run on boot, after VanillaItemTable_Init() has populated MOD_NONE.
static RegisterShipInitFunc liveGenItemsInit(LiveGen_RegisterItems, {});
