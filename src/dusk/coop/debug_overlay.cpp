#include "dusk/coop/debug_overlay.h"

#include "d/d_debug_viewer.h"
#include "dusk/coop/bokoblin_attack_probe.h"
#include "dusk/coop/damage_owner.h"
#include "dusk/coop/defender_owner.h"
#include "dusk/coop/enemy_targeting.h"
#include "SSystem/SComponent/c_math.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_view.h"
#include "dolphin/gx.h"
#include "imgui.h"
#include "m_Do/m_Do_graphic.h"

#include <cstdio>
#include <cstring>

namespace dusk::coop::debug_overlay {
namespace {

bool s_enemyTargetOverlayEnabled = true;
bool s_damageHitOverlayEnabled = true;

constexpr int kMaxTextDecisions = 10;
constexpr int kMaxChosenDecisions = 16;
constexpr int kMaxLineLabels = 32;
constexpr float kMaxActiveOverlayDistanceXZ = 5000.0f;
constexpr float kVisionConeLengthXZ = 1200.0f;
constexpr s16 kVisionConeHalfAngle = 0x4000;
constexpr int kVisionConeSegments = 8;
constexpr int kRecentDecisionMaxFrameAge = 10;
constexpr int kRecentDamageHitMaxFrameAge = 60;
constexpr int kRecentDefenderContactMaxFrameAge = 60;

struct ChosenDecision {
    const EnemyTargetDecisionDebug* decision = nullptr;
    int priority = 0;
};

struct LineLabel {
    int x = 0;
    int y = 0;
    char text[96] = {};
};

LineLabel s_lineLabels[kMaxLineLabels];
int s_lineLabelCount = 0;

GXColor slotColor(PlayerSlot slot, bool committed) {
    if (committed) {
        return {0xff, 0x30, 0x20, 0xe0};
    }

    switch (slot) {
    case PlayerSlot::Slot0:
        return {0x80, 0xc0, 0xff, 0xc0};
    case PlayerSlot::Slot1:
        return {0x40, 0xff, 0x90, 0xc0};
    case PlayerSlot::Slot2:
        return {0xd0, 0x90, 0xff, 0xc0};
    case PlayerSlot::Slot3:
        return {0xff, 0xd0, 0x60, 0xc0};
    default:
        return {0xc0, 0xc0, 0xc0, 0xa0};
    }
}

GXColor lineColor(const EnemyTargetDecisionDebug& decision, int priority) {
    if (decision.committed || decision.reason == EnemyTargetReason::RetainCommitted) {
        return {0xff, 0x30, 0x20, 0xe0};
    }
    if (priority >= 30) {
        return {0xff, 0xc0, 0x20, 0xd0};
    }
    return slotColor(decision.selected.slot, false);
}

int slotIndex(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid ? static_cast<int>(slot) : -1;
}

cXyz debugPos(const fopAc_ac_c* actor, float yOffset) {
    cXyz pos = actor->current.pos;
    pos.y += yOffset;
    return pos;
}

int latestSimFrame(const EnemyTargetingDebugState& state) {
    u32 frame = 0;
    for (int i = 0; i < state.decisionCount; i++) {
        if (state.decisions[i].currentSimFrame > frame) {
            frame = state.decisions[i].currentSimFrame;
        }
    }
    return static_cast<int>(frame);
}

bool isRecentDecision(const EnemyTargetDecisionDebug& decision, int latestFrame) {
    return latestFrame - static_cast<int>(decision.currentSimFrame) <= kRecentDecisionMaxFrameAge;
}

bool systemContains(const EnemyTargetDecisionDebug& decision, const char* needle) {
    return std::strstr(decision.label, needle) != nullptr;
}

int decisionPriority(const EnemyTargetDecisionDebug& decision) {
    if (decision.committed || systemContains(decision, ".attack")) {
        return 40;
    }
    if (systemContains(decision, ".find") || systemContains(decision, ".move_out")) {
        return 30;
    }
    if (systemContains(decision, ".search")) {
        return 10;
    }
    return 0;
}

bool shouldDrawDecision(const EnemyTargetDecisionDebug& decision) {
    return decision.observer != nullptr && decision.selected.found &&
           decision.selected.slot != PlayerSlot::Invalid &&
           decision.selected.localActor != nullptr;
}

bool shouldDrawWorldDecision(const EnemyTargetDecisionDebug& decision, int priority) {
    if (!shouldDrawDecision(decision)) {
        return false;
    }
    if (priority < 30) {
        return false;
    }
    if (decision.selected.distanceXZ > kMaxActiveOverlayDistanceXZ) {
        return false;
    }
    return true;
}

bool shouldDrawConeDecision(const EnemyTargetDecisionDebug& decision, int priority) {
    if (!shouldDrawDecision(decision)) {
        return false;
    }
    if (priority < 10) {
        return false;
    }
    if (decision.selected.distanceXZ > kMaxActiveOverlayDistanceXZ) {
        return false;
    }
    return true;
}

bool sameSelectedActor(const EnemyTargetDecisionDebug& lhs, const EnemyTargetDecisionDebug& rhs) {
    return lhs.selected.localActor == rhs.selected.localActor && lhs.selected.slot == rhs.selected.slot;
}

void drawWorldDecision(const EnemyTargetDecisionDebug& decision, int priority) {
    if (!shouldDrawWorldDecision(decision, priority)) {
        return;
    }

    GXColor color = lineColor(decision, priority);
    GXColor targetColor = slotColor(decision.selected.slot, false);
    cXyz observerPos = debugPos(decision.observer, 80.0f);
    cXyz targetPos = debugPos(decision.selected.localActor, 120.0f);
    dDbVw_drawLineXlu(observerPos, targetPos, color, TRUE, 12);
    dDbVw_drawSphereXlu(observerPos, 22.0f, color, TRUE);
    dDbVw_drawSphereXlu(targetPos, 18.0f, targetColor, TRUE);
}

bool shouldDrawDamageHit(const damage_owner::DamageOwnerHitDebug& hit, u32 currentFrame) {
    return hit.eventId != 0 && hit.owner.found && hit.owner.slot != PlayerSlot::Invalid &&
           currentFrame >= hit.simFrame &&
           currentFrame - hit.simFrame <= static_cast<u32>(kRecentDamageHitMaxFrameAge);
}

bool shouldDrawDefenderContact(const defender_owner::DefenderOwnerDecisionDebug& decision,
                               u32 currentFrame) {
    return decision.eventId != 0 && decision.defender.found &&
           decision.defender.slot != PlayerSlot::Invalid &&
           currentFrame >= decision.simFrame &&
           currentFrame - decision.simFrame <= static_cast<u32>(kRecentDefenderContactMaxFrameAge);
}

const char* defenderContactSource(const char* label) {
    if (std::strstr(label, "attack_guard.0") != nullptr) {
        return "atk-sphere0";
    }
    if (std::strstr(label, "attack_guard.1") != nullptr) {
        return "atk-sphere1";
    }
    return "contact";
}

void drawDamageHitWorld() {
    if (!isDamageHitOverlayEnabled()) {
        return;
    }

    const damage_owner::DamageOwnerDebugState& state = damage_owner::getDamageOwnerDebugState();
    for (int i = 0; i < state.hitCount; i++) {
        const damage_owner::DamageOwnerHitDebug& hit = state.hits[i];
        if (!shouldDrawDamageHit(hit, state.currentSimFrame)) {
            continue;
        }

        const GXColor color = slotColor(hit.owner.slot, false);
        cXyz hitPos = hit.hitPos;
        dDbVw_drawSphereXlu(hitPos, 30.0f, color, TRUE);
    }
}

void drawDefenderContactWorld() {
    const defender_owner::DefenderOwnerDebugState& state =
        defender_owner::getDefenderOwnerDebugState();
    for (int i = 0; i < state.decisionCount; i++) {
        const defender_owner::DefenderOwnerDecisionDebug& decision = state.decisions[i];
        if (!shouldDrawDefenderContact(decision, state.currentSimFrame)) {
            continue;
        }

        GXColor color = slotColor(decision.defender.slot, false);
        if (decision.defender.guarded) {
            color = {0x40, 0xff, 0xff, 0xd0};
        }
        cXyz hitPos = decision.defender.hitPos;
        dDbVw_drawSphereXlu(hitPos, decision.defender.guarded ? 42.0f : 30.0f, color, TRUE);
    }
}

void endpointFromAngle(const cXyz& origin, s16 angle, float length, cXyz* out) {
    out->x = origin.x + cM_ssin(angle) * length;
    out->y = origin.y;
    out->z = origin.z + cM_scos(angle) * length;
}

void drawVisionCone(const EnemyTargetDecisionDebug& decision, int priority) {
    if (!shouldDrawConeDecision(decision, priority)) {
        return;
    }

    cXyz origin = debugPos(decision.observer, 45.0f);
    cXyz center;
    cXyz left;
    cXyz right;
    // Co-op: this is an overlay approximation of the common Bokoblin forward wake cone. The
    // policy still relies on the actor's real sight/range checks; this only makes "close but
    // behind/outside cone" cases visible while debugging target handoffs.
    const float length = kVisionConeLengthXZ;
    endpointFromAngle(origin, decision.observer->shape_angle.y, length, &center);
    endpointFromAngle(origin, decision.observer->shape_angle.y - kVisionConeHalfAngle, length, &left);
    endpointFromAngle(origin, decision.observer->shape_angle.y + kVisionConeHalfAngle, length, &right);

    const GXColor coneColor = {0xff, 0xff, 0x80, 0x70};
    const GXColor centerColor = {0xff, 0xff, 0xff, 0x60};
    dDbVw_drawLineXlu(origin, center, centerColor, TRUE, 10);
    dDbVw_drawLineXlu(origin, left, coneColor, TRUE, 10);
    dDbVw_drawLineXlu(origin, right, coneColor, TRUE, 10);

    cXyz prev = left;
    for (int i = 1; i <= kVisionConeSegments; i++) {
        const s16 angle = decision.observer->shape_angle.y - kVisionConeHalfAngle +
                          static_cast<s16>((static_cast<int>(kVisionConeHalfAngle) * 2 * i) /
                                           kVisionConeSegments);
        cXyz next;
        endpointFromAngle(origin, angle, length, &next);
        dDbVw_drawLineXlu(prev, next, coneColor, TRUE, 10);
        prev = next;
    }
}

bool projectPoint(const cXyz& pos, const view_class* view, const view_port_class* viewport,
                  f32* outX, f32* outY, f32* outZ) {
    f32 projection[7];
    f32 gxViewport[6] = {
        viewport->x_orig,
        viewport->y_orig,
        viewport->width,
        viewport->height,
        viewport->near_z,
        viewport->far_z,
    };
    GXGetProjectionv(projection);
    GXProject(pos.x, pos.y, pos.z, view->viewMtx, projection, gxViewport, outX, outY, outZ);
    return *outZ >= 0.0f && *outZ <= 1.0f;
}

void captureLineLabel(const EnemyTargetDecisionDebug& decision, int priority, const view_class* view,
                      const view_port_class* viewport) {
    if (!shouldDrawWorldDecision(decision, priority) || s_lineLabelCount >= kMaxLineLabels) {
        return;
    }
    if (decision.selected.slot == PlayerSlot::Invalid) {
        return;
    }

    cXyz observerPos = debugPos(decision.observer, 80.0f);
    cXyz targetPos = debugPos(decision.selected.localActor, 120.0f);
    cXyz midpoint;
    midpoint.x = (observerPos.x + targetPos.x) * 0.5f;
    midpoint.y = (observerPos.y + targetPos.y) * 0.5f;
    midpoint.z = (observerPos.z + targetPos.z) * 0.5f;

    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    if (!projectPoint(midpoint, view, viewport, &x, &y, &z)) {
        return;
    }

    LineLabel& label = s_lineLabels[s_lineLabelCount++];
    label.x = static_cast<int>(x);
    label.y = static_cast<int>(y);
    std::snprintf(label.text, sizeof(label.text), "P%d %s%s",
                  slotIndex(decision.selected.slot) + 1,
                  enemyTargetReasonName(decision.reason),
                  decision.committed ? " atk" : "");
}

void captureDamageHitLabels(const view_class* view, const view_port_class* viewport) {
    if (!isDamageHitOverlayEnabled()) {
        return;
    }

    const damage_owner::DamageOwnerDebugState& state = damage_owner::getDamageOwnerDebugState();
    for (int i = 0; i < state.hitCount && s_lineLabelCount < kMaxLineLabels; i++) {
        const damage_owner::DamageOwnerHitDebug& hit = state.hits[i];
        if (!shouldDrawDamageHit(hit, state.currentSimFrame)) {
            continue;
        }

        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
        if (!projectPoint(hit.hitPos, view, viewport, &x, &y, &z)) {
            continue;
        }

        LineLabel& label = s_lineLabels[s_lineLabelCount++];
        label.x = static_cast<int>(x);
        label.y = static_cast<int>(y);
        if (damage_owner::damageOwnerAttackUsesCutState(hit.owner.attackType)) {
            std::snprintf(label.text, sizeof(label.text), "P%d %s cut=%s count=%d",
                          slotIndex(hit.owner.slot) + 1,
                          damage_owner::damageOwnerAttackName(hit.owner.attackType),
                          damage_owner::damageOwnerCutTypeName(hit.owner.cutType),
                          hit.owner.cutCount);
        } else {
            std::snprintf(label.text, sizeof(label.text), "P%d %s",
                          slotIndex(hit.owner.slot) + 1,
                          damage_owner::damageOwnerAttackName(hit.owner.attackType));
        }
    }
}

void captureDefenderContactLabels(const view_class* view, const view_port_class* viewport) {
    const defender_owner::DefenderOwnerDebugState& state =
        defender_owner::getDefenderOwnerDebugState();
    for (int i = 0; i < state.decisionCount && s_lineLabelCount < kMaxLineLabels; i++) {
        const defender_owner::DefenderOwnerDecisionDebug& decision = state.decisions[i];
        if (!shouldDrawDefenderContact(decision, state.currentSimFrame)) {
            continue;
        }

        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
        if (!projectPoint(decision.defender.hitPos, view, viewport, &x, &y, &z)) {
            continue;
        }

        LineLabel& label = s_lineLabels[s_lineLabelCount++];
        label.x = static_cast<int>(x);
        label.y = static_cast<int>(y);
        std::snprintf(label.text, sizeof(label.text), "%s P%d %s",
                      defenderContactSource(decision.label),
                      slotIndex(decision.defender.slot) + 1,
                      decision.defender.guarded ? "guard" : "hit");
    }
}

int chooseDecisions(const EnemyTargetingDebugState& state, ChosenDecision* choices, int maxChoices) {
    const int latestFrame = latestSimFrame(state);
    int choiceCount = 0;

    for (int i = 0; i < state.decisionCount; i++) {
        const EnemyTargetDecisionDebug& decision = state.decisions[i];
        if (!shouldDrawDecision(decision) || !isRecentDecision(decision, latestFrame)) {
            continue;
        }

        const int priority = decisionPriority(decision);
        if (priority == 0) {
            continue;
        }

        int existing = -1;
        for (int j = 0; j < choiceCount; j++) {
            if (choices[j].decision != nullptr &&
                choices[j].decision->observer == decision.observer)
            {
                existing = j;
                break;
            }
        }

        if (existing >= 0) {
            const EnemyTargetDecisionDebug& chosen = *choices[existing].decision;
            const bool chosenIsActive = choices[existing].priority >= 30 && choices[existing].priority < 40;
            const bool candidateIsActive = priority >= 30 && priority < 40;
            const bool chosenIsCommitted = choices[existing].priority >= 40;
            const bool candidateIsCommitted = priority >= 40;
            // Co-op: if a committed sub-system disagrees with active chase/find targeting, prefer the
            // active target in the overlay. The policy records remain visible in diagnostics.
            if (candidateIsCommitted && chosenIsActive && !sameSelectedActor(decision, chosen)) {
                continue;
            }
            if (candidateIsActive && chosenIsCommitted && !sameSelectedActor(decision, chosen)) {
                choices[existing].decision = &decision;
                choices[existing].priority = priority;
                continue;
            }
            if (priority > choices[existing].priority) {
                choices[existing].decision = &decision;
                choices[existing].priority = priority;
            }
            continue;
        }

        if (choiceCount < maxChoices) {
            choices[choiceCount].decision = &decision;
            choices[choiceCount].priority = priority;
            choiceCount++;
        }
    }

    return choiceCount;
}

void drawText(const ChosenDecision* choices, int choiceCount) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    const ImU32 shadowColor = IM_COL32(0, 0, 0, 190);
    const ImU32 textColor = IM_COL32(255, 255, 255, 230);
    const ImU32 labelColor = IM_COL32(255, 220, 120, 235);
    int y = 72;
    drawList->AddText(ImVec2(14.0f, static_cast<float>(y + 2)), shadowColor,
                      "co-op enemy targets  orange=active red=committed");
    drawList->AddText(ImVec2(12.0f, static_cast<float>(y)), textColor,
                      "co-op enemy targets  orange=active red=committed");
    y += 15;

    int shown = 0;
    for (int i = 0; i < choiceCount && shown < kMaxTextDecisions; i++) {
        const EnemyTargetDecisionDebug& decision = *choices[i].decision;
        char line[160];
        std::snprintf(line, sizeof(line), "%s slot %d %s%s %.2f/%.2fs xz %.0f",
                      decision.label,
                      slotIndex(decision.selected.slot),
                      enemyTargetReasonName(decision.reason),
                      decision.committed ? " committed" : "",
                      decision.stickyElapsedSeconds,
                      decision.retainSeconds,
                      decision.selected.distanceXZ);
        drawList->AddText(ImVec2(14.0f, static_cast<float>(y + 2)), shadowColor, line);
        drawList->AddText(ImVec2(12.0f, static_cast<float>(y)), textColor, line);
        y += 15;
        shown++;
    }

    const bokoblin_attack_probe::BokoblinAttackProbeDebugState& bokoState =
        bokoblin_attack_probe::getBokoblinAttackProbeDebugState();
    for (int i = 0; i < bokoState.probeCount && shown < kMaxTextDecisions; i++) {
        const bokoblin_attack_probe::BokoblinAttackProbe& probe = bokoState.probes[i];
        if (probe.eventId == 0 || !probe.loopSuspect) {
            continue;
        }

        char line[160];
        if (probe.targetSlot != PlayerSlot::Invalid) {
            std::snprintf(line, sizeof(line),
                          "boko %d attack-loop? state %d bck %d frame %.1f speed %.2f target P%d",
                          probe.actorId, probe.state, probe.bck, probe.animFrame, probe.playSpeed,
                          slotIndex(probe.targetSlot) + 1);
        } else {
            std::snprintf(line, sizeof(line),
                          "boko %d attack-loop? state %d bck %d frame %.1f speed %.2f target none",
                          probe.actorId, probe.state, probe.bck, probe.animFrame, probe.playSpeed);
        }
        drawList->AddText(ImVec2(14.0f, static_cast<float>(y + 2)), shadowColor, line);
        drawList->AddText(ImVec2(12.0f, static_cast<float>(y)), IM_COL32(255, 120, 90, 245),
                          line);
        y += 15;
        shown++;
    }

    for (int i = 0; i < s_lineLabelCount; i++) {
        const ImGuiIO& io = ImGui::GetIO();
        const float scaleX = io.DisplaySize.x > 0.0f ? io.DisplaySize.x / static_cast<float>(FB_WIDTH) : 1.0f;
        const float scaleY = io.DisplaySize.y > 0.0f ? io.DisplaySize.y / static_cast<float>(FB_HEIGHT) : 1.0f;
        const float labelX = static_cast<float>(s_lineLabels[i].x) * scaleX;
        const float labelY = static_cast<float>(s_lineLabels[i].y) * scaleY;
        const ImVec2 shadowPos(labelX + 2.0f, labelY + 2.0f);
        const ImVec2 textPos(labelX, labelY);
        drawList->AddText(shadowPos, shadowColor, s_lineLabels[i].text);
        drawList->AddText(textPos, labelColor, s_lineLabels[i].text);
    }
}

}  // namespace

bool isEnemyTargetOverlayEnabled() {
    return s_enemyTargetOverlayEnabled;
}

void setEnemyTargetOverlayEnabled(bool enabled) {
    s_enemyTargetOverlayEnabled = enabled;
}

void toggleEnemyTargetOverlay() {
    setEnemyTargetOverlayEnabled(!isEnemyTargetOverlayEnabled());
}

bool isDamageHitOverlayEnabled() {
    return s_damageHitOverlayEnabled;
}

void setDamageHitOverlayEnabled(bool enabled) {
    s_damageHitOverlayEnabled = enabled;
}

void toggleDamageHitOverlay() {
    setDamageHitOverlayEnabled(!isDamageHitOverlayEnabled());
}

void drawEnemyTargetOverlay() {
    if (!isEnemyTargetOverlayEnabled()) {
        return;
    }

    const EnemyTargetingDebugState& state = getEnemyTargetingDebugState();
    ChosenDecision choices[kMaxChosenDecisions] = {};
    const int choiceCount = chooseDecisions(state, choices, kMaxChosenDecisions);
    s_lineLabelCount = 0;
    for (int i = 0; i < choiceCount; i++) {
        drawVisionCone(*choices[i].decision, choices[i].priority);
        drawWorldDecision(*choices[i].decision, choices[i].priority);
    }
    drawDamageHitWorld();
    drawDefenderContactWorld();
}

void captureEnemyTargetOverlayLabels(const view_class* view, const view_port_class* viewport) {
    if (!isEnemyTargetOverlayEnabled() || view == nullptr || viewport == nullptr) {
        return;
    }
    // Co-op: collect labels once for each rendered viewport. The final ImGui pass is fullscreen, so
    // viewport-aware projection is what keeps split-screen labels on the side whose camera sees them.

    const EnemyTargetingDebugState& state = getEnemyTargetingDebugState();
    ChosenDecision choices[kMaxChosenDecisions] = {};
    const int choiceCount = chooseDecisions(state, choices, kMaxChosenDecisions);
    for (int i = 0; i < choiceCount; i++) {
        captureLineLabel(*choices[i].decision, choices[i].priority, view, viewport);
    }
    captureDamageHitLabels(view, viewport);
    captureDefenderContactLabels(view, viewport);
}

void drawEnemyTargetTextOverlay() {
    if (!isEnemyTargetOverlayEnabled()) {
        return;
    }

    const EnemyTargetingDebugState& state = getEnemyTargetingDebugState();
    ChosenDecision choices[kMaxChosenDecisions] = {};
    const int choiceCount = chooseDecisions(state, choices, kMaxChosenDecisions);
    drawText(choices, choiceCount);
}

}  // namespace dusk::coop::debug_overlay
