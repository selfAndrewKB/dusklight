# Co-op Native Split-Screen Camera Plan

This is the active co-op milestone after the first secondary ALINK item ownership pass.

The reopened ownership audit for this milestone lives in
`docs/coop-split-screen-api-audit.md`. Use that audit when deciding whether an old split-screen
containment hook should remain as-is, move behind a Dusk-owned API family, or be replaced.

## Purpose

Give local co-op a real second camera and a real second render viewport so P2 can be tested and played without being tethered to P1's view.

The goal is not to build a cheap camera imitation. The goal is to use Dusk and Twilight Princess' existing camera/process/render-window concepts as far as they will go, then add a small Dusk-owned extension layer where the original game only stored one slot.

## Current Evidence

- P2 controller input already works for the secondary ALINK prototype.
- P2 can move, roll, swing, use several item families, ride Spinner rails, place bombs, and use bow/slingshot/fishing/Dominion Rod paths after owner-routing fixes.
- The current shared-camera workflow is now a testing bottleneck: every P2 test is constrained by P1's view and forces the user to juggle two controllers.
- `dComIfG_play_c` exposes indexed camera/window/player APIs: `getWindow(i)`, `setWindow(i, ...)`, `getCamera(i)`, `setCameraInfo(i, ...)`, `getPlayer(i)`, and `setPlayerInfo(i, ...)`.
- The backing storage is still single-slot: `mWindow[1]`, `mCameraInfo[1]`, `mPlayerInfo[1]`, and `mPlayerStatus[1][4]` in `include/d/d_com_inf_game.h`.
- `dCamera_c` already derives camera identity through `fopCamM_GetParam(i_camera)` and then resolves player/window through `dComIfGp_getCameraPlayer1ID(camera_id)` and `dComIfGp_getCameraWinID(camera_id)`.
- Scene play init seeds only slot 0: `dComIfGp_setPlayerInfo(0, NULL, 0)`, `dComIfGp_setWindow(0, ...)`, and `dComIfGp_setCameraInfo(0, NULL, 0, 0, -1)` in `src/d/d_s_play.cpp`.
- The main painter path in `src/m_Do/m_Do_graphic.cpp` checks `dComIfGp_getWindowNum()` but then uses `dComIfGp_getWindow(0)`, resolves that window's camera, and draws a single 3D view.
- `fopCamM_Create(i_cameraIdx, ...)` stores camera process IDs in a four-entry camera manager table, so camera index 1 is at least represented at the process-manager layer.
- Many non-camera systems still directly ask for camera 0. These must be classified by behavior. Some are global by design; others should eventually resolve the owning player's camera.

## Terminology

- **Render window** or **viewport** means the internal game/Dusk render target region represented by `dDlst_window_c` and view/scissor state.
- It does not mean a Windows OS window.
- **Extension layer** means Dusk-owned camera/window/player slot storage behind the existing accessors. It is not inherently temporary; it preserves original struct layout while allowing co-op slots.

## Working Assumptions

- P1 and vanilla single-player remain the compatibility baseline.
- Slot 0 should continue to route through the original game storage.
- Slot 1 should route through Dusk-owned extension storage at first, rather than immediately resizing original arrays in `dComIfG_play_c`.
- The extension layer is viable long-term if all co-op-capable code goes through narrow accessors instead of indexing original arrays directly.
- The first split-screen target is normal field gameplay. Boss cameras, event cameras, message cameras, cutscene cameras, HUD layout, fades, save/restart camera state, and special overlays are diagnostic targets first, not V1 fixes.
- HUD and 2D overlays can remain P1-owned for the first camera milestone. In split-screen V1, the HUD should be constrained to P1's render window rather than spanning both player views.
- Co-op code in original/decomp files still needs concise `Co-op:` comments that explain why the hook exists.

## Design Direction

Use an API-first extension layer:

```text
camera/window/player index 0 -> original dComIfG_play_c storage
camera/window/player index 1 -> Dusk co-op extension storage
```

Then make existing native-looking calls work:

```cpp
dComIfGp_getCamera(1)
dComIfGp_getWindow(1)
dComIfGp_getPlayer(1)
dComIfGp_getCameraPlayer1ID(1)
dComIfGp_getCameraWinID(1)
```

This avoids making `mWindow[2]`, `mCameraInfo[2]`, or `mPlayerInfo[2]` the first move. Resizing those arrays changes the layout of a large reverse-engineered game-state object and can create distant regressions even when the immediate camera code compiles.

## Implementation Plan

### 1. Add Camera And Render Diagnostics

Add structured diagnostics before behavior changes.

Providers:

- `camera.registry`: camera id, player id, render window id, camera pointer, camera process state if cheaply available.
- `camera.state`: eye, center, up, fovy, bank, camera mode/type/status, attention status, and owner player slot.
- `render.windows`: window count, viewport/scissor rectangles, camera id per render window, current window/view/viewport during draw.
- `player.camera`: player slot, actor pointer, assigned camera id, ALINK camera field.

Rules:

- `latest.json` may include exact camera vectors every frame.
- `events.jsonl` should emit only semantic changes: camera created/deleted, assignment changed, layout changed, camera mode changed, null/mismatch detected.
- Do not add per-frame text logs for camera vectors.

### 2. Add Dusk Co-op Camera Extension Storage

Add a small Dusk-owned storage surface for co-op camera/window/player info.

The first version only needs one extra slot:

- render window 1,
- camera info 1,
- player info 1,
- camera attention/zoom fields needed by existing accessors.

Accessor policy:

- Index 0 uses the original `dComIfG_play_c` members.
- Index 1 uses Dusk extension storage when co-op split-screen is enabled or initialized.
- Out-of-range indices should preserve existing behavior as much as practical; do not build a broad new error framework.

### 3. Seed Camera/Window/Player Slot 1

Create a real second camera through the camera manager path, not a fake render-only transform.

Expected shape:

- create camera 1 with `fopCamM_Create(1, fpcNm_CAMERA2_e, params)`;
- set camera info 1 to camera 1, render window 1, player slot 1;
- set player info 1 to the secondary ALINK actor and camera 1;
- set the secondary ALINK camera id field to camera 1;
- set render window 0 and 1 rectangles for split-screen.

Do this behind a debug/UI toggle first. Co-op disabled should leave vanilla camera/window/player setup untouched.

### 4. Teach The Painter To Draw Active Render Windows

The painter currently draws through window 0. Change the relevant 3D draw path to loop active render windows.

For each render window:

- resolve the window's camera id;
- get that camera;
- set viewport and scissor from the window;
- set current window/view/viewport;
- install that camera's view/projection;
- draw the normal 3D lists for that camera.

Keep 2D/HUD/fade/menu behavior P1/global for V1 unless a concrete regression forces a narrower patch.

### 5. Add Minimal UI Controls

Add a Dusk debug control for:

- enable/disable native split-screen prototype;
- choose initial layout, with vertical split as the first default unless testing proves horizontal is better;
- show camera 0/1 pointers, player assignments, and render window rectangles;
- optionally toggle camera diagnostics profile.

Do not add a broad settings system before the prototype works.

### 6. Classify Camera 0 Call Sites By Evidence

Do not mass-replace `dComIfGp_getCamera(0)`.

Classify sites into:

- global camera policy, should stay camera 0 for now;
- actor/player-relative camera lookup, should use owner player's camera id;
- event/cutscene/boss/message camera behavior, should be documented and deferred unless it blocks normal field testing;
- rendering/environment effects that may need per-viewport draw handling.

Fix only the sites needed for the current split-screen validation scenario.

## Progress

- [x] Confirmed P2 control/item ownership has advanced far enough that shared camera is now a testing bottleneck.
- [x] Identified native indexed camera/window/player APIs.
- [x] Identified single-slot original backing arrays in `dComIfG_play_c`.
- [x] Identified main painter path still draws window 0.
- [x] Added camera/render diagnostics providers for `camera.state` and `render.windows`.
- [x] Added co-op camera extension storage for slot 1 in `dusk::coop::camera`.
- [x] Seeded camera/window/player slot 1 behind the prototype toggle/hotkey path.
- [x] Routed secondary ALINK camera id to camera 1.
- [x] Updated the painter to draw each active render window in normal field gameplay.
- [x] Tightened camera 1 readiness so the sidecar exposes two render windows only after the native `dCamera_c` body finishes `init_phase2`.
- [x] Fixed camera append parameter decoding on PC so camera id `1` is not read as byte-swapped `0x01000000` during `fopCam_Create`.
- [x] Scoped camera draw audio listener updates to camera 0 because `Z2Audience` and `Z2SpotMic` still store one audio camera/mic slot.
- [x] Fixed PC `dCamera_c::ResetView()` clobbering both split-screen render windows back to fullscreen every camera tick.
- [x] Disabled the fullscreen post-effect tail during split-screen V1 after diagnostics showed correct window rectangles but black 3D output with HUD still alive.
- [x] Restored split-screen 3D rendering after disabling the fullscreen post-effect tail exposed a working viewport path.
- [x] Scoped camera 1 draw side effects so it does not clear camera 0's global audio polygon state or leave global J3D view state on camera 1 after the camera draw phase.
- [x] Confirmed the lighting ownership model is understood at a practical level: restoring camera 0's global J3D view state after camera 1 draw moved the lighting influence from P2 back to P1.
- [x] Added richer `camera.state` diagnostics for camera distance, body state, style, trim, gear, and window dimensions while keeping JSONL event keys semantic.
- [x] Validated the HUD after constraining the V1 2D pass to P1's render window. HUD is intentionally P1-owned for now.
- [x] Found that camera 1 was using controller/player id `1` for vanilla `mPlayerStatus[1][4]` lookups. That table only has row 0, so camera 1 could read arbitrary status bits and choose arbitrary type/mode behavior.
- [x] Added a camera-status containment guard so camera 1 can keep controller 1 input while camera status checks remain on the vanilla row 0 until a real per-player camera-status sidecar exists.
- [x] Added camera map-tool diagnostics so `latest.json` shows each camera's active room, stage, default-room, and tag camera tool source alongside the final native type/style.
- [x] Scoped camera room/default-camera selection to each camera's owning player actor on PC. This keeps camera 1 from using the global stay room when P2 eventually crosses room boundaries independently.
- [x] Found that P2's `FieldWide` behavior came from camera tags being evaluated only against `dComIfGp_getLinkPlayer()` and applied only through `dCam_getBody()` camera 0. Added a PC split-screen path that evaluates the same native tag volume against P2 and applies it to camera 1.
- [ ] Design proper per-player/per-viewport environment-light ownership so split-screen lighting can be correct for all local players and future online peers.
- [x] Added `render_visibility` and bypassed actor/world/background draw-time culling during native PC split screen.
- [ ] Validate P1/P2 independent camera follow in a simple field/test room.
- [ ] Decide and implement real per-player camera status storage/routing for P2-specific item, lock-on, and special camera modes.
- [ ] Classify first batch of camera 0 call sites encountered during validation.

## Implementation Notes

- `include/dusk/coop/camera.h` and `src/dusk/coop/camera.cpp` own the slot-1 camera/window/player sidecar. This preserves the original `dComIfG_play_c` layout and keeps slot 0 on vanilla storage.
- `include/d/d_com_inf_game.h` routes only index 1 accessors to the sidecar under `TARGET_PC`. Index 0 remains the compatibility baseline.
- Secondary ALINK now reads `dComIfGp_getPlayerCameraID(1)` for its stored camera id only while split screen is enabled and camera 1 is fully initialized. A camera process pointer is not enough: `init_phase1` can publish the pointer before `init_phase2` constructs the native `dCamera_c` body.
- Camera creation on PC must read `fopCamM_prm_class::base.parameters` through the typed `BE(u32)` field. Raw `u32*` access byte-swapped camera id `1` into `0x01000000`, causing `dComIfG_play_c::setCamera` to index outside the vanilla one-slot camera array before the sidecar could handle slot 1.
- Camera 1 now uses the native `CAMERA2` profile name instead of creating a second `CAMERA` process. The camera id still comes from the append parameters, but using the existing second-camera profile keeps process identity/draw priority closer to the game's own indexed camera model.
- `Z2Audience` is not camera-slot extensible yet: it has `mAudioCamera[1]`, `mSpotMic[1]`, and `Z2SpotMic` per-camera arrays of length 1. Camera 1 must not call `setAudioCamera(..., camID=1)` in V1; audio listener, map info, and camera polygon position stay owned by camera 0 until a dedicated audio ownership plan exists. Camera 1 should also avoid clearing camera 0's global audio polygon state while skipping its own audio listener update.
- On PC, `dCamera_c::Run()` calls `ResetView()` every camera tick. Without a split-aware reset, camera 0 and camera 1 each rewrite their owning render window to the full game viewport, producing two valid camera views drawn over each other instead of a split. The split layout must be treated as authoritative after this reset path.
- Split-screen camera aspect and trim/scissor correction must be derived from each camera's active render window. Fullscreen aspect and originless scissor math make both cameras behave as if they own the same viewport.
- The painter's late post-effect tail includes fullscreen framebuffer capture/retry, depth-of-field, bloom, trim, and fade helpers. For split-screen V1 this tail is disabled while native split screen is active; re-enable individual effects only after their viewport/framebuffer ownership is made explicit.
- The 2D/HUD pass is still P1-owned in V1. The initial fullscreen restore left the vanilla HUD spanning both split views (hearts on the left side of the whole screen, item buttons on the right side), so the HUD pass is now constrained to camera/window 0 while split screen is active. A later HUD plan should decide whether to duplicate, partition, or redesign HUD elements per player.
- Vanilla `dComIfG_play_c` also stores `mPlayerStatus[1][4]`, not one row per camera/player slot. `dCamera_c` historically uses `mPadID` for both controller input and player-status lookups. For camera 1, `mPadID == 1` is correct for controller reads but unsafe for `dComIfGp_checkPlayerStatus*` because row 1 does not exist. V1 clamps those camera-local status reads to row 0 as a containment fix. This should stabilize camera 1 type/mode choices, but it is not the final online-friendly design; P2-specific camera modes need explicit per-player status ownership rather than borrowing P1's global row.
- On PC, camera room/default-camera selection now uses the camera owner's actor room when an owner exists. Vanilla single-player still resolves to the same room, while camera 1 can choose room camera data from P2's room instead of `dComIfGp_roomControl_getStayNo()`.
- Native camera tags are actor-driven globals in vanilla: `daTag_Cam_c` checks Link's position and writes camera data via `dCam_getBody()`. Split-screen V1 now keeps that original camera 0 path and, when camera 1 is ready, evaluates the same tag volume against P2's actor position and applies the tag to camera 1. This is intentionally local to camera tags; it does not make every camera event or cutscene multiplayer-safe yet.
- Camera 1 currently still relies on several global camera/render objects. After camera 1's draw method runs, camera 0's view matrix is restored as the authoritative global J3D view so follow-up global lighting/debug code does not accidentally inherit P2's camera. The user confirmed this changed lighting influence from P2 back to P1, proving the symptom is controlled by global camera/render state. That is a containment step, not the final design: correct split screen needs per-viewport environment/light setup so each local player, and eventually each online peer view, renders from its own camera without stealing global lighting from another view.
- Actor, world, and background draw-time culling were vanilla one-camera paths. PC split-screen now routes the known draw visibility paths through `dusk::coop::render_visibility::shouldBypassDrawCulling()` and bypasses those P1-camera clipper decisions while native split screen is active. This does not change actor execution, lifecycle, room loading, event approval, or explicit `NODRAW` behavior.
- `Ctrl+F12` now enables diagnostics, enables the native split-screen prototype, resets co-op probes, and spawns P2. The Actor Spawner also exposes `Native split screen` and `Ensure P2 camera` controls.
- V1 keeps HUD/2D/fades P1-owned. Only the normal 3D window pass loops over active render windows.
- `latest.json` may include exact camera eye/center/aspect/viewport state, camera type name, camera distance, trim, style timer, and window dimensions. `events.jsonl` event keys are semantic and should not churn from camera coordinates, distance, or timers alone.
- `camera.state.body.map` records native map-tool inputs for camera type selection: current map-tool type, room tool, stage tool, default-room tool, and tag tool. Use this before forcing camera 1 to mimic camera 0; if P2 is legitimately inside a different camera volume, the map tools should say so.
- `camera.state` events carry `event_context.player_slots` for current P1/P2 position and room at the moment of a semantic camera transition. Player movement remains context only; it does not drive camera events.

## Test Plan

The user owns Visual Studio/CMake builds unless explicitly delegated to Codex.

Light Codex checks:

```text
git diff --check
```

Manual validation sequence:

1. Build with Visual Studio MSVC debug.
2. Boot a save with split-screen disabled.
3. Confirm normal single-player camera and rendering.
4. Enable the camera diagnostics profile.
5. Spawn P2 with the existing co-op hotkey/test flow.
6. Enable native split-screen prototype.
7. Confirm the screen divides into two render viewports.
8. Confirm camera 0 follows P1 and camera 1 follows P2 during normal field movement.
9. Move P1 and P2 apart enough that the old shared-camera workflow would fail.
10. Flush diagnostics if either camera/window assignment or viewport rendering is wrong.

First acceptance target:

- P1 and P2 are visible in separate render viewports during normal field gameplay.
- P1 camera behavior remains normal when split-screen is disabled.
- P2's camera is a real `dCamera_c` camera process, not a hand-written view transform.
- HUD/2D overlays may still be P1/global and constrained to the P1 viewport.
- Event/cutscene/boss/message camera behavior may remain unresolved but must be documented when observed.

Failure conditions:

- P1 single-player camera regresses when co-op split-screen is disabled.
- Camera 1 exists only as a copied transform without using native camera process/state.
- The first patch requires broad resizing of original game-state structs without proving the extension layer cannot work.
- `events.jsonl` becomes noisy from per-frame camera vectors.

## Cleanup Notes

- Once camera 1 is stable, decide whether the camera extension layer should also own player-status fields for slot 1.
- Move any old camera debug logs into structured diagnostics or remove them.
- Keep the camera 0 call-site classification table in this plan until it grows large enough to justify a separate audit doc.
- Revisit HUD and targeting reticles after normal 3D split-screen works.
- Reintroduce fullscreen post effects for split screen one system at a time: fade, trimming, bloom, motion blur, depth-of-field, and framebuffer capture helpers should each become viewport-aware or be classified as P1/fullscreen.

## Decision Log

- Chose native `dCamera_c` split-screen over a cheap imitation camera because the game already has extensive camera state, modes, event behavior, and projection setup that should remain authoritative.
- Chose an extension-layer/API-first plan over immediate array resizing because `dComIfG_play_c` is a reverse-engineered global game-state object with single-slot original backing arrays.
- Chose diagnostics before behavior changes because camera/render bugs are easy to misread visually and need structured evidence for viewport, assignment, and camera-mode state.
