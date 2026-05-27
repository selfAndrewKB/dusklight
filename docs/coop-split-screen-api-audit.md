# Co-op Split-Screen API Audit

This audit reopens the native split-screen work with the same discipline used for enemy co-op:
classify the ownership question first, then route the code through the narrow Dusk-owned API that
owns that question.

The current split-screen prototype works well enough to prove the direction, but several fixes were
made as containment hooks while we were still discovering the engine's camera and render surfaces.
This document records which pieces should remain, which pieces should be wrapped into cleaner API
families, and which systems are intentionally deferred.

## API Families

| Question | API family | Status |
| --- | --- | --- |
| Which player owns this camera, render window, or camera decision? | `camera_owner` / existing `dusk::coop::camera` | Partially implemented as the camera/window/player sidecar |
| Which viewport is being rendered right now? | `viewport_owner` / render-window context | Partially implemented in the painter loop |
| Which player-status bits should a camera or camera tag read? | `player_camera_status` | Not implemented; camera 1 currently borrows/clamps unsafe vanilla status reads |
| Which render state must be installed per viewport? | `viewport_render_state` | Not implemented; lighting/global J3D state is currently contained, not truly per-viewport |
| Which fullscreen effect owns this viewport/framebuffer? | `viewport_effect_owner` | Partially implemented through `dusk::coop::render_effects` policy helpers and the central per-window painter replay |
| Which viewport owns real-shadow submission culling and baked shadow matrices? | `render_shadows` | Partially implemented through `dusk::coop::render_shadows` for shared-list culling bypass and per-viewport real-shadow refresh |
| Which viewport should camera-facing 3D line/ribbon geometry use? | shared 3D-line material refresh | Implemented for `mDoExt_3DlineMat0_c` and `mDoExt_3DlineMat1_c` during the per-window painter pass |
| Which player owns HUD, reticles, prompts, and message UI? | `hud_owner` / `ui_owner` | Not implemented; HUD is constrained to P1's viewport |
| Which player activated an NPC/object/event trigger? | `interaction_owner` / `event_trigger_owner` | Not implemented |
| Which camera or player should audio listener state follow? | `audio_listener_owner` | Not implemented; audio listener stays camera 0/P1-owned |
| Should this actor, world chunk, foliage/detail, or background part be draw-culled for local split-screen? | `render_visibility` | Initial PC split-screen bypass implemented for known P1-camera draw-culling paths |

## Current Split-Screen Patches To Revisit

### Camera/window/player sidecar

Files:

- `include/dusk/coop/camera.h`
- `src/dusk/coop/camera.cpp`
- `include/d/d_com_inf_game.h`

Current behavior:

- Index 0 remains vanilla storage.
- Index 1 routes to Dusk-owned camera/window/player sidecar state.
- Camera 1 uses native `CAMERA2` process identity.

Audit decision:

- Keep this architecture. It is the correct equivalent of the enemy sidecar registry pattern.
- Rename or layer future helpers around ownership language only when it clarifies callsites. The
  current `dusk::coop::camera` module can remain the storage owner.
- Extend this family rather than resizing `dComIfG_play_c` arrays as the first move.

### Split-aware window layout and `ResetView()`

Current behavior:

- Split layout is reapplied after camera reset paths that would otherwise make both windows
  fullscreen.
- Window aspect is derived from the active render window.

Audit decision:

- Keep as `viewport_owner` behavior.
- Any future patch touching camera reset, viewport, scissor, aspect, or trim should route through
  the camera/window sidecar rather than hardcoding window 0 or fullscreen dimensions.

### Painter window loop

File:

- `src/m_Do/m_Do_graphic.cpp`

Current behavior:

- Normal 3D draw loops active render windows.
- HUD/2D remains P1-constrained.
- The late world/effect draw-list tail now runs per active render window after that window's
  view/viewport/render state is installed. Fullscreen framebuffer captures/filters inside that
  tail remain gated during split-screen until they have explicit viewport framebuffer ownership.

Audit decision:

- Keep the central painter loop. Do not add per-actor or per-enemy draw hooks for split screen.
- Reframe the loop as the first `viewport_owner` implementation.
- Late world/effect surfaces such as invisible lists, projection particles, Z-xlu, filter lists,
  screen particles, and 3D-last packets should stay in this centralized per-window replay path.
  Fullscreen effects such as motion blur, depth-of-field capture, indirect screen draw, bloom,
  trimming, fade, and 2D-screen overlays need explicit viewport/framebuffer ownership before being
  enabled in split-screen. If an individual fullscreen framebuffer effect misbehaves, fix or
  classify that effect's ownership in `dusk::coop::render_effects` instead of disabling the whole
  tail again.
- Heat-haze projection particles bind the particle resource `dummy` texture, which is backed by the
  framebuffer. Split-screen refreshes that framebuffer texture from the active viewport immediately
  before projection particles draw; this is separate from the later indirect-screen draw list, which
  remains disabled in split-screen because it contains mixed fullscreen weather/effect packets.

### Real-shadow ownership

Files:

- `include/dusk/coop/render_shadows.h`
- `src/dusk/coop/render_shadows.cpp`
- `include/d/d_drawlist.h`
- `src/d/d_drawlist.cpp`
- `src/m_Do/m_Do_graphic.cpp`

Current behavior:

- Real shadows are still submitted once into the shared draw list, but camera-0 depth/frustum
  rejection is bypassed during native PC split-screen so P2-visible real shadows are not discarded
  before P2's viewport renders.
- Each split viewport primes the active view/light/material state before the real-shadow texture
  pass, then `dDlst_shadowReal_c` refreshes its baked projection matrices from the original shadow
  setup inputs.

Audit decision:

- Keep this as `render_shadows`, not a generic visibility patch or material patch.
- Do not make actor files manually resubmit shadows for P2. Shared-list culling and baked shadow
  matrix ownership should stay centralized in `dusk::coop::render_shadows` and the draw-list shadow
  classes.
- If future shadow classes misbehave, add explicit policy to `render_shadows` instead of copying
  camera-0 bypass checks to each caller.

### Camera 1 audio listener containment

Current behavior:

- Camera 1 avoids `Z2Audience` / `Z2SpotMic` camera id 1 writes because those arrays are single
  slot.
- Camera 1 avoids clearing camera 0's global audio polygon state.

Audit decision:

- Keep the containment until `audio_listener_owner` exists.
- Do not make camera 1 write singleton audio state by accident.
- Future audio work should decide whether local split-screen has one listener, a blend, P1-owned
  listener, or per-player/online replicated listener state.

### Camera status containment

Current behavior:

- Camera 1 keeps controller id 1 where needed, but unsafe vanilla player-status lookups are clamped
  away from row 1 because `mPlayerStatus[1][4]` only has row 0.

Audit decision:

- This is a known containment hook, not the final model.
- The proper replacement is `player_camera_status`: per-slot status facts used by camera modes,
  camera tags, item cameras, lock-on cameras, and special movement cameras.
- Do not broaden the current clamp into more camera behavior. Add the status sidecar before trying
  to make P2 item/lock-on/special camera modes fully native.

### Camera tag evaluation

File:

- `src/d/actor/d_a_tag_camera.cpp`

Current behavior:

- Vanilla camera tags still apply to camera 0 through the native P1/global path.
- Split-screen also evaluates the same tag volume against P2 and applies it to camera 1.

Audit decision:

- Keep this as a successful proof that camera decisions can be player-owned.
- Rework future camera-tag/status reads around `camera_owner` plus `player_camera_status` so tag
  conditions ask about the tag's candidate player, not always P1.
- Do not treat all camera events/cutscenes as solved by this path.

### Global J3D view and lighting containment

Current behavior:

- After camera 1 draw, camera 0's global J3D view is restored so later global lighting/debug code
  does not accidentally inherit P2's camera.
- This moved the visible lighting influence back from P2 to P1, proving the symptom is caused by
  global render state ownership.

Audit decision:

- Keep this containment only until `viewport_render_state` exists.
- The durable fix is to install environment light/fog/global render state per rendered viewport,
  draw that viewport, then restore a known global baseline.
- This should be the first serious split-screen rendering correctness pass after draw culling.

### HUD/2D pass

Current behavior:

- HUD is constrained to P1's viewport.

Audit decision:

- Keep for now. It is intentionally P1-owned until the HUD item/health/weapons ownership work is
  ready.
- Future HUD work should duplicate or partition HUD surfaces by player slot. It should not be
  hidden inside the camera module.

## Render Visibility And Culling Decision

Twilight Princess' draw culling is aggressive because the original game had one camera and a fixed
console performance budget. Local split-screen changes that assumption: an actor, world chunk,
foliage/detail model, or background part outside P1's view may be visible to P2. Doing exact
"visible to any active viewport" culling is more complex than the immediate problem deserves.

The implemented V1 policy is:

```text
If native split screen is active on PC, bypass draw-time frustum culling in the known P1-camera
visibility paths.
```

Why this is the right first move:

- The main draw gate is centralized in `fopAcM_Draw()` through `fopAcM_cullingCheck()`.
- World/background drawing also performs view-dependent clipping. `daBg_c::draw()` hides individual
  shapes through `mDoLib_clipper::clip(j3dSys.getViewMtx(), ...)`, and `dBgp_c::draw()` clips
  background-part models after asking camera 0 for `HideBGPartsOk()`.
- Bypassing these checks is simpler and less fragile than running each clipper once per
  camera/window.
- It avoids P1-camera disappearance and popping bugs for P2 without teaching every actor, shape, or
  background unit about multiple views.
- The user has explicitly accepted PC-side performance headroom for co-op correctness.

Boundaries:

- This should only bypass draw-time visibility culling while native split screen is active.
- It should not change actor execution, room loading, event logic, or actor lifecycle.
- It should not remove explicit `NODRAW`, event approval, pause, or actor stop behavior.
- It should not disable authored state changes that are not camera visibility decisions.
- It should be documented as `render_visibility`, not as camera targeting or player ownership.

Future option:

- If drawing everything becomes visually or practically noisy, replace the bypass with
  "visible to any active co-op viewport" culling inside one shared `render_visibility` helper. That
  helper would run the same actor/world/background tests against each active viewport and cull only
  when all viewports reject the object. Do not implement that complexity before the simple PC
  split-screen policy proves insufficient.

Implemented first surfaces:

- `src/f_op/f_op_actor.cpp`: actor draw gate through `fopAcM_cullingCheck()`.
- `src/d/actor/d_a_bg.cpp`: room/background model shapes hidden through the active clipper view.
- `src/d/d_bg_parts.cpp`: background-part model entry clipped against camera 0/global view, with
  camera 0-driven `HideBGPartsOk()` section hiding.

The public hook is `dusk::coop::render_visibility::shouldBypassDrawCulling()`. Original/decomp
render paths should call that policy helper instead of reaching into the camera module directly.

Expected visible symptom before this pass:

- P2 can see plants, background pieces, or world details pop/shift/disappear when P1 moves or turns
  the camera, even though P2's own camera and viewport have not changed.

Validation result:

- First pass fixed enemy drawing and much of the world/background popping in split-screen testing.
- Remaining visible P1-camera-owned symptoms include large grass/detail patch placement, world
  lighting, and occasional door or similar render-object behavior. Treat those as separate
  `viewport_render_state`, foliage/detail visibility, and actor-specific render ownership surfaces
  rather than broadening the draw-culling bypass blindly.

Follow-up investigation:

- Grass and flower packets precompute detail matrices in their update step with
  `j3dSys.getViewMtx()`, then reuse those matrices during the per-viewport draw loop. In native
  split-screen this can bake P1's view into large detail patches before P2 renders. The current
  follow-up stores world-space detail matrices while split-screen visibility bypass is active and
  applies the active viewport view in `dGrass_packet_c::draw()` / `dFlower_packet_c::draw()`.
- `daDoor20_c::draw()` has an actor-local `fopAcM_cullingCheck(this)` in addition to the central
  actor draw gate. This is now routed through the same `render_visibility` policy so shutter-style
  doors do not remain P1-camera-culled after the central actor gate has been bypassed.
- World lighting remains a separate `viewport_render_state` problem. `dKy_setLight_nowroom_common()`
  and related environment paths still read `dComIfGp_getCamera(0)` for camera-eye-dependent light
  selection even though the split-screen painter has an active current viewport camera. The durable
  next step is a current-view/current-camera render-state API for environment code, not more
  visibility bypasses.

## First Reopened Split-Screen Pass

1. Switch development back to the co-op split-screen branch/context.
2. Done: add split-screen visibility bypasses behind the existing native split-screen enabled check for
   actor draw culling and the known world/background clipper paths.
3. Add diagnostics or overlay evidence only if objects still disappear or shift in P2's view.
4. Start `player_camera_status` for camera 1 item/lock-on/special mode correctness.
5. Start `viewport_render_state` with environment lighting/fog ownership.
6. Revisit HUD/reticles after camera/render correctness is stable.

## Acceptance Targets

- P1 single-player rendering is unchanged when split screen is disabled.
- P1 and P2 views no longer lose actors, plants, background parts, or world details merely because
  those objects are outside P1's camera.
- Camera 1 remains a native `dCamera_c` process with its own render window.
- Known containment hooks are either wrapped in the right API family or explicitly documented here.
- No new split-screen work hardcodes a player or camera without classifying the ownership question.
