#pragma once

struct view_class;
struct view_port_class;

namespace dusk::coop::debug_overlay {

bool isEnemyTargetOverlayEnabled();
void setEnemyTargetOverlayEnabled(bool enabled);
void toggleEnemyTargetOverlay();
bool isDamageHitOverlayEnabled();
void setDamageHitOverlayEnabled(bool enabled);
void toggleDamageHitOverlay();
void drawEnemyTargetOverlay();
void captureEnemyTargetOverlayLabels(const view_class* view, const view_port_class* viewport);
void drawEnemyTargetTextOverlay();

}  // namespace dusk::coop::debug_overlay
