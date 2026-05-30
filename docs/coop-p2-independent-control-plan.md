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
- `dCamera_c` gameplay paths that already own an `mpPlayerActor` now read attention through that
  actor, so camera 1 can see P2's slot-local lock state instead of P1 global attention.
- The play scene now draws additional players' slot-local attention cursors, and
  `player_attention` exposes `isActorLockedByAnyPlayer()` for enemy/object code that asks whether
  it is currently locked-on by a player.
- `dAttention_c` lock acquisition and target reporting now ask `player_attention` whether the
  owning slot is lock-blocked. P1 preserves vanilla singleton player-status gating; additional
  players no longer have their slot-local lock state vetoed by P1-only status bits.
- P2 keeps `attention_info.flags = 0` as an actor so P1/world scans do not target the secondary
  Link, but `player_attention` now supplies the normal player capability mask to P2's own
  `dAttention_c` scanner. This keeps actor targetability separate from "what can this player
  target?" capability.
- Additional players' scanners reject registered player actors as lock-on candidates. This keeps
  P2 from target-locking P1 after restoring P2's normal player target capability mask.
- `attention.state` diagnostics now include a `slots` array for slot-local attention objects, so
  P2 lock-on failures can be separated into missing candidate list, button-state, lock promotion,
  or ALINK target handoff failures.
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

First pass implemented:

- `dusk::coop::player_button_status` stores slot-local Do/A/R/Z and 3D prompt status for additional
  players while P1 still forwards to the vanilla global meter fields.
- ALINK's prompt write wrappers now route Do/B/A/R/Z/3D status through the acting player slot.
- ALINK gameplay reads of Do/R status now use owner-local wrappers, so P2 guard/action branches do
  not consume P1's prompt state after setting their own.
- Message progression reads A/B input from the event owner, so P2-started dialogue can advance from
  P2's controller.
- Force-status fields, item button HUD state, and actual P2 meter rendering remain future
  `hud_owner`/UI work.

### `event_owner`

Owns "which player requested this accepted event/demo?" once the vanilla event manager has chosen an
order.

The first implementation derives ownership from existing event state instead of storing a parallel
sidecar:

- `Pt1` is the request actor set by `dEvt_control_c::setParam()`;
- if `Pt1` is a registered player, that slot owns the event;
- otherwise, explicit fallback actors and then P1 preserve vanilla behavior.

Door demos are the first consumer. Knob/shutter door demo placement, facing, scene-change, restart,
wolf-animation, and animation-rate reads now use the event owner so P2 can open doors without
teleporting P1. ALINK's generic door-demo staff consumption is also owner-gated so P1 does not play
the door animation for P2-owned events.

### `player_camera_status`

Owns per-slot camera/action status such as bow, slingshot, Hawkeye, iron ball subject mode,
hookshot subject mode, and camera attention flags.

First pass implemented:

- `dusk::coop::player_camera_status` stores slot-local player status 0/1 bits for additional
  players while P1 still forwards to vanilla row 0.
- `d_camera.cpp` status reads now consume the owner slot instead of clamping camera 1 to row 0.
- Camera attention bits and subject zoom/focus writes are routed by camera id so first-person and
  item aiming state stays viewport-local.
- Bow/slingshot, Hawkeye, iron ball subject mode, and hookshot subject/hang/flight writes now use
  the owner player's camera-status slot.
- Fishing rod camera/status work has a first owner pass: camera actions, cast line momentum, lure
  standby/cast input, and rod-angle reads route through the MG_ROD owner slot.

Global HUD/meter display state remains P1-owned for now; the sidecar only covers gameplay camera
status that the split-screen cameras consume. Hawkeye scope overlay and boomerang lock reticles are
still HUD/2D-packet ownership work, separate from the gameplay camera status conversion.

### `interaction_owner`

Owns "which player is using this prompt or object?" for talk, inspect, pick up, climb/enter, howl,
and similar action-button interactions.

This is separate from enemy targeting and item ownership. It should be used when the world prompt
itself is the owner, especially for NPC/object interactions and howl tags.

The current knob/shutter prompt eligibility patches are interaction-owner-shaped but still
actor-local: they search active players and seed the shared door side from the nearest eligible
player. A future pass should lift that selected prompt actor/slot into `interaction_owner` before
broader talk/check/pickup prompt work, rather than duplicating door-specific nearest-player state.

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
   - Route owner-camera lock-on reads through the same attention owner.

2. Add per-slot action/R/Z status reads for ALINK gameplay. First pass complete.
   - ALINK Do/A/R/Z/3D prompt writes are slot-owned.
   - ALINK Do/R gameplay reads now use the acting player's prompt owner.
   - Keep force-status, item HUD, and meter rendering P1-owned until a HUD milestone exists.

3. Convert first-person/item aiming status.
   - Bow/slingshot: replace the current secondary status skip with slot-owned camera/status.
   - Hawkeye: make scope mode a slot-local flag instead of `dComIfGp_checkPlayerStatus0(0, 0x200000)`.
   - Iron ball and hookshot subject modes: route subject camera/status to the owning slot/camera.

4. Convert basic interaction prompts through `interaction_owner`.
   - Lift the knob/shutter door prompt owner out of actor-local active-player scans.
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
| Attention core | `src/d/d_attention.cpp`, `include/d/d_attention.h` | Scanner reset and broader action prompt/event ownership still need a dedicated pass; knob/shutter door prompt eligibility now checks active players. |
| ALINK target/guard/status | `src/d/actor/d_a_alink.cpp`, `src/d/actor/d_a_alink_guard.inc` | `checkAttentionLock()`, `setAtnList()`, `checkGuardActionChange()`, `checkNormalAction()`, `checkMoveDoAction()`. |
| First-person and item aim | `src/d/actor/d_a_alink_bow.inc`, `src/d/actor/d_a_alink_ironball.inc`, hookshot code in ALINK, `src/d/actor/d_a_arrow.cpp` | Bow/slingshot ownership exists, camera/status ownership does not. |
| Interaction prompts | `src/d/actor/d_a_alink.cpp`, `src/f_op/f_op_actor_mng.cpp`, NPC/object actors with action prompts | Do/R/Z gameplay status is slot-local; knob/shutter door prompt side selection is active-player aware; event-owner door demos now move the requester; HUD rendering and broader prompt owners still need a dedicated pass. |
| Climb/hang camera hints | `src/d/actor/d_a_alink_hang.inc` | Hang, ladder, climb, and roof-hang camera status writes route through `player_camera_status` so P2 climb states do not write into P1's camera row. |
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
