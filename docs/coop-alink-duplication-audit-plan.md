# Co-op ALINK Duplication Audit Plan

This plan is the next co-op decision point after the secondary ALINK prototype. The goal is not to build player 2 from scratch by default. The goal is to understand, document, and isolate the singleton/shared-state hazards that prevent a second `daAlink_c` from being safely instantiated.

## Purpose

The project still prefers reusing ALINK if that can be made safe. Rebuilding Link behavior in a proxy actor would mean reimplementing movement, animation, combat, items, reactions, traversal, equipment, and many edge cases. That path may become necessary later, but it should not be chosen just because the first naive second-ALINK spawn failed.

The current evidence says: spawning a second ALINK blindly is unsafe. It does not say that ALINK duplication is impossible.

This audit exists to find exactly which systems must become slot-aware, split per actor, or protected as primary-only before another serious second-ALINK attempt.

## Current Runtime Evidence

Observed during manual Visual Studio MSVC debug builds on 2026-05-11:

- A secondary ALINK can be spawned from the Actor Spawner when marked with actor `argument == -2`.
- The secondary actor registers in Dusk's sidecar player slot 1.
- Gating secondary create/delete prevents the secondary from replacing or clearing vanilla player 0 globals.
- Clearing secondary `attention_info.flags` prevents the yellow targeting reticule.
- Skipping secondary `execute()` prevents the full secondary state machine from ticking, but does not fix player 1's standing-animation lock.
- Creation checkpoints show player 1 remains in `PROC_WAIT` while idle during secondary creation, with stable base animation pointer and advancing frame/rate.
- Runtime checkpoints after the secondary exists show player 1 can still enter `PROC_MOVE`; input and action state are not frozen.
- In those `PROC_MOVE` samples, player 1's visible base animation remains pinned to the same animation pointer seen during wait.

Interpretation: the remaining bug is below the input/action layer. It likely involves shared animation/model/resource binding, matrix calculation, or another ALINK-internal singleton path. The prototype should be treated as evidence for an audit, not as proof that a proxy actor must replace ALINK reuse.

## Working Assumptions

- Primary Link remains the only actor that owns `dComIfGp_setPlayer(0, this)` and `dComIfGp_setLinkPlayer(this)` until a specific plan proves otherwise.
- Slot 0 behavior must remain the compatibility baseline.
- Existing `dComIfGp_getPlayer(0)` call sites should not be mass-replaced. They must be classified by intent first.
- ALINK ownership writes are higher risk than ALINK reads.
- A helper is useful only when its call sites have a clear semantic class. A vague "multiplayer helper" will hide bugs.
- Every co-op edit in original/decomp code needs a concise `Co-op:` why-comment.
- Debug logging should remain gated or sampled. Broad per-frame logging is allowed only for a short plan-backed diagnostic.

## Callsite Classification

Before replacing singleton helpers, classify each callsite by what it actually means:

- `Primary player`: the single-player hero, camera anchor, UI owner, or save-state owner.
- `Current ALINK actor`: the actor currently executing ALINK code.
- `Actor's nearest player`: enemy targeting, distance checks, simple attention decisions.
- `Interacting player`: talk, item pickup, button prompt, or collision owner.
- `Camera player`: systems that must follow the camera's canonical player, at least initially.
- `Global story/player state`: flags, equipment, health, event, cutscene, Midna, and scene-transition ownership.
- `Resource/model/animation owner`: model data, animation heaps, frame controllers, matrix calculators, draw state, or shared archives.

Do not convert a callsite until its class is known.

## Audit Buckets

### Player Singleton Ownership

Files and APIs already identified:

- `src/d/actor/d_a_alink.cpp`
- `include/d/d_com_inf_game.h`
- `dComIfGp_setPlayer(0, ...)`
- `dComIfGp_setLinkPlayer(...)`
- `dComIfGp_getPlayer(0)`
- `daPy_getPlayerActorClass()`

Questions:

- Which writes must remain primary-only forever?
- Which reads can become slot-aware safely?
- Which reads really mean "nearest player" or "interacting player"?

### Input Ownership

Files and APIs already identified:

- `src/d/actor/d_a_alink.cpp`
- `include/m_Do/m_Do_controller_pad.h`
- `dusk::coop::captureInputSnapshot(...)`
- `dusk::coop::getPadForSlot(...)`

Questions:

- Which direct `PAD_1` reads remain inside ALINK after the input snapshot milestone?
- Which reads are action-critical versus UI/camera/debug?
- Can ALINK consume a slot input context without changing primary behavior?

### Attention And Targeting

Files and APIs already identified:

- `src/d/d_attention.cpp`
- `attention_info.flags`
- `dComIfGp_getAttention()`

Questions:

- Which attention paths assume exactly one player?
- Which actor lists should include secondary players?
- Which systems should intentionally ignore secondary ALINK/prototypes?

### Animation And Model State

This is the current highest-priority bucket.

Files and APIs to inspect first:

- `src/d/actor/d_a_alink.cpp`
- `include/d/actor/d_a_alink.h`
- `daAlink_createHeap`
- `daAlink_c::playerInit()`
- `daAlink_c::setMatrix()`
- `daAlink_c::allAnimePlay()`
- `daAlink_c::modelCalc(...)`
- `mNowAnmPackUnder`
- `mNowAnmPackUpper`
- `mUnderFrameCtrl`
- `mUpperFrameCtrl`
- `field_0x1f20`
- `field_0x1f24`
- `J3DModel`
- `J3DModelData`
- `mDoExt_MtxCalcAnmBlendTbl`
- `mDoExt_MtxCalcAnmBlendTblOld`

Questions:

- Does creating a second ALINK mutate shared model data rather than only actor-local model instances?
- Do ALINK animation resources return shared mutable `J3DAnmTransform` pointers?
- Do matrix calculators or frame controllers store global/static state?
- Is P1's stuck visible animation caused by shared animation pointer reuse, shared model data binding, or draw/model-calc ordering?
- Can a second ALINK allocate isolated animation/model state if resource loading is separated from primary-only player initialization?

Static findings so far:

- `daAlink_createHeap` allocates actor-local `mUnderAnmHeap`, `mUpperAnmHeap`, `field_0x2060`, `field_0x1f20`, and `field_0x1f24`.
- `getAnimeResource(...)` loads animation data through the actor's `daPy_anmHeap_c`, then ALINK stores the returned `J3DAnmTransform*` in `mNowAnmPackUnder` / `mNowAnmPackUpper`.
- `commonSingleAnime(...)`, `commonDoubleAnime(...)`, `setUpperAnime(...)`, `setUnderAnime(...)`, and `animePlay(...)` set frame values directly on the current `J3DAnmTransform`.
- That direct frame mutation is safe only if the transforms are actor-owned. If any resource path aliases the same mutable `J3DAnmTransform` across primary and secondary ALINK, the actors can overwrite each other's visible animation frames.
- `draw()` also touches model/material state through `modelDraw(...)`, `entryTevRegAnimator(...)`, `removeTevRegAnimator(...)`, mirror entry, and item model drawing. That makes draw/model/material state a plausible separate culprit from animation heap ownership.

### Camera And UI Ownership

Files and APIs already identified:

- `dComIfGp_getPlayerCameraID(0)`
- `mCameraInfo[1]`
- `mPlayerStatus[1][4]`
- button status helpers
- meter/UI systems

Questions:

- Which camera/UI writes must remain primary-only for local co-op?
- Which status bits are global campaign/UI state versus per-player state?

### Midna, Events, Save, And Scene Transitions

Files and APIs already identified:

- `daPy_py_c::m_midnaActor`
- Midna creation in ALINK startup
- event helpers in `src/f_op/f_op_actor_mng.cpp`
- restart-room writes in ALINK create

Questions:

- Which startup side effects can never be run by a secondary ALINK?
- Which global systems need "park or hide secondary player" rules during cutscenes and transitions?

## Near-Term Work Plan

1. Archive the secondary ALINK prototype findings in `docs/coop-secondary-player-prototype-plan.md`.
2. Keep the prototype button and guarded ALINK path only as a diagnostic tool until this audit decides otherwise.
3. Keep the confirmed player 1 model-data ownership restore enabled while auditing the next shared-state hazards.
4. Audit other ALINK shared resource/model-data ownership writes before re-enabling secondary draw or execute.
5. Convert proven ownership hazards into narrow helpers only after at least two call sites show the same semantic need.
6. Only after the shared resource/model-data hazards are understood, consider re-enabling any secondary ALINK runtime beyond the current render/lifecycle probe.

## Debugging Methodology That Worked

Use this pattern again when a secondary ALINK bug appears:

1. Preserve a clean baseline and make P1 behavior the compatibility oracle.
2. Add a diagnostic button or toggle instead of forcing a rebuild for every hypothesis.
3. Keep logs sparse and phase-based first: creation checkpoints, then sampled runtime logs only when creation is not enough.
4. Separate symptom layers before changing code: attention/targeting, input/action state, animation pointer/frame state, draw/model calc, resource/model-data ownership.
5. Prefer negative tests that rule out broad buckets. In this case, skipping execute, draw, late animation playback, model calc, face texture animation, item setup, matrix setup, and wait binding narrowed the bug to earlier shared ownership.
6. When late calls are ruled out, inspect the code for ownership writes rather than more runtime guessing. Look for shared-resource mutation: `J3DModelData` callbacks, animator entries/removals, material state, `setUserArea()`, global status writes, and static state.
7. Convert the strongest code finding into the smallest reversible mitigation. Restoring P1's shared model-data owner after secondary `playerInit()` fixed the lock and confirmed the theory.
8. Record both failed probes and the successful probe. The dead ends are useful because they prevent future sessions from repeating the same tests.

## Next Specific Work

1. Audit `changeModelDataDirect()` and `changeModelDataDirectWolf()` fully.
   - Classify every write as actor-local, shared `J3DModelData`, shared material/shape, callback, or user-area ownership.
   - Decide whether secondary ALINK should avoid installing those writes, install then restore P1, or use isolated model data later.
2. Audit ALINK material/texture animator ownership.
   - Start with `entryTexMtxAnimator`, `entryTexNoAnimator`, `entryTevRegAnimator`, and matching removal calls in `d_a_alink.cpp` and included ALINK files.
   - Record which ones target shared Link body/face/hat/sword model data.
3. Add logs or toggles only for the next specific suspected ownership write.
   - Do not add a broad "skip everything" mode.
   - Keep `Skip execute`, `Skip draw`, and `Restore P1 model data owner` as the default containment harness.
4. Once shared model-data ownership is mapped, test whether secondary draw can be re-enabled with P1 ownership restored.
   - Expected risk: secondary draw may steal model-data owner again or mutate material animators.
   - If it breaks, inspect draw-time `modelDraw(...)`, `modelCalc(...)`, material animator entry/removal, and `J3DModel::setUserArea(...)` paths.
5. Only after draw is understood, test a narrow secondary execute slice.
   - Start with logs around proc changes and animation setters.
   - Do not route P2 input yet unless the create/draw ownership path stays stable.

## Cleanup List

Do these after the audit stops changing shape:

- Move temporary secondary ALINK probe flags out of `player_slots.*` if they survive beyond this audit.
- Rename probe UI labels if a flag graduates from diagnostic to normal mitigation.
- Remove or hide dangerous toggles such as `Skip start proc init` once no longer needed; it is known to crash.
- Reduce `dusk::coop.alink` checkpoint logging once the next milestone no longer needs creation-phase traces.
- Decide whether the Actor Spawner button remains a developer diagnostic, moves to a dedicated co-op debug panel, or is removed.
- Revisit default probe flags so default behavior reflects the current safe harness, not stale historical tests.
- Replace "prototype" wording in code comments only if secondary ALINK duplication becomes an actual supported path.
- Keep failed-probe results in docs, but archive completed plan details if they start crowding the active audit.

## Active Diagnostic

The previous diagnostic skipped both secondary ALINK `execute()` and secondary ALINK `draw()`. The user confirmed on 2026-05-11 that player 1 still became stuck in standing animation, while player 2 did not appear. That means the ordinary secondary draw path is not required to trigger the lock.

The next diagnostic returned from secondary `create()` before the final create-time animation/model setup. The user confirmed it crashed after the `skip-final-anime-init` checkpoint. That means the early return left ALINK too partially initialized for the actor framework or cleanup path.

The next diagnostic kept secondary `create()` structurally complete and skipped only secondary create-time `allAnimePlay()`. The user confirmed on 2026-05-11 that the crash was fixed, but player 1's visible animation still broke. That means create-time animation playback alone is not the full culprit.

The current diagnostic replaces one-off rebuild probes with runtime toggles in the Actor Spawner's `Secondary ALINK probes` tree. The toggles are backed by `dusk::coop::SecondaryAlinkProbeFlag` so ALINK and the UI share one bitmask.

Available toggles:

- Skip secondary `execute()`.
- Skip secondary `draw()`.
- Skip secondary wait-animation binding.
- Skip secondary `setStartProcInit()`.
- Skip secondary `setMatrix()`.
- Skip secondary create-time `allAnimePlay()`.
- Skip secondary create-time `mpLinkModel->calc()`.
- Skip secondary `playFaceTextureAnime()`.
- Skip secondary `setItemMatrix()` / `setWolfItemMatrix()`.
- Skip secondary `setItemActor()`.
- Restore player 1's shared ALINK model-data owner after secondary `playerInit()` / `changeLink()`.
- Scope shared ALINK model-data ownership to secondary only during secondary draw, then restore player 1 immediately afterward.
- Scope shared ALINK model-data ownership to secondary only during secondary execute, then restore player 1 immediately afterward.

Current default bits preserve the latest visible idle-P2 harness: skip secondary `execute()`; restore player 1's shared model-data owner after secondary initialization; and keep scoped draw/execute ownership ready for deliberate follow-up tests. Secondary create-time `allAnimePlay()`, create-time `mpLinkModel->calc()`, and `draw()` are enabled by default now that scoped model-data ownership makes the prototype visible without reintroducing the original player 1 animation lock. Re-enable the create-time skips or `Skip draw` only for isolation tests.

The latest manual toggle sweep ruled out the remaining late create-time candidates. With the cautious default bits still enabled, skipping secondary face texture animation, item matrix setup, item actor setup, set matrix, and wait animation binding did not fix player 1's visible animation lock. Skipping `setStartProcInit()` crashed and is treated as structural, not an optional side effect.

The confirmed culprit is `changeModelDataDirect()` / `changeModelDataDirectWolf()`. `changeLink()` calls these during `playerInit()`, and they write `field_0x1f20` / `field_0x1f24` matrix calculators directly onto shared `J3DModelData` joint nodes. A secondary ALINK can therefore make shared Link body model data point at the secondary actor's animation packs. This matched the observed symptom: player 1's logic reached movement/action procs, but its rendered skeleton remained stuck on the secondary-created idle binding.

Restoring player 1's model-data owner after secondary `playerInit()` fixed the player 1 animation lock in the user's Visual Studio build. Keep that mitigation as the first proven ALINK shared-state fix. The next audit should look for other `J3DModelData` callbacks, material animators, texture animators, and user-area writes that are installed on shared resource/model data rather than on per-actor model instances.

Purpose:

- Test combinations without rebuilding for every probe.
- Keep each skipped call explicit in the log, using `dusk::coop.alink` checkpoints such as `skip-create-model-calc`.
- Prefer enabling one new skip at a time, starting from the current default bits. Combination testing is useful only after individual results are recorded.

Expected manual check:

1. Build with Visual Studio MSVC.
2. Confirm player 1 animates normally before spawning the prototype.
3. Spawn `Spawn Secondary Link Prototype`.
4. The secondary actor is expected to be visible with the current default harness.
5. Move, stop, attack, and shield/block with player 1.
6. Confirm player 1's visible animation remains correct with `Restore P1 model data owner` checked.

First visible-P2 draw test result:

- User tested on 2026-05-11 with `Skip draw` unchecked while the cautious defaults still skipped secondary create-time `allAnimePlay()` and `mpLinkModel->calc()`.
- Player 2 remained invisible, but secondary `draw()` returned `1`.
- The `draw-owner` logs showed the scoped owner swap working: before secondary install, shared body model data still pointed at player 1's matrix calculators; after secondary install it pointed at player 2's calculators; after draw, it was restored to player 1.
- This means the current invisibility is probably caused by leaving secondary create-time animation/model setup skipped, not by the draw wrapper failing to run.

Second visible-P2 draw test result:

- User tested with `Skip draw` unchecked and `Skip create model calc` unchecked while `Skip create animation play` remained checked.
- Player 2 remained invisible.
- MSVC debug asserted in `J3DModelData::calc()` through `MTXQuat(): zero-value quaternion`. Pressing Ignore allowed the game to continue.
- Treat that flag combination as invalid: secondary model calc needs the matching create-time animation playback. The probe flags now normalize so `Skip create animation play` also implies `Skip create model calc`; the UI also clears `Skip create animation play` when `Skip create model calc` is unchecked.

Third visible-P2 draw test result:

- User tested with both create-time animation playback and create-time model calc restored.
- Player 2 still did not appear, and MSVC debug again asserted in `MTXQuat(): zero-value quaternion`.
- The create log reached `after-anime-init`, but there were no useful follow-up `draw-test` lines in the supplied excerpt, so the failure is still happening during or immediately around secondary create-time setup rather than being explained by the draw wrapper alone.
- Code review found a concrete ordering problem: the earlier mitigation restores player 1's shared `J3DModelData` calculator ownership immediately after secondary `playerInit()`, but secondary create later calls `allAnimePlay()` and `mpLinkModel->calc()`. Those calls therefore run while shared model data points back at player 1.
- The current patch scopes shared model-data ownership back to player 2 only for that late create-time animation/model setup, then restores player 1 immediately afterward. This mirrors the proven draw-time ownership discipline.

Fourth visible-P2 draw test result:

- User retested with `Skip create model calc` and `Skip draw` off, with the create-time and draw-time ownership scopes in place.
- Player 2 visibly appeared.
- The create log showed:
  - `secondary create installed secondary model data owner for anime/model setup`
  - `secondary create restored primary model data owner after anime/model setup`
- The draw logs again showed the expected P1 -> P2 -> P1 matrix-calculator ownership handoff around secondary draw.
- Player 1 continued through normal-looking idle, walk/run, attack, and other proc transitions afterward. The supplied runtime logs do not show the earlier visible-animation lock returning.
- The user noted the immediately prior invisible/asserting test may not have been rebuilt correctly, so treat the successful rebuilt run as the current authority.

Current visible-P2 conclusion:

- A secondary ALINK can now create and render visibly without reintroducing the original player 1 animation lock, as long as shared `J3DModelData` matrix-calculator ownership is scoped:
  - restore player 1 after secondary `playerInit()` / `changeLink()`;
  - temporarily give ownership back to player 2 for secondary create-time animation/model setup;
  - temporarily give ownership back to player 2 for secondary draw, then restore player 1 immediately afterward.
- The next audit should move from "make P2 visible" to "which minimal secondary execution/input pieces can be reintroduced without breaking that ownership discipline?"

## Current Execute Probe

The next diagnostic keeps `Skip execute` checked by default, but adds a separate `Scoped execute model data owner` toggle. When `Skip execute` is unchecked with that scoped-owner toggle still enabled, secondary ALINK `execute()` runs once per frame under temporary player 2 model-data ownership, then restores player 1 immediately afterward.

Searchable logs:

- `secondary execute before ...`
- `secondary execute after ...`

These are sampled like the existing primary runtime logs: they emit on secondary proc/animation pointer changes and every 30 samples otherwise. The intent is to answer the next narrow question without opening a broad control path yet:

- Does secondary ALINK advance its own runtime state at all?
- Does its base animation frame/rate move?
- Does player 1 remain visually and behaviorally clean once secondary execute is admitted under explicit ownership scoping?

Manual test sequence:

1. Start from the current visible-P2 harness.
2. Spawn the secondary prototype and confirm both actors are visible and player 1 still behaves normally.
3. Leave `Scoped execute model data owner` checked.
4. Uncheck `Skip execute`.
5. Observe whether player 2 remains stable/visible and whether player 1 regresses.
6. Capture `secondary execute` and `primary runtime` logs together so proc/animation movement can be compared directly.

First scoped-execute result:

- User tested on 2026-05-12 with the visible-P2 harness, then unchecked `Skip execute` after the secondary actor had spawned.
- Player 2 remained visible and began playing idle animations correctly.
- Player 2 also performed the same target/shield-raise animation when player 1 activated it.
- Interpretation: secondary ALINK execute is now partially viable under scoped model-data ownership. The immediate remaining coupling is likely input/action ownership or shared player status for target/shield actions, not basic create/draw animation ownership.
- Next question: does secondary execute read primary controller/action state directly, or does a global target/shield status bit drive both actors?

Next narrow audit target:

1. Inspect shield/target action paths in `d_a_alink.cpp`.
2. Classify each relevant read/write as direct controller input, global player status, attention/camera state, or current-actor state.
3. Add the smallest log or toggle that distinguishes "P2 reads P1 input" from "P2 mirrors a shared action/status bit".
4. Do not route real P2 input until the mirror source is identified.

Current target/shield mirror diagnostic:

- `setStickData()` already uses `dusk::coop::readInputForActor(this)` for ALINK stick and item button snapshots, including `BTN_R`.
- The next suspect is therefore either a remaining hard-coded `PAD_1` path near target/shield behavior, or global attention/UI status such as `checkAttentionLock()` / `dComIfGp_getRStatus()`.
- The scoped execute probe now emits `secondary action-mirror ...` logs whenever P2's relevant action state changes.
- Each line compares P2's derived ALINK state with raw P1/P2 lock and trigger input:
  - `inputR`: P2 `checkInputOnR()`.
  - `atnLock`: P2 `checkAttentionLock()`.
  - `itemBtnR` / `itemTrigR`: P2's ALINK item button state.
  - `p1LockR` / `p1TrigLockR`: raw primary slot lock-R input.
  - `p2LockR` / `p2TrigLockR`: raw secondary slot lock-R input.
  - `p1BtnR/L/Z` and `p2BtnR/L/Z`: raw held trigger-button bits.
  - `rStatus`: global R-button status.

The first manual run only produced the clean baseline line, so the diagnostic was widened to include raw held R/L/Z button bits in addition to lock-R. This should catch the actual P1 shield/target activation even if it does not travel through `getHoldLockR()`.

The action-mirror diagnostic now has an optional structured recorder. In the Actor Spawner's co-op section, enable `Record action mirror diagnostics`, reproduce the scoped-execute test, then press `Flush diagnostics`. Dusk writes JSON artifacts under the config path's `diagnostics/` directory:

- `latest/manifest.json`, `latest/latest.json`, and `latest/events.jsonl`.
- `sessions/<session-id>/local/manifest.json`, `latest.json`, and `events.jsonl`.

The recorder captures `scene.current`, `render.stats`, `player.slots`, `input.pad`, `coop.probes`, and `alink.secondary` using a stable event envelope. V1 still uses actor pointers as metadata rather than stable actor IDs, so do not use pointer values as long-lived identity across separate captures.

How to read the next result:

- If `p2HoldR` is false but `itemBtnR` or `inputR` becomes true, P2 is still receiving P1 input somewhere in ALINK's input path.
- If `p2HoldR` and `itemBtnR` stay false but `atnLock`, `target`, or `rStatus` changes with P1, the mirror is likely shared attention/status state.
- If P2 only mirrors when both `p2HoldR` and P2 derived state become true, then input routing is probably behaving and the remaining issue is that both actors are intentionally seeing the same world/attention context.

Structured diagnostic result:

- The 2026-05-15 captures show P1's target/shield input changing while P2 raw input remains zero.
- During the same window, P2's `inputR`, `itemBtnR`, `itemTrigR`, `target`, and `rStatus` stay false/zero, but P2 `checkAttentionLock()` flips true.
- `checkAttentionLock()` is an inline wrapper around `mAttention->Lockon()`, and ALINK initializes `mAttention` from the global `dComIfGp_getAttention()` object. This confirms the target/shield mirror is shared attention state, not raw P2 input leakage.
- Do not "fix" this by forcing secondary `checkAttentionLock()` false by default. That would hide the coupling we need to understand for real co-op state decoupling. A secondary-only isolation toggle is acceptable only as an explicitly named diagnostic control if a later test needs to quarantine this symptom.

Next narrow audit target:

1. Add `attention.state` diagnostics around the global attention object: lock-on state, lock-on target, action/check-object lists if available, and the current player slot/actor context that sampled it.
2. Audit ALINK reads of `checkAttentionLock()`, `mTargetedActor`, `mAttention`, and global player status around target/shield, guard, side-step, and attention movement.
3. Classify each callsite as "must become per-player", "global camera/UI state", or "safe shared world query".
4. Preserve the visible shield mirror as a known symptom until a real per-player attention/status path exists.

## Non-Goals

- Do not build a full proxy/replica Link actor as the next default path.
- Do not resize `dComIfG_play_c` player/camera/status arrays in this audit.
- Do not mass-replace `dComIfGp_getPlayer(0)` or `daPy_getPlayerActorClass()`.
- Do not add networking.
- Do not introduce a broad co-op framework or deep directory tree.
- Do not remove the committed player-slot registry or input snapshot milestones.

## Proxy Actor Position

A proxy/replica actor remains a fallback or temporary debug tool, not the preferred architecture yet.

Use a proxy if:

- ALINK animation/model ownership cannot be isolated without destabilizing the game.
- ALINK's core state machine proves inseparable from player 0 globals.
- A narrow visual stand-in is useful while auditing.

Do not choose a proxy merely because the first naive ALINK duplication failed. The project should first make a serious, evidence-backed attempt to crack the singleton/shared-state problem so it can reuse as much real Link behavior as possible.

## Helper Strategy

Helpers are still the right tool, but only after callsite meaning is known.

Already useful:

- `dusk::coop::registerPlayer(...)`
- `dusk::coop::unregisterPlayer(...)`
- `dusk::coop::getPlayer(...)`
- `dusk::coop::getPrimaryPlayer()`
- `dusk::coop::isPrimaryPlayer(...)`
- `dusk::coop::isSecondaryPlayerPrototype(...)`
- `dusk::coop::captureInputSnapshot(...)`

Potential future helpers, only when a plan needs them:

- `getPlayerForActorContext(...)`
- `getNearestPlayer(...)`
- `getInteractingPlayer(...)`
- `getCameraPlayer(...)`
- `withPlayerSlotContext(...)`

Do not add these speculatively.

## Validation

Manual validation is expected until the repository has a test harness.

For each diagnostic patch:

1. Build using the user's Visual Studio MSVC flow.
2. Boot a normal save.
3. Confirm P1 control and animation before spawning secondary ALINK.
4. Spawn `Spawn Secondary Link Prototype`.
5. Move, stop, shield/block, and attack with P1.
6. Record `dusk::coop` and `dusk::coop.alink` log excerpts in the relevant plan.
7. Update this audit if the evidence changes the likely culprit.

## Recovery Notes

If a diagnostic breaks startup or primary play, remove only the active diagnostic changes. Keep the committed player-slot and input-snapshot milestones unless the failure directly implicates them.

If the secondary ALINK prototype itself becomes too disruptive, remove or disable the Actor Spawner button and keep this audit as the durable record of why the path paused.
