# AGENTS.md

This file is the short map for future Codex sessions. Keep it small. Put durable project facts, audits, and milestone details in `docs/`, then link them here.

## Top Co-op Lessons

- Mark co-op edits in original/decomp code with a concise `Co-op:` comment explaining why the hook exists. Keep comments focused on intent, not obvious mechanics.
- Prefer Dusk's existing debug output paths for observability. Dusk-owned code can use `aurora::Module` via `dusk/logging.h`; original debug prints often use `OS_REPORT`, routed by `src/dusk/OSReport.cpp`. Avoid per-frame log spam unless a plan calls for gated or sampled tracing.
- The user handles CMake/Visual Studio builds unless they explicitly ask Codex to run one. Codex should still use lightweight checks such as `git diff --check` and source inspection.
- Use detailed commit bodies for meaningful co-op checkpoints. Record the intent, the concrete runtime finding or code hazard addressed, and what was or was not manually validated so later sessions do not have to reconstruct the story from diffs alone.

## Repository Map

- `README.md`: project overview, supported game dumps, source-build entry point.
- `docs/building.md`: prerequisites, CMake presets, build commands, and run command shape. Read before configuring, building, or validating.
- `docs/code-conventions.md`: Dusk-specific contribution rules. Read before editing original game/decomp code, especially for `#if TARGET_PC` and Dusk-modified code expectations.
- `docs/codex-hooks.md`: repo-local Codex hooks and guardrails for session context, broad staging, destructive commands, and co-op why-comments.
- `docs/co-op-roadmap.md`: co-op roadmap, current code evidence, singleton-touchpoint counts, risks, and first recommended implementation plans.
- `docs/coop-player-slots-plan.md`: completed first co-op milestone: no-behavior-change player slot registry.
- `docs/coop-input-snapshot-plan.md`: completed second co-op milestone: slot-aware input snapshot while preserving primary-player `PAD_1` behavior.
- `docs/coop-secondary-player-prototype-plan.md`: diagnostic third co-op milestone: secondary ALINK prototype evidence.
- `docs/coop-alink-duplication-audit-plan.md`: current next co-op plan: audit ALINK singleton/shared-state hazards before choosing proxy fallback or reattempting duplication.
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
