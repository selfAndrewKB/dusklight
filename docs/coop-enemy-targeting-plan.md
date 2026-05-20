# Co-op Enemy Targeting Policy Plan

## Summary

Add a small Dusk-owned `enemy_targeting` policy layer between enemy actor patches and `player_query`.

The Bokoblin proof showed that raw nearest-player queries are useful but incomplete: recognition could switch to P2, but aggression stayed weak until chase, attack gates, and attack follow-through used the same selected target. This plan turns that lesson into a reusable policy without mass-converting enemy actors or pretending all enemy state machines behave alike.

V1 should stay deliberately small:

- sticky target retention,
- actor-supplied attack commitment,
- structured diagnostics,
- no aggro scores, no target-pressure balancing, no squad behavior, and no broad enemy conversion.

The architecture should still leave room for those later policies so local co-op and future host-authoritative online multiplayer do not need a redesign.

## Layering

```text
actor patches -> enemy_targeting -> player_query
```

- `player_query`: raw facts about active player candidates, distances, angles, slots, and diagnostics.
- `enemy_targeting`: target selection, retention policy, and target-state accessors for facts that should belong to the selected target rather than always to P1.
- actor patches: narrow hooks in concrete enemy files that pass actor-local context and consume the selected target.

Do not replace `fopAcM_searchPlayerDistance*`, `fopAcM_searchPlayerAngleY`, `dComIfGp_getPlayer(0)`, or `daPy_getPlayerActorClass()` globally. Actor files should opt in only where a tested behavior needs co-op-aware targeting.

The intended shape is still broad in spirit: every player should be eligible when enemy logic is truly targeting a player. The implementation should avoid naive text replacement because many singleton reads are not targeting reads. Classify each touched callsite as targeting, selected-target state, primary/global state, damage-owner, caught/grab-owner, or collision-owner before patching.

## Goals

- Preserve single-player behavior when only slot 0 is active.
- Avoid instant target flicker when two players cross nearest-player thresholds.
- Preserve an attack target once an enemy has committed to an attack or follow-through, unless the target disappears or becomes invalid.
- Keep policy state in a Dusk sidecar keyed by enemy actor identity and system name, not inside decompiled enemy structs.
- Let actors select once per state/system tick and reuse that decision for distance, angle, position, and selected-target state so disjoint callsites agree.
- Make diagnostics explain the policy decision: selected slot, reason, retain timer, committed hint, candidates, and fallback path.
- Keep the shape online-friendly: the host should own enemy target selection, and clients should treat the selected target as replicated truth later.

## Non-Goals

- No broad enemy conversion in this plan.
- No boss, miniboss, vehicle, grab/caught-state, cutscene, or scripted-demo targeting work in V1.
- No aggro/threat score system yet.
- No target-pressure or crowd-distribution system yet.
- No generic committed-attack detector across all enemies.
- No changes to original enemy struct layout.

## Proposed API Shape

The first code pass should prefer a narrow surface, approximately:

```cpp
namespace dusk::coop {

enum class EnemyTargetReason : unsigned char {
    None,
    AcquireNearest,
    RetainSticky,
    RetainCommitted,
    LostTarget,
    FallbackPrimary,
};

struct EnemyTargetContext {
    const fopAc_ac_c* observer = nullptr;
    const char* system = nullptr;
    bool committed = false;
    int minRetainFrames = 30;
};

struct EnemyTargetResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* actor = nullptr;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
    EnemyTargetReason reason = EnemyTargetReason::None;
    bool found = false;
    bool changed = false;
};

EnemyTargetResult selectEnemyTarget(const EnemyTargetContext& context);
void clearEnemyTarget(const fopAc_ac_c* observer, const char* system);

bool selectedTargetIsWolf(const EnemyTargetResult& target);
bool selectedTargetIsGuarding(const EnemyTargetResult& target);

}  // namespace dusk::coop
```

This is a sketch, not a lock. Match the actual style of `player_query` and `player_slots` when implementing. The selected-target state helpers should be added only as needed; they are listed here to prevent wolf/guard/speed checks from being mislabeled as permanently primary-player-only.

## V1 Policy

For each `(observer actor, system)` pair:

1. Query active candidates through `player_query`.
2. If no candidate is found, clear or mark the state lost.
3. If the actor says it is committed, retain the previous valid target.
4. If the previous target is still valid and the sticky timer has not expired, retain it.
5. Otherwise acquire the nearest active player.
6. Record the reason and reset/update the retain timer.

V1 should treat "committed" as actor-supplied context. For Bokoblin, the patch can pass `committed = true` from attack/follow-through paths where retargeting would look wrong. Do not try to infer committed attack state generically from unknown enemy internals.

## Diagnostics

Add an `enemy.targeting` diagnostics provider when implementation begins.

`latest.json` should be rich:

- observer pointer/profile/id/room,
- system name,
- selected slot/actor,
- reason,
- committed hint,
- retain frames remaining or elapsed,
- candidate slots and distances from `player_query`,
- whether the selected target changed.

`events.jsonl` should stay semantic:

- selected slot changed,
- selected actor changed,
- reason changed,
- found/lost target changed,
- committed state entered/exited.

Distance, angle, animation frame, and timer drift may appear in latest/context but must not emit every frame by themselves.

## First Implementation Target

Use basic Bokoblin (`src/d/actor/d_a_e_oc.cpp`, `E_OC`) as the first policy-backed specimen. The audit now has enough classification to move from the raw-query proof to the reusable policy spine, as long as the conversion stays narrow.

Why Bokoblin:

- It already proved the full chain from recognition to chase to attack gates.
- It is a regular enemy, not a boss or scripted event actor.
- The current raw-query hooks provide a known-good baseline for behavior.

Initial conversion should replace only the existing Bokoblin raw-query helper calls with `enemy_targeting`. Do not broaden to new Bokoblin systems in the same pass unless a test shows the old raw-query proof is incomplete.

After Bokoblin validates, port the same pattern to Tektite (`src/d/actor/d_a_e_tt.cpp`, `E_TT`). Tektite is the first non-Bokoblin proof because its search/chase/attack callsites are compact and mostly isolated. Pick one accessible compact ground enemy after that (`E_KG`, `E_BS`, or `E_SH`) before tackling target-state-sensitive families such as White Wolfos.

## Future Policy Knobs

These are intentionally deferred, but the sidecar state and diagnostics should leave room for them:

- recent attacker bias,
- damage/threat weighting,
- target-pressure balancing,
- line-of-sight and reachability scoring,
- vertical/flying enemy scoring,
- role or enemy-family-specific policy profiles,
- host-authored target replication for online play.

Add these only when a tested enemy family needs them.

## Audit Gate

The first small policy-backed wave is:

- Bokoblin as the already validated specimen,
- Tektite as the first compact non-Bokoblin ground enemy,
- one accessible compact ground melee backup (`E_KG`, `E_BS`, or `E_SH`),
- target-state-sensitive enemies (`E_WW`, `E_GI`, `E_KK`) only after selected-target state helpers exist.

This prevents designing the policy exclusively around Bokoblin while still keeping the code surface small. Continue the broader audit as more enemies are encountered, but do not block the V1 policy spine on every remaining enemy file.

## Test Plan

- Run `git diff --check`.
- User builds with Visual Studio MSVC.
- With only P1 active, confirm Bokoblin behavior matches the current raw-query proof and normal single-player behavior.
- Spawn P2 and confirm Bokoblin can acquire P2.
- Move P1/P2 across the nearest-player boundary and confirm the target does not flicker every frame.
- Start an attack and confirm the target is retained through the committed attack/follow-through path.
- Flush diagnostics and confirm `enemy.targeting` explains selected target, reason, committed hint, and candidate facts.
- Confirm `events.jsonl` does not grow from distance/angle drift while players stand still.
