# Co-op Player Query + World-Acknowledgement Diagnostics V1

## Summary

This phase starts the world-acknowledgement pass with narrow proofs: a game actor can ask a Dusk-owned co-op helper for the nearest active player instead of implicitly asking for player 0.

The first converted behavior is the hanging Helmasaur upward-wait trigger in `src/d/actor/d_a_e_hm.cpp`. On PC builds, that one proximity check now wakes for the nearest registered player slot. Non-PC builds keep the original `fopAcM_searchPlayerDistanceXZ(this)` path.

The second converted behavior is basic Bokoblin (`E_OC`) recognition in `src/d/actor/d_a_e_oc.cpp`. Sight checks, idle head-search recognition, basic find/chase steering, and the first close-range attack-facing gates first proved the raw nearest-player query. They now route through `dusk::coop::enemy_targeting` so Bokoblin has sticky target retention and committed attack retention on top of `player_query`. Damage, guard, demo, event, and cutscene paths remain intentionally primary-player/global until specific tests require them.

## Implemented Surface

- `dusk::coop::findNearestPlayer(...)` and `findNearestPlayerToPos(...)` select the nearest active player from the slot registry.
- `dusk::coop::forEachActivePlayer(...)` is the small iterator for future focused conversions.
- `dusk::coop::selectEnemyTarget(...)` is the V1 policy layer for enemy actor patches that need retention over raw nearest-player facts.
- `coop.player_query` diagnostics record recent query decisions in `latest.json`.
- `events.jsonl` payloads for `coop.player_query` are compact per-decision semantic snapshots: system, observer identity, selected slot/actor, found state, and candidate availability. Distance and angle values are latest/context only and must not drive event churn.
- `enemy.targeting` diagnostics record policy decisions in `latest.json` and emit semantic per-decision events for selected target, reason, found/lost state, and committed state.

## Current Proof Target

- `e_hm.up_wait`: converted only the upward-wait proximity check.
- Intent: prove that ordinary world/enemy logic can acknowledge P2 without rewriting global player helpers.
- Out of scope: all other targeting, combat, damage, event, or camera behavior in `d_a_e_hm.cpp`.
- `e_oc.search`, `e_oc.search_head`, `e_oc.find`, `e_oc.find_stay`, `e_oc.move_out`, `e_oc.attack`: converted basic Bokoblin recognition, simple chase steering, close-range attack gates, and attack follow-through facing to policy-backed targeting.
- Out of scope: Bokoblin cut reactions, guard checks, demo/camera sequences, bridge/event behavior, and deeper combat ownership paths that still call player-0 helpers.

## Validation Notes

- Basic Bokoblin testing confirmed the player-query helper selected P1 first, switched to P2 when P2 became closer, switched back to P1 when P1 re-entered range, and returned to P2 at close range.
- Initial recognition/chase conversion was not enough by itself: aggression toward P2 remained weak because downstream attack gates still re-called vanilla P1 angle/distance helpers.
- Routing the close-range attack gates and attack follow-through through the selected co-op target lined Bokoblin behavior up correctly for P2 in the tested area.
- `coop.player_query` events were changed to compact per-decision semantic payloads after the first Bokoblin test showed full latest-style decision tables could exceed the provider payload limit and be dropped from `events.jsonl`.

## Test Plan

- Run `git diff --check`.
- User builds with Visual Studio MSVC.
- With no P2 spawned, confirm the selected query slot is `0` and Helmasaur behavior matches normal P1 proximity.
- Spawn P2, keep P1 outside the search area, move P2 into range, and confirm the Helmasaur wakes/falls for P2.
- In Faron/Forest Bokoblin areas, keep P1 outside recognition range, move P2 into a basic Bokoblin's sight/range, and confirm the Bokoblin recognizes/chases P2 rather than staying idle or steering to P1.
- Put both players near the actor and confirm diagnostics select the nearest slot.
- Stand still near the actor and confirm `events.jsonl` does not grow every frame from distance or angle drift.
- Spot-check P2 movement/items and split-screen behavior.

## Next Candidates

Convert future actors only when a concrete milestone needs them. Good next classes are simple proximity wakeups, hazards, and one small enemy family. Do not mass-rewrite `fopAcM_searchPlayerDistance*`, `dComIfGp_getPlayer(0)`, or actor targeting code.

The audit has since chosen the next implementation shape: validate policy-backed Bokoblin targeting, then port the same pattern to Tektite as the first non-Bokoblin specimen. See `docs/coop-enemy-audit.md` and `docs/coop-enemy-targeting-plan.md`.
