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
- **Viewport/render ownership:** split-screen render passes, post effects, lighting, fog, HUD
  projection, and draw-time visibility culling should be owned by viewport/render policy. Do not
  scatter actor-specific culling fixes when a central PC split-screen visibility policy can answer
  actor, world, foliage/detail, and background-part popping in one family.
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
