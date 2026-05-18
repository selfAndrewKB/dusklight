# Co-op Secondary ALINK Input Routing Plan

This milestone is complete. It moved the secondary ALINK prototype from "a second ALINK can exist under containment" to "a second ALINK consumes player 2 input for basic movement and simple actions."

The next completed milestone was `docs/coop-secondary-alink-item-ownership-plan.md`; the current active milestone is `docs/coop-native-split-screen-camera-plan.md`.

## Purpose

Route the secondary ALINK prototype toward slot 1 input in the smallest useful slice, while preserving player 1 behavior and the known containment fixes:

- scoped shared `J3DModelData` ownership;
- primary-only player/global ownership writes;
- secondary isolation from P1's shared `dAttention_c::Lockon()` state.

This milestone is not full combat, item use, camera, UI, targeting, interaction, or networking. The first visible win should be boring: P2 moves from controller 2 without breaking P1.

## Current Evidence

- Secondary ALINK can be spawned with actor `argument == -2` and registered as sidecar player slot 1.
- P2 can render visibly when shared Link model-data ownership is scoped to the secondary during create-time animation/model setup, draw, and execute, then restored to P1.
- P1's earlier animation lock was caused by shared `J3DModelData` matrix-calculator ownership, not by input/action state.
- Scoped secondary `execute()` is partially viable: P2 can idle animate while P1 remains stable.
- P2 shield/target pose mirroring was caused by secondary `checkAttentionLock()` consuming P1's shared global `dAttention_c::Lockon()` state.
- With `Ignore shared attention lock` enabled, P2 no longer mirrors P1's shield/target pose.
- `dusk::coop::readInputForActor(this)` already exists and the first ALINK input cluster uses slot-aware snapshots for stick and item button state.
- Diagnostics can distinguish `input.pad`, `attention.state`, `player.status`, `coop.probes`, and `alink.secondary`.
- Secondary slot input already maps to `PAD_2` through `dusk::coop::getPadForSlot(PlayerSlot::Secondary)`.
- `alink.secondary` now refreshes from the secondary execute probe, so locomotion tests can compare P2 pad input, secondary stick/move values, speed, angles, and position without depending on R/attention changes.
- User validation confirmed the second controller moved the spawned Link. Rolling worked, and a basic combat swing worked.
- Item/action ownership remains P1/global in several paths: pulling out the fishing hook made it invisible in P2's hands and visible on P1, and throwing the boomerang caused P1 to catch it and blocked P2 from throwing again.
- After this milestone, `Skip execute` is no longer part of the default secondary ALINK probe set. It remains available only as a recovery/debug checkbox.

## Working Assumptions

- P1 remains the compatibility oracle. If P1 regresses, stop and classify the shared-state write/read before expanding scope.
- P2 input routing should start with basic movement and facing. Combat/action routing comes after locomotion is stable.
- `checkAttentionLock()` is the first confirmed per-player attention helper candidate, but the current secondary-only containment probe remains acceptable during this milestone.
- Global `dAttention_c`, HUD prompts, camera state, story state, and save/event state remain P1-owned unless a later plan proves otherwise.
- Do not mass-replace `PAD_1` or every `checkAttentionLock()` call. Convert only callsites required by the current test.
- Co-op edits in original/decomp code need concise `Co-op:` why-comments.

## Implementation Plan

1. Confirm the exact ALINK input surface already covered by `readInputForActor(this)`.
   - Verify movement stick value/angle and item button snapshots are actor-slot-aware in `setStickData()`.
   - List remaining direct `PAD_1` reads near movement, guard, target, and action decisions.
2. Add diagnostics only if the current providers cannot answer the next test.
   - Prefer using existing `input.pad`, `player.status`, `attention.state`, and `alink.secondary`.
   - Keep `events.jsonl` semantic; do not add frame-churn event keys.
3. Test secondary execute with current default containment and P2 controller input.
   - Default probes should include scoped model-data ownership and `Ignore shared attention lock`.
   - Spawn P2, uncheck `Skip execute`, move P2 stick, and flush diagnostics.
4. If P2 does not move, inspect the blocked layer:
   - input snapshot not reaching secondary ALINK;
   - action/proc gate still reading P1/global status;
   - movement accepted but overridden by attention/camera/status state;
   - model/animation updates running but physics/current position not changing.
5. Convert the smallest next callsite only after the diagnostic identifies it.
   - Prefer a semantic helper when at least two callsites mean the same thing.
   - Keep P1 behavior identical.
6. Once P2 moves, freeze the minimal locomotion baseline in docs before adding combat/actions.

## Progress

- [x] P2 visible under scoped model-data ownership.
- [x] P2 execute can be enabled without reintroducing P1 animation lock.
- [x] P2 no longer mirrors P1 shield/target pose with shared attention lock ignored.
- [x] Structured diagnostics can separate input, attention, status, and secondary ALINK state.
- [x] Audit the first basic-locomotion input path and confirm secondary slot reads `PAD_2`.
- [x] Expand `alink.secondary` diagnostics to report execute-fed locomotion state.
- [x] Run the first P2-controller locomotion test with secondary execute enabled.
- [x] Validate P2 receives independent controller 2 movement input.
- [x] Validate basic P2 rolling and combat swing behavior.
- [x] Classify the next blocker as item/action ownership, not basic input routing.

## Test Plan

The user owns Visual Studio/CMake builds unless explicitly delegated to Codex.

Manual test sequence:

1. Build with Visual Studio MSVC debug.
2. Boot a save and confirm P1 baseline behavior.
3. Open Actor Spawner and reset secondary ALINK probes to `Default`.
4. Enable the diagnostics profile if a capture is needed.
5. Spawn `Spawn Secondary Link Prototype`.
6. Confirm P2 is visible and P1 still animates normally.
7. Confirm `Skip execute` is unchecked under the default probe set.
8. Move controller 2's stick and observe whether P2 moves, turns, or changes proc/animation state.
9. Move P1 separately and confirm P1 still behaves normally.
10. Flush diagnostics and inspect `input.pad`, `alink.secondary`, `player.status`, and `attention.state`.

For the next test, the key fields are:

- `input.pad.p2.stick_value`: proves controller 2 is being read.
- `alink.secondary.stick_value` and `move_value`: proves secondary ALINK consumed the slot snapshot.
- `alink.secondary.speed_f` and `pos`: proves movement made it past input/proc gates into actor motion.
- `alink.secondary.proc` and `anim`: show whether ALINK entered a movement proc or only animated in place.

Expected first big win:

- P2 responds to controller 2 for basic movement without P1 animation, attention, or status regressions.

Observed:

- P2 responded to controller 2 for movement.
- P2 rolling and basic combat swing worked.
- Fishing hook and boomerang exposed item actor/global ownership hazards that still route visible item state or item return state through P1.

Acceptable partial result:

- Diagnostics prove where P2 input is blocked, with no P1 regression.

Failure conditions:

- P1 animation lock returns.
- P2 consumes P1 input again.
- Global attention/status changes force P2 action state despite clean P2 input.
- P2 movement corrupts camera, UI, event, or save-owned state.

## Cleanup Notes

Track these once basic locomotion works:

- Decide which secondary ALINK probe flags graduate into normal containment.
- Rename `Ignore shared attention lock` if it becomes a real per-player helper.
- Reduce old `dusk::coop.alink` checkpoint logging once diagnostics artifacts cover the same facts.
- Move the Actor Spawner secondary ALINK harness into a dedicated co-op debug panel if it survives beyond this milestone.
- Archive completed audit details if `docs/coop-alink-duplication-audit-plan.md` becomes too bulky for active use.
