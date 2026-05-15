# Dusk AI Diagnostics

This document defines the intended shape for an AI-friendly diagnostic recorder in Dusk. V1 is partially implemented for the secondary ALINK action-mirror audit. The goal is to stop relying on the user as the main sensor during co-op debugging, while keeping the first version small, file-based, and easy to remove or extend.

## Purpose

Co-op work has already needed repeated manual loops:

1. Codex adds a narrow log or probe.
2. The user builds in Visual Studio.
3. The user reproduces the issue and describes what happened.
4. Codex tries to infer runtime state from screenshots, observations, and log excerpts.

The recorder should turn that into:

1. Enable or select a diagnostic profile.
2. Run the test normally.
3. Dusk writes structured recent state, events, and a manifest.
4. Codex reads the capture bundle and compares the same facts across runs.

The first version should export files. Do not start with live process introspection or an MCP server. JSONL bundles are replayable, diffable, attachable to bug reports, and already fit the repo's existing JSON patterns in `src/dusk/imgui/ImGuiStateShare.cpp`.

## Existing Runtime Sources

Dusk already exposes useful debug truth through code, not guesswork:

- Player and horse position, angle, and speed in `src/dusk/imgui/ImGuiMenuTools.cpp`.
- Render/backend statistics from `dusk::lastFrameAuroraStats` and the debug overlay.
- Process tree and actor/create queue traversal in `src/dusk/imgui/ImGuiProcessOverlay.cpp`.
- Heap tree, heap checks, and block validity in `src/dusk/imgui/ImGuiHeapOverlay.cpp`.
- Camera transform/state in `src/dusk/imgui/ImGuiCameraOverlay.cpp`.
- Controller state in `src/dusk/imgui/ImGuiControllerOverlay.cpp`.
- Actor Spawner co-op probe flags in `src/dusk/imgui/ImGuiActorSpawner.cpp`.
- Original debug text and shape output through `src/d/d_debug_viewer.cpp`.
- `OS_REPORT` routing into Dusk logging through `src/dusk/OSReport.cpp`.

Prefer sampling these existing sources or their underlying data over adding bespoke per-test logs.

## Architecture

The recorder should have a small Dusk-owned spine:

```text
runtime data
  -> named diagnostic providers
  -> active profile
  -> in-memory ring buffer
  -> manual export or automatic flush trigger
  -> append-only JSONL + latest snapshot + manifest
```

The ring buffer is part of v1 design, even if the first implementation uses a short default window. The important failure often happens before the user knows it matters. A manual `Capture` button is useful for exploratory work, but it should not be the only trigger.

## Capture Triggers

The recorder should support both manual and automatic flushes.

Manual triggers:

- User presses a diagnostic capture/export button.
- Codex asks for a specific profile run.

Automatic triggers:

- Dusk assertion or important `OS_REPORT`/error event.
- Heap check or block-validity failure from the heap tooling.
- ALINK enters a known-bad diagnostic state, such as secondary execute mirroring primary target/shield state unexpectedly.
- Future host/client state-share divergence.

Automatic triggers should flush the buffered tail plus a small amount of follow-up state if possible. Design this in before code depends on direct-to-file writes.

## Artifact Layout

Use append-only files so later tools can follow captures live:

```text
diagnostics/
  latest/
    manifest.json
    latest.json
    events.jsonl
  sessions/
    <session-id>/
      <role>/
        manifest.json
        latest.json
        events.jsonl
        notes.md
```

`events.jsonl` is append-only. Do not rewrite old lines.

`latest.json` is overwritten with the newest full snapshot. It should update when provider state changes, with light throttling to avoid per-frame disk writes, and manual flush should force an immediate write.

`diagnostics/latest/events.jsonl` is the convenience view for the newest session and is cleared when a new diagnostic session starts. The durable append-only capture for a run is `diagnostics/sessions/<session-id>/<role>/events.jsonl`.

`manifest.json` describes the run, profile, build, session identity, role, and provider configuration.

`notes.md` is optional human observation. It should not be required for machine interpretation.

The exact directory root can be chosen during implementation, but it should live near Dusk's existing runtime logs/config outputs rather than inside the source tree.

## Event Envelope

Every JSONL event should use one stable outer envelope:

```json
{
  "event_version": 1,
  "session_id": "2026-05-15T14-30-00Z-7f3a",
  "role": "local",
  "frame": 1624,
  "time_us": 918273645,
  "profile": "coop.secondary_alink.action_mirror",
  "provider": "player.slots",
  "provider_schema_version": 1,
  "kind": "snapshot",
  "data": {}
}
```

Required envelope fields:

- `event_version`: version of the outer event envelope.
- `session_id`: shared ID for the play session. Future host/client captures must use the same value.
- `role`: `local`, `host`, `client`, or another explicit role.
- `frame`: local game frame number.
- `time_us`: monotonic timestamp in microseconds.
- `profile`: active diagnostic profile name.
- `provider`: provider name.
- `provider_schema_version`: schema version for that provider's `data`.
- `kind`: `snapshot`, `change`, `marker`, `assertion`, `flush`, or another small controlled value.
- `data`: provider-owned payload.

Record both `frame` and `time_us`. Frame is best for single-process game logic. A monotonic clock is needed to align host/client streams when frame numbers diverge.

## Manifest

The manifest should be present from v1, even for local-only debugging:

```json
{
  "manifest_version": 1,
  "session_id": "2026-05-15T14-30-00Z-7f3a",
  "role": "local",
  "profile": "coop.secondary_alink.action_mirror",
  "created_utc": "2026-05-15T14:30:00Z",
  "build": {
    "branch": "co-op",
    "commit": "unknown",
    "configuration": "windows-msvc-debug"
  },
  "slots": [
    {"slot": 0, "peer": "local", "actor_uid": 1001},
    {"slot": 1, "peer": "local", "actor_uid": 1002}
  ],
  "providers": [
    {"name": "scene.current", "schema_version": 1, "sample_every_frames": 30},
    {"name": "player.slots", "schema_version": 1, "sample_every_frames": 1}
  ]
}
```

Session and role are not optional design details. Co-op desync captures will eventually produce at least two streams for the same play session, and the important bugs are where those streams disagree.

## Provider Contract

Providers should be boring, named collectors:

```cpp
registerProvider("player.slots", collectPlayerSlots);
registerProvider("scene.current", collectSceneState);
registerProvider("render.stats", collectRenderStats);
registerProvider("input.pad", collectInputState);
registerProvider("actor.processes", collectProcessSummary);
registerProvider("attention.state", collectAttentionState);
```

Each provider should declare:

- Name, using a reserved namespace.
- Schema version.
- Cost class: `cheap`, `medium`, or `walks_tree`.
- Default sample cadence.
- Whether it supports change detection.

Profiles choose providers and can override cadence per provider. Do not make profile sampling all-or-nothing; `render.stats` and process-tree traversal do not cost the same.

Change detection may be provider-owned, profile-enabled, or both. Decide this explicitly during implementation. The likely default is: providers expose a stable digest or previous-value comparison when cheap; profiles choose whether to emit change events.

## Extensibility Roadmap

The recorder is intentionally small but should grow through provider/profile additions, not one-off logs. The next useful expansion is a set of reusable provider families:

- `attention.state`: global attention lock state, current lock-on/action/check targets, and enough actor metadata to tell which player slot is observing the global attention object.
- `player.status`: selected `dComIfGp` player status bits, R/Z/attention UI status, health/magic/rupee/item state, and any slot-specific replacement once the state becomes decoupled.
- `actor.lifecycle`: create/delete/register/unregister events with future stable actor UIDs, profile names, rooms, arguments, and parent/process-tree relationships.
- `actor.processes`: low-rate or manual process-tree summaries derived from the same data used by `ImGuiProcessOverlay`.
- `camera.state`: active camera IDs, eye/center/up/FOV, mode, and target actor metadata.
- `heap.summary`: heap health, failed block checks, and selected allocation summaries from the heap overlay validators.
- `debug.viewer`: original debug text/shape records so captures include the game's own annotations.
- `osreport.events`: selected structured assertions, warnings, and important engine reports as timeline markers and auto-flush triggers.

Keep profiles task-shaped. A profile should answer one question, such as `coop.secondary_alink.action_mirror` or `coop.attention.decoupling`, by composing existing providers with per-provider cadence and change detection. Avoid a single "record everything" profile except for short, explicit manual captures.

The target architecture is broad observability with bounded output:

- Cheap providers may sample every frame but should emit only on change.
- Medium providers should sample at low cadence or on explicit markers.
- Tree-walking providers should be manual, low-rate, or scoped to a known actor/profile.
- Every provider should include `schema_version` and stable namespaces from this document.
- Actor-heavy providers should wait for stable actor UIDs before they become authoritative.

This keeps the recorder extensible enough for future co-op systems without turning it into a permanent log-spam machine.

## Provider Namespaces

Reserve these top-level namespaces:

- `scene.*`: stage, room, layer, event, pause, and transition state.
- `player.*`: sidecar player slots, canonical player actor, health/status summaries.
- `input.*`: raw pad state and co-op input snapshots.
- `render.*`: backend/frame/render statistics.
- `camera.*`: camera eye/center/FOV and active camera IDs.
- `actor.*`: actor/process tree, create queue, actor lifecycle IDs.
- `attention.*`: lock-on/action/check-object lists and target state.
- `heap.*`: heap summary, heap checks, block validity.
- `debug.*`: original debug viewer text/shape annotations.
- `coop.*`: co-op registry, profile flags, slot/peer mapping.
- `alink.*`: ALINK-specific action, animation, model-data ownership, and probe state.
- `osreport.*`: selected structured OSReport/assertion events if the recorder becomes a sink.

This prevents flat-name drift such as co-op probe flags and attention state competing in the same namespace.

## Stable Actor Identity

Raw actor pointers are not stable identities because memory can be reused. Captures should include pointers as metadata, not as the primary key.

Before actor-heavy providers become serious, Dusk should assign a monotonic actor UID at actor/process construction time and preserve it until destruction. The process traversal used by `ImGuiProcessOverlay` is the natural place to expose those IDs to diagnostics.

Provider payloads should prefer:

```json
{
  "actor_uid": 1002,
  "ptr": "0x20781fa24cc",
  "profile": "fpcNm_ALINK_e",
  "room": 0
}
```

not:

```json
{
  "actor": "0x20781fa24cc"
}
```

## OSReport Policy

The recorder should not accidentally become a second, inconsistent logging system.

Preferred policy:

- Normal Dusk logs and `OS_REPORT` remain human-readable logs.
- The diagnostic recorder records selected structured `osreport.*` and assertion events when they are useful flush triggers or timeline markers.
- Do not mirror every log line into JSONL by default.

If this policy changes, update this document before implementation.

## Debug Viewer Provider

The original debug viewer text/shape data is worth a first-class provider. It is the game annotating what it thinks matters.

Future provider:

- `debug.viewer.text`: current debug text entries, sampled on change or every N frames.
- `debug.viewer.shapes`: current debug shape list or summary, sampled on change or at a low cadence.

Do not parse rendered output. Capture the underlying text/shape records if accessible.

## Initial Providers

The smallest useful implementation starts with:

- `scene.current`: stage, room, layer, frame, event/pause/transition basics.
- `render.stats`: existing Aurora frame/backend stats.
- `player.slots`: sidecar co-op slots, actor UID/pointer, profile, room, position, angle, speed.
- `input.pad`: raw pad state and current co-op input snapshot for player slots 0 and 1.
- `coop.probes`: current secondary ALINK probe flags.
- `alink.secondary`: the secondary ALINK state already being investigated: proc, animation frame/rate, relevant input/action bits, and model-data ownership summary.

Leave process-tree, heap, attention-list, OSReport sink, and debug-viewer providers for follow-up unless the first implementation needs them to answer the current ALINK question.

## First Co-op Profile

First profile candidate:

```json
{
  "name": "coop.secondary_alink.action_mirror",
  "ring_buffer_seconds": 10,
  "providers": [
    {"name": "scene.current", "sample_every_frames": 30},
    {"name": "render.stats", "sample_every_frames": 30},
    {"name": "player.slots", "sample_every_frames": 1, "emit_on_change": true},
    {"name": "input.pad", "sample_every_frames": 1, "emit_on_change": true},
    {"name": "coop.probes", "sample_every_frames": 30, "emit_on_change": true},
    {"name": "alink.secondary", "sample_every_frames": 1, "emit_on_change": true}
  ],
  "flush_triggers": [
    "assertion",
    "alink.secondary.known_bad_state",
    "manual"
  ]
}
```

This profile should answer whether secondary ALINK shield/target mirroring is caused by P1 input leaking into P2, shared attention/status state, or both actors legitimately seeing the same world state.

## V1 Implementation Status

Implemented:

- `include/dusk/diagnostics.h` and `src/dusk/diagnostics.cpp` define a small Dusk-owned recorder.
- The hardcoded profile is `coop.secondary_alink.action_mirror`.
- The recorder emits one stable event envelope with `event_version`, `session_id`, `role`, `frame`, `time_us`, `profile`, `provider`, `provider_schema_version`, `kind`, and `data`.
- The output path is under Dusk's config path:
  - `diagnostics/latest/`
  - `diagnostics/sessions/<session-id>/local/`
- Per-session `events.jsonl` is append-only. The convenience `diagnostics/latest/events.jsonl` is reset when a new session starts so captures from previous runs do not mix with the newest manifest.
- `latest.json` is overwritten when provider state changes, throttled to avoid per-frame writes, and manual flush forces a write with the newest provider data.
- `manifest.json` records profile, role, provider schema versions, sample cadence, `emit_on_change`, the Dusk log path, and the current stable-actor-ID limitation.
- Provider snapshots refresh the in-memory latest state on their cadence, but only append to `events.jsonl` when the provider payload changes. This keeps the JSONL stream useful for AI review instead of recording identical per-frame state.
- The ring buffer keeps the latest 3600 emitted events in memory and is flushed through the same event path.
- Actor Spawner exposes `Record action mirror diagnostics`, `Flush diagnostics`, and the active output path near the secondary ALINK controls.
- The existing ALINK action-mirror helper now also feeds `alink.secondary` structured state whenever it emits the human-readable `secondary action-mirror` log. Its `"phase"` field is informational; identical state is not re-emitted just because the helper saw a new before/after phase.

Still deferred:

- Stable actor UIDs. V1 records pointers as metadata and marks `stable_actor_uid_deferred`.
- Process tree, heap, attention-list, OSReport sink, and debug-viewer providers.
- Automatic assertion/heap/desync flush triggers.
- Live MCP or process introspection.

## Non-Goals For V1

- Do not add a live MCP server.
- Do not expose arbitrary process memory.
- Do not record every actor every frame by default.
- Do not mirror all Dusk logs into JSONL.
- Do not build a large provider directory tree before the first slice proves useful.
- Do not replace the existing human-readable Dusk log.

## Implementation Notes

Keep the first patch small:

- One or two Dusk-owned source files are better than a deep new tree.
- Update `files.cmake` if new C++ files are added.
- Keep original/decomp code touches minimal and marked with `Co-op:` comments only when they are truly co-op hooks.
- Prefer providers that read existing Dusk/debug state over new per-frame logging in ALINK.
- Use `nlohmann::json`, matching existing Dusk JSON code.
- Preserve append-only JSONL semantics from the first write path.

## Open Questions

- Exact runtime output directory.
- Whether the first UI belongs in the existing Tools menu or the Actor Spawner's co-op section.
- Where to assign and store stable actor UIDs with the least disruption.
- Which Dusk assertion path should trigger automatic diagnostic flush first.
- Whether provider registration should be compile-time static or explicit during Dusk startup.
