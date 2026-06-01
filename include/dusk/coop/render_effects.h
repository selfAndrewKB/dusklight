#pragma once

namespace dusk::coop::render_effects {

bool shouldReplayLateWorldEffectTail();
bool shouldRunFullscreenFramebufferEffects();
bool shouldDrawViewportTrim();
bool shouldRefreshInvisibleListFramebuffer();
bool shouldRefreshProjectionParticleFramebuffer();
bool shouldBypassSharedParticleCreationCulling();

}  // namespace dusk::coop::render_effects
