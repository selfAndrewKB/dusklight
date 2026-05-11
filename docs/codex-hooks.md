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
- After supported edit/check tools run, add focused review context for C++ edits, original/decomp edits, docs-map drift, new source files that may need `files.cmake`, and real `git diff --check` whitespace failures.

## Files

- `.codex/config.toml`: enables workspace-local hooks and wires supported Codex events.
- `.codex/hooks/session_context.ps1`: adds short co-op context at session start.
- `.codex/hooks/pre_tool_use_policy.ps1`: blocks risky shell/edit operations before they run.
- `.codex/hooks/post_tool_use_review.ps1`: adds post-edit review reminders and forces attention on `git diff --check` whitespace failures.

## Limitations

Codex hooks are still guardrails. They do not intercept every possible tool path, and they do not replace code review. They also do not stop Codex app background Git integration outside the agent tool loop.

Keep hooks deterministic, fast, and narrow. Do not make hooks run builds or expensive scans.

## Source

Hook behavior follows the official Codex hooks documentation: `https://developers.openai.com/codex/hooks`.
