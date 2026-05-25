# P2 Independent Control Ownership Plan

## Summary

This milestone replaces the old secondary-ALINK `Ignore shared attention lock` probe with real
co-op ownership for player-facing control state. The goal is to let P2 target, guard/block,
first-person aim, use item camera modes, interact with eligible prompts, and participate in Hidden
Skill-style training without borrowing P1's attention, button status, camera status, or event
player.

This is not a global replacement of `dComIfGp_getPlayer(0)`, `daPy_getPlayerActorClass()`, or
`dComIfGp_getAttention()`. Each callsite still needs to be classified by the question it asks.

## Current Evidence

- P2 raw controller input is already routed through `dusk::coop::readInputForActor(this)` in
  `daAlink_c::setStickData()`.
- P2's old shield/target pose mirroring was not raw input leakage. Diagnostics showed P2's derived
  item/button state stayed clean while `checkAttentionLock()` followed the shared global
  `dAttention_c::Lockon()` result.
- The former `SecondaryAlinkProbe_IgnoreSharedAttentionLock` flag made P2 return false from
  `checkAttentionLock()`. That was useful containment, but it also prevented real P2 lock-on/guard
  semantics, so it is now superseded by `player_attention`.
- Bow/slingshot projectile ownership is partially fixed, but secondary bow/sling status currently
  skips the global camera/status bits. That avoided P1 camera hijacking, but it leaves real P2
  first-person/item camera ownership unfinished.
- Hidden Skill code (`NPC_KN`) is P1-heavy: it reads `daPy_getPlayerActorClass()` for cut type,
  side-step, sword state, position, training flags, and forced player placement. The training owner
  should be retained once a lesson starts.
- Howling-stone/tag entry points still check P1/wolf state directly through
  `daAlink_getAlinkActorClass()`, `daPy_getPlayerActorClass()`, and `dComIfGp_getPlayer(0)`.

## Current Implementation

- `player_attention` V1 owns a slot-local `dAttention_c` for additional ALINK actors while P1 keeps
  the global `dComIfGp_getAttention()` path.
- `daAlink_c::setAtnList()` updates and binds `mAttention` through `player_attention` before ALINK
  derives `mTargetedActor`, mark state, and guard/target facts.
- The legacy `Ignore shared attention lock` probe is no longer part of default secondary ALINK
  behavior or the Actor Spawner UI. It was a containment switch, not the final ownership model.

## API Shape

### `player_attention`

Owns slot-local attention facts for ALINK gameplay:

- lock held/triggered for this slot;
- current lock target for this slot;
- lock truth/release state for this slot;
- action prompt candidates visible to this slot;
- whether this slot should draw or consume the lock cursor.

V1 can be a sidecar wrapper around the existing global attention scan, but it must not expose P1's
`dAttention_c::Lockon()` as P2's answer. P1 can continue to use global `dAttention_c` for camera,
HUD, and vanilla compatibility.

### `player_button_status`

Owns per-slot Do/R/Z status when ALINK gameplay asks "what action is available to me?".

The existing global `dComIfGp_getDoStatus()`, `dComIfGp_getRStatus()`, and `dComIfGp_getZStatus()`
should remain P1/HUD status until the HUD is expanded. P2 gameplay should read a slot-local answer,
while P1 still writes the global meter state as before.

### `player_camera_status`

Owns per-slot camera/action status such as bow, slingshot, Hawkeye, iron ball subject mode,
hookshot subject mode, and camera attention flags.

This should build on the existing `dusk::coop::camera` split-screen sidecar. P2 first-person and
item aiming should use camera 1 and slot 1 status, not global player-status bits that drive P1's
camera/HUD.

### `interaction_owner`

Owns "which player is using this prompt or object?" for talk, inspect, pick up, climb/enter, howl,
and similar action-button interactions.

This is separate from enemy targeting and item ownership. It should be used when the world prompt
itself is the owner, especially for NPC/object interactions and howl tags.

### `training_owner`

Owns retained training sequences such as Hidden Skills. Once a trainer starts a lesson against a
slot, cut-type checks, forced position/angle changes, required action checks, and completion state
should talk to that same player slot until the lesson ends.

Global save bits for learned skills can remain shared campaign state in V1. The actor/player being
trained should not be forced to P1.

## Priority Order

1. Promote `checkAttentionLock()` from a probe into `player_attention`.
   - Keep P1 behavior identical.
   - Replace the secondary-only false result with a semantic slot-local answer.
   - Route `setAtnList()` through the same owner so P2 can have `mTargetedActor` again.

2. Add per-slot action/R/Z status reads for ALINK gameplay.
   - Start with guard/block and side-step/roll branches that currently read
     `dComIfGp_getDoStatus()` or `dComIfGp_getRStatus()`.
   - Keep global meter writes P1-owned until a HUD milestone exists.

3. Convert first-person/item aiming status.
   - Bow/slingshot: replace the current secondary status skip with slot-owned camera/status.
   - Hawkeye: make scope mode a slot-local flag instead of `dComIfGp_checkPlayerStatus0(0, 0x200000)`.
   - Iron ball and hookshot subject modes: route subject camera/status to the owning slot/camera.

4. Convert basic interaction prompts through `interaction_owner`.
   - Begin with howl tags/stones because they are small and visibly P1-owned.
   - Then audit talk/check/pickup prompt reads in `setAtnList()`, `orderTalk()`, and normal action
     entry.

5. Convert Hidden Skills through `training_owner`.
   - Bind `NPC_KN` to the player that initiated the lesson.
   - Route training cut checks, side-step checks, forced placement, and training flags to that slot.
   - Leave save/event completion shared unless a later milestone needs per-slot skill progress.

## Hot Files

| Area | Files | Notes |
| --- | --- | --- |
| Attention core | `src/d/d_attention.cpp`, `include/d/d_attention.h` | `Run()` still sets `mpPlayer = dComIfGp_getPlayer(0)` and `mPadNo = PAD_1`. |
| ALINK target/guard/status | `src/d/actor/d_a_alink.cpp`, `src/d/actor/d_a_alink_guard.inc` | `checkAttentionLock()`, `setAtnList()`, `checkGuardActionChange()`, `checkNormalAction()`, `checkMoveDoAction()`. |
| First-person and item aim | `src/d/actor/d_a_alink_bow.inc`, `src/d/actor/d_a_alink_ironball.inc`, hookshot code in ALINK, `src/d/actor/d_a_arrow.cpp` | Bow/slingshot ownership exists, camera/status ownership does not. |
| Interaction prompts | `src/d/actor/d_a_alink.cpp`, `src/f_op/f_op_actor_mng.cpp`, NPC/object actors with action prompts | Do/R/Z status is still global. |
| Howling/Hidden Skills | `src/d/actor/d_a_tag_howl.cpp`, `src/d/actor/d_a_obj_smw_stone.cpp`, `src/d/actor/d_a_npc_kn.cpp` | Howl entry is P1/wolf-owned; Hidden Skill trainer is P1/training-owned. |

## Test Plan

- P1 baseline:
  - P1 lock-on, guard, shield attack, first-person bow/slingshot/Hawkeye, iron ball subject mode,
    hookshot subject mode, and Hidden Skills still behave as before.
- P2 attention:
  - P2 can target independently while P1 is not locked.
  - P1 and P2 can target different actors at the same time.
  - P1 lock-on does not force P2 guard/target pose.
- P2 guard/block:
  - P2 can hold guard without `Ignore shared attention lock`.
  - P1 can guard or not guard independently.
  - Enemy `defender_owner` still reads the actual contacted defender.
- P2 aiming:
  - P2 bow/slingshot/Hawkeye uses P2 camera and does not hijack P1 view.
  - P1 can aim independently while P2 is in or out of aim mode.
- P2 interactions:
  - P2 can trigger eligible prompts without requiring P1 to stand in range.
  - P1 HUD prompts remain stable until a P2 HUD milestone exists.
- Hidden Skills:
  - The lesson binds to the initiating player slot.
  - Required moves are checked against that slot.
  - Forced placement/angle changes move that slot, not always P1.

## Non-Goals

- Do not resize vanilla player-status arrays as the first step.
- Do not convert story/save/demo ownership broadly.
- Do not make HUD fully per-player in this milestone.
- Do not let P2 blindly mutate global `dAttention_c` or meter button state just to make one action
  work.
- Do not use enemy-targeting APIs for player lock-on; this is player attention ownership.
