#pragma once

namespace dusk::coop {

// Co-op: historical runtime isolation switches for the slot-1 ALINK audit harness.
enum SecondaryAlinkProbeFlag : unsigned int {
    SecondaryAlinkProbe_SkipExecute = 1u << 0,
    SecondaryAlinkProbe_SkipDraw = 1u << 1,
    SecondaryAlinkProbe_SkipWaitAnimeBind = 1u << 2,
    SecondaryAlinkProbe_SkipStartProcInit = 1u << 3,
    SecondaryAlinkProbe_SkipSetMatrix = 1u << 4,
    SecondaryAlinkProbe_SkipCreateAnimePlay = 1u << 5,
    SecondaryAlinkProbe_SkipCreateModelCalc = 1u << 6,
    SecondaryAlinkProbe_SkipFaceTextureAnime = 1u << 7,
    SecondaryAlinkProbe_SkipItemMatrix = 1u << 8,
    SecondaryAlinkProbe_SkipSetItemActor = 1u << 9,
};

constexpr unsigned int kDefaultSecondaryAlinkProbeFlags = 0;

unsigned int getSecondaryAlinkProbeFlags();
void setSecondaryAlinkProbeFlags(unsigned int flags);
bool hasSecondaryAlinkProbeFlag(SecondaryAlinkProbeFlag flag);

}  // namespace dusk::coop
