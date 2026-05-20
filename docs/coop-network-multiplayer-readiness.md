# Co-op Network Multiplayer Readiness Notes

## Summary

This document captures network-readiness decisions discovered during local co-op work. It is not an implementation plan for transport, prediction, rollback, matchmaking, or LAN play. It records the architectural seams that should be preserved so local co-op systems can become host-authoritative online systems without a redesign.

The current direction remains:

- Local co-op proves player identity, input, cameras, item ownership, enemy targeting, and diagnostics first.
- Later online play should use a host-authoritative server model. The first "server" can be the host player's Dusk process, but the authority boundary should still be server-shaped: clients send commands, the host/server decides truth, and clients render replicated results.
- Clients should send player commands and render replicated truth rather than independently deciding enemy/world state.

## Host Authority Baseline

The roadmap chooses a host-authoritative server model over deterministic lockstep or peer/client-relay. That choice fits this codebase because Twilight Princess and Dusk were not built around cross-machine deterministic simulation.

For online co-op:

- The host/server owns player joins, authoritative actor state, enemy AI, item/world ownership, damage, drops, and progression.
- Clients send per-slot input/action commands.
- Clients may derive presentation details locally, but should not independently decide enemy truth or world progression truth.

## Player Slots Are The Replication Vocabulary

`dusk::coop::PlayerSlot` is the right stable identifier to replicate. Raw actor pointers are process-local and must not cross the network boundary.

Good current properties:

- Enemy targeting results already carry `PlayerSlot`.
- Player-query and enemy-targeting diagnostics already report slots.
- Additional-player spawning is now slot-based rather than ImGui-owned.
- Spawn-time ALINK arguments are bootstraps only; runtime identity comes from the player-slot registry.

Future network messages should identify players by slot or peer-to-slot mapping, not by local actor pointer.

## Enemy Targeting Readiness

The current `enemy_targeting` direction is more network-friendly than the old six-system-key design would have been.

Good current properties:

- One retained target per `(observer actor, EnemyTargetScope)` means there is one combat target truth to replicate per enemy scope, not several independent decisions.
- `EnemyTargetResult::changed` is a useful replication trigger. The host can emit target changes instead of broadcasting every frame.
- `EnemyTargetReason` is useful metadata. Replicating a reason such as `AcquireNearest`, `RetainSticky`, or `RetainCommitted` helps client debugging and light prediction without requiring clients to re-run the full policy.
- `EnemyTargetMode` is local callsite policy, not replicated state. Clients need the selected slot/reason/scope, not every actor-local callsite label.

Important boundary:

- `EnemyTargetResult::localActor` and sidecar `TargetState::actor` are local cached pointers. They are valid only inside the current process. Network payloads should carry the selected slot, scope, reason, and optional timing/debug metadata. The receiving process should derive the actor with `getPlayer(slot)`.

## Required Future Seam

Today, `selectEnemyTarget(...)` always runs local policy: it queries candidates through `player_query`, advances retention, chooses a target, and updates sidecar state.

For online play, this needs a host/client split:

- Host path: run `selectEnemyTarget(...)` as the authoritative policy.
- Client path: apply a replicated decision without re-running nearest-player selection or retention policy.

The natural future API shape is something like:

```cpp
struct ReplicatedEnemyTarget {
    fopAc_ac_c* observer = nullptr;          // local enemy actor resolved by the client
    EnemyTargetScope scope = EnemyTargetScope::Combat;
    PlayerSlot slot = PlayerSlot::Invalid;   // replicated truth
    EnemyTargetReason reason = EnemyTargetReason::AcquireNearest;
    float stickyElapsedSeconds = 0.0f;       // optional debug/prediction context
};

EnemyTargetResult applyReplicatedEnemyTarget(const ReplicatedEnemyTarget& target);
```

The exact names can change later. The important rule is that client application must skip `findNearestPlayer(...)` and must not independently expire sticky windows as gameplay truth.

## Sticky Timing And Network Delay

Enemy retention currently uses simulation seconds derived from simulation frame deltas. That is correct for local gameplay and better than wall-clock or render-frame timing.

For online play, sticky timers should remain host-owned gameplay state. Clients can display elapsed time and reason metadata from host snapshots, but client-side timer expiry must not decide target truth. Otherwise packet delay or client frame drift could make enemies appear to switch targets before the host has switched them.

## Actor Pointer Rule

Do not replicate these directly:

- `fopAc_ac_c*`
- `daAlink_c*`
- any Dusk sidecar raw pointer
- local process IDs unless they have a dedicated network identity layer

Instead:

- Replicate player ownership as `PlayerSlot`.
- Replicate enemy/world actor identity through a future stable actor/network ID, not a pointer.
- Re-derive local pointers from slot registries or actor-ID lookup tables on each process.

Stable actor IDs are already a known diagnostics need. They become mandatory before enemy/world replication.

The eventual actor-ID layer should be a Dusk sidecar registry, not fields threaded through every enemy or item actor patch. Actor code should continue to pass local pointers to co-op modules; the registry can map those pointers to stable diagnostic/network IDs at module boundaries. That keeps current local co-op code simple while still supporting long-lived diagnostics, pointer-reuse disambiguation, actor duplication per player, and future host/client replication.

## Diagnostics Implications

The diagnostics recorder should keep recording both local facts and future role/session facts:

- `session_id`
- `role` (`host`, `client`, or `local`)
- local frame and monotonic time
- selected player slot
- reason
- scope
- local actor pointer as metadata only

When networking exists, host and client captures should be alignable by session ID and time/frame markers. The raw pointer fields can remain useful for local debugging, but they should never be interpreted as cross-process identity.

## Design Rules To Preserve

- Keep local co-op APIs network-shaped where it costs little: slots, command snapshots, authoritative sidecar decisions, and structured diagnostics.
- Do not make clients run enemy AI independently and hope decisions match.
- Do not replicate `EnemyTargetResult` wholesale without stripping or re-deriving pointer fields.
- Do not let debug overlays or diagnostics become the replication mechanism.
- Do not solve networking by adding transport before the command/state ownership seams are explicit.

## Open Future Work

- Add a stable actor/network identity layer for enemies, item actors, drops, and world objects.
- Add host/client command flow for player input before internet transport.
- Add `applyReplicatedEnemyTarget(...)` or equivalent when enemy state replication begins.
- Decide how much enemy presentation clients may predict between authoritative host updates.
- Extend diagnostics bundles so host/client captures can be compared by session, role, frame, and monotonic timestamp.
