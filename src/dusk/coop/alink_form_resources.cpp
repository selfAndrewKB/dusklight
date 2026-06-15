#include "dusk/coop/alink_form_resources.h"

#include "SSystem/SComponent/c_phase.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/logging.h"
#include "m_Do/m_Do_ext.h"

#include <cstring>

namespace dusk::coop::alink_form_resources {
namespace {

aurora::Module FormLog("dusk::coop::alink_form_resources");

constexpr int kMaxFormArcs = 8;
constexpr u32 kAlinkArcHeapSize = 0xA2800;
constexpr int kPhaseCompleteId = 2;

struct ArcState {
    char arcName[12] = {};
    request_of_phase_process_class phase = {};
    JKRExpHeap* heap = nullptr;
    int retainCount = 0;
    bool loaded = false;
};

struct SlotState {
    daAlink_c* actor = nullptr;
    const char* currentArc = nullptr;
    const char* pendingReleaseArc = nullptr;
    bool desiredWolf = false;
    bool desiredKnown = false;
    bool swapping = false;
};

ArcState s_arcs[kMaxFormArcs];
SlotState s_slots[kPlayerSlotCount];
DebugState s_debug;

bool isValidSlot(PlayerSlot slot) {
    return slot == PlayerSlot::Slot0 || slot == PlayerSlot::Slot1 ||
           slot == PlayerSlot::Slot2 || slot == PlayerSlot::Slot3;
}

int slotIndex(PlayerSlot slot) {
    return static_cast<int>(slot);
}

bool sameArc(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    return std::strcmp(lhs, rhs) == 0;
}

PlayerSlot slotForPlayer(const daAlink_c* player) {
    PlayerSlot slot = getSlotForActor(player);
    if (slot == PlayerSlot::Invalid) {
        slot = getAdditionalPlayerSpawnRequestSlot(player);
    }
    return slot;
}

SlotState* stateForPlayer(daAlink_c* player, bool bindActor = true) {
    const PlayerSlot slot = slotForPlayer(player);
    if (!isValidSlot(slot)) {
        return nullptr;
    }

    SlotState& state = s_slots[slotIndex(slot)];
    if (bindActor) {
        state.actor = player;
    }
    return &state;
}

ArcState* findArc(const char* arcName) {
    if (arcName == nullptr) {
        return nullptr;
    }

    for (ArcState& arc : s_arcs) {
        if (arc.arcName[0] != '\0' && sameArc(arc.arcName, arcName)) {
            return &arc;
        }
    }
    return nullptr;
}

ArcState* getOrCreateArc(daAlink_c* player, const char* arcName) {
    if (arcName == nullptr || player == nullptr) {
        return nullptr;
    }

    if (ArcState* existing = findArc(arcName)) {
        return existing;
    }

    for (ArcState& arc : s_arcs) {
        if (arc.arcName[0] == '\0') {
            std::strncpy(arc.arcName, arcName, sizeof(arc.arcName) - 1);
            player->setOriginalHeap(&arc.heap, kAlinkArcHeapSize);
            if (arc.heap != nullptr) {
                JKRHEAP_NAME(arc.heap, "Co-op Alink FormArcHeap");
            }
            FormLog.debug("created form arc {} heap 0x{:x}", arc.arcName,
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(arc.heap)));
            return &arc;
        }
    }

    FormLog.error("no free form arc slot for {}", arcName);
    return nullptr;
}

void deleteArcRegistration(ArcState& arc) {
    if (arc.arcName[0] == '\0') {
        return;
    }

    if (arc.phase.id == kPhaseCompleteId) {
        if (!dComIfG_resDelete(&arc.phase, arc.arcName)) {
            dComIfG_deleteObjectResMain(arc.arcName);
        }
    } else if (arc.phase.id != 0 || arc.phase.mpHandlerTable != nullptr) {
        // Co-op: interrupted loads registered the global archive name before completion.
        dComIfG_deleteObjectResMain(arc.arcName);
    }
    cPhs_Reset(&arc.phase);
    arc.loaded = false;
}

void releaseArcRetain(const char* arcName) {
    ArcState* arc = findArc(arcName);
    if (arc == nullptr || arc->retainCount <= 0) {
        return;
    }

    arc->retainCount--;
    if (arc->retainCount != 0) {
        s_debug.revision++;
        return;
    }

    deleteArcRegistration(*arc);

    if (arc->heap != nullptr) {
        mDoExt_destroyExpHeap(arc->heap);
    }
    FormLog.debug("released final form arc {}", arc->arcName);
    *arc = {};
    s_debug.revision++;
}

void retainSlotArc(SlotState& slotState, const char* arcName) {
    if (sameArc(slotState.currentArc, arcName)) {
        return;
    }

    if (slotState.currentArc != nullptr) {
        releaseArcRetain(slotState.currentArc);
    }

    ArcState* arc = findArc(arcName);
    if (arc == nullptr) {
        return;
    }

    arc->retainCount++;
    slotState.currentArc = arc->arcName;
    s_debug.revision++;
}

int ensureArcLoaded(daAlink_c* player, const char* arcName) {
    ArcState* arc = getOrCreateArc(player, arcName);
    if (arc == nullptr || arc->heap == nullptr) {
        return cPhs_ERROR_e;
    }

    if (arc->loaded) {
        return cPhs_COMPLEATE_e;
    }

    const int phase = dComIfG_resLoad(&arc->phase, arc->arcName, static_cast<JKRHeap*>(arc->heap));
    if (phase == cPhs_COMPLEATE_e) {
        arc->loaded = true;
        s_debug.revision++;
        FormLog.debug("loaded form arc {} heap 0x{:x}", arc->arcName,
                      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(arc->heap)));
    }
    return phase;
}

void refreshDebug() {
    s_debug.arcCount = 0;
    for (const ArcState& arc : s_arcs) {
        if (arc.arcName[0] == '\0') {
            continue;
        }

        DebugArcState& debugArc = s_debug.arcs[s_debug.arcCount++];
        debugArc.arcName = arc.arcName;
        debugArc.heap = reinterpret_cast<uintptr_t>(arc.heap);
        debugArc.retainCount = arc.retainCount;
        debugArc.phaseId = arc.phase.id;
        debugArc.loaded = arc.loaded;
    }

    for (int i = 0; i < kPlayerSlotCount; i++) {
        const SlotState& slot = s_slots[i];
        DebugSlotState& debugSlot = s_debug.slots[i];
        debugSlot.slot = static_cast<PlayerSlot>(i);
        debugSlot.actor = reinterpret_cast<uintptr_t>(slot.actor);
        debugSlot.currentArc = slot.currentArc;
        debugSlot.pendingReleaseArc = slot.pendingReleaseArc;
        debugSlot.desiredWolf = slot.desiredWolf;
        debugSlot.desiredKnown = slot.desiredKnown;
        debugSlot.swapping = slot.swapping;
    }
}

}  // namespace

void applyDesiredFormOnCreate(daAlink_c* player) {
    SlotState* state = stateForPlayer(player);
    if (state == nullptr || player == nullptr) {
        return;
    }

    const PlayerSlot slot = slotForPlayer(player);
    if (slot == PlayerSlot::Primary) {
        // Co-op: P1's form on create is owned by the native save/startup path. The retained
        // desired-form cache exists for runtime additional slots and must not force a later
        // wolf save back to a stale human state.
        state->desiredWolf = player->checkWolf();
        state->desiredKnown = true;
    } else if (!state->desiredKnown) {
        daAlink_c* primary = static_cast<daAlink_c*>(getPrimaryPlayer());
        state->desiredWolf = primary != nullptr ? primary->checkWolf() : player->checkWolf();
        state->desiredKnown = true;
    }

    if (state->desiredWolf) {
        player->onNoResetFlg1(daPy_py_c::FLG1_IS_WOLF);
    } else {
        player->offNoResetFlg1(daPy_py_c::FLG1_IS_WOLF);
    }
}

int loadInitial(daAlink_c* player, const char* arcName) {
    SlotState* state = stateForPlayer(player);
    if (state == nullptr) {
        return cPhs_ERROR_e;
    }

    const int phase = ensureArcLoaded(player, arcName);
    if (phase == cPhs_COMPLEATE_e) {
        retainSlotArc(*state, arcName);
    }
    return phase;
}

void beginSwap(daAlink_c* player, const char* oldArcName) {
    SlotState* state = stateForPlayer(player);
    if (state == nullptr || state->swapping) {
        return;
    }

    state->pendingReleaseArc = oldArcName;
    state->swapping = true;
    s_debug.revision++;
}

int loadSwapArc(daAlink_c* player, const char* arcName) {
    SlotState* state = stateForPlayer(player);
    if (state == nullptr) {
        return cPhs_ERROR_e;
    }

    const int phase = ensureArcLoaded(player, arcName);
    if (phase == cPhs_COMPLEATE_e && !sameArc(state->currentArc, arcName)) {
        ArcState* arc = findArc(arcName);
        if (arc != nullptr) {
            arc->retainCount++;
            state->currentArc = arc->arcName;
            s_debug.revision++;
        }
    }
    return phase;
}

void completeSwap(daAlink_c* player) {
    SlotState* state = stateForPlayer(player);
    if (state == nullptr) {
        return;
    }

    const char* oldArc = state->pendingReleaseArc;
    state->pendingReleaseArc = nullptr;
    state->swapping = false;

    if (oldArc != nullptr && !sameArc(oldArc, state->currentArc)) {
        releaseArcRetain(oldArc);
    }
    s_debug.revision++;
}

bool releaseActor(daAlink_c* player) {
    SlotState* state = stateForPlayer(player, false);
    if (state == nullptr || state->actor != player) {
        return false;
    }

    const char* currentArc = state->currentArc;
    const char* pendingReleaseArc = state->pendingReleaseArc;
    state->actor = nullptr;
    state->currentArc = nullptr;
    state->pendingReleaseArc = nullptr;
    state->swapping = false;

    if (currentArc != nullptr) {
        releaseArcRetain(currentArc);
    }
    if (pendingReleaseArc != nullptr && !sameArc(pendingReleaseArc, currentArc)) {
        releaseArcRetain(pendingReleaseArc);
    }
    s_debug.revision++;
    return true;
}

void setSlotWolf(daAlink_c* player, bool wolf) {
    SlotState* state = stateForPlayer(player);
    if (state == nullptr) {
        return;
    }

    state->desiredWolf = wolf;
    state->desiredKnown = true;
    s_debug.revision++;
}

bool canInstallModelDataOwner(daAlink_c* player) {
    SlotState* state = stateForPlayer(player, false);
    return state != nullptr && state->actor == player && !state->swapping &&
           state->currentArc != nullptr;
}

void resetRuntime() {
    for (int i = 0; i < kPlayerSlotCount; i++) {
        SlotState& slot = s_slots[i];
        slot.actor = nullptr;
        slot.currentArc = nullptr;
        slot.pendingReleaseArc = nullptr;
        slot.swapping = false;
        if (i == slotIndex(PlayerSlot::Primary)) {
            // Co-op: additional-slot desired forms are session intent, but P1 form is reloaded
            // from native save/startup state on each primary actor create.
            slot.desiredWolf = false;
            slot.desiredKnown = false;
        }
    }

    for (ArcState& arc : s_arcs) {
        if (arc.arcName[0] != '\0') {
            deleteArcRegistration(arc);
            if (arc.heap != nullptr) {
                mDoExt_destroyExpHeap(arc.heap);
            }
            arc = {};
        }
    }
    s_debug.revision++;
}

const DebugState& getDebugState() {
    refreshDebug();
    return s_debug;
}

}  // namespace dusk::coop::alink_form_resources
