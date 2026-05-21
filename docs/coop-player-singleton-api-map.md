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

## Routing Table

| Question the callsite is asking | Use | Current status |
| --- | --- | --- |
| "Which co-op slot owns this actor?" | `dusk::coop::player_slots` | Implemented |
| "Which active player is nearest or eligible by raw distance/angle facts?" | `dusk::coop::player_query` | Implemented |
| "Who is this enemy fighting right now?" | `dusk::coop::enemy_targeting` | Implemented for scoped combat targeting |
| "Who caused this hit?" | `dusk::coop::damage_owner` | Implemented for direct players and known owned items |
| "What is the selected target's form/speed/guard/horse/swim/damage state?" | selected-target state helpers | Not implemented yet |
| "Which player collided, rode, pushed, stood on, or picked this up?" | collision-owner helpers | Not implemented yet |
| "Which player is caught, grabbed, carried, swallowed, or retained by this actor?" | caught/grab-owner helpers | Not implemented yet |
| "Which player owns this item/tool instance?" | item-owner helpers / owner keeps | Partially implemented by item ownership patches |
| "Which player owns camera/HUD/message/story/save state?" | camera/HUD/story-specific APIs | Partially implemented for split-screen camera only |

## Classification Rules

- **Targeting:** distance, angle, position, or pointer reads used to search, chase, face, aim at, or
  attack a player. Route through actor-local helper wrappers over `enemy_targeting` or raw
  `player_query` only for narrow world-acknowledgement proofs.
- **Damage-owner:** cut type/count, weapon owner, hit direction, attacker equipment, and hit reaction
  ownership. Route through `damage_owner`. Never use nearest player or current enemy target to answer
  "who hit me?"
- **Selected-target state:** form, speed, guard, swim, horse, damage-wait, or facing checks that
  modify behavior toward the chosen target. Do not leave these permanently P1-only by accident, but
  do not fake them with nearest-player guesses. Add selected-target state helpers when converting
  the first enemy that truly needs them.
- **Collision-owner:** contact-driven logic with no explicit search/chase surface. Keep it out of
  `enemy_targeting`; it needs its own ownership model.
- **Caught/grab-owner:** a retained interaction with one specific player. It must not retarget to the
  nearest player while the grab is active.
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
