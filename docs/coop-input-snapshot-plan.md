# Co-op Input Snapshot Plan

This plan covers the second co-op milestone: route one concentrated Link input cluster through a slot-aware input snapshot while preserving current single-player behavior.

## Purpose

Add a small Dusk-owned input snapshot for player slots. For this milestone, primary player input must still come from `PAD_1`, and all observed single-player controls should behave the same.

This is not a general input rewrite, not network input, and not a second-player control implementation. It only creates the command-shaped input surface needed before local co-op or networking.

## Assumptions

- `dusk::coop::PlayerSlot::Primary` maps to `PAD_1`.
- `dusk::coop::PlayerSlot::Slot1` maps to `PAD_2`, but no secondary actor consumes it in this milestone.
- Unknown actors fall back to primary input for now. This keeps current singleton-shaped call sites stable.
- The input snapshot should wrap `mDoCPd_c` reads, not replace the low-level controller system.
- The first conversion should stay inside one `src/d/actor/d_a_alink.cpp` input cluster.

## Current Code Evidence

- `include/m_Do/m_Do_controller_pad.h` defines controller ports `PAD_1` through `PAD_4`, with `m_gamePad[4]` and `m_cpadInfo[4]`.
- `include/m_Do/m_Do_controller_pad.h` already exposes the exact reads this milestone needs: stick value/angle, trigger buttons, hold buttons, and lock-R trigger/hold state.
- `src/d/actor/d_a_alink.cpp` reads movement stick input directly through `mDoCPd_c::getStickValue(PAD_1)` and `mDoCPd_c::getStickAngle3D(PAD_1)` around the movement/item input cluster.
- The same cluster sets `mItemTrigger` from `getTrigB/A/X/Y/Z/L/LockR(PAD_1)` and `mItemButton` from `getHoldA/B/X/Y/Z/L/LockR(PAD_1)`.
- Other direct `PAD_1` reads exist nearby and later in the file, including debug and special-case controls. Those are intentionally out of scope until a milestone needs them.
- The first converted PC path now reads a `dusk::coop::PlayerInputState` for `this` inside `daAlink_c::setStickData()`. The original `PAD_1` path remains in `#else`.

## Files

Expected edits:

- `docs/coop-input-snapshot-plan.md`
- `include/dusk/coop/input.h`
- `src/dusk/coop/input.cpp`
- `src/d/actor/d_a_alink.cpp`
- `files.cmake`

Do not edit unrelated input paths in this milestone.

## Implementation

1. Add a `dusk::coop::PlayerInputState` struct with only the fields needed by the first Link input cluster.
2. Add `readLocalInput(PlayerSlot slot)` that maps the slot to a local pad through `getPadForSlot()`.
3. Add `readInputForActor(const fopAc_ac_c* actor)` that resolves the actor's slot with the player slot registry and falls back to primary when unknown.
4. Convert the concentrated `d_a_alink.cpp` cluster that currently reads:
   - `getStickValue(PAD_1)`
   - `getStickAngle3D(PAD_1)`
   - item trigger buttons
   - item hold buttons
5. Keep fish-rod stick input behavior unchanged; that path reads from the rod actor, not directly from `PAD_1`.
6. Add only low-volume debug logging if it helps prove slot-to-pad mapping, and avoid per-frame logs by default.

## Progress

- [x] Confirmed the first target input cluster in `src/d/actor/d_a_alink.cpp`.
- [x] Confirmed the low-level pad API already supports multiple local pads.
- [x] Add the input snapshot API.
- [x] Convert only the first Link input cluster.
- [x] Update `files.cmake` with the new input files.
- [x] Build with the Visual Studio MSVC debug preset. User-owned unless explicitly delegated to Codex.
- [x] Manually validate primary movement and item buttons.

## Decisions

- Do not mass-replace `PAD_1`.
- Do not touch debug movement controls around the later Dusk debug block in this milestone.
- Do not serialize input yet. The snapshot shape should make serialization possible later without adding network code now.
- Keep the snapshot data plain and small. Add fields only when the converted cluster needs them.
- Preserve mirror-mode behavior by reading stick angle through the same `mDoCPd_c::getStickAngle3D()` path.
- Store normal trigger/hold buttons as raw pad bitfields plus explicit lock-R fields. This keeps the snapshot small while preserving the cluster's old `getTrig*`, `getHold*`, and lock-R behavior.
- Do not add input logging in this milestone. The converted call runs every frame, so logging here would be noise unless a later debugging plan adds sampling or gating.

## Validation

Build command from `docs/building.md`, from this repo root on Windows. The user owns this build/manual validation step unless they explicitly delegate it to Codex:

```sh
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

Expected behavior:

- Primary player still moves from controller port 1.
- Stick angle and mirror-mode behavior match the old path.
- A, B, X, Y, Z, L, and lock-R trigger/hold behavior match the old path.
- Fishing rod stick input still comes from the rod actor path.
- No secondary player input is visible yet.

Manual scenario after a successful build:

- Boot a normal save.
- Move Link with the primary stick.
- Press and hold the item/action buttons used by the converted cluster.
- Toggle mirror mode if practical and confirm stick direction is still correct.
- Confirm no new co-op UI, second actor, or network behavior appears.

Validation performed:

- User built the Visual Studio MSVC debug configuration after the input snapshot patch.
- User reported no regressions and player 1 controls working as expected.
- Runtime logs continued to show healthy primary slot lifecycle behavior: one primary actor registered/refreshed/unregistered, then a later primary actor registered/refreshed. No replacement or ignored-unregister messages were reported.

## Recovery Notes

If the snapshot causes a build or runtime issue, remove only these changes:

- `include/dusk/coop/input.h`
- `src/dusk/coop/input.cpp`
- the input snapshot call sites in `src/d/actor/d_a_alink.cpp`
- the `input` entries in `files.cmake`

Do not alter the existing player slot registry as part of recovery unless the failure directly involves slot lookup.
