#include "dusk/coop/hud_diagnostics.h"

#include "dusk/coop/player_item_selection.h"
#include "imgui.h"

#include <cstdio>

namespace dusk::coop::hud_diagnostics {
namespace {

HudPresentationDebugState s_state;
bool s_overlayEnabled = true;

int slotIndex(PlayerSlot slot) {
    return slot != PlayerSlot::Invalid ? static_cast<int>(slot) : -1;
}

void refreshSlotItems() {
    for (int slot = 0; slot < 2; slot++) {
        for (int item = 0; item < 2; item++) {
            const PlayerSlot playerSlot = static_cast<PlayerSlot>(slot);
            const player_item_selection::ItemSelectionSnapshot snapshot =
                player_item_selection::inspectItem(playerSlot, item);
            s_state.slotItems[slot][item] = {
                snapshot.selectIndex,
                snapshot.mixIndex,
                snapshot.item,
                snapshot.count,
                snapshot.maxCount,
            };
        }
    }
}

void drawLine(ImDrawList* drawList, int& y, ImU32 color, const char* text) {
    const ImU32 shadowColor = IM_COL32(0, 0, 0, 190);
    drawList->AddText(ImVec2(14.0f, static_cast<float>(y + 2)), shadowColor, text);
    drawList->AddText(ImVec2(12.0f, static_cast<float>(y)), color, text);
    y += 15;
}

}  // namespace

void recordSnapshot(const ReplaySnapshot& snapshot) {
    const int phase = static_cast<int>(snapshot.phase);
    if (phase < 0 || phase >= static_cast<int>(ReplayPhase::Count)) {
        return;
    }

    s_state.snapshots[phase] = snapshot;
    s_state.snapshots[phase].valid = true;
    refreshSlotItems();
    s_state.revision++;
}

const HudPresentationDebugState& getState() {
    return s_state;
}

bool isOverlayEnabled() {
    return s_overlayEnabled;
}

void setOverlayEnabled(bool enabled) {
    s_overlayEnabled = enabled;
}

const char* replayPhaseName(ReplayPhase phase) {
    switch (phase) {
    case ReplayPhase::BeforeSecondaryApply:
        return "pre";
    case ReplayPhase::SecondaryApplied:
        return "p2";
    case ReplayPhase::PrimaryRestored:
        return "restore";
    default:
        return "invalid";
    }
}

void drawTextOverlay() {
    if (!isOverlayEnabled() || s_state.revision == 0 || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (drawList == nullptr) {
        return;
    }

    const ImU32 titleColor = IM_COL32(160, 235, 255, 245);
    const ImU32 textColor = IM_COL32(255, 255, 255, 235);
    int y = 300;
    char line[256];
    std::snprintf(line, sizeof(line), "co-op HUD replay diagnostics rev %u", s_state.revision);
    drawLine(drawList, y, titleColor, line);

    for (int slot = 0; slot < 2; slot++) {
        std::snprintf(line, sizeof(line), "P%d resolver", slot + 1);
        drawLine(drawList, y, textColor, line);
        for (int item = 0; item < 2; item++) {
            const ItemResolverDebug& resolved = s_state.slotItems[slot][item];
            std::snprintf(line, sizeof(line), " %c sel=%u mix=%u item=%u num=%d/%d",
                          item == 0 ? 'X' : 'Y', static_cast<unsigned int>(resolved.selectIndex),
                          static_cast<unsigned int>(resolved.mixIndex),
                          static_cast<unsigned int>(resolved.item), static_cast<int>(resolved.count),
                          resolved.maxCount);
            drawLine(drawList, y, textColor, line);
        }
    }

    for (int i = 0; i < static_cast<int>(ReplayPhase::Count); i++) {
        const ReplaySnapshot& snapshot = s_state.snapshots[i];
        if (!snapshot.valid) {
            continue;
        }

        std::snprintf(line, sizeof(line), "%s owner=P%d do=%u", replayPhaseName(snapshot.phase),
                      slotIndex(snapshot.presentationSlot) + 1,
                      static_cast<unsigned int>(snapshot.doStatus));
        drawLine(drawList, y, textColor, line);
        for (int item = 0; item < 2; item++) {
            const ItemResolverDebug& resolved = snapshot.resolved[item];
            const ItemPaneDebug& pane = snapshot.panes[item];
            std::snprintf(
                line, sizeof(line),
                " %c sel=%u mix=%u item=%u num=%d/%d pane=%d tex=%d a=%u/%.2f d3=%d",
                item == 0 ? 'X' : 'Y', static_cast<unsigned int>(resolved.selectIndex),
                static_cast<unsigned int>(resolved.mixIndex), static_cast<unsigned int>(resolved.item),
                static_cast<int>(resolved.count), resolved.maxCount, pane.visible ? 1 : 0,
                pane.textureVisible ? 1 : 0, static_cast<unsigned int>(pane.alpha), pane.alphaRate,
                pane.thirdDigitVisible ? 1 : 0);
            drawLine(drawList, y, textColor, line);
        }
    }
}

}  // namespace dusk::coop::hud_diagnostics
