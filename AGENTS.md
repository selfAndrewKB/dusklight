# AGENTS.md

This file is the short map for future Codex sessions. Keep it small. Put durable project facts, audits, and milestone details in `docs/`, then link them here.

## Top Co-op Lessons

- Work on branch `co-op` for co-op implementation and docs unless the user explicitly asks for upstream/main work. Check `git status --short --branch` when branch context is uncertain.
- Mark co-op edits in original/decomp code with a concise `Co-op:` comment explaining why the hook exists. Keep comments focused on intent, not obvious mechanics.
- Prefer Dusk's existing debug output paths for observability. Dusk-owned code can use `aurora::Module` via `dusk/logging.h`; original debug prints often use `OS_REPORT`, routed by `src/dusk/OSReport.cpp`. Avoid per-frame log spam unless a plan calls for gated or sampled tracing.
- The user handles CMake/Visual Studio builds unless they explicitly ask Codex to run one. Codex should still use lightweight checks such as `git diff --check` and source inspection.
- Use detailed commit bodies for meaningful co-op checkpoints. Record the intent, the concrete runtime finding or code hazard addressed, and what was or was not manually validated so later sessions do not have to reconstruct the story from diffs alone.
- Match local C++ style exactly. In early file-scope helpers, qualify class enum members such as `daAlink_c::BTN_R`; do not assume unqualified member names are visible outside member-function scope.
- When logging through `aurora::Module` / fmt on MSVC, cast small integer, enum, `BOOL`, and bool-ish expressions to ordinary `int` / `unsigned int` as needed. Avoid clever format arguments that trip fmt compile-time checks.
- For secondary ALINK shield/attention bugs, do not treat clean P2 input as proof that state is decoupled. Current evidence points at shared attention/player-status state, so capture status facts before adding behavior fixes.
- `checkAttentionLock()` is the first confirmed singleton hazard: P2 stopped mirroring P1's shield/target pose once secondary ALINK ignored the shared `dAttention_c::Lockon()` result. Future fixes should turn that into per-player attention semantics, not remove global attention from P1 camera/HUD/story uses.
- Diagnostics must keep `latest.json` rich and `events.jsonl` semantic. Continuous values may appear in latest snapshots or emitted payload context, but they should not drive JSONL events unless the profile is explicitly testing frame-level churn.
- In-game co-op debug overlays are for fast visual inspection only. Keep durable evidence in structured diagnostics and avoid making overlays mutate gameplay, diagnostics, or target policy.
- Runtime co-op identity should come from the player-slot registry (`getSlotForActor`, `isPlayerInSlot`, `isAdditionalPlayer`). ALINK negative actor arguments are only spawn-time bootstraps before extra-slot registration exists.
- Additional player spawning should go through slot-based `dusk::coop::spawnPlayer(...)`; ImGui and hotkeys are debug callers, not the lifecycle owner.
- Keep durable player-slot identity in `dusk::coop::player_slots`; slot-1 ALINK audit toggles live in `dusk::coop::alink_probes` so prototype probes do not become registry architecture.
- Enemy co-op work must classify touched singleton reads before patching: targeting, selected-target state, primary/global state, damage-owner, caught/grab-owner, or collision-owner. Do not do naive global replacements; use `enemy_targeting` policy for repeated search/chase/attack patterns.
- Enemy targeting conversions must use an actor-local helper near the top of each enemy file. The helper sets a reusable behavior scope such as `EnemyTargetScope::Combat`; callsite strings such as `e_oc.find` are diagnostic labels only and must not create independent retention state. Use target modes for callsite policy, e.g. awareness/wake reads may use immediate acquisition while chase/attack reads use sticky combat. Actor deletion must clear target sidecar state with `clearAllEnemyTargets`.
- Damage-owner reads must use `dusk::coop::damage_owner`, not nearest-player or current enemy target. Use it for cut type/count, weapon owner, hit direction, and hit-reaction ownership questions where the real question is "who hit me?"
- Selected-target state reads must use `dusk::coop::selected_target_state` once a target identity is known. Use it for target speed, facing, position, form, horse, swim, guard, or damage-state facts; do not answer those with fresh nearest-player guesses or P1 globals.
- Defender/collision-owner reads must use `dusk::coop::defender_owner` when the real question is "who did my attack touch?" Use it for enemy-attack contact, guard/block, and defender-state checks; do not answer those with damage-owner, nearest-player, selected-target, or P1 globals.
- Retained stun/grab/carry/hang effects need a retained owner slot. Use `dusk::coop::caught_stun_owner` for Gibdo-style scream stun ownership; nearby players may be affected by the same retained effect, but do not recompute the owner from nearest player, current enemy target, or damage owner after the effect begins.
- Enemy batch conversions must leave an audit breadcrumb for every deferred or intentionally untouched player-singleton surface in the enemy's audit row. Include camera/presentation ownership, master/child or spawned-enemy ownership, hookshot/item awareness, caught/grab/swallow/hang ownership, culling/render visibility, and any remaining P1/global reads that are not converted in that pass.
- Do not propose or implement "temporary now, proper later" co-op fixes unless the user explicitly asks for a disposable experiment. Measure the engine behavior as much as needed, then choose the durable architecture first so prototype debt does not become the project foundation.

## Repository Map

- `README.md`: project overview, supported game dumps, source-build entry point.
- `docs/building.md`: prerequisites, CMake presets, build commands, and run command shape. Read before configuring, building, or validating.
- `docs/code-conventions.md`: Dusk-specific contribution rules. Read before editing original game/decomp code, especially for `#if TARGET_PC` and Dusk-modified code expectations.
- `docs/codex-hooks.md`: repo-local Codex hooks and guardrails for session context, broad staging, destructive commands, and co-op why-comments.
- `docs/co-op-roadmap.md`: co-op roadmap, current code evidence, singleton-touchpoint counts, risks, and first recommended implementation plans.
- `docs/dusk-ai-diagnostics.md`: planned AI-friendly diagnostics recorder shape: profiles, ring buffer, JSONL artifacts, session/role metadata, provider schemas, and stable actor identity.
- `docs/coop-player-slots-plan.md`: completed first co-op milestone: no-behavior-change player slot registry.
- `docs/coop-input-snapshot-plan.md`: completed second co-op milestone: slot-aware input snapshot while preserving primary-player `PAD_1` behavior.
- `docs/coop-secondary-player-prototype-plan.md`: diagnostic third co-op milestone: secondary ALINK prototype evidence.
- `docs/coop-alink-duplication-audit-plan.md`: completed first ALINK duplication audit phase: confirmed model-data and attention singleton hazards plus the visible-P2 containment harness.
- `docs/coop-secondary-alink-input-routing-plan.md`: completed secondary ALINK input-routing milestone: P2 moves from controller 2 and basic rolling/combat swing works.
- `docs/coop-secondary-alink-item-ownership-plan.md`: completed item/action ownership pass: boomerang, fishing rod, Dominion Rod, bow/arrow, Spinner, bombs, slingshot, and Iron Boots owner-routing findings.
- `docs/coop-native-split-screen-camera-plan.md`: completed split-screen camera prototype milestone and known V1 render/HUD limitations.
- `docs/coop-network-multiplayer-readiness.md`: host-authoritative multiplayer readiness notes, especially player-slot identity, enemy-target replication seams, and pointer-vs-slot rules.
- `docs/coop-player-singleton-api-map.md`: central routing guide for `daPy_getPlayerActorClass()`, `dComIfGp_getPlayer(0)`, and which co-op API family should answer each player-identity question.
- `docs/coop-world-acknowledgement-plan.md`: completed world-acknowledgement proof: player-query helpers, diagnostics, Hanging Helmasaur proof, and Bokoblin raw-query evidence.
- `docs/coop-player-owner-lookup-audit.md`: reusable audit table for item/weapon actors that still ask global P1 when they should ask the owning ALINK slot.
- `docs/coop-enemy-audit.md`: enemy/world acknowledgement map, actor triage table, and the target-policy layering for `actor patches -> enemy_targeting -> player_query`.
- `docs/coop-enemy-targeting-plan.md`: current active co-op plan: policy-backed enemy targeting over `player_query`; V1 validated Bokoblin and Tektite, with damage-owner now split into its own API.
- `.codex/config.toml`: Codex hook wiring. Keep hook behavior narrow and documented in `docs/codex-hooks.md`.
- `files.cmake`: explicit source-file list. Update it when adding C++ source/header files that must be built.

For co-op work, create one focused, self-contained plan in `docs/`, such as `docs/coop-player-slots-plan.md`. Do not put audit details, singleton counts, or long design notes in this file; put them in the roadmap or the active plan.

## Working Rules

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked without telling me.
- No abstractions for single-use code without telling me.
- No "flexibility" or "configurability" that wasn't requested, without telling me. Suggestions are fine.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting without telling me.
- Don't refactor things that aren't broken, without telling me.
- Match existing style, even if you'd do it differently. If you consider otherwise, tell me why with proof.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code without telling me.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
