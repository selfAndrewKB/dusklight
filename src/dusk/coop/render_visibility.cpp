#include "dusk/coop/render_visibility.h"

#include "SSystem/SComponent/c_counter.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/player_sense.h"
#include "dusk/coop/render_effects.h"
#include "f_op/f_op_actor_mng.h"

namespace dusk::coop::render_visibility {
namespace {

constexpr int kSenseModelCapacity = 128;
constexpr int kActorDrawStackCapacity = 4;

struct SenseModelEntry {
    J3DModel* model = nullptr;
    fopAc_ac_c* actor = nullptr;
    fpc_ProcID processId = fpcM_ERROR_PROCESS_ID_e;
    unsigned int lastSeenFrame = 0;
};

SenseModelEntry s_senseModels[kSenseModelCapacity] = {};
DebugState s_debugState = {};
fopAc_ac_c* s_actorDrawStack[kActorDrawStackCapacity] = {};
int s_actorDrawDepth = 0;

unsigned int currentFrame() {
    return static_cast<unsigned int>(g_Counter.mCounter0);
}

bool isCurrentFrame(unsigned int registeredFrame, unsigned int frame) {
    return registeredFrame == frame || registeredFrame + 1 == frame;
}

bool isLiveActor(const SenseModelEntry& entry) {
    if (entry.actor == nullptr || entry.processId == fpcM_ERROR_PROCESS_ID_e) {
        return false;
    }

    fopAc_ac_c* actor = nullptr;
    fopAcM_SearchByID(entry.processId, &actor);
    return actor == entry.actor;
}

}  // namespace

bool shouldBypassDrawCulling() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldRefreshViewportOwnedWorldState();
#else
    return false;
#endif
}

bool shouldUseViewportVisibility() {
#if TARGET_PC
    return dusk::coop::event_presentation::shouldRefreshViewportOwnedWorldState();
#else
    return false;
#endif
}

void beginActorDraw(fopAc_ac_c* actor) {
#if TARGET_PC
    if (s_actorDrawDepth < kActorDrawStackCapacity) {
        s_actorDrawStack[s_actorDrawDepth++] = actor;
    }
#endif
}

void endActorDraw() {
#if TARGET_PC
    if (s_actorDrawDepth > 0) {
        s_actorDrawStack[--s_actorDrawDepth] = nullptr;
    }
#endif
}

void reset() {
#if TARGET_PC
    for (SenseModelEntry& entry : s_senseModels) {
        entry = {};
    }
    for (int i = 0; i < kActorDrawStackCapacity; i++) {
        s_actorDrawStack[i] = nullptr;
    }
    s_actorDrawDepth = 0;
    s_debugState = {};
#endif
}

void registerSubmittedModel(J3DModel* model) {
#if TARGET_PC
    if (s_actorDrawDepth == 0) {
        return;
    }

    fopAc_ac_c* actor = s_actorDrawStack[s_actorDrawDepth - 1];
    if (player_sense::requiresSense(actor)) {
        registerSenseOnlyModel(model, actor);
    }
#endif
}

void registerSenseOnlyModel(J3DModel* model, fopAc_ac_c* actor) {
#if TARGET_PC
    if (model == nullptr || actor == nullptr || !shouldUseViewportVisibility()) {
        return;
    }

    const unsigned int frame = currentFrame();
    SenseModelEntry* freeEntry = nullptr;
    for (SenseModelEntry& entry : s_senseModels) {
        if (entry.model == model) {
            entry.actor = actor;
            entry.processId = fopAcM_GetID(actor);
            entry.lastSeenFrame = frame;
            return;
        }
        if (freeEntry == nullptr &&
            (entry.model == nullptr ||
             (frame >= entry.lastSeenFrame && frame - entry.lastSeenFrame > 2)))
        {
            freeEntry = &entry;
        }
    }

    if (freeEntry != nullptr) {
        freeEntry->model = model;
        freeEntry->actor = actor;
        freeEntry->processId = fopAcM_GetID(actor);
        freeEntry->lastSeenFrame = frame;
    }
#endif
}

bool shouldDrawModel(const J3DModel* model) {
#if TARGET_PC
    if (model == nullptr || !shouldUseViewportVisibility()) {
        return true;
    }

    const unsigned int frame = currentFrame();
    for (const SenseModelEntry& entry : s_senseModels) {
        if (entry.model == model && isCurrentFrame(entry.lastSeenFrame, frame)) {
            const PlayerSlot slot = render_effects::hasViewport()
                                        ? render_effects::currentViewportSlot()
                                        : PlayerSlot::Primary;
            const bool senseReady = player_sense::isRevealReady(slot);
            const bool culled = isLiveActor(entry) &&
                                fopAcM_CheckStatus(entry.actor, fopAcStts_CULL_e) &&
                                fopAcM_cullingCheck(entry.actor);
            s_debugState.lastModel = model;
            s_debugState.lastActor = entry.actor;
            s_debugState.lastSlot = static_cast<int>(slot);
            s_debugState.lastSenseReady = senseReady;
            s_debugState.lastCulled = culled;
            if (!senseReady) {
                return false;
            }

            // Co-op: actor submission is shared, but the native culling test belongs to the
            // camera currently replaying this model.
            return !culled;
        }
    }
#endif
    return true;
}

DebugState getDebugState() {
    DebugState state = s_debugState;
#if TARGET_PC
    const unsigned int frame = currentFrame();
    for (const SenseModelEntry& entry : s_senseModels) {
        if (entry.model != nullptr && isCurrentFrame(entry.lastSeenFrame, frame)) {
            state.senseOnlyModelCount++;
        }
    }
#endif
    return state;
}

}  // namespace dusk::coop::render_visibility
