#pragma once

#include "SSystem/SComponent/c_xyz.h"
#include "dolphin/types.h"
#include "dusk/coop/player_slots.h"

class view_class;
class view_port_class;

namespace dusk::coop::ui_owner {

struct ViewportState {
    f32 viewport[6];
    u32 scissor[4];
};

PlayerSlot currentSlot();
int currentPad();

void pushPresentationSlot(PlayerSlot slot);
void popPresentationSlot();

void retainSingularSlot(PlayerSlot slot);
void clearSingularSlot();
PlayerSlot singularSlot();

bool beginViewport(PlayerSlot slot, ViewportState* state);
void endViewport(const ViewportState& state);
bool projectWorldPointLocal(PlayerSlot slot, const cXyz& point, Vec* out);

}  // namespace dusk::coop::ui_owner
