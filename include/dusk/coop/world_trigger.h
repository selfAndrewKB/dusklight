#pragma once

#include "dolphin/types.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/player_slots.h"

#include <cstdint>

class fopAc_ac_c;

namespace dusk::coop::world_trigger {

enum class TriggerFamily : u8 {
    SwitchArea = 0,
    TagEvent,
    TagEvt,
    ActorLocal,
};

enum class ActivationPolicy : u8 {
    PrimaryOnly = 0,
    AnyActivePlayer,
};

enum class SubjectPolicy : u8 {
    Primary = 0,
    TriggeringPlayer,
};

enum class PresentationPolicy : u8 {
    Split = 0,
    PrimaryFullscreen,
    TriggeringPlayerFullscreen,
};

enum class Transition : u8 {
    Accept = 0,
    Release,
};

enum class ReleaseReason : u8 {
    Accepted = 0,
    EventEnded,
    CameraRestored,
    SourceDeleted,
    PlayerDeleted,
    Superseded,
    Cleared,
};

struct TriggerPolicy {
    TriggerFamily family = TriggerFamily::ActorLocal;
    ActivationPolicy activation = ActivationPolicy::PrimaryOnly;
    SubjectPolicy subject = SubjectPolicy::Primary;
    PresentationPolicy presentation = PresentationPolicy::Split;
};

struct TriggerMetadata {
    int eventId = -1;
    int switchNo = -1;
};

struct TriggerMatch {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    u8 candidateMask = 0;
    u8 eligibleMask = 0;
    u32 failureFlags[kPlayerSlotCount] = {};
    bool found = false;
};

struct TriggerState {
    char label[64] = {};
    fopAc_ac_c* source = nullptr;
    int sourceId = -1;
    int sourceProfile = -1;
    int sourceRoom = -1;
    fopAc_ac_c* triggeringPlayer = nullptr;
    int triggeringPlayerId = -1;
    PlayerSlot triggeringSlot = PlayerSlot::Invalid;
    TriggerPolicy policy;
    TriggerMetadata metadata;
    u8 candidateMask = 0;
    u8 eligibleMask = 0;
    bool active = false;
    bool presenting = false;
};

struct TriggerDecisionDebug {
    char label[64] = {};
    uintptr_t source = 0;
    int sourceId = -1;
    int sourceProfile = -1;
    int sourceRoom = -1;
    TriggerPolicy policy;
    PlayerSlot selectedSlot = PlayerSlot::Invalid;
    u8 candidateMask = 0;
    u8 eligibleMask = 0;
    u32 failureFlags[kPlayerSlotCount] = {};
    bool found = false;
};

struct TriggerTransitionDebug {
    u64 eventId = 0;
    Transition transition = Transition::Accept;
    ReleaseReason reason = ReleaseReason::Accepted;
    TriggerState state;
};

struct WorldTriggerDebugState {
    TriggerState states[32] = {};
    int stateCount = 0;
    TriggerDecisionDebug decisions[32] = {};
    int decisionCount = 0;
    TriggerTransitionDebug transitions[64] = {};
    int transitionCount = 0;
    u32 revision = 0;
};

TriggerMatch evaluate(fopAc_ac_c* source, const char* label, const TriggerPolicy& policy,
                      PlayerQueryPredicate predicate, void* userData);
void accept(fopAc_ac_c* source, const char* label, const TriggerPolicy& policy,
            const TriggerMatch& match, const TriggerMetadata& metadata = {});
void release(fopAc_ac_c* source, const char* label, ReleaseReason reason);
void clearSource(fopAc_ac_c* source, const char* label);
void clearPlayer(const fopAc_ac_c* player);

TriggerState stateForSource(fopAc_ac_c* source);
PlayerSlot subjectSlotForSource(fopAc_ac_c* source);
fopAc_ac_c* subjectPlayerForSource(fopAc_ac_c* source);

bool beginPresentation(fopAc_ac_c* source, const char* label);
void endPresentation(fopAc_ac_c* source, const char* label);

const WorldTriggerDebugState& getWorldTriggerDebugState();
const char* triggerFamilyName(TriggerFamily family);
const char* activationPolicyName(ActivationPolicy policy);
const char* subjectPolicyName(SubjectPolicy policy);
const char* presentationPolicyName(PresentationPolicy policy);
const char* transitionName(Transition transition);
const char* releaseReasonName(ReleaseReason reason);

}  // namespace dusk::coop::world_trigger
