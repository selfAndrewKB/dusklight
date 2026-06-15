#include "dusk/coop/world_switch_probe.h"

#include "f_op/f_op_actor_mng.h"

namespace dusk::coop::world_switch_probe {
namespace {

constexpr int kRecordCount = 64;

WorldSwitchDebugState s_debugState;
u32 s_currentSimFrame = 0;
u64 s_nextEventId = 1;
int s_nextRecordEvict = 0;

struct SuppressedSwitch {
    bool active = false;
    int switchNo = -1;
    int roomNo = -1;
};

SuppressedSwitch s_suppressedSwitch;

bool consumeSuppressedSwitch(int switchNo, int roomNo) {
    if (!s_suppressedSwitch.active || s_suppressedSwitch.switchNo != switchNo ||
        s_suppressedSwitch.roomNo != roomNo)
    {
        return false;
    }

    s_suppressedSwitch = {};
    return true;
}

WorldSwitchRecord* nextRecord() {
    if (s_debugState.recordCount < kRecordCount) {
        return &s_debugState.records[s_debugState.recordCount++];
    }

    return &s_debugState.records[s_nextRecordEvict++ % kRecordCount];
}

}  // namespace

void advanceWorldSwitchProbeFrame(u32 frame) {
    s_currentSimFrame = frame;
    s_debugState.currentSimFrame = frame;
}

void recordSwitchOn(const fopAc_ac_c* sourceActor, int switchNo, int roomNo, bool wasOnBefore,
                    const char* source) {
    if (switchNo == -1 || switchNo == 255) {
        return;
    }

    if (sourceActor == nullptr && consumeSuppressedSwitch(switchNo, roomNo)) {
        return;
    }

    WorldSwitchRecord* record = nextRecord();
    if (record == nullptr) {
        return;
    }

    record->eventId = !wasOnBefore ? s_nextEventId++ : 0;
    record->simFrame = s_currentSimFrame;
    record->sourceActor = reinterpret_cast<uintptr_t>(sourceActor);
    record->sourceActorId = sourceActor != nullptr ? fopAcM_GetID(sourceActor) : -1;
    record->sourceProfile = sourceActor != nullptr ? fopAcM_GetProfName(sourceActor) : -1;
    record->sourceRoom = sourceActor != nullptr ? fopAcM_GetRoomNo(sourceActor) : -1;
    record->switchNo = switchNo;
    record->roomNo = roomNo;
    record->wasOnBefore = wasOnBefore;
    record->actorSource = sourceActor != nullptr;
    record->source = source;
}

void suppressNextDirectSwitchOn(int switchNo, int roomNo) {
    if (switchNo == -1 || switchNo == 255) {
        return;
    }

    s_suppressedSwitch.active = true;
    s_suppressedSwitch.switchNo = switchNo;
    s_suppressedSwitch.roomNo = roomNo;
}

const WorldSwitchDebugState& getWorldSwitchDebugState() {
    return s_debugState;
}

}  // namespace dusk::coop::world_switch_probe
