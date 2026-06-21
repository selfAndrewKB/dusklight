#pragma once

class J3DModel;
class fopAc_ac_c;

namespace dusk::coop::render_visibility {

bool shouldBypassDrawCulling();
bool shouldUseViewportVisibility();

void beginActorDraw(fopAc_ac_c* actor);
void endActorDraw();
void reset();
void registerSubmittedModel(J3DModel* model);
void registerSenseOnlyModel(J3DModel* model, fopAc_ac_c* actor);
bool shouldDrawModel(const J3DModel* model);

struct DebugState {
    int senseOnlyModelCount = 0;
    const J3DModel* lastModel = nullptr;
    const fopAc_ac_c* lastActor = nullptr;
    int lastSlot = -1;
    bool lastSenseReady = false;
    bool lastCulled = false;
};

DebugState getDebugState();

}  // namespace dusk::coop::render_visibility
