#pragma once

#include "dusk/coop/player_slots.h"

class J3DModel;
class J3DModelData;
class cXyz;
struct dKy_tevstr_c;

namespace dusk::coop::render_materials {

constexpr int kDebugRegistrationCapacity = 16;
constexpr int kLightingProbeCapacity = 8;
constexpr int kLightingProbeLightCount = 6;

struct LightingValueDebugState {
    unsigned int frame = 0;
    int slot = 0;
    int cameraId = 0;
    int ambientR = 0;
    int ambientG = 0;
    int ambientB = 0;
    int lightR[kLightingProbeLightCount] = {};
    int lightG[kLightingProbeLightCount] = {};
    int lightB[kLightingProbeLightCount] = {};
    float lightX[kLightingProbeLightCount] = {};
    float lightY[kLightingProbeLightCount] = {};
    float lightZ[kLightingProbeLightCount] = {};
};

struct LightingProbeDebugState {
    const char* label = nullptr;
    const void* actor = nullptr;
    const J3DModel* model = nullptr;
    const dKy_tevstr_c* tevstr = nullptr;
    int tevstrType = 0;
    LightingValueDebugState submission = {};
    LightingValueDebugState refresh = {};
};

struct LightingPassDebugState {
    unsigned int frame = 0;
    int slot = 0;
    int cameraId = 0;
    bool fullscreen = false;
    bool enemyAuthoredDemo = false;
    bool gxLightReloaded = false;
    bool materialRefreshRequested = false;
    bool materialRefreshExecuted = false;
};

struct DebugState {
    int viewDependentModelCount = 0;
    const J3DModel* viewDependentModels[kDebugRegistrationCapacity] = {};
    int lightProjectionModelCount = 0;
    const J3DModel* lightProjectionModels[kDebugRegistrationCapacity] = {};
    unsigned int lightProjectionMaterialMasks[kDebugRegistrationCapacity] = {};
    int lightingProbeCount = 0;
    LightingProbeDebugState lightingProbes[kLightingProbeCapacity] = {};
    LightingPassDebugState lightingPasses[kPlayerSlotCount] = {};
    LightingPassDebugState lastEnemyAuthoredDemoLightingPasses[kPlayerSlotCount] = {};
};

void registerKankyoTevstrContext(int tevstrType, const cXyz* pos, dKy_tevstr_c* tevstr);
void registerKankyoMaterial(J3DModelData* modelData, dKy_tevstr_c* tevstr);
void registerKankyoModel(J3DModel* model, dKy_tevstr_c* tevstr);
void registerViewDependentModel(J3DModel* model);
void registerLightProjectionModel(J3DModel* model, unsigned int materialMask);
void registerLightingProbe(const char* label, const void* actor, J3DModel* model,
                           dKy_tevstr_c* tevstr, int tevstrType);
void recordGxLightReloadForCurrentView(bool reloaded);
void refreshKankyoMaterialsForCurrentView();
DebugState getDebugState();

}  // namespace dusk::coop::render_materials
