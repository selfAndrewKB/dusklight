# Co-op Player Slots Plan

This plan covers the first co-op implementation milestone: add explicit player-slot identity without changing single-player behavior.

## Purpose

Add a tiny Dusk-owned player slot registry. For this milestone, slot 0 must remain the existing Link actor and the game must continue to behave exactly like vanilla single player.

This is intentionally not a second-player spawn, networking layer, camera rewrite, or input rewrite. Those belong in later plans after this vocabulary exists.

## Assumptions

- Slot 0 is the primary player and maps to the actor currently stored by `dComIfGp_setPlayer(0, this)`.
- Slot 1 may exist in the API but is empty in this milestone.
- The registry is a sidecar. It does not own actor lifetime, save data, heaps, or scene state.
- Original game code changes must be clearly Dusk-specific and fenced with `#if TARGET_PC` where practical, per `docs/code-conventions.md`.
- No original game structs are resized in this milestone.

## Current Code Evidence

- `src/d/actor/d_a_alink.cpp` registers Link globally during create with `dComIfGp_setPlayer(0, this)` and `dComIfGp_setLinkPlayer(this)`.
- `src/d/actor/d_a_alink.cpp` clears those globals during delete with `dComIfGp_setPlayer(0, NULL)` and `dComIfGp_setLinkPlayer(NULL)`.
- `include/d/d_com_inf_game.h` has one player/camera/status slot in `mPlayerInfo[1]`, `mCameraInfo[1]`, and `mPlayerStatus[1][4]`.
- `include/d/actor/d_a_player.h` defines `daPy_getPlayerActorClass()` as `dComIfGp_getPlayer(0)`.
- `files.cmake` explicitly lists Dusk source files, so any new C++ file must be added there.

## Files

Expected edits:

- `docs/coop-player-slots-plan.md`
- `include/dusk/coop/player_slots.h`
- `src/dusk/coop/player_slots.cpp`
- `src/d/actor/d_a_alink.cpp`
- `files.cmake`

## Implementation

1. Add `dusk::coop::PlayerSlot` with `Primary`, `Secondary`, and `Invalid`.
2. Add a small fixed-size registry for two player actor pointers.
3. Add helpers for slot count, slot registration, unregister-if-matching, player lookup, primary-player lookup, actor-to-slot lookup, and local controller port lookup.
4. Register primary Link next to the existing `dComIfGp_setPlayer(0, this)` call.
5. Unregister primary Link before clearing the existing global player pointers, and only clear the registry if the deleting actor matches the registered actor.

## Progress

- [x] Read `AGENTS.md`, `docs/building.md`, `docs/code-conventions.md`, and `docs/co-op-roadmap.md`.
- [x] Confirmed the existing Link create/delete singleton hook points.
- [x] Add the slot registry API.
- [x] Hook primary Link registration and unregistration.
- [x] Validate source references and attempt a build check.

## Decisions

- Use a Dusk sidecar under `include/dusk/coop` and `src/dusk/coop` rather than resizing `dComIfG_play_c`.
- Keep the first API concrete and fixed to two slots. More slots can be added later only when a real milestone needs them.
- Return `PlayerSlot::Invalid` for unknown actors instead of silently treating every actor as primary.
- Keep `getPadForSlot()` small: primary maps to `PAD_1`, secondary maps to `PAD_2`, invalid falls back to `PAD_1`.
- Mark original-code hooks with `Co-op:` comments so future readers can identify why Dusk is touching decomp code.
- Use Dusk's existing logging path for low-volume co-op diagnostics: `aurora::Module` via `dusk/logging.h`. The original `OS_REPORT` path is also available and routed through `src/dusk/OSReport.cpp`.

## Discoveries

- The co-op registry logs primary slot register/unregister events through module `dusk::coop`.
- The work now lives in `dusk-coop`, a clone of the GitHub fork with real Dusk history.
- `extern/aurora` is present in this clone because it was cloned with submodules.
- A Visual Studio MSVC debug build succeeded. Runtime logs showed primary Link registration firing several times and unregister firing during normal game execution. The registry logging now distinguishes refresh, replacement, matched unregister, and ignored unregister mismatch so lifecycle behavior can be read directly from logs.
- Follow-up runtime logs showed slot 0 registering once, refreshing the same actor several times, then unregistering the same actor. A later Link actor repeated the same register/refresh pattern. No replacement or ignored-unregister messages appeared, so the sidecar registry is tracking the vanilla Link lifecycle cleanly.
- The slot-registry patch is committed locally on branch `co-op` as `75496d4daa Add co-op player slot registry`. Remote push is intentionally deferred.

## Validation

Build command from `docs/building.md`, from the Dusk root on Windows:

```sh
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

Expected behavior:

- The project builds.
- Existing single-player startup and Link control are unchanged.
- After Link create, `dusk::coop::getPrimaryPlayer()` returns the same actor that vanilla code registered through `dComIfGp_setPlayer(0, this)`.
- During Link delete, the registry clears primary only when the deleting actor matches the registered actor.

Manual scenario after a successful build:

- Boot a normal save or new game.
- Confirm player movement and basic buttons still work through controller port 1.
- Load or transition once so Link delete/create paths are exercised.
- Confirm no new co-op UI, secondary actor, camera change, save change, or input remapping appears.

Validation performed:

- Source references were checked with `rg`.
- Configure/build validation still needs to be rerun from `dusk-coop` after porting this patch onto the fork clone.

## Recovery Notes

If the registry causes a build or runtime issue, remove only these changes:

- `include/dusk/coop/player_slots.h`
- `src/dusk/coop/player_slots.cpp`
- the two `dusk::coop` hook blocks in `src/d/actor/d_a_alink.cpp`
- the `player_slots` entries in `files.cmake`

Do not alter existing `dComIfGp_setPlayer`, `dComIfGp_setLinkPlayer`, player arrays, camera arrays, or `daPy_getPlayerActorClass()` as part of recovery.
