# Co-op Player Owner Lookup Audit

This audit tracks original-game actors that consult the canonical player through helpers such as `daAlink_getAlinkActorClass()`, `daPy_getLinkPlayerActorClass()`, or `daPy_getPlayerActorClass()`.

The goal is not to replace every global player lookup. The goal is to identify callsites where an actor already has a real owner relationship and should ask that owning ALINK instead of global P1.

## Classification Rules

- Owner-specific: held-item matrices, return/catch ownership, item actor availability, owner action callbacks, owner animation/arm state, and owner sound emitted through Link.
- Global/P1 policy: camera, HUD prompts, story/event state, save data, scene/minigame authority, and compatibility behavior that should remain tied to canonical P1 until a larger policy exists.
- Ambiguous: anything that mixes owner animation with global camera/HUD/minigame state. Add diagnostics or isolate one smaller callsite before changing behavior.

## Current Pattern

- Boomerang proved the return/catch/availability pattern. `daBoomerang_c` owned actor state through P2's `mThrowBoomerangAcKeep`, but still asked global P1 for held matrices, aim/catch position, speed/range, lock state, and `returnBoomerang()`.
- Fishing rod is the second proof target. `daAlink_c::checkFishingRodGrab(actor)` already defines whether a rod actor belongs to a given ALINK through `mItemAcKeep`, so the first safe owner resolver can use that relationship without inventing new ownership state.
- Dominion Rod follows the same item-actor ownership rule in two phases: the held CROD actor is in `mItemAcKeep`, and the thrown copy-rod ball is in `mCopyRodAcKeep`.

## Audit Table

| Area | File | Global player lookup count | Known owner relationship | Current status |
| --- | --- | ---: | --- | --- |
| Boomerang | `src/d/actor/d_a_boomerang.cpp` | 1 fallback after fix | `mThrowBoomerangAcKeep` | Fixed locally; P2 return/rethrow validated, P1 still works |
| Fishing rod/hook | `src/d/actor/d_a_mg_rod.cpp` | 93 | `mItemAcKeep` via `checkFishingRodGrab(actor)` | Fixed first owner clusters; hand attachment, cast recovery, and simultaneous P1/P2 rod use validated |
| Arrow/bow | `src/d/actor/d_a_arrow.cpp` | 9 | likely item/projectile owner; needs audit | Not started |
| Dominion Rod | `src/d/actor/d_a_crod.cpp` | 2 fallback sites after fix | `mItemAcKeep` / `mCopyRodAcKeep` | Fixed first owner cluster; P2 throw/return validated |
| Bombs | `src/d/actor/d_a_nbomb.cpp` | 16 | item/grab/carry ownership likely mixed with world collision | Not started |
| Spinner | `src/d/actor/d_a_spinner.cpp` | 9 | likely player action actor; needs audit | Not started |

## Procedure

1. Count global player lookups in the actor file.
2. Find the actor's real owner relationship, if one exists.
3. Classify each candidate callsite as owner-specific, global/P1 policy, or ambiguous.
4. Patch one owner-specific cluster first.
5. Use `alink.secondary` diagnostics to confirm the item actor, item actor id/name, equip item, proc, and action state before expanding the fix.
6. Keep P1 fallback behavior for vanilla paths.

## Diagnostics Notes

`alink.secondary` records the currently kept item actor pointer, item actor id/name, thrown boomerang actor, copy-rod actor, copy-rod control/camera actors, equipped item, selected item slot, item button/trigger masks, and use-button flags. Add actor-specific providers only after this generic item ownership snapshot cannot answer the next question.
