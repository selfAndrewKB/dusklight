#include "dusk/coop/render_materials.h"

#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "SSystem/SComponent/c_counter.h"
#include "d/d_kankyo.h"
#include "dusk/coop/camera.h"

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

std::vector<KankyoMaterialEntry> s_materials;
std::vector<KankyoModelEntry> s_models;
std::vector<KankyoTevstrEntry> s_tevstrs;
bool s_refreshing = false;

unsigned int currentFrame() {
    return static_cast<unsigned int>(g_Counter.mCounter0);
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

void refreshKankyoMaterialsForCurrentView() {
#if TARGET_PC
    if (!dusk::coop::camera::isSplitScreenEnabled() || s_refreshing) {
        return;
    }

    // Co-op: actor/background draw submission patches shared J3D material state once before
    // split-screen replay. Refresh those same patches after each viewport camera is active so
    // P2 does not inherit camera-0 TEV/light material state.
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
            g_env_light.setLightTevColorType_MAJI(entry.modelData, entry.tevstr);
        }
    }
    for (KankyoModelEntry& entry : s_models) {
        if (isCurrentRefreshFrame(entry.lastSeenFrame, frame) && entry.model != nullptr &&
            entry.tevstr != nullptr)
        {
            refreshTevstrForCurrentView(entry.tevstr);
            g_env_light.setLightTevColorType_MAJI(entry.model->getModelData(), entry.tevstr);
            diffModelKankyoMaterial(entry.model, entry.tevstr);
        }
    }
    s_refreshing = false;
#endif
}

}  // namespace dusk::coop::render_materials
