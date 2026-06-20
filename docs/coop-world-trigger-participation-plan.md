# Co-op World-Trigger Participation Plan

## Purpose

Make every registered player eligible for classified authored proximity/volume triggers without
changing canonical Link getters, replaying trigger actors, or replacing native event, switch,
timer, dialogue, and scene state machines. Trigger eligibility, scripted subject ownership, and
presentation ownership remain three independent decisions.

## Progress

- [x] Add the `dusk::coop::world_trigger` participation and retained-activation sidecar.
- [x] Add `world.trigger` latest snapshots and semantic accept/release diagnostics.
- [x] Migrate `SwAreaC/S`, `TAG_EVENT`, and `TAG_EVT` through native predicates.
- [x] Convert the first rolling Goron, Darknut opening, and Shadow Kargarok grass-call gate.
- [x] Add function-scope source scanner and pass all six required anchors.
- [x] Update project maps, audits, and narrow hook reminders.
- [x] Field-validate the corrected first-Goron triggering-subject camera, charge, catch, and throw lifecycle.
- [x] User build validation through the corrected first-Goron retest build.
- [ ] User field validation across P1/P2/simultaneous activation and camera teardown paths.
- [x] Commit authorized after successful first-Goron retest.

## Policy Matrix

| Producer | Activation | Scripted subject | Presentation |
| --- | --- | --- | --- |
| `SwAreaC/S` | any active player or slot-owned horse | P1/global | no trigger-owned collapse |
| `TAG_EVENT` | any active player | P1 | no trigger-owned collapse |
| `TAG_EVT` | any active player | P1 | no trigger-owned collapse |
| First rolling Goron | any active player | triggering player through the complete duel | triggering-player fullscreen during the accepted opening |
| Darknut proximity opening | any active player | P1 | P1 fullscreen after acceptance |
| Shadow Kargarok `ACT_TKUSA` intro | any active player | P1 | P1 fullscreen after acceptance |

When several players qualify in the same simulation tick, slot order wins: P1 first, then the
lowest additional slot. A P2-only generic/Darknut/Shadow-Kargarok match is accepted immediately
and retained separately from P1. The first Goron is the classified exception: its opening camera,
continuation ranges, charge, grapple/collision result, and retry states all consume the same Link,
so the triggering player must remain the scripted subject until native duel completion.

## Implemented Architecture

`world_trigger::evaluate()` enumerates registered player slots and applies one actor-supplied
`PlayerQueryEligibility` predicate to every candidate. Conditions such as form, equipment, horse,
status, vertical bounds, and volume geometry stay in that predicate so an ineligible P1 cannot
mask an eligible P2. `accept()` runs only at the native state transition and retains source/player
pointers plus process IDs, slot, policy, candidate masks, and optional event/switch metadata.

`release()` and `clearSource()` end any keyed `EnemyAuthoredDemo` presentation claim and preserve a
semantic transition record. Actor deletion clears source state; player unregistration clears every
activation retained by that player. Classified triggering-subject dialogue resolves through the
retained state without modifying `event_owner`, event `Pt1`, canonical player getters, or
event-manager state.

The generic trigger families preserve their original downstream behavior:

- `SwAreaC/S` keeps native latch/reset/off/one-shot and event-follow-up types. Horse-only volumes
  test each slot's registered horse while retaining the owning player slot.
- `TAG_EVENT` evaluates players only after room, switch, and event-bit arrival terms pass, then keeps
  chained map events and canonical Epona rodeo behavior unchanged.
- `TAG_EVT` evaluates players only after native delete/timer terms and only while its event selector
  is idle, preventing duplicate orders while keeping talk and scene-change choreography P1-authored.

The first Goron repeats the exact fly/climb/status and radial/area predicate for each player, then
retains the accepted player through opening dialogue/camera, charge, collision, carry/throw camera,
and retries. The opening snapshots, drives, and restores that slot's camera; only its accepted
opening is fullscreen. The earlier P2 refusal in `procGoatCatchInit()` has been removed so the
retained subject can enter native `PROC_GOAT_CATCH`, establish carrier ownership, and drive the
existing catch/left-right throw callbacks. Ownership releases at the native successful-duel event
bit or actor deletion.
This wider subject lifetime is required because field testing proved that keeping P1 as subject
started Camera 0 from a remote position and let P1's out-of-range charge abort/retrigger forever.

Darknut repeats the original world-space annulus. Shadow Kargarok uses the original full-3D `< 1300`
grass-call range independently of sticky Combat targeting; the separate `BOW_IKKI2` `z > 35000`
setpiece remains untouched. Those two proofs retain P1 choreography and begin P1 fullscreen only
after native demo acceptance, releasing after native camera reset/start, skip, or actor deletion.

## Audit and Diagnostics

The `world.trigger` provider keeps current candidate decisions and active retained states in
`latest.json`. `events.jsonl` receives only bounded accept/release records with source profile/room,
triggering slot, policies, candidate/eligible masks, metadata, and release reason.

Run the source audit with:

    python tools/audit_world_triggers.py --min-score 3

The scanner strips comments/literals, bounds matches to C++ function definitions, and ranks direct
canonical-player reads, cached player-distance reads, `world_trigger` participation, event orders,
switch writes, and demo transitions. It marks converted functions without hiding them. The milestone
validation command additionally requires anchors for `SwArea`, both generic tags, and all three
actor-local proofs.

## Validation

The user owns CMake/Visual Studio builds and runtime testing. Field validation must cover:

1. P1-only, P2-only, and simultaneous entry for representative cylinder/box tags and conditioned
   switch volumes, including a slot-owned horse volume.
2. P2 activation of the first Goron with P2 supplying the opening camera seed, dialogue input,
   charge/grapple subject, sustained `PROC_GOAT_CATCH`, left/right throw input, collision result,
   and retry state through native duel completion.
3. P2 activation of Darknut and Shadow Kargarok while P1 remains their manipulated/scripted Link.
4. Triggering-player fullscreen for the accepted Goron opening, P1 fullscreen for the other two
   Camera-0 demos, and split-screen restoration after normal completion, skip, and actor teardown.
5. No duplicate event orders, timer advancement, switch mutations, talk starts, or scene changes.
6. Single-player behavior and the existing Shadow Bulblin carry-owner demo modes 1-4 remain intact.

## Decisions and Boundaries

- Do not override `daPy_getPlayerActorClass()` or `dComIfGp_getPlayer(0)` globally.
- Do not execute a trigger actor once per player.
- Generic trigger activation does not imply triggering-player subject or fullscreen presentation.
- If a native event's camera seed and continuation/termination checks consume the same Link, retain
  that triggering subject through the whole authored interaction rather than only its opening gate.
- The Darknut change covers the proximity opening, not room/change/ending ownership.
- The Shadow Kargarok change covers `ACT_TKUSA`; other bridge/field/`BOW_IKKI2` gates stay authored.
- No commit is authorized until the user reports a successful build and field test.
