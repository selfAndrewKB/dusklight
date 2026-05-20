# Co-op Player Owner Lookup Audit

This audit tracks original-game actors that consult the canonical player through helpers such as `daAlink_getAlinkActorClass()`, `daPy_getLinkPlayerActorClass()`, or `daPy_getPlayerActorClass()`.

The goal is not to replace every global player lookup. The goal is to identify callsites where an actor already has a real owner relationship and should ask that owning ALINK instead of global P1.

Enemy AI and world acknowledgement are adjacent, but they are not the same audit. Item actors usually need an owner resolver: "which ALINK owns this actor?" Enemy AI usually needs a target policy: "which player should this actor care about right now?" Track enemy coverage in `docs/coop-enemy-audit.md`.

## Classification Rules

- Owner-specific: held-item matrices, return/catch ownership, item actor availability, owner action callbacks, owner animation/arm state, and owner sound emitted through Link.
- Global/P1 policy: camera, HUD prompts, story/event state, save data, scene/minigame authority, and compatibility behavior that should remain tied to canonical P1 until a larger policy exists.
- Ambiguous: anything that mixes owner animation with global camera/HUD/minigame state. Add diagnostics or isolate one smaller callsite before changing behavior.

## Relationship To Enemy Targeting

Do not treat `player_query` as the finished enemy AI policy. Its job is raw facts: active player slots, candidate actors, distances, angles, and diagnostics.

Enemy actor patches should stay narrow and call a higher-level policy once it exists:

```text
actor patches -> enemy_targeting -> player_query
```

The first Bokoblin proof currently calls `player_query` directly because `enemy_targeting` does not exist yet. That is a deliberate proof-of-pipe step, not the final shape. After `docs/coop-enemy-audit.md` classifies regular-enemy coverage, the next AI implementation pass should move target stability, attack follow-through, recent attacker bias, and target-count pressure into `dusk::coop::enemy_targeting` so every enemy file does not grow its own version of multiplayer target selection.

## Current Pattern

- Boomerang proved the return/catch/availability pattern. `daBoomerang_c` owned actor state through P2's `mThrowBoomerangAcKeep`, but still asked global P1 for held matrices, aim/catch position, speed/range, lock state, and `returnBoomerang()`.
- Fishing rod is the second proof target. `daAlink_c::checkFishingRodGrab(actor)` already defines whether a rod actor belongs to a given ALINK through `mItemAcKeep`, so the first safe owner resolver can use that relationship without inventing new ownership state.
- Dominion Rod follows the same item-actor ownership rule in two phases: the held CROD actor is in `mItemAcKeep`, and the thrown copy-rod ball is in `mCopyRodAcKeep`.
- Arrow/bow confirms the projectile variant of the same pattern. The arrow starts in the owning ALINK's `mItemAcKeep`, then must preserve that owner after the keep is cleared for the shot; otherwise flight origin, held-arrow matrix, owner HIO values, and hit sounds all fall back to P1.
- Spinner is the first ride-action version of the same problem. The actor lives in `mRideAcKeep`, so lifecycle, draw, movement constants, sounds, and local pad input should ask the ALINK whose ride keep owns the spinner actor. During `PROC_SPINNER_READY`, `mRideAcKeep` is already set before `mRideStatus` becomes `RIDETYPE_SPINNER`, so ownership resolution must not depend only on `checkSpinnerRideOwn()`.
- Bombs proved the counter-lifetime variant. Normal/water bombs increment `mActiveBombNum` on the ALINK that creates them, and bomblings increment `field_0x2fcf`, but `daNbomb_c` deletion originally decremented global P1. Player-made bombs need to preserve their creating ALINK until deletion so the same slot's active-bomb count is released.
- Slingshot adds a create-time owner hazard to the arrow/bow pattern. Sling stones call their launch setup during `fopAcM_fastCreate()`, before the post-create `setOwner()` call can run, so owner lookup needs a temporary pending owner for the create path.
- Iron Boots expose shared equipment model-data visibility. Their equip path hides/show feet and leg `J3DShape`s directly, which affects every ALINK using the shared model data until model visibility is split per player.
- Iron Boots also exposed audio-owner leakage. `Z2LinkSoundStarter::startSound()` is called by an owning Link sound starter, but heavy-boot footstep conversion used global `Z2GetLink()` state.

## Audit Table

| Area | File | Global player lookup count | Known owner relationship | Current status |
| --- | --- | ---: | --- | --- |
| Boomerang | `src/d/actor/d_a_boomerang.cpp` | 1 fallback after fix | `mThrowBoomerangAcKeep` | Fixed locally; P2 return/rethrow validated, P1 still works |
| Fishing rod/hook | `src/d/actor/d_a_mg_rod.cpp` | 93 | `mItemAcKeep` via `checkFishingRodGrab(actor)` | Fixed first owner clusters; hand attachment, cast recovery, and simultaneous P1/P2 rod use validated |
| Arrow/bow | `src/d/actor/d_a_arrow.cpp` | 1 fallback after fix | `mItemAcKeep` plus preserved spawned-arrow owner | Fixed locally; P2 no longer forces P1 first-person, arrows fire from each owner, sound validated |
| Slingshot | `src/d/actor/d_a_alink_bow.inc`, `src/d/actor/d_a_arrow.cpp` | shares bow/arrow path | pending create owner plus preserved spawned sling-stone owner | Fixed locally; P2 slingshot visibility, direction, and camera behavior validated |
| Dominion Rod | `src/d/actor/d_a_crod.cpp` | 2 fallback sites after fix | `mItemAcKeep` / `mCopyRodAcKeep` | Fixed first owner cluster; P2 throw/return validated |
| Bombs | `src/d/actor/d_a_nbomb.cpp` | 15 fallback sites after first patch | preserved creating ALINK owner for player-made bomb counter decrement | Fixed first counter-lifetime cluster; P2 bomb limit resets after explosion, P2 pickup remains separate object-interaction work |
| Spinner | `src/d/actor/d_a_spinner.cpp`, `src/d/actor/d_a_tag_sppath.cpp` | 1 fallback after first patch | `mRideAcKeep` via ride actor identity; spinner rail tags use active rider position | Fixed locally; P2 spawn/despawn and rail/slot entry validated |
| Iron Boots | `src/d/actor/d_a_alink_hvyboots.inc`, `src/Z2AudioLib/Z2LinkMgr.cpp` | ALINK-local code plus global audio state | equipment state on ALINK; feet/leg shape visibility is shared; sound starter has owning `Z2CreatureLink` | Fixed locally; cross-player leg visibility and heavy boot sounds validated |

## Procedure

1. Count global player lookups in the actor file.
2. Find the actor's real owner relationship, if one exists.
3. Classify each candidate callsite as owner-specific, global/P1 policy, or ambiguous.
4. Patch one owner-specific cluster first.
5. Use `alink.secondary` diagnostics to confirm the item actor, item actor id/name, equip item, proc, and action state before expanding the fix.
6. Keep P1 fallback behavior for vanilla paths.
7. If an actor's lifetime outlives the original ALINK keep, preserve the owning ALINK at creation time rather than rediscovering it from global state during deletion or return.

## Diagnostics Notes

`alink.secondary` records the currently kept item actor pointer, ride actor pointer, actor id/name values, thrown boomerang actor, equipped item, selected item slot, item button/trigger masks, and use-button flags. Item-specific blocks such as `copy_rod` and `bomb` are optional and should appear only while that item state is active. Add actor-specific providers only after this generic ownership snapshot cannot answer the next question.
