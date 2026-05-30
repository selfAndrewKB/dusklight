# Co-op Player Singleton API Map

This is the central routing guide for original-game code that asks for "the player" through
`dComIfGp_getPlayer(0)`, `daPy_getPlayerActorClass()`, `daAlink_getAlinkActorClass()`, or nearby
singleton helpers.

Do not mass-replace these calls. Classify the question first, then route it to the narrow co-op API
that owns that kind of player identity.

## Vanilla Meaning

`daPy_getPlayerActorClass()` is the common vanilla shortcut for "Link as a `daPy_py_c*`." In this
codebase it ultimately means P1/the story protagonist because vanilla Twilight Princess has one
player actor.

In co-op, that helper should be read as **primary/global protagonist** unless a callsite is
deliberately converted. Some callsites should remain P1/global forever or until a dedicated story,
camera, HUD, or save-system milestone exists.

## Why These Questions Split Apart

Vanilla code often makes different gameplay decisions through the same P1 singleton helpers. That
was fine when the game only had one meaningful player, because "the target," "the attacker," "the
defender," and "the story protagonist" were all Link. In co-op, those identities can be different
actors in the same frame, so each callsite must be classified by the gameplay question it is asking.

`enemy_targeting` answers **"who am I fighting?"** It owns search, chase, facing, attack-range, and
follow-through target selection. This is where sticky combat targets and committed attack retention
belong.

`damage_owner` answers **"who hit me?"** It owns hit reactions, cut type/count, weapon owner, and
attacker-state reads tied to the collider that caused damage. This must not use nearest player or
current enemy target, because an enemy can be fighting P1 while P2 hits it from behind.

Selected-target state helpers answer **"what is my current target doing?"** These are behavior
reads after the enemy already has a target: target speed, facing, form, horse state, swim state,
guard state, damage state, or similar facts. These should follow the selected target, not P1 and not
a fresh nearest-player query.

`defender_owner` answers **"who did my attack touch?"** These are enemy-attack contact reads such as
"which player blocked this swing?" They are separate from `damage_owner` because the enemy is the
attacker and the player is the defender.

The first proof surface is Bokoblin guard collision. See
`docs/coop-defender-owner-contact-investigation.md` for the current attack-sphere findings and the
known V1 limitation that a sphere's retained hit object is not a full multi-contact trace.

`caught_stun_owner` answers **"which player is retained by this enemy effect?"** These are not
ordinary target or damage reads: once a grab, scream, stun, carry, or hang begins, the enemy must
keep talking to that same player slot until release. The first proof surface is Gibdo scream stun,
which now binds release input and best-effort camera lock to the retained slot, while also allowing
the same scream timer to animate nearby affected player slots.

Gibdo currently preserves the vanilla single global scream owner (`m_cry_gi`) because that pointer
also coordinates follow-up attacks between Gibdos. Dusk broadens the affected-player range for the
single owned scream instead of allowing simultaneous per-player screams. A future retained-effect
policy may allow one active scream per player slot, but that must explicitly preserve or replace
the native group choreography.

Split-screen follows the same classification rule outside enemy code. Camera, viewport, HUD,
lighting, audio, and render-culling questions should not be patched as generic "P2 fixes." Route
them through the split-screen ownership families recorded in
`docs/coop-split-screen-api-audit.md`. In particular, draw frustum culling is a
`render_visibility` question, not a combat, selected-target, or player-query question.

P2 control independence follows the same rule inside ALINK. The old secondary-only
`Ignore shared attention lock` probe was a containment flag, not the final design. Player lock-on,
guard/block availability, first-person item camera modes, prompts, and Hidden Skill training need
slot-local ownership APIs so P2 can answer "what am I targeting or doing?" without consuming P1's
global attention/status state. See `docs/coop-p2-independent-control-plan.md`.

Accepted events and demos are also not automatically primary-player state. The event manager already
retains the requester in `Pt1`, so scripted interaction code should ask `event_owner` when the
question is "which player started this accepted event?" Door demos are the first proof surface:
prompt eligibility may still be interaction-owned, but the accepted door animation/placement belongs
to the requesting player slot.

## Routing Table

| Question the callsite is asking | Use | Current status |
| --- | --- | --- |
| "Which co-op slot owns this actor?" | `dusk::coop::player_slots` | Implemented |
| "Which active player is nearest or eligible by raw distance/angle facts?" | `dusk::coop::player_query` | Implemented |
| "Who is this enemy fighting right now?" | `dusk::coop::enemy_targeting` | Implemented for scoped combat targeting |
| "Who caused this hit?" | `dusk::coop::damage_owner` | Implemented for direct players and known owned items |
| "What is the selected target's form/speed/guard/horse/swim/damage state?" | `dusk::coop::selected_target_state` | Initial implementation for target speed/facing/position/cut/horse facts |
| "Who did this enemy attack touch, and was that player guarding/blocking?" | `dusk::coop::defender_owner` | Initial direct-player implementation for Bokoblin guard collision |
| "Which player collided, rode, pushed, stood on, or picked this up?" | broader collision-owner helpers | Not implemented yet |
| "Which player is caught, stunned, grabbed, carried, swallowed, or retained by this actor?" | `dusk::coop::caught_stun_owner` / future caught-grab helpers | Initial implementation for Gibdo scream stun |
| "Which player owns this item/tool instance?" | item-owner helpers / owner keeps | Partially implemented by item ownership patches |
| "What is this player slot locked onto or allowed to target?" | `dusk::coop::player_attention` | V1 gives additional ALINK actors their own `dAttention_c`; lock acquisition/status gating, owner target-capability masks, owner camera gameplay, cursor drawing, and actor-observed "am I locked-on?" checks use the same attention owner while P1 remains on global attention for HUD/story compatibility |
| "What Do/R/Z/R action status should this ALINK consume?" | `dusk::coop::player_button_status` | First pass implemented for ALINK gameplay Do/A/R/Z/3D prompt state; global meter/HUD rendering remains P1-owned |
| "Which player owns first-person/item camera status?" | `dusk::coop::player_camera_status` over `dusk::coop::camera` | First pass implemented for slot-local status 0/1 bits, camera attention bits, subject zoom/focus, bow/slingshot, Hawkeye, iron ball subject mode, hookshot subject/hang/flight status, and MG_ROD camera/cast status; global HUD/meter status and 2D item reticles remain P1/2D-packet work |
| "Which player owns this prompt/object interaction?" | future `interaction_owner` | Planned for talk/check/pickup/howl prompts; knob/shutter door prompt side selection is active-player aware but still actor-local |
| "Which player requested this accepted event/demo?" | `dusk::coop::event_owner` | Initial implementation derives from event `Pt1`; message input, ALINK door-demo staff consumption, and knob/shutter door demos use it so P2-started scripted interactions do not animate or move P1 |
| "Which player is retained by this training sequence?" | future `training_owner` | Planned for Hidden Skills / `NPC_KN` |
| "Which player owns camera/HUD/message/story/save state?" | camera/HUD/story-specific APIs | Partially implemented for split-screen camera only |
| "Which viewport owns this render pass, post effect, lighting, fog, or culling decision?" | split-screen viewport/render ownership APIs | Initial audit in `coop-split-screen-api-audit.md`; `render_visibility` implemented for known draw-culling paths |

## Classification Rules

- **Targeting:** distance, angle, position, or pointer reads used to search, chase, face, aim at, or
  attack a player. Route through actor-local helper wrappers over `enemy_targeting` or raw
  `player_query` only for narrow world-acknowledgement proofs.
- **Damage-owner:** cut type/count, weapon owner, hit direction, attacker equipment, and hit reaction
  ownership. Route through `damage_owner`. Never use nearest player or current enemy target to answer
  "who hit me?"
- **Selected-target state:** form, speed, position, guard, swim, horse, damage-wait, or facing
  checks that modify behavior toward a known target. Route these through
  `dusk::coop::selected_target_state` once the target identity is known. Do not leave them
  permanently P1-only by accident, but do not fake them with fresh nearest-player guesses.
- **Defender/collision-owner:** enemy-attack contact reads such as guard/block/defender state.
  Route through `dusk::coop::defender_owner`. Never use `damage_owner`, nearest-player, selected
  target, or P1 globals to answer "who did my attack touch?"
- **Broader collision-owner:** contact-driven logic with no explicit search/chase surface, such as
  ride, push, stand-on, pickup, and object interaction. Keep it out of `enemy_targeting`; it needs
  its own ownership model.
- **Caught/grab-owner / caught-stun-owner:** a retained interaction with one specific player. It
  must not retarget to the nearest player while the grab/stun is active, and it must not borrow P1
  camera/body/controller state for P2. Gibdo scream stun now uses `caught_stun_owner`; Gibdo
  wolf-bite ownership remains deferred caught/grab work.
- **Player attention:** ALINK lock-on, target actor, attention truth/release, and slot-local prompt
  candidates. Do not let P2 consume P1's `dAttention_c::Lockon()` as its own gameplay lock state.
- **Player button status:** Do/R/Z/R action availability consumed by ALINK gameplay. Global meter
  status may stay P1-owned until HUD work expands, but P2 action checks need a slot-local answer.
- **Player camera status:** first-person and item-aiming states such as bow, slingshot, Hawkeye,
  hookshot, and iron ball subject mode. Route through slot camera ownership rather than global
  player-status bits.
- **Interaction owner:** prompt-driven actions such as talk, check, pickup, howl, and object use.
  Keep it separate from enemy targeting and item owner lookup.
- **Event owner:** accepted event/demo ownership after the event manager chooses an order. Derive
  from `dComIfGp_event_getPt1()` where possible so scripted interaction placement, input, and
  animation follow the requesting player. Do not use it for raw prompt eligibility before an event is
  accepted; that is `interaction_owner`.
- **Training owner:** retained instructional/event combat sequences such as Hidden Skills. Once a
  trainer binds to a slot, required move checks and forced placement should follow that slot.
- **Viewport/render ownership:** split-screen render passes, post effects, lighting, fog, HUD
  projection, draw-time visibility culling, and shadows should be owned by viewport/render policy.
  Use `render_visibility` for shared draw-culling decisions, `render_materials` for viewport-owned
  kankyo/J3D material state, `render_effects` for late world/effect versus fullscreen framebuffer
  ownership, and `render_shadows` for real-shadow culling or baked shadow matrix ownership. Do not
  scatter actor-specific render fixes when a central PC split-screen policy can answer the question.
- **Primary/global state:** story protagonist, demo/cutscene, save/restart, HUD, message, or
  single-camera state. Keep P1/global until a dedicated milestone proves otherwise.

## Enemy Conversion Rule

Every converted enemy should have an actor-local helper near the top of the file. The helper should
translate that actor's state-machine vocabulary into the shared API:

- reusable behavior scope, such as `EnemyTargetScope::Combat`;
- diagnostic label, such as `e_oc.find`;
- callsite policy mode, such as immediate awareness or sticky combat;
- committed/follow-through hints supplied by the actor.

The label is diagnostics only. It must not create a separate target-retention machine.

## Diagnostics Rule

When adding a new API family or converting a new singleton category, add structured diagnostics at
the same time. Keep `latest.json` rich and `events.jsonl` semantic: continuous values can appear as
context, but should not drive event spam.
