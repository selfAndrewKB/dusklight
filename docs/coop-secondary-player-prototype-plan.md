# Co-op Secondary Player Prototype Plan

This plan covers the third co-op milestone: test whether a second ALINK actor can exist safely enough for a local co-op slice, and gather evidence for the follow-up ALINK duplication audit in `docs/coop-alink-duplication-audit-plan.md`.

## Purpose

Create an intentionally gated secondary-player prototype. The primary player must remain the vanilla player 0 singleton. The prototype exists to gather runtime evidence, not to declare full Link duplication solved or abandoned.

This is not a camera rewrite, a networking layer, a second save slot, or a full multiplayer rules pass. It is also not a decision to rebuild Link from scratch as a proxy actor.

## Assumptions

- Primary Link remains the only actor allowed to call `dComIfGp_setPlayer(0, this)` and `dComIfGp_setLinkPlayer(this)`.
- Additional Link spawn requests are marked by actor arguments only during ALINK creation: `-2` requests slot 1, `-3` requests slot 2, and `-4` requests slot 3.
- The secondary prototype registers in Dusk's sidecar slot 1 and reads the slot 1 input snapshot, which maps to `PAD_2`.
- The prototype is expected to expose additional singleton issues. Any crash or bad global side effect is useful evidence for deciding between full ALINK duplication and a proxy actor.
- A proxy/replica actor is a fallback or temporary visual/debug tool, not the preferred architecture unless the ALINK duplication audit proves ALINK reuse is untenable.
- Visual Studio MSVC builds and manual runtime validation are user-owned unless explicitly delegated to Codex.

## Current Code Evidence

- `include/f_pc/f_pc_name.h` defines `fpcNm_ALINK_e` as `0x0FD`.
- `src/dusk/imgui/ImGuiActorSpawner.cpp` already creates actors through `fopAcM_create(...)` and exposes the actor `argument` field.
- The secondary prototype button now appears at the top of the Actor Spawner window under `Co-op`, before the generic actor spawn controls.
- `include/f_op/f_op_actor.h` stores the actor `argument` at `fopAc_ac_c::argument`.
- `src/d/actor/d_a_alink.cpp` primary creation normally calls `dComIfGp_setPlayer(0, this)`, `dComIfGp_setLinkPlayer(this)`, and creates primary-only companions/start side effects.
- `src/d/actor/d_a_alink.cpp` primary deletion normally clears player 0 globals.
- `include/dusk/coop/player_slots.h` now defines `kFirstAdditionalPlayerSpawnArgument = -2`, `getAdditionalPlayerSpawnRequestSlot(...)`, slot-first runtime helpers such as `isAdditionalPlayer(...)`, and the thin slot-based `spawnPlayer(...)` API.

## Files

Expected edits:

- `docs/coop-secondary-player-prototype-plan.md`
- `AGENTS.md`
- `include/dusk/coop/player_slots.h`
- `src/dusk/coop/player_slots.cpp`
- `src/d/actor/d_a_alink.cpp`
- `src/dusk/imgui/ImGuiActorSpawner.cpp`

Do not resize `dComIfG_play_c` or mass-replace player singleton helpers in this milestone.

## Implementation

1. Add Dusk-owned markers for additional ALINK creation: actor `argument == -2` requests slot 1, `-3` requests slot 2, and `-4` requests slot 3.
2. In ALINK create, register marked actors as sidecar slot 1 instead of overwriting player 0 globals.
3. Keep primary ALINK behavior unchanged for unmarked actors.
4. Skip primary-only startup side effects for marked secondary actors where they are clearly global:
   - clothing/startup save state,
   - restart-room update,
   - duplicate debug HIO entry,
   - Midna/TKS/start portal/start switch setup.
5. In ALINK delete, unregister marked actors from sidecar slot 1 and do not clear player 0 globals.
6. Add a debug Actor Spawner button that creates an additional ALINK near player 1 through the slot-based spawn API, using current player parameters, current room, and current facing.
7. Keep marked secondary ALINK actors out of vanilla attention lists.
8. Do not tick marked secondary ALINK actors through `execute()` after creation. Runtime testing showed full secondary ALINK ticking corrupts primary player animation/state.
9. Log primary ALINK action/animation checkpoints during secondary creation only. This keeps tracing focused on creation-time singleton damage instead of adding broad per-frame noise.
10. While the secondary prototype exists, sample primary runtime state on action/animation changes and every 30 frames. This checks whether player 1 leaves `PROC_WAIT` for `PROC_MOVE` while its visible base animation remains pinned.
11. Temporarily skip secondary ALINK `draw()` as an audit probe. If this fixes player 1's visible animation, the next audit target is draw/model/material/model-calc state; if it does not, the next target is persistent heap/init/anime setup.
12. A too-aggressive early return before final create-time animation/model setup crashed after the `skip-final-anime-init` checkpoint. Keep later probes structurally complete so ALINK cleanup/framework code still sees normal create-time fields.
13. Temporarily skip only secondary create-time `allAnimePlay()`. If this fixes player 1's visible animation, the next audit target is shared `J3DAnmTransform` frame mutation.
14. Temporarily skip secondary create-time `mpLinkModel->calc()`. If this fixes player 1's visible animation, the next audit target is shared model/matrix state during secondary model calculation.
15. Replace one-off rebuild probes with Actor Spawner runtime toggles so combinations can be tested without rebuilding after every skipped call.
16. Add a scoped secondary `execute()` probe that temporarily installs player 2's shared model-data ownership only while the execute call runs, restores player 1 immediately afterward, and samples secondary proc/animation state for comparison with the existing primary runtime logs.

## Progress

- [x] Confirmed ALINK actor ID and actor creation path.
- [x] Added the secondary ALINK spawn marker helper.
- [x] Gated primary singleton ownership in ALINK create/delete.
- [x] Added a debug Actor Spawner button for secondary Link.
- [x] Moved the button to the top `Co-op` section so it is not hidden below the generic spawn controls.
- [x] Manually attempted a secondary spawn from the Actor Spawner.
- [x] Recorded whether full ALINK duplication looks viable or whether a proxy actor is safer.
- [x] Gated secondary attention and execute ticking after the first runtime findings.
- [x] Built and manually attempted the mitigated secondary spawn from the Actor Spawner.
- [x] Recorded that the secondary actor is no longer targetable, but player 1 still becomes stuck in standing animation.
- [x] Added focused creation-checkpoint logging for the remaining animation-lock issue.
- [x] Built and manually attempted the creation-checkpoint logging.
- [x] Recorded that secondary creation leaves player 1 in `PROC_WAIT` with a stable wait animation pointer while player 1 is idle.
- [x] Added sampled primary runtime logging for the state after secondary creation.
- [x] Built and manually attempted the primary runtime logging.
- [x] Recorded that player 1 enters `PROC_MOVE` after the secondary exists, so input/action state still works.
- [x] Recorded that player 1's visible base animation remains pinned to the same animation pointer across wait and move samples.
- [x] Decided not to pivot blindly to a proxy actor; next milestone is the ALINK duplication audit.
- [x] Build and manually attempt the draw-skipped secondary ALINK diagnostic.
- [x] Build and manually attempt the final-animation-init-skipped secondary ALINK diagnostic.
- [x] Build and manually attempt the create-time `allAnimePlay()`-skipped secondary ALINK diagnostic.
- [x] Build and manually attempt the create-time `mpLinkModel->calc()`-skipped secondary ALINK diagnostic.
- [ ] Build and manually attempt the runtime-toggle secondary ALINK diagnostics.
- [x] Build and manually attempt the scoped secondary execute diagnostic.
- [x] Graduated runtime identity away from spawn arguments: negative ALINK arguments now mean only "additional ALINK spawn request", while registered slot state is the runtime source of truth.

## Decisions

- Use negative actor arguments instead of stealing bits from ALINK parameters. ALINK parameters already encode start room, start mode, and start event.
- Use the existing Actor Spawner instead of adding a new co-op UI panel.
- Spawn beside player 1 with a simple local X offset. This is crude but keeps the first test focused on lifecycle, singleton ownership, and input routing.
- This path is no longer labeled as a runtime prototype. The plan remains the historical record for the ALINK duplication experiment, but current code treats the path as the supported local secondary Link spawn for co-op testing.
- Spawn arguments are not runtime identity. They are create-time bootstraps so `daAlink_c::create()` can avoid claiming vanilla player 0 before extra-slot registration exists. After registration, runtime code should use `getSlotForActor`, `isPlayerInSlot`, or `isAdditionalPlayer`.
- The spawn operation lives in `dusk::coop::spawnPlayer(...)`, not the ImGui panel. The Actor Spawner and `Ctrl+F12` are debug callers; future menu player-count settings, controller "press Start to join", and online host join flows should call the same co-op lifecycle API. The registry and request encoding are four-slot-shaped now, while camera/render support remains validated only for slot 1.
- First runtime evidence argues against full ALINK duplication as the next path. A full secondary ALINK appeared and idled, but it became targetable, made player 1's animation stick in idle while movement/attacks still applied, mirrored shield/block animation, and spun while targeted. This points at attention, animation, and singleton state coupling beyond the create/delete globals.
- After that evidence, the prototype is reduced to a render/lifecycle probe: secondary ALINK registers/draws but does not enter the full `execute()` loop.
- Clearing secondary attention flags fixed the yellow reticule, but skipping secondary `execute()` did not fix player 1's standing-animation lock. That means the next evidence should come from ALINK creation checkpoints, not per-frame secondary logic.
- Creation checkpoints did not show an immediate primary mutation while player 1 was idle: player 1 remained in `PROC_WAIT`, with a stable wait animation pointer and advancing frame/rate. The next check is whether player 1 later enters move/attack procs while its base animation remains stuck.
- Runtime checkpoints showed player 1 does enter `PROC_MOVE` after the secondary exists, with nonzero speed and stick values. Input and action state are not frozen.
- The same runtime checkpoints showed the base animation pointer remained pinned while player 1 moved. The current likely culprit is shared animation/model/resource binding or matrix/model-calc state, not controller input, attention, or ALINK action proc selection.
- Do not treat this prototype as proof that a proxy actor is the correct architecture. Rebuilding Link from scratch would be costly and should remain a fallback. The next plan is `docs/coop-alink-duplication-audit-plan.md`, which audits the singleton/shared-state problem before another serious ALINK duplication attempt.
- Skipping secondary ALINK drawing did not fix player 1's stuck visible animation, and player 2 did not appear as expected. The ordinary secondary draw path is not required to trigger the lock.
- Returning before the final create-time animation/model setup block crashed after the `skip-final-anime-init` checkpoint. That return path is retired because it leaves the actor too partially initialized.
- Skipping secondary create-time `allAnimePlay()` fixed the early-return crash but did not fix player 1's animation corruption. Direct animation playback alone is not the full culprit.
- Skipping secondary create-time `mpLinkModel->calc()` also did not fix player 1's animation corruption.
- The next active diagnostic uses Actor Spawner runtime toggles for the remaining probes. This avoids rebuilding after every skipped call while keeping the audit explicit.
- The toggle sweep ruled out face texture animation, item matrix setup, item actor setup, set matrix, and wait animation binding as sufficient fixes. Skipping start proc init crashed, so it is structural. The next focused mitigation restores player 1 as the owner of shared ALINK model-data matrix calculators after secondary `playerInit()` / `changeLink()`.
- Restoring player 1 as the shared ALINK model-data owner fixed player 1's visible animation lock. The first proven ALINK duplication hazard is shared `J3DModelData` matrix-calculator ownership.

## Validation

Expected manual path after a successful build:

1. Boot a normal save and confirm player 1 still controls normally.
2. Open Dusk's Tools menu and the Actor Spawner.
3. Click `Spawn Secondary Link`.
4. Watch the log for `dusk::coop` messages.

Expected log signal:

- Primary player slot 0 still registers, refreshes, and unregisters as before.
- Secondary spawn should log a player slot 1 registration if ALINK creation reaches the prototype hook.
- Player slot 0 should not show a replacement caused by the secondary spawn.
- `dusk::coop.alink` should print secondary creation checkpoints such as `begin`, `after-arc-load`, `after-solid-heap`, `after-player-init`, `after-bg-ready`, and `after-anime-init`, each including player 1's proc, speed, stick, base animation frame/rate, animation pointer, and attention flags.
- After the secondary exists, `dusk::coop.alink` should print `primary runtime` lines when player 1's proc or base animation pointer changes, plus a sampled line every 30 frames.
- Deleting or transitioning should not show secondary actors clearing player slot 0.

Expected behavior:

- Player 1 remains controllable from controller port 1.
- If the secondary actor completes creation, it may appear as a non-ticking idle Link shell.
- During the draw-skip diagnostic, the secondary actor may be invisible or otherwise non-rendering.
- The mitigated prototype should not show a yellow targeting reticule, should not make player 1's animation stick in idle, and should not mirror player 1's shield/block animation.
- Crashes, invisible actors, duplicate model/resource issues, or global state oddities should be recorded here. They are evidence for the full-ALINK-vs-proxy decision.

Validation performed:

- User built and spawned the first secondary ALINK prototype.
- The actor appeared with idle standing animation.
- The actor received a yellow targeting reticule, player 1 became stuck in idle animation while still moving/attacking, shield/block animation mirrored to the secondary actor, and the secondary actor spun while targeted by player 1.
- Interpretation: full ALINK duplication is not a small next step. A proxy/replica actor is likely safer for the first playable local co-op slice.
- User built and spawned the mitigated secondary ALINK prototype.
- The actor was no longer targetable, but player 1 still became stuck in standing animation.
- Interpretation: the targeting bug was attention-list coupling, while the remaining animation bug happens during secondary ALINK creation before secondary `execute()` is allowed to run.
- User built and spawned the runtime-logged secondary ALINK prototype.
- Runtime logs showed player 1 reached `PROC_MOVE` with normal-looking speed and stick values after secondary spawn.
- Runtime logs also showed player 1's base animation pointer remained unchanged across wait and move samples while the visible standing-animation lock persisted.
- Interpretation: the next work should audit ALINK animation/model/resource ownership and broader singleton callsite meanings. A proxy actor remains possible, but the project should not abandon ALINK reuse without that audit.
- User built and spawned the draw-skipped secondary ALINK prototype.
- Player 2 did not appear, which was expected because secondary `draw()` was skipped.
- Player 1 still became stuck in standing animation.
- Interpretation: the ordinary secondary draw path is not required to trigger the lock. The next diagnostic skips final create-time animation/model setup to decide whether the culprit is that block or an earlier creation step.
- User built and spawned the final-animation-init-skipped secondary ALINK prototype.
- The game crashed after logging `secondary create skip-final-anime-init`.
- Interpretation: returning early from ALINK `create()` is not a safe diagnostic because later actor framework or cleanup paths still expect normal create-time fields.
- User built and spawned the create-time `allAnimePlay()`-skipped secondary ALINK prototype.
- The crash was fixed, but player 1's visible animation still broke.
- Interpretation: direct create-time animation playback is not sufficient to explain the lock. The next diagnostic skips secondary create-time model calculation.
- User built and spawned the create-time `mpLinkModel->calc()`-skipped secondary ALINK prototype.
- Player 1's visible animation still broke.
- Interpretation: create-time model calculation is not sufficient to explain the lock. Runtime toggles are now needed to test the remaining candidate calls without a rebuild loop.
- User tested the runtime toggle sweep.
- With the cautious default bits still checked, adding `Skip face texture animation`, `Skip item matrix`, `Skip item actor setup`, `Skip set matrix`, or `Skip wait animation bind` did not fix player 1's animation lock.
- Adding `Skip start proc init` crashed the game.
- Interpretation: the culprit is earlier than the late create-time animation/model/item calls. The strongest current suspect is shared `J3DModelData` ownership from `changeModelDataDirect()`, which installs actor-specific matrix calculators on shared Link body model data during `changeLink()`.
- User built and spawned the prototype with `Restore P1 model data owner` enabled in the default probe set.
- Player 1's animation lock was fixed.
- Interpretation: secondary `changeLink()` / `changeModelDataDirect()` was installing secondary-owned matrix calculators onto shared Link model data. Restoring player 1 ownership after secondary initialization is the first confirmed mitigation for ALINK duplication.
- User rebuilt and spawned the visible-secondary prototype with create-time model calc and draw enabled, plus the scoped ownership handoffs around secondary create-time animation/model setup and secondary draw.
- Player 2 visibly appeared. The logs showed the secondary create-time owner install/restore pair and the draw-time P1 -> P2 -> P1 owner swap behaving as intended.
- Player 1 continued through normal-looking idle, movement, and attack-related proc transitions afterward, with no report that the earlier visible-animation lock returned.
- Interpretation: visible secondary ALINK rendering is now proven viable under explicit shared `J3DModelData` ownership scoping. The next question is not whether a second ALINK can render, but which minimal secondary execution and input paths can be restored without disturbing the primary actor or reintroducing shared-state corruption.
- Historical note: this probe kept `Skip execute` enabled by default, but added `Scoped execute model data owner` so a deliberate manual test could re-enable secondary runtime while preserving the proven ownership discipline and producing paired `secondary execute` / `primary runtime` evidence. Later input-routing work promoted secondary execute into the default harness.
- User tested the scoped secondary execute probe by spawning player 2 first, then unchecking `Skip execute`.
- Player 2 remained visible, began playing idle animations correctly, and mirrored player 1's target/shield-raise animation when activated.
- Interpretation: scoped secondary execute admits at least idle animation runtime without the original player 1 animation lock. The next suspected hazard is shared input/action/status ownership for targeting or shield state.
- Added a follow-up `secondary action-mirror` diagnostic under the scoped execute probe. It compares P2's `checkInputOnR()`, attention lock, target actor, item R state, raw P1/P2 lock-R input, and global R status so the next manual run can distinguish P1 input leakage from shared attention/status mirroring.

## Recovery Notes

If the prototype breaks startup or primary play, remove only these changes:

- `docs/coop-secondary-player-prototype-plan.md`
- the secondary prototype marker in `include/dusk/coop/player_slots.h` and `src/dusk/coop/player_slots.cpp`
- the `coop_secondary` guarded blocks in `src/d/actor/d_a_alink.cpp`
- the `Spawn Secondary Link` block in `src/dusk/imgui/ImGuiActorSpawner.cpp`

Keep the committed player-slot registry and input snapshot milestones unless the failure directly involves them.
