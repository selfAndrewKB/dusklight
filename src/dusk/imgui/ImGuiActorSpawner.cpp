#include "imgui.h"

#include "ImGuiConsole.hpp"
#include "ImGuiMenuTools.hpp"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/alink_probes.h"
#include "dusk/coop/camera.h"
#include "dusk/coop/debug_overlay.h"
#include "dusk/coop/player_slots.h"
#include "dusk/diagnostics.h"
#include "dusk/hotkeys.h"
#include "dusk/io.hpp"
#include "f_op/f_op_actor_mng.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "SSystem/SComponent/c_xyz.h"

namespace dusk {
namespace {

struct ActorSpawnerState {
    int actorId    = 0;
    int params     = -1;
    int argument   = -1;
    int angleX     = 0;
    int angleY     = 0;
    int angleZ     = 0;
    float scaleX   = 1.0f;
    float scaleY   = 1.0f;
    float scaleZ   = 1.0f;
    bool usePlayerRoom = true;
    int manualRoom = 0;
    int spawnCount = 1;
    bool hasResult = false;
    unsigned int lastResult = 0;
    int lastAttempted = 0;
};

ActorSpawnerState s_state;

void tryCoopHotkeySpawnSecondary() {
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl || io.KeyShift || io.KeyAlt || !ImGui::IsKeyPressed(ImGuiKey_F12)) {
        return;
    }

    dusk::diagnostics::setSecondaryAlinkActionMirrorProfileEnabled(true);
    dusk::coop::camera::setSplitScreenEnabled(true);
    dusk::coop::setSecondaryAlinkProbeFlags(dusk::coop::kDefaultSecondaryAlinkProbeFlags);

    daAlink_c* player = (daAlink_c*)dComIfGp_getPlayer(0);
    if (player == nullptr) {
        DuskToast("Co-op diagnostics enabled; primary Link is not available");
        return;
    }

    if (dusk::coop::getPlayer(dusk::coop::PlayerSlot::Slot1) != nullptr) {
        dusk::coop::camera::ensureSecondaryCamera();
        DuskToast("Co-op diagnostics and split screen enabled; secondary Link already exists");
        return;
    }

    s_state.lastResult = dusk::coop::spawnPlayer(dusk::coop::PlayerSlot::Slot1, player);
    s_state.lastAttempted = 1;
    s_state.hasResult = true;

    if (s_state.lastResult != 0) {
        DuskToast("Co-op diagnostics and split screen enabled; spawned secondary Link");
    } else {
        DuskToast("Co-op diagnostics and split screen enabled; secondary Link spawn failed");
    }
}

void tryCoopHotkeyToggleEnemyTargetOverlay() {
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl || !io.KeyShift || io.KeyAlt || !ImGui::IsKeyPressed(ImGuiKey_F12)) {
        return;
    }

    dusk::coop::debug_overlay::toggleEnemyTargetOverlay();
    DuskToast(dusk::coop::debug_overlay::isEnemyTargetOverlayEnabled()
                  ? "Co-op enemy target overlay enabled"
                  : "Co-op enemy target overlay disabled");
}

void secondaryAlinkProbeCheckbox(const char* label, dusk::coop::SecondaryAlinkProbeFlag flag) {
    unsigned int flags = dusk::coop::getSecondaryAlinkProbeFlags();
    bool enabled = (flags & static_cast<unsigned int>(flag)) != 0;
    if (!ImGui::Checkbox(label, &enabled)) {
        return;
    }

    if (enabled) {
        flags |= static_cast<unsigned int>(flag);
    } else {
        flags &= ~static_cast<unsigned int>(flag);
    }
    if (flag == dusk::coop::SecondaryAlinkProbe_SkipCreateModelCalc && !enabled) {
        // Co-op: this test combo needs animation playback restored before model calc can safely run.
        flags &= ~static_cast<unsigned int>(dusk::coop::SecondaryAlinkProbe_SkipCreateAnimePlay);
    } else if (flag == dusk::coop::SecondaryAlinkProbe_SkipCreateAnimePlay && enabled) {
        // Co-op: skipping animation playback leaves secondary model calc with invalid startup pose data.
        flags |= static_cast<unsigned int>(dusk::coop::SecondaryAlinkProbe_SkipCreateModelCalc);
    }
    dusk::coop::setSecondaryAlinkProbeFlags(flags);
}

}  // namespace

void ImGuiMenuTools::ShowActorSpawner() {
    tryCoopHotkeySpawnSecondary();
    tryCoopHotkeyToggleEnemyTargetOverlay();

    if (!m_showActorSpawner) {
        return;
    }

    if (!ImGui::Begin("Actor Spawner", &m_showActorSpawner)) {
        ImGui::End();
        return;
    }

    daAlink_c* player = (daAlink_c*)dComIfGp_getPlayer(0);

    ImGui::SeparatorText("Co-op");
    bool secondarySlotAvailable = dusk::coop::getPlayer(dusk::coop::PlayerSlot::Slot1) == nullptr;
    bool canSpawnSecondary = player != nullptr && secondarySlotAvailable;
    if (!canSpawnSecondary) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Spawn Secondary Link", ImVec2(-1, 0))) {
        s_state.lastResult = dusk::coop::spawnPlayer(dusk::coop::PlayerSlot::Slot1, player);
        s_state.lastAttempted = 1;
        s_state.hasResult = true;
    }

    if (!canSpawnSecondary) {
        ImGui::EndDisabled();
    }
    if (player == nullptr) {
        ImGui::TextDisabled("Player not available");
    } else if (!secondarySlotAvailable) {
        ImGui::TextDisabled("Secondary slot already occupied");
    }

    bool diagnosticsEnabled = dusk::diagnostics::isSecondaryAlinkActionMirrorProfileEnabled();
    if (ImGui::Checkbox("Record action mirror diagnostics", &diagnosticsEnabled)) {
        dusk::diagnostics::setSecondaryAlinkActionMirrorProfileEnabled(diagnosticsEnabled);
    }
    bool splitScreenEnabled = dusk::coop::camera::isSplitScreenEnabled();
    if (ImGui::Checkbox("Native split screen", &splitScreenEnabled)) {
        dusk::coop::camera::setSplitScreenEnabled(splitScreenEnabled);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Ensure P2 camera")) {
        if (dusk::coop::camera::ensureSecondaryCamera()) {
            DuskToast("Secondary camera ready");
        } else {
            DuskToast("Secondary camera not ready");
        }
    }
    ImGui::TextDisabled("P2 camera: %s%s",
                        dusk::coop::camera::isSecondaryCameraReady() ? "ready" : "not ready",
                        dusk::coop::camera::isSecondaryCameraRequested() ? " (requested)" : "");
    ImGui::TextDisabled("Hotkey: %s enables diagnostics/split screen, resets probes, and spawns P2",
                        dusk::hotkeys::COOP_SPAWN_SECONDARY_LINK);
    bool enemyTargetOverlayEnabled = dusk::coop::debug_overlay::isEnemyTargetOverlayEnabled();
    if (ImGui::Checkbox("Show enemy target overlay", &enemyTargetOverlayEnabled)) {
        dusk::coop::debug_overlay::setEnemyTargetOverlayEnabled(enemyTargetOverlayEnabled);
    }
    ImGui::TextDisabled("Hotkey: %s toggles enemy target overlay",
                        dusk::hotkeys::COOP_TOGGLE_ENEMY_TARGET_OVERLAY);
    if (diagnosticsEnabled) {
        if (ImGui::SmallButton("Flush diagnostics")) {
            dusk::diagnostics::flush("manual-ui");
        }
        ImGui::TextWrapped("Output: %s",
            dusk::io::fs_path_to_string(dusk::diagnostics::getOutputPath()).c_str());
    }

    if (ImGui::TreeNode("Secondary ALINK probes")) {
        if (ImGui::SmallButton("Default")) {
            dusk::coop::setSecondaryAlinkProbeFlags(dusk::coop::kDefaultSecondaryAlinkProbeFlags);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            dusk::coop::setSecondaryAlinkProbeFlags(0);
        }

        secondaryAlinkProbeCheckbox("Skip execute", dusk::coop::SecondaryAlinkProbe_SkipExecute);
        secondaryAlinkProbeCheckbox("Skip draw", dusk::coop::SecondaryAlinkProbe_SkipDraw);
        secondaryAlinkProbeCheckbox("Skip wait animation bind", dusk::coop::SecondaryAlinkProbe_SkipWaitAnimeBind);
        secondaryAlinkProbeCheckbox("Skip start proc init", dusk::coop::SecondaryAlinkProbe_SkipStartProcInit);
        secondaryAlinkProbeCheckbox("Skip set matrix", dusk::coop::SecondaryAlinkProbe_SkipSetMatrix);
        secondaryAlinkProbeCheckbox("Skip create animation play", dusk::coop::SecondaryAlinkProbe_SkipCreateAnimePlay);
        secondaryAlinkProbeCheckbox("Skip create model calc", dusk::coop::SecondaryAlinkProbe_SkipCreateModelCalc);
        secondaryAlinkProbeCheckbox("Skip face texture animation", dusk::coop::SecondaryAlinkProbe_SkipFaceTextureAnime);
        secondaryAlinkProbeCheckbox("Skip item matrix", dusk::coop::SecondaryAlinkProbe_SkipItemMatrix);
        secondaryAlinkProbeCheckbox("Skip item actor setup", dusk::coop::SecondaryAlinkProbe_SkipSetItemActor);
        secondaryAlinkProbeCheckbox(
            "Restore P1 model data owner",
            dusk::coop::SecondaryAlinkProbe_RestorePrimaryModelDataOwner
        );
        secondaryAlinkProbeCheckbox(
            "Scoped draw model data owner",
            dusk::coop::SecondaryAlinkProbe_ScopedDrawModelDataOwner
        );
        secondaryAlinkProbeCheckbox(
            "Scoped execute model data owner",
            dusk::coop::SecondaryAlinkProbe_ScopedExecuteModelDataOwner
        );
        secondaryAlinkProbeCheckbox(
            "Ignore shared attention lock",
            dusk::coop::SecondaryAlinkProbe_IgnoreSharedAttentionLock
        );
        ImGui::TreePop();
    }

    ImGui::SeparatorText("Actor");
    ImGui::InputInt("Actor ID", &s_state.actorId);
    ImGui::InputInt("Params (hex)", &s_state.params, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::InputInt("Argument", &s_state.argument);
    s_state.argument = (s_state.argument < -128) ? -128 : (s_state.argument > 127) ? 127 : s_state.argument;

    ImGui::SeparatorText("Angle");
    ImGui::InputInt("Angle X", &s_state.angleX);
    ImGui::InputInt("Angle Y", &s_state.angleY);
    ImGui::InputInt("Angle Z", &s_state.angleZ);

    ImGui::SeparatorText("Scale");
    ImGui::InputFloat("Scale X", &s_state.scaleX, 0.1f, 1.0f);
    ImGui::InputFloat("Scale Y", &s_state.scaleY, 0.1f, 1.0f);
    ImGui::InputFloat("Scale Z", &s_state.scaleZ, 0.1f, 1.0f);

    ImGui::SeparatorText("Spawn");
    ImGui::InputInt("Count", &s_state.spawnCount);
    if (s_state.spawnCount < 1) {
        s_state.spawnCount = 1;
    }

    ImGui::SeparatorText("Position");
    ImGui::Checkbox("Use player room", &s_state.usePlayerRoom);
    if (!s_state.usePlayerRoom) {
        ImGui::InputInt("Room No", &s_state.manualRoom);
    }

    if (player != nullptr) {
        ImGui::Text("Spawn pos: %.2f, %.2f, %.2f",
            player->current.pos.x, player->current.pos.y, player->current.pos.z);
    } else {
        ImGui::TextDisabled("Player not available");
    }

    ImGui::Separator();

    bool canSpawn = player != nullptr;
    if (!canSpawn) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Spawn", ImVec2(-1, 0))) {
        cXyz pos = player->current.pos;
        csXyz angle;
        angle.set((s16)s_state.angleX, (s16)s_state.angleY, (s16)s_state.angleZ);
        cXyz scale(s_state.scaleX, s_state.scaleY, s_state.scaleZ);
        int roomNo = s_state.usePlayerRoom ? player->current.roomNo : s_state.manualRoom;

        layer_class* savedLayer = fpcLy_CurrentLayer();
        base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
        if (playScene != nullptr) {
            fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
        }

        s_state.lastResult = 0;
        s_state.lastAttempted = s_state.spawnCount;
        for (int i = 0; i < s_state.spawnCount; ++i) {
            unsigned int result = fopAcM_create(
                (s16)s_state.actorId,
                (u32)s_state.params,
                &pos,
                roomNo,
                &angle,
                &scale,
                (s8)s_state.argument
            );
            if (result != 0) {
                s_state.lastResult = result;
            }
        }
        s_state.hasResult = true;

        fpcLy_SetCurrentLayer(savedLayer);
    }

    if (!canSpawn) {
        ImGui::EndDisabled();
    }

    if (s_state.hasResult) {
        if (s_state.lastResult != 0) {
            if (s_state.lastAttempted == 1) {
                ImGui::Text("Spawned: proc ID %u", s_state.lastResult);
            } else {
                ImGui::Text("Spawned %d (last proc ID %u)", s_state.lastAttempted, s_state.lastResult);
            }
        } else {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Spawn failed (returned 0)");
        }
    }

    ImGui::End();
}

}  // namespace dusk
