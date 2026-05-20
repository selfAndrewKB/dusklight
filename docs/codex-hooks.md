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
- Remind attention/status work that clean P2 input does not prove independent P2 state; capture shared `dComIfGp`/attention facts before changing shield or lock-on behavior.
- Remind plan/doc edits to reconcile plan lifecycle: exactly one current active co-op plan in `AGENTS.md`, completed evidence docs pointing forward, and stale `current`/`active`/`next`/`todo` language cleaned up when a milestone changes state.
- After supported edit/check tools run, add focused review context for C++ edits, original/decomp edits, docs-map drift, new source files that may need `files.cmake`, fmt/MSVC logging hazards, and real `git diff --check` whitespace failures.

## Files

- `.codex/config.toml`: enables workspace-local hooks and wires supported Codex events.
- `.codex/hooks/session_context.ps1`: adds short co-op context at session start.
- `.codex/hooks/pre_tool_use_policy.ps1`: blocks risky shell/edit operations before they run.
- `.codex/hooks/post_tool_use_review.ps1`: adds post-edit review reminders and forces attention on `git diff --check` whitespace failures.
  - Also watches diagnostics provider edits and reminds Codex to apply the bounded-output checklist before adding new JSONL-producing data.
  - Also watches enemy actor edits and reminds Codex to classify singleton reads before touching targeting behavior.
  - Also watches plan-map edits and reminds Codex to retire completed plans cleanly instead of leaving stale active-plan breadcrumbs.

## Limitations

Codex hooks are still guardrails. They do not intercept every possible tool path, and they do not replace code review. They also do not stop Codex app background Git integration outside the agent tool loop.

Keep hooks deterministic, fast, and narrow. Do not make hooks run builds or expensive scans.

## Source

Hook behavior follows the official Codex hooks documentation: `https://developers.openai.com/codex/hooks`.
