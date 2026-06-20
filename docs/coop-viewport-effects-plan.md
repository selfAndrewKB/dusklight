# Per-Viewport Effect Presentation

## Status

Implemented on `co-op-viewport-effects` in two checkpoints. Source inspection and
`git diff --check` are the Codex-side validation; Visual Studio/CMake and field testing remain with
the user.

## Checkpoint 1: World Effects

- `render_materials` registers current-frame models whose native `viewCalc()` must run for every
  rendered camera. The Gale Boomerang wind model registers at native display-list submission.
- `render_materials` also registers camera-light projection materials by explicit bitmask.
  `Obj_MHole` keeps its native draw-time calculation and registers materials 0 and 1 for viewport
  replay.
- Replay changes presentation state only. It does not advance actor simulation, animation, or frame
  interpolation, and the painter restores Camera 0's canonical model/material baseline afterward.

## Checkpoint 2: Sense, Twilight, And Bloom

- `render_effects` owns a fixed-storage viewport context: slot, window, camera, view, and viewport.
  Every painted window, including P2 fullscreen presentation, enters and leaves that context.
- Each active ALINK advances the native Wolf Sense fade and persistent-emitter lifecycle once per
  simulation tick. Tagged emitters share particle simulation but draw only in their owner's
  viewport; pooled emitter deletion clears ownership before address reuse.
- Native draw-time Sense consumers use the active viewport owner: particle ambient lighting,
  grass/flower packet colors, odour geometry, and Twilight evil-effect colors. Twilight effect
  distance/facing reads use the active view, while shared effect rotation advances once per
  simulation frame.
- Environment simulation computes the ordinary palette once. The environment actor captures that
  Base snapshot, derives the native Sense color/fog/dungeon-light/sky/bloom mutation without
  advancing timers again, captures it, and restores P1's selected variant. Painter replay installs
  the current viewport owner's snapshot.
- Twilight camera lights run their native selection, height, size, attenuation, and fade calculation
  once for each active slot's player/camera. Painter replay installs that slot's light snapshot and
  rebuilds camera-facing orientation from the active view before material/GX-light refresh.
- Dusk and Classic bloom capture the active logical viewport into a native-pixel-sized texture,
  process a private viewport-sized Aurora framebuffer, and composite back through the original
  viewport/scissor. Motion blur, depth of field, indirect-screen passes, fullscreen 2D overlays,
  and fades remain separately gated during ordinary split-screen.

## Diagnostics

`render.effects` records the last viewport owner, logical viewport/scissor, native bloom source and
composite rectangles, effect-family policy, Base/Sense snapshot validity, per-slot Sense activation
and emitters, per-slot Twilight camera/player/light mask, and current registered view-dependent and
projected-material models. Continuous fade strength and buffer dimensions live in `latest.json`;
JSONL change keys use ownership, activation, registration, and policy state.

## Field Validation

- Simultaneous Gale Boomerang wind meshes from opposite views.
- `Obj_MHole` projection from opposite camera angles.
- P1-only and P2-only Sense activation, fade, particles, odour view, and deactivation.
- Twilight camera lighting without Camera 0 leakage.
- Dusk and Classic bloom at windowed and ultrawide resolutions.
- P2 fullscreen presentation with Camera 1 effects.
- Single-player plus reticle, water, heat-haze, shadow, sky/cloud, foliage, Wolf HUD, and frame-
  interpolation regressions.
