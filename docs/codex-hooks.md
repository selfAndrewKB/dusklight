# Codex Hooks

This workspace includes a small Codex hooks layer under `.codex/`.

For the current Codex desktop workspace, `.codex/` should live at the workspace root:

    TwilightPrincessCo-op/.codex/

The Dusk repository itself is nested under:

    TwilightPrincessCo-op/dusk-coop/

If a future Codex session opens directly in `dusk-coop`, place or copy `.codex/` there instead.

## Purpose

The hooks are guardrails, not a replacement for judgment. They exist to preserve the project's working rules across new Codex sessions and context compaction:

- Load the current co-op plan context on session start.
- Remind Codex that the user owns Visual Studio/CMake builds unless explicitly requested.
- Block broad agent-issued staging commands such as `git add -A` and `git add .`.
- Block obviously destructive shell commands such as `git reset --hard`, forceful `git clean`, recursive `Remove-Item`, and `rm -rf`.
- Block co-op patches to original/decomp code when the patch lacks a `Co-op:` why-comment.
- Remind sessions and post-edit reviews to match local C++ style, including qualified class enum references in file-scope helpers and explicit fmt/MSVC casts for small integer, enum, `BOOL`, and bool-ish log arguments.
- Remind diagnostics-provider edits to keep `events.jsonl` bounded: semantic-change events, no empty/default spam, no frame-churn fields unless the profile specifically tests them, sensible cadence, and lean events versus richer `latest.json`.
- Remind enemy actor edits to classify player-singleton reads before patching: targeting, selected-target state, primary/global state, damage-owner, caught/grab-owner, or collision-owner. This is meant to prevent naive enemy-wide replacements and keep conversions routed through `enemy_targeting`.
- Remind enemy-targeting edits to use actor-local helpers, keep `EnemyTargetScope` as the behavior owner, keep callsite strings as diagnostic labels, use `EnemyTargetMode` for callsite policy, and avoid independent per-callsite retention machines.
- Remind enemy-targeting edits that `EnemyTargetResult::slot` is the durable identity and `EnemyTargetResult::localActor` is the local process pointer; do not reintroduce old `result.actor` usage in enemy patches.
- Remind damage-owner edits that nearest player and current enemy target do not answer "who hit me?"; route cut type/count, weapon-owner, hit direction, and hit-reaction ownership through `dusk::coop::damage_owner`.
- Remind selected-target-state edits that target speed, facing, position, form, horse, swim, guard, and damage-state reads belong behind `dusk::coop::selected_target_state` once target identity is known.
- Remind defender/collision-owner edits that damage-owner, nearest player, selected target, and P1 globals do not answer "who did my attack touch?"; route enemy-attack guard/block/defender-state checks through `dusk::coop::defender_owner`.
- Remind caught/stun-owner edits that targeting and damage ownership do not answer "which player is retained by this effect?"; route scream/stun/grab/carry/hang release ownership through a retained owner API such as `dusk::coop::caught_stun_owner`, and name local pointer caches explicitly, such as `localPlayerActor` or `affectedLocalActors`.
- Remind independent-player-control edits that raw P2 input is not enough; route ALINK lock-on, button status, item camera state, world prompts, and retained training state through the matching ownership family (`player_attention`, `player_button_status`, `player_camera_status`, `interaction_owner`, or `training_owner`) instead of borrowing P1 global attention/status.
- Remind UI/item edits to keep durable slot-local X/Y assignment in `player_item_selection`, meter prompt presentation in `hud_owner`, and transient viewport-local UI context in `ui_owner`. Hawkeye scope, ALINK live reticles, boomerang lock markers, and fishing forced-wheel entry use `ui_owner`; fishing line/bobber geometry remains world-render ownership.
- Remind horse edits to classify canonical campaign Epona, slot-assigned runtime Epona, current rider, any-active-horse world rules, and horse-local collision ownership separately. Do not mass-replace `dComIfGp_getHorseActor()` or let runtime clones overwrite story/save state.
- Remind presentation fixes not to rewrite interpolated points, shared pane positions, draw-list contents, or shared render buffers from symptoms alone. Trace the vanilla producer, retained owner, submission point, viewport replay, and restore path first; extend the narrow native ownership API only after diagnostics identify the broken boundary.
- Remind viewport replay fixes that presentation refresh is independent from simulation interpolation. Camera-facing ribbons, projections, and similar replayed geometry must rebuild from submitted native state for every active viewport even when interpolation is disabled.
- Remind all added-player fixes to begin with: "What vanilla lifecycle, state write, replay, or presentation pass is P2 missing?" Restore that missing native work at its ownership boundary before adding local correction code.
- Remind interaction/event edits that singular authored sequences are not automatically player-owned. Keep howling stones P1/global in V1 and route future opt-in fullscreen collapse plus additional-player hiding through the planned `event_presentation` family rather than disabling split-screen capability or duplicating global minigame state.
- Remind attention/status work that clean P2 input does not prove independent P2 state; capture shared `dComIfGp`/attention facts before changing shield or lock-on behavior.
- Remind plan/doc edits to reconcile plan lifecycle: exactly one current active co-op plan in `AGENTS.md`, completed evidence docs pointing forward, and stale `current`/`active`/`next`/`todo` language cleaned up when a milestone changes state.
- After supported edit/check tools run, add focused review context for C++ edits, original/decomp edits, docs-map drift, new source files that may need `files.cmake`, fmt/MSVC logging hazards, and real `git diff --check` whitespace failures.

## Files

- `.codex/config.toml`: enables workspace-local hooks and wires supported Codex events.
- `.codex/hooks/session_context.ps1`: adds short co-op context at session start.
- `.codex/hooks/pre_tool_use_policy.ps1`: blocks risky shell/edit operations before they run.
- `.codex/hooks/post_tool_use_review.ps1`: adds post-edit review reminders and forces attention on `git diff --check` whitespace failures.
  - Also watches diagnostics provider edits and reminds Codex to apply the bounded-output checklist before adding new JSONL-producing data.
  - Also watches enemy actor edits and reminds Codex to classify singleton reads before touching targeting behavior, then route targeting through actor-local helper wrappers over scoped `enemy_targeting`.
  - Also reminds enemy-targeting edits to consume `EnemyTargetResult::localActor` rather than any old `actor` field; selected-target snapshots may still expose `SelectedTargetState::actor`.
  - Also reminds enemy/damage edits that hit-reaction ownership belongs to `damage_owner`, not nearest-player or current-target policy.
  - Also reminds enemy/player-state edits that selected target facts belong to `selected_target_state`, not direct P1 globals or new nearest-player guesses.
  - Also reminds enemy/collision edits that defender contact belongs to `defender_owner`, not damage-owner, targeting, or primary-player state.
  - Also reminds enemy/caught-state edits that retained stun/grab/carry/hang effects need a retained owner slot, not nearest-player or current-target recomputation.
  - Also reminds independent-player-control edits to keep lock-on, button status, item camera state, prompts, and training routed through their specific ownership APIs instead of broad P1 singleton replacement.
  - Also reminds UI/item edits to separate slot-local item assignment, meter prompt presentation, transient viewport-local overlays, and world-rendered fishing geometry instead of folding them into one broad HUD patch.
  - Also reminds horse edits to preserve canonical campaign Epona compatibility while routing rider-local clone lifecycle, input, camera, reins, and collision through the dedicated `horse_owner` plan.
  - Also reminds presentation fixes to diagnose the native producer-to-viewport path before changing interpolated geometry, shared pane placement, draw-list mutation, or shared render buffers.
  - Also reminds viewport replay fixes to keep per-view camera-facing geometry expansion independent from frame interpolation enablement.
  - Also reminds every added-player fix to identify which vanilla lifecycle P2 is missing before adding correction code.
  - Also reminds interaction/event edits to classify singular authored sequences separately and keep howling stones P1/global until the planned `event_presentation` family exists.
  - Also watches plan-map edits and reminds Codex to retire completed plans cleanly instead of leaving stale active-plan breadcrumbs.

## Limitations

Codex hooks are still guardrails. They do not intercept every possible tool path, and they do not replace code review. They also do not stop Codex app background Git integration outside the agent tool loop.

Keep hooks deterministic, fast, and narrow. Do not make hooks run builds or expensive scans.

## Source

Hook behavior follows the official Codex hooks documentation: `https://developers.openai.com/codex/hooks`.
