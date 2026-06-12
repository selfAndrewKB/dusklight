#include "dusk/coop/alink_probes.h"

namespace dusk::coop {
namespace {

// Co-op: isolation probes default off; proven model-data ownership lives in its runtime sidecar.
unsigned int s_secondaryAlinkProbeFlags = kDefaultSecondaryAlinkProbeFlags;

}  // namespace

unsigned int getSecondaryAlinkProbeFlags() {
    return s_secondaryAlinkProbeFlags;
}

void setSecondaryAlinkProbeFlags(unsigned int flags) {
    if ((flags & SecondaryAlinkProbe_SkipCreateAnimePlay) != 0) {
        // Co-op: running secondary model calc without the matching animation playback hit a zero-quaternion assert.
        flags |= SecondaryAlinkProbe_SkipCreateModelCalc;
    }
    s_secondaryAlinkProbeFlags = flags;
}

bool hasSecondaryAlinkProbeFlag(SecondaryAlinkProbeFlag flag) {
    return (s_secondaryAlinkProbeFlags & static_cast<unsigned int>(flag)) != 0;
}

}  // namespace dusk::coop
