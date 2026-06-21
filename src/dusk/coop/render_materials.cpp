#include "dusk/coop/render_materials.h"

#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "SSystem/SComponent/c_counter.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/render_effects.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <vector>

namespace dusk::coop::render_materials {
namespace {

struct KankyoMaterialEntry {
    J3DModelData* modelData = nullptr;
    dKy_tevstr_c* tevstr = nullptr;
    unsigned int lastSeenFrame = 0;
};

struct KankyoModelEntry {
    J3DModel* model = nullptr;
    dKy_tevstr_c* tevstr = nullptr;
    unsigned int lastSeenFrame = 0;
};

struct KankyoTevstrEntry {
    dKy_tevstr_c* tevstr = nullptr;
    cXyz pos = {};
    int tevstrType = 0;
    bool hasPos = false;
    unsigned int lastSeenFrame = 0;
};

struct ViewDependentModelEntry {
    J3DModel* model = nullptr;
    unsigned int lastSeenFrame = 0;
};

struct LightProjectionModelEntry {
    J3DModel* model = nullptr;
    unsigned int materialMask = 0;
    unsigned int lastSeenFrame = 0;
};

std::vector<KankyoMaterialEntry> s_materials;
std::vector<KankyoModelEntry> s_models;
std::vector<KankyoTevstrEntry> s_tevstrs;
std::vector<ViewDependentModelEntry> s_viewDependentModels;
std::vector<LightProjectionModelEntry> s_lightProjectionModels;
bool s_refreshing = false;
LightingProbeDebugState s_lightingProbes[kLightingProbeCapacity] = {};
LightingPassDebugState s_lightingPasses[kPlayerSlotCount] = {};
LightingPassDebugState s_lastEnemyAuthoredDemoLightingPasses[kPlayerSlotCount] = {};

unsigned int currentFrame() {
    return static_cast<unsigned int>(g_Counter.mCounter0);
}

int currentViewportSlotIndex() {
    if (!render_effects::hasViewport()) {
        return 0;
    }

    const int slot = static_cast<int>(render_effects::currentViewportSlot());
    return slot >= 0 && slot < kPlayerSlotCount ? slot : 0;
}

void captureLightingValue(LightingValueDebugState* out, dKy_tevstr_c* tevstr) {
    if (out == nullptr || tevstr == nullptr) {
        return;
    }

    *out = {};
    out->frame = currentFrame();
    out->slot = currentViewportSlotIndex();
    out->cameraId = render_effects::hasViewport()
                        ? render_effects::currentViewport().cameraId
                        : 0;
    out->ambientR = tevstr->AmbCol.r;
    out->ambientG = tevstr->AmbCol.g;
    out->ambientB = tevstr->AmbCol.b;
    for (int i = 0; i < kLightingProbeLightCount; i++) {
        const J3DLightInfo* light = tevstr->mLights[i].getLightInfo();
        out->lightR[i] = light->mColor.r;
        out->lightG[i] = light->mColor.g;
        out->lightB[i] = light->mColor.b;
        out->lightX[i] = light->mLightPosition.x;
        out->lightY[i] = light->mLightPosition.y;
        out->lightZ[i] = light->mLightPosition.z;
    }
}

LightingPassDebugState& currentLightingPass() {
    LightingPassDebugState& pass = s_lightingPasses[currentViewportSlotIndex()];
    const unsigned int frame = currentFrame();
    if (pass.frame != frame) {
        if (pass.enemyAuthoredDemo) {
            s_lastEnemyAuthoredDemoLightingPasses[pass.slot] = pass;
        }
        pass = {};
        pass.frame = frame;
        pass.slot = currentViewportSlotIndex();
        pass.cameraId = render_effects::hasViewport()
                            ? render_effects::currentViewport().cameraId
                            : 0;
        const event_presentation::DebugState& presentation =
            event_presentation::getDebugState();
        pass.fullscreen = presentation.fullscreen;
        pass.enemyAuthoredDemo = presentation.enemyAuthoredDemoDepth != 0;
    }
    return pass;
}

void captureLightingProbeRefresh(dKy_tevstr_c* tevstr) {
    for (int i = 0; i < kLightingProbeCapacity; i++) {
        if (s_lightingProbes[i].tevstr == tevstr) {
            captureLightingValue(&s_lightingProbes[i].refresh, tevstr);
        }
    }
}

template <typename Entry, typename Value>
Entry* findEntry(std::vector<Entry>& entries, Value Entry::*field, Value value) {
    for (Entry& entry : entries) {
        if (entry.*field == value) {
            return &entry;
        }
    }

    return nullptr;
}

bool isCurrentRefreshFrame(unsigned int entryFrame, unsigned int frame) {
    // Co-op: split-screen viewport replay can run one counter tick after the original
    // actor/background submission that registered kankyo material state. Treat the newest
    // current-or-previous-frame entry as live instead of requiring exact counter equality.
    return entryFrame == frame || entryFrame + 1 == frame;
}

bool isStaleRefreshEntry(unsigned int entryFrame, unsigned int frame) {
    return frame >= entryFrame && frame - entryFrame > 2;
}

template <typename Entry>
void sweepStaleEntries(std::vector<Entry>& entries, unsigned int frame) {
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [frame](const Entry& entry) {
                                     return isStaleRefreshEntry(entry.lastSeenFrame, frame);
                                 }),
                  entries.end());
}

void sweepStaleRegistrations(unsigned int frame) {
    // Co-op: these registries are pointer-keyed from transient actor/model submissions.
    // Drop entries after the replay window so freed pointers cannot accumulate or be reused
    // under a stale kankyo role during long sessions.
    sweepStaleEntries(s_materials, frame);
    sweepStaleEntries(s_models, frame);
    sweepStaleEntries(s_tevstrs, frame);
    sweepStaleEntries(s_viewDependentModels, frame);
    sweepStaleEntries(s_lightProjectionModels, frame);
}

bool refreshTevstrForCurrentView(dKy_tevstr_c* tevstr) {
    KankyoTevstrEntry* tevstr_entry =
        findEntry(s_tevstrs, &KankyoTevstrEntry::tevstr, tevstr);
    if (tevstr_entry == nullptr) {
        return false;
    }

    cXyz* pos = tevstr_entry->hasPos ? &tevstr_entry->pos : nullptr;
    g_env_light.settingTevStruct(tevstr_entry->tevstrType, pos, tevstr_entry->tevstr);
    return true;
}

void diffModelKankyoMaterial(J3DModel* model, dKy_tevstr_c* tevstr) {
    if (model == nullptr) {
        return;
    }

    // Co-op: refresh only the viewport-owned kankyo fields. Re-running calcMaterial() here can
    // advance unrelated material animation/texture selection during split-screen replay.
    const u32 original_diff_flag = model->mDiffFlag;
    const bool refresh_tex_mtx = tevstr != nullptr && (tevstr->Type & 0x20);
    if (refresh_tex_mtx) {
        model->getModelData()->simpleCalcMaterial((MtxP)j3dDefaultMtx);
    }

    // Co-op: preserve the allocation contract of the model's native differed display list while
    // excluding texture-number and sampler-adjacent state owned by normal model entry.
    u32 refresh_diff_flag =
        original_diff_flag &
        (J3DDiffFlag_MatColor | J3DDiffFlag_ColorChan | J3DDiffFlag_AmbColor |
         J3DDiffFlag_TevReg | J3DDiffFlag_KonstColor | J3DDiffFlag_Fog);
    refresh_diff_flag |= J3D_DIFF_LIGHTOBJNUM(getDiffFlag_LightObjNum(original_diff_flag));
    if (refresh_tex_mtx) {
        refresh_diff_flag |=
            J3D_DIFF_TEXGENNUM(std::min<u32>(2, getDiffFlag_TexGenNum(original_diff_flag)));
    }

    model->mDiffFlag = refresh_diff_flag;
    model->diff();
    model->mDiffFlag = original_diff_flag;
}

void refreshLightProjectionModel(J3DModel* model, unsigned int materialMask) {
    if (model == nullptr || materialMask == 0 || dComIfGd_getView() == nullptr) {
        return;
    }

    J3DModelData* model_data = model->getModelData();
    Mtx effect_mtx;
    MTXLightPerspective(effect_mtx, dComIfGd_getView()->fovy, dComIfGd_getView()->aspect, 1.0f,
                        1.0f, -0.01f, 0.0f);
#if WIDESCREEN_SUPPORT
    mDoGph_gInf_c::setWideZoomLightProjection(effect_mtx);
#endif

    bool refreshed = false;
    for (u16 i = 0; i < model_data->getMaterialNum() && i < 32; i++) {
        if ((materialMask & (1u << i)) == 0) {
            continue;
        }

        J3DMaterial* material = model_data->getMaterialNodePointer(i);
        J3DTexMtx* tex_mtx = material->getTexGenBlock()->getTexMtx(0);
        if (tex_mtx == nullptr) {
            continue;
        }

        tex_mtx->getTexMtxInfo().setEffectMtx(effect_mtx);
        refreshed = true;
    }

    if (!refreshed) {
        return;
    }

    model_data->simpleCalcMaterial((MtxP)j3dDefaultMtx);
    const u32 original_diff_flag = model->mDiffFlag;
    model->mDiffFlag = J3D_DIFF_TEXGENNUM(1);
    model->diff();
    model->mDiffFlag = original_diff_flag;
}

}  // namespace

void registerKankyoTevstrContext(int tevstrType, const cXyz* pos, dKy_tevstr_c* tevstr) {
#if TARGET_PC
    if (s_refreshing || tevstr == nullptr) {
        return;
    }

    KankyoTevstrEntry* entry = findEntry(s_tevstrs, &KankyoTevstrEntry::tevstr, tevstr);
    if (entry != nullptr) {
        entry->tevstrType = tevstrType;
        entry->hasPos = pos != nullptr;
        if (pos != nullptr) {
            entry->pos = *pos;
        }
        entry->lastSeenFrame = currentFrame();
        return;
    }

    KankyoTevstrEntry new_entry = {};
    new_entry.tevstr = tevstr;
    new_entry.tevstrType = tevstrType;
    new_entry.hasPos = pos != nullptr;
    if (pos != nullptr) {
        new_entry.pos = *pos;
    }
    new_entry.lastSeenFrame = currentFrame();
    s_tevstrs.push_back(new_entry);
#endif
}

void registerKankyoMaterial(J3DModelData* modelData, dKy_tevstr_c* tevstr) {
#if TARGET_PC
    if (s_refreshing || modelData == nullptr || tevstr == nullptr) {
        return;
    }

    KankyoMaterialEntry* entry =
        findEntry(s_materials, &KankyoMaterialEntry::modelData, modelData);
    if (entry != nullptr) {
        entry->tevstr = tevstr;
        entry->lastSeenFrame = currentFrame();
        return;
    }

    s_materials.push_back({modelData, tevstr, currentFrame()});
#endif
}

void registerKankyoModel(J3DModel* model, dKy_tevstr_c* tevstr) {
#if TARGET_PC
    if (s_refreshing || model == nullptr || tevstr == nullptr) {
        return;
    }

    KankyoModelEntry* entry = findEntry(s_models, &KankyoModelEntry::model, model);
    if (entry != nullptr) {
        entry->tevstr = tevstr;
        entry->lastSeenFrame = currentFrame();
        return;
    }

    s_models.push_back({model, tevstr, currentFrame()});
#endif
}

void registerViewDependentModel(J3DModel* model) {
#if TARGET_PC
    if (s_refreshing || model == nullptr) {
        return;
    }

    ViewDependentModelEntry* entry =
        findEntry(s_viewDependentModels, &ViewDependentModelEntry::model, model);
    if (entry != nullptr) {
        entry->lastSeenFrame = currentFrame();
        return;
    }

    s_viewDependentModels.push_back({model, currentFrame()});
#endif
}

void registerLightProjectionModel(J3DModel* model, unsigned int materialMask) {
#if TARGET_PC
    if (s_refreshing || model == nullptr || materialMask == 0) {
        return;
    }

    LightProjectionModelEntry* entry =
        findEntry(s_lightProjectionModels, &LightProjectionModelEntry::model, model);
    if (entry != nullptr) {
        entry->materialMask = materialMask;
        entry->lastSeenFrame = currentFrame();
        return;
    }

    s_lightProjectionModels.push_back({model, materialMask, currentFrame()});
#endif
}

void registerLightingProbe(const char* label, const void* actor, J3DModel* model,
                           dKy_tevstr_c* tevstr, int tevstrType) {
#if TARGET_PC
    if (label == nullptr || actor == nullptr || model == nullptr || tevstr == nullptr) {
        return;
    }

    LightingProbeDebugState* available = nullptr;
    LightingProbeDebugState* oldest = &s_lightingProbes[0];
    for (int i = 0; i < kLightingProbeCapacity; i++) {
        LightingProbeDebugState& probe = s_lightingProbes[i];
        if (probe.actor == actor) {
            available = &probe;
            break;
        }
        if (probe.actor == nullptr && available == nullptr) {
            available = &probe;
        }
        if (probe.submission.frame < oldest->submission.frame) {
            oldest = &probe;
        }
    }
    if (available == nullptr) {
        available = oldest;
    }

    if (available->label != label || available->tevstr != tevstr) {
        available->refresh = {};
    }
    available->label = label;
    available->actor = actor;
    available->model = model;
    available->tevstr = tevstr;
    available->tevstrType = tevstrType;
    captureLightingValue(&available->submission, tevstr);
#endif
}

void recordGxLightReloadForCurrentView(bool reloaded) {
#if TARGET_PC
    currentLightingPass().gxLightReloaded = reloaded;
#endif
}

void refreshKankyoMaterialsForCurrentView() {
#if TARGET_PC
    LightingPassDebugState& lightingPass = currentLightingPass();
    lightingPass.materialRefreshRequested = true;
    if (!dusk::coop::event_presentation::shouldRefreshViewportOwnedWorldState() ||
        s_refreshing)
    {
        return;
    }
    lightingPass.materialRefreshExecuted = true;

    // Co-op: actor/background draw submission patches shared J3D material state once before
    // viewport replay. Refresh those same patches after the presented camera is active so
    // split views and collapsed fullscreen presentation do not inherit stale TEV/light state.
    const unsigned int frame = currentFrame();
    sweepStaleRegistrations(frame);

    s_refreshing = true;
    for (KankyoMaterialEntry& entry : s_materials) {
        if (isCurrentRefreshFrame(entry.lastSeenFrame, frame) && entry.modelData != nullptr &&
            entry.tevstr != nullptr)
        {
            // Co-op: material entries are the current-frame draw surface. Recompute the
            // underlying tevstr through its remembered creation inputs before patching it.
            refreshTevstrForCurrentView(entry.tevstr);
            captureLightingProbeRefresh(entry.tevstr);
            g_env_light.setLightTevColorType_MAJI(entry.modelData, entry.tevstr);
        }
    }
    for (KankyoModelEntry& entry : s_models) {
        if (isCurrentRefreshFrame(entry.lastSeenFrame, frame) && entry.model != nullptr &&
            entry.tevstr != nullptr)
        {
            refreshTevstrForCurrentView(entry.tevstr);
            captureLightingProbeRefresh(entry.tevstr);
            g_env_light.setLightTevColorType_MAJI(entry.model->getModelData(), entry.tevstr);
            diffModelKankyoMaterial(entry.model, entry.tevstr);
        }
    }
    for (ViewDependentModelEntry& entry : s_viewDependentModels) {
        if (isCurrentRefreshFrame(entry.lastSeenFrame, frame) && entry.model != nullptr) {
            // Co-op: viewCalc() is presentation work. Rebuild camera-facing/billboard matrices
            // after the active viewport view is installed without advancing model animation.
            entry.model->viewCalc();
        }
    }
    for (LightProjectionModelEntry& entry : s_lightProjectionModels) {
        if (isCurrentRefreshFrame(entry.lastSeenFrame, frame) && entry.model != nullptr) {
            refreshLightProjectionModel(entry.model, entry.materialMask);
        }
    }
    s_refreshing = false;
#endif
}

DebugState getDebugState() {
    DebugState state = {};
    const unsigned int frame = currentFrame();
    for (const ViewDependentModelEntry& entry : s_viewDependentModels) {
        if (state.viewDependentModelCount >= kDebugRegistrationCapacity ||
            !isCurrentRefreshFrame(entry.lastSeenFrame, frame))
        {
            continue;
        }
        state.viewDependentModels[state.viewDependentModelCount++] = entry.model;
    }
    for (const LightProjectionModelEntry& entry : s_lightProjectionModels) {
        if (state.lightProjectionModelCount >= kDebugRegistrationCapacity ||
            !isCurrentRefreshFrame(entry.lastSeenFrame, frame))
        {
            continue;
        }
        const int index = state.lightProjectionModelCount++;
        state.lightProjectionModels[index] = entry.model;
        state.lightProjectionMaterialMasks[index] = entry.materialMask;
    }
    for (int i = 0; i < kLightingProbeCapacity; i++) {
        if (s_lightingProbes[i].actor == nullptr) {
            continue;
        }
        state.lightingProbes[state.lightingProbeCount++] = s_lightingProbes[i];
    }
    for (int i = 0; i < kPlayerSlotCount; i++) {
        state.lightingPasses[i] = s_lightingPasses[i];
        state.lastEnemyAuthoredDemoLightingPasses[i] =
            s_lightingPasses[i].enemyAuthoredDemo
                ? s_lightingPasses[i]
                : s_lastEnemyAuthoredDemoLightingPasses[i];
    }
    return state;
}

}  // namespace dusk::coop::render_materials
