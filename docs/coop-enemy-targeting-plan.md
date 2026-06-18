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
- `enemy_targeting`: target selection and retention policy.
- `selected_target_state`: target facts after identity is known, such as position, speed, facing, cut state, and horse state.
- actor patches: narrow hooks in concrete enemy files that pass actor-local context and consume the selected target.

Do not replace `fopAcM_searchPlayerDistance*`, `fopAcM_searchPlayerAngleY`, `dComIfGp_getPlayer(0)`, or `daPy_getPlayerActorClass()` globally. Actor files should opt in only where a tested behavior needs co-op-aware targeting.

The intended shape is still broad in spirit: every player should be eligible when enemy logic is truly targeting a player. The implementation should avoid naive text replacement because many singleton reads are not targeting reads. Classify each touched callsite as targeting, selected-target state, primary/global state, damage-owner, caught/grab-owner, or collision-owner before patching.

For the central routing guide across all player-singleton categories, see `docs/coop-player-singleton-api-map.md`.

## Goals

- Preserve single-player behavior when only slot 0 is active.
- Avoid instant target flicker when two players cross nearest-player thresholds.
- Preserve an attack target once an enemy has committed to an attack or follow-through, unless the target disappears or becomes invalid.
- Keep policy state in a scalable Dusk sidecar keyed by enemy actor identity and `EnemyTargetScope`, not inside decompiled enemy structs.
- Let actors read one canonical target owner per behavior scope and reuse that decision for distance, angle, position, and selected-target state so disjoint callsites agree.
- Keep actor-specific strings such as `e_oc.find` as diagnostic labels only; labels must never create independent retention machines.
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
    AcquireNearest,
    RetainSticky,
    RetainCommitted,
    LostTarget,
    FallbackPrimary,
};

enum class EnemyTargetScope : unsigned char {
    Combat,
};

enum class EnemyTargetMode : unsigned char {
    StickyCombat,
    ImmediateAcquire,
};

struct EnemyTargetContext {
    fopAc_ac_c* observer = nullptr;
    EnemyTargetScope scope = EnemyTargetScope::Combat;
    EnemyTargetMode mode = EnemyTargetMode::StickyCombat;
    const char* label = nullptr;
    bool committed = false;
    float retainSeconds = kDefaultEnemyTargetRetainSeconds;
};

struct EnemyTargetResult {
    PlayerSlot slot = PlayerSlot::Invalid;
    fopAc_ac_c* localActor = nullptr;
    f32 distance = 0.0f;
    f32 distanceXZ = 0.0f;
    s16 angleY = 0;
    bool found = false;
    bool changed = false;
    EnemyTargetReason reason = EnemyTargetReason::LostTarget;
};

EnemyTargetResult selectEnemyTarget(const EnemyTargetContext& context);
void clearEnemyTarget(fopAc_ac_c* observer, EnemyTargetScope scope);
void clearAllEnemyTargets(fopAc_ac_c* observer);

}  // namespace dusk::coop
```

`EnemyTargetResult::slot` is the durable target identity. `localActor` is only the local process pointer resolved from that slot so original enemy code can keep using actor position, distance, and angle helpers. Future host-authoritative networking should replicate slot/scope/reason and re-resolve `localActor` locally; do not treat the pointer as portable target truth.

Selected-target state helpers now exist as a separate API family for facts about a known target. Keep using `enemy_targeting` to choose who the enemy is fighting; then use `dusk::coop::selected_target_state` when original code asks what that chosen target is doing.

Retention is expressed as simulation seconds, not frame counts. V1 uses `retainSeconds = 2.0f` by default and advances elapsed retention with `frameDelta * dusk::game_clock::sim_pace()`. Do not use raw wall-clock time or presentation frame count for gameplay target retention; `std::chrono` remains for diagnostics timestamps only.

`enemy_targeting` should not become a catch-all for every P1 singleton read. When a conversion finds a related but non-targeting read, route it into the correct future API family:

- `dusk::coop::selected_target_state` for facts about the chosen target, such as form, speed, facing, guard, horse, swim, or damage-wait state;
- `dusk::coop::damage_owner` for facts about the player/weapon that actually struck an enemy, such as cut type and hit reaction ownership. Enemy targeting must not answer cut type/count, weapon owner, boomerang/head-jump hit direction, or hit-reaction ownership.
- `dusk::coop::defender_owner` for enemy-attack contact facts, such as which player blocked or guarded the enemy's swing. This must not use damage-owner, nearest-player, selected-target, or P1 global state.
- `dusk::coop::caught_stun_owner` for retained stun effects such as Gibdo scream, where the enemy must keep using the same owner slot for release input and camera ownership until the effect ends, while optional affected slots can share the same effect timer;
- `dusk::coop::wolf_catch_owner` for wolf-bite enemy ownership, where `damage_owner` starts the bite from the hit owner but the ongoing release, throw, and mouth-matrix attachment follow the retained wolf slot;
- future caught/grab-owner helpers for a player currently captured, carried, eaten, hung from, or otherwise physically retained by an enemy;
- broader collision-owner helpers for contact-driven actors with no explicit search/chase targeting surface;
- `dusk::coop::item_awareness` or similar item-awareness helpers for immediate reactions to active player-owned tools, such as Keese boomerang wind checks and White Wolfos hookshot side-step checks, where the enemy should scan active players/items without making that item state the sticky combat target;
- presentation/camera-owner helpers for spawn intros, master/child facing, or flourish angles, such as White Wolfos master/child spawning and target-slot camera presentation;
- render/visibility or split-screen culling helpers for distance checks that only gate model calculation or presentation work.

Leaving such reads conservative during an enemy-targeting patch is intentional when the owning API does not exist yet. Add them to the appropriate future pass instead of faking them with nearest-player guesses.

## V1 Policy

For each `(observer actor, EnemyTargetScope)` pair:

1. Query active candidates through `player_query`.
2. If no candidate is found, clear or mark the state lost.
3. If the actor says it is committed, retain the previous valid target and pause the sticky timer.
4. If the callsite uses `EnemyTargetMode::ImmediateAcquire`, choose the nearest eligible visible candidate immediately and write it back to the same scope. Actor-specific range, height, facing-cone, LOS, form, or status rules must filter candidates before nearest selection; selecting nearest first and rejecting that player afterward can let an ineligible P1 mask an eligible P2. This is for awareness/wake gates that must not be blocked by stale chase stickiness.
5. Otherwise, if the previous target is still valid and the sticky timer has not expired, retain it.
6. Otherwise acquire the nearest active player.
7. Record the reason, diagnostic label, mode, and retain timer.

V1 should treat "committed" as actor-supplied context. For Bokoblin, the patch passes `committed = true` from attack/follow-through paths where retargeting would look wrong. Do not try to infer committed attack state generically from unknown enemy internals.

Commitment belongs to the same behavior scope selected by search/chase/attack gates. Do not create separate attack-only retention state. For Bokoblin, all current `e_oc.*` combat labels share `EnemyTargetScope::Combat`, so attack follow-through freezes the active combat target instead of maintaining its own stale target. "Freezes" means pause elapsed retention time during the committed state, not reset it; after the attack, the enemy resumes from the pre-attack sticky elapsed time and can reconsider promptly.

`EnemyTargetMode` is not a second state key. It is a read/update policy for the current callsite. This distinction matters: `e_oc.search` and `e_oc.search_head` use immediate acquisition because they answer "who can I notice right now?", while `e_oc.find`, `e_oc.find_stay`, `e_oc.move_out`, and `e_oc.attack` use sticky combat because they answer "who am I currently fighting?"

Targeting state storage must scale with normal and multiplied enemy density. Do not reintroduce a small fixed gameplay pool or silent overwrite fallback. If an exceptional storage failure ever occurs, log it loudly and preserve vanilla-like behavior for that decision rather than making an enemy randomly stop acting.

Every converted enemy should use an actor-local helper near the top of the file, such as `coOpSelectCombatTarget(...)`, to build `EnemyTargetContext`. The helper sets the reusable behavior scope and each original callsite passes a manual diagnostic label.

## Diagnostics

`enemy.targeting` diagnostics are part of V1.

`latest.json` should be rich:

- observer pointer/profile/id/room,
- scope and diagnostic label,
- selected slot/actor,
- nearest slot/actor,
- reason,
- mode,
- committed hint,
- retention seconds and sticky elapsed seconds,
- whether retention blocked a nearer candidate,
- candidate slots and distances from `player_query`,
- whether the selected target changed.

`events.jsonl` should stay semantic:

- selected slot changed,
- selected actor changed,
- reason changed,
- found/lost target changed,
- committed state entered/exited.

Distance, angle, animation frame, and timer drift may appear in latest/context but must not emit every frame by themselves.

## Live Overlay

`dusk::coop::debug_overlay` is a visual aid over `enemy_targeting` debug state. It draws selected-target lines/spheres in the active 3D view and a compact text list with label, selected slot, reason, committed flag, and sticky retention timing.

The overlay is currently on by default during enemy conversion work and can be toggled from Actor Spawner or `Ctrl+Shift+F12`. It must remain read-only: do not make it select targets, update policy state, or emit diagnostics. JSON diagnostics remain the durable evidence for later review.

The overlay also draws an approximate forward awareness cone for active enemy decisions. This is intentionally a visual debugging aid, not the policy source of truth; actor-specific range/LOS/cone gates still live in the original enemy logic. Use it to inspect cases where a player is very close but behind or outside the enemy's wake cone.

V1 line labels are captured from the first rendered camera pass only. World lines draw per viewport, but label projection is intentionally first-pass until split-screen labels become viewport-aware.

## First Implementation Target

Use basic Bokoblin (`src/d/actor/d_a_e_oc.cpp`, `E_OC`) as the first policy-backed specimen. The audit now has enough classification to move from the raw-query proof to the reusable policy spine, as long as the conversion stays narrow.

Why Bokoblin:

- It already proved the full chain from recognition to chase to attack gates.
- It is a regular enemy, not a boss or scripted event actor.
- The current raw-query hooks provide a known-good baseline for behavior.

Initial conversion should replace only the existing Bokoblin raw-query helper calls with `enemy_targeting`. Do not broaden to new Bokoblin systems in the same pass unless a test shows the old raw-query proof is incomplete.

Bokoblin is also the first validation surface for the foundation rewrite:

- state keyed by `{observer, EnemyTargetScope::Combat}`;
- labels such as `e_oc.find`, `e_oc.find_stay`, and `e_oc.attack` are diagnostics only;
- `_delete()` calls `clearAllEnemyTargets(this)` so targeting and overlay/debug state cannot outlive the actor;
- invalid overlay slots must not render as `P0`;
- attack commitment freezes the active combat target.
- wake/search checks use immediate acquisition on the same combat owner so stale retention cannot suppress a closer eligible player.

Tektite (`src/d/actor/d_a_e_tt.cpp`, `E_TT`) is the first non-Bokoblin proof because its search/chase/attack callsites are compact and mostly isolated. Its first pass uses the same actor-local helper pattern for `checkPlayerSearch`, `executeChase`, `executeAttack`, and `executeOutRange`. Damage/cut-type ownership is split to `damage_owner`; selected target facts such as facing, position, speed, and horse state are routed through `selected_target_state` in both ordinary combat paths and the rarer first-attack prediction path; and culling belongs to render/visibility or split-screen culling work.

Stalhound (`src/d/actor/d_a_e_sh.cpp`, `E_SH`) is the first breadth proof after Tektite. Its central action metrics, movement speed reads, attack commitment, head tracking, and damage knockback angle use the same API families without introducing a new owner model.

Baby Stalfos (`src/d/actor/d_a_e_bs.cpp`, `E_BS`) is the swarm-style ground melee proof. Its recognition, chase/attack target metrics, selected-target facing checks, head tracking, and attack guard response use the same API families.

Gibdo (`src/d/actor/d_a_e_gi.cpp`, `E_GI`) is the first target-state-sensitive humanoid/undead proof after the compact melee and swarm passes. Its sleep/wait awareness uses immediate acquisition, while chase/damage recovery and head tracking use sticky combat ownership. The close-range attack/scream gate uses immediate acquisition on the same combat scope so a stale chase target outside attack range cannot suppress a nearer in-range player. Ordinary sword cut reactions use `damage_owner`. Scream stun uses `caught_stun_owner` so the retained slot owns release input and best-effort camera lock, while nearby affected slots receive the same scream animation for the same vanilla timer. The affected-player radius is intentionally wider than vanilla detection range during co-op tests so one vanilla-owned scream can cover nearby partners without opening simultaneous scream ownership. Wolf-bite ownership remains deferred caught/grab work because that path physically retains a player and should not be faked with targeting or damage-owner guesses.

The current Gibdo pass has a focused `gibdo.state` probe because in-game testing showed a real P2-first failure where Gibdos can become active but inert without an actual scream resolving. The probe records native wake/chase/attack gates, current BCK, animation progress, `field_0x684` attack delay, `m_cry_gi` state, selected slot, range/angle/LOS checks, and loop-suspect state. Treat this as a measurement surface for finding the broken interaction between Gibdo's native choreography and the co-op APIs, not as an explanation that the behavior is vanilla.

Simultaneous Gibdo screams are deferred. The logs show vanilla `m_cry_gi` is both scream ownership and group attack choreography: one Gibdo can own the scream while another waits, then follows up once the owner reaches attack start. A future retained-effect policy could allow one active scream per player slot, but V1 keeps the single vanilla owner and broadens the affected-player area instead.

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

## Batch Conversion Guardrail

When converting enemies in batches, do not let the speed of repeated API use erase the audit trail.
For every enemy touched, update `docs/coop-enemy-audit.md` with both the converted surfaces and the
deferred surfaces. The row should explicitly name any left-out API hook, such as "wolf-bite
caught/grab deferred," "bomb-search item-awareness deferred," "spawn child ownership deferred,"
"camera presentation deferred," or "culling remains render/visibility work." This keeps later
passes from rediscovering the same P1/global reads after the behavior already looked mostly good in
game.

## Audit Gate

The first small policy-backed wave is:

- Bokoblin as the already validated specimen,
- Tektite as the first compact non-Bokoblin ground enemy,
- one accessible compact ground melee backup (`E_BS`, `E_SH`, or another audited compact enemy),
- target-state-sensitive enemies (`E_WW`, `E_GI`, `E_KK`) only after selected-target state helpers exist.
- White Wolfos (`E_WW`) is the current target-state-sensitive first pass: combat movement uses one
  Combat owner plus selected-target state. Its master spawn staging now uses nearest active-player
  awareness, child facing follows the encounter anchor, and presentation angles use the target
  slot's local camera when available.

This prevents designing the policy exclusively around Bokoblin while still keeping the code surface small. Continue the broader audit as more enemies are encountered, but do not block the V1 policy spine on every remaining enemy file.

## Test Plan

- Run `git diff --check`.
- User builds with Visual Studio MSVC.
- With only P1 active, confirm Bokoblin behavior matches the validated raw-query baseline and normal single-player behavior.
- Spawn P2 and confirm Bokoblin can acquire P2.
- Move P1/P2 across the nearest-player boundary and confirm the target does not flicker every frame.
- Start an attack and confirm the target is retained through the committed attack/follow-through path.
- Flush diagnostics and confirm `enemy.targeting` explains selected target, reason, committed hint, and candidate facts.
- Confirm `events.jsonl` does not grow from distance/angle drift while players stand still.
- For Tektite, P2 can wake, chase, face, be attacked, drive damage-owner cut reactions, and populate `selected_target.state` during ordinary chase/attack/out-range tests without changing culling paths.
- For Bokoblin guard collision, confirm `defender_owner` chooses the player actually hit by the attack sphere before reading guard/block state.

## Implementation Progress

- [x] Added `dusk::coop::enemy_targeting` V1 as a sidecar policy over `player_query`.
- [x] Added `enemy.targeting` diagnostics with rich latest snapshots and semantic per-decision events.
- [x] Converted only the existing Bokoblin raw-query proof systems to policy-backed targeting.
- [x] Validate Bokoblin sticky retention and committed attack retention in game.
- [x] Add read-only in-game overlay for live `enemy_targeting` decisions.
- [x] Refactor targeting state to `{observer, EnemyTargetScope}` so Bokoblin combat callsite labels share one retained target.
- [x] Replace the fixed gameplay targeting-state pool with scalable sidecar storage and explicit cleanup.
- [x] Document the universal actor-local helper wrapper pattern for future enemy conversions.
- [x] Add callsite modes so Bokoblin wake/search gates can immediately acquire without creating separate retention state.
- [x] Make committed attack frames pause sticky retention instead of refreshing the post-attack window.
- [x] Add latest-only nearest-vs-selected diagnostics for retention mismatch analysis.
- [x] Convert Tektite in a separate follow-up patch after Bokoblin validates.
- [x] Add `selected_target_state` V1 and route Tektite ordinary combat target facts, Tektite first-attack prediction, and Bokoblin sword-sound awareness through it.
- [x] Add `defender_owner` V1 and route Bokoblin guard collision through the actual hit defender.
- [x] Add a White Wolfos first pass over combat targeting and selected-target state.
- [x] Validate Tektite in game and inspect `enemy.targeting` labels `e_tt.search`, `e_tt.chase`, `e_tt.attack`, and `e_tt.out_range`.
- [x] First-pass validate Stalhound `E_SH` as the next compact ground-melee breadth proof.
- [x] Validate Baby Stalfos `E_BS` as the first swarm-style ground-melee proof.
- [x] Validate Gibdo `E_GI` as the first target-state-sensitive humanoid/undead proof, with wolf-bite hang ownership deferred to a later caught/grab-owner proof enemy.
