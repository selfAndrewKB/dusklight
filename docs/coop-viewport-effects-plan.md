# Per-Viewport Effect Presentation

## Status

Implemented on `co-op-viewport-effects`. The user compiled and field-validated the camera-relative
Goron Mines cloud/haze and framebuffer ordering fix on 2026-06-21. Codex validation remains source
inspection and `git diff --check`; Visual Studio/CMake builds remain user-owned.

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
packed target/composite rectangles, effect-family policy, Base/Sense snapshot validity, per-slot
Sense activation and owned emitters, shared reveal/inactive emitter counts, Sense-only model count,
per-slot Twilight camera/player/light mask, and current registered view-dependent and
projected-material models. Schema 5 also records projection-particle resources per viewport and the
kankyo cloud/haze simulation source, active draw camera, projection camera, per-slot draw count,
visible-cloud count, aggregate alpha, and the framebuffer refresh policy seen by each consumer.
Continuous fade strength, positions, and buffer dimensions live in `latest.json`; JSONL change keys
use ownership, activation, registration, and policy state.

## Sense Reveal Follow-Up

- `player_sense` owns the gameplay question "can this slot reveal this actor now?" It reads the
  live ALINK Sense state and that slot's native fade threshold; it does not replace shared actor
  simulation, story switches, or authored event ownership.
- Shared reveal actors register during their native execute lifecycle. Registration is frame-scoped
  and process-ID validated so down/dead phases and pooled actor addresses cannot inherit stale
  reveal requirements.
- Shared actor simulation may expose a spirit when any active player uses Sense, but model packets,
  real shadows, Sense-reveal particles, and inverse spirit wisps are filtered during each viewport's
  native draw-list replay. Actor submission carries the live actor into J3D registration so custom
  NPC draw paths retain native camera culling. Hidden actors also remain unavailable to that slot's
  attention scanner.
- Particle simulation remains singular. Native Sense particles stay draw-enabled when any slot
  needs them; viewport replay temporarily applies that slot's reveal/inverse-wisp alpha and restores
  the shared emitter immediately afterward.
- Generic Twilight NPC families, custom Castle Town spirits, Frozen Zora, Ghost Soldiers, Poes,
  Ghost Rats, and Shadow Insects use this boundary. Generic dig places preserve vanilla's two-step
  producer order: the shared actor is exposed to the nearest active wolf, then each ALINK's own
  Sense state controls the Dig action. Dig completion separately retains the acting wolf through
  `retained_interaction_owner::Dig` so item/switch writes cannot fall back to P1 after animation.

## Validated Goron Mines Effect Ownership

Dusk bloom deliberately renders its downsample pyramid through a source-sized viewport into a
smaller packed offscreen target, whose scissor now matches that target. Water, refraction models,
and projection particles permanently bind the native framebuffer `ResTIMG`, so every viewport
refresh must continue writing the canonical `getFrameBufferTex()` address. The previous texture
object used by direct bloom sampling is reset before the copy and rebuilt afterward, while Aurora
updates the canonical destination's resolved texture handle in place. Do not replace or evict that
copy identity between viewport replays: already-created `fbtex_dummy` / `dummy` resource objects
depend on it. Rebuilding JParticle's common and room `dummy` sampler bindings did not affect the
Goron Mines haze and was removed. `render.effects` retains the useful evidence instead: native
projection emitters per viewport, submitted camera matrices, kankyo cloud/haze simulation source,
active draw camera, projection camera, and framebuffer refresh policy.

The resulting capture identified two distinct native systems. The kankyo cloud packet was in mode
6 and still simulated from P1, but its indirect-screen packet was not drawn during ordinary split-
screen. The visible distortion sheet was room particle `0x80E2`, produced by `daYkgr_c`: native
code positions its shared emitter from Camera 0, calculates strength from P1's distance to the
authored path, and submits it to screen-particle group 13. `render_effects` now classifies this as a
camera-relative emitter. Its particles remain singular, but each viewport draw transforms the exact
submitted Camera-0 particle matrix into the active viewport, preserving frame interpolation and
native projection. Each slot owns the independently smoothed path strength consumed at draw, and P1
state is restored immediately afterward.

The screen-particle group also depends on the native second framebuffer capture after ordinary
particles. Split-screen refreshes that capture for each viewport immediately before group 13 without
enabling motion blur, depth of field, or the mixed fullscreen indirect list. Without this exact
capture phase, the distortion sheet samples a pre-particle image while lava jets, ash, fire, and
Torch Slugs are already live, producing the observed flicker wherever the sheet overlaps them.

Mode-6 cloud/smoke remains distinct from the `daYkgr_c` distortion sheet. Because modes below 50
are world-space cloud/mist rather than framebuffer distortion, `render_effects` replays only that
classified packet when the mixed indirect list is gated. The native cloud update retains a fixed
per-slot `CLOUD_EFF` sidecar driven by that slot's live camera, player room ratio, and a private RNG;
P1 keeps the canonical packet and vanilla RNG. Additional slots use fixed `CLOUD_EFF[50]` sidecars,
their own player/camera and room ratio, and private visual RNG so they never advance the vanilla
global random stream. Draw replay selects the presenting slot's simulation, uses that camera's
environment-color query, and advances shared texture rotation only once per frame.

This is the important architectural distinction: most shared actors and particles simulate once and
only need viewport-local presentation filtering, but a visual effect that reads a camera or player
while updating retained history is itself missing a per-slot visual simulation lifecycle. Replaying
the canonical P1 packet under Camera 1 cannot reconstruct positions, alpha easing, room ratio, or RNG
decisions that were already committed during update.

## Durable Rendering Rules

- Trace effects through the complete native chain: simulation producer, retained history, draw-list
  submission, framebuffer capture before the consumer, viewport draw, and canonical restore.
- Classify camera-independent shared simulation separately from camera-retained visual simulation.
  Keep gameplay and shared emitters singular; give additional slots fixed sidecar history only when
  the native updater consumes camera/player state and stores the result for later drawing.
- Keep P1 canonical. Additional visual simulations use fixed session storage, clear with the native
  packet lifecycle, and never replace the native packet or its resource identity.
- Additional visual simulation must use private RNG. It must never perturb the vanilla global random
  stream, because that can change unrelated actor and effect behavior.
- Replayed camera-relative particles use the exact matrix captured at native submission, including
  frame interpolation. A raw camera-body matrix is not an equivalent later reconstruction.
- Framebuffer captures are data dependencies of particular consumers, not one blanket fullscreen-
  effect policy. Preserve the native capture phase needed by water, refraction, distortion, or other
  samplers while independently gating motion blur, depth of field, fades, and mixed indirect passes.
- Keep the canonical framebuffer `ResTIMG` address stable and refresh it sequentially per viewport.
  Existing `fbtex_dummy` / `dummy` resources may already retain that exact address.
- Diagnostics should expose ownership and phase boundaries, not only final pixels: source camera,
  submitted matrix, slot history, draw camera, capture policy, consumer group, and restore state.

## Field Validation

- Simultaneous Gale Boomerang wind meshes from opposite views.
- `Obj_MHole` projection from opposite camera angles.
- P1-only and P2-only Sense activation, fade, particles, odour view, and deactivation.
- P1-only and P2-only reveal visibility, attention, spirit wisps, Poes, insects, and dig places.
- Twilight camera lighting without Camera 0 leakage.
- Dusk and Classic bloom at windowed and ultrawide resolutions.
- Field-validated Goron Mines mode-6 cloud/smoke, group-13 heat haze, water/refraction, lava jets,
  ash, fire, and Torch Slugs without cross-camera haze movement or particle flicker.
- P2 fullscreen presentation with Camera 1 effects.
- Single-player plus reticle, water, heat-haze, shadow, sky/cloud, foliage, Wolf HUD, and frame-
  interpolation regressions.
