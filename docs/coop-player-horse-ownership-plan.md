# Co-op Player Horse Ownership Plan

## Summary

Give every active player slot a distinct runtime Epona whenever the authored campaign Epona exists.
Keep the authored horse as the one canonical story/save actor, then spawn session-only clones for
additional player slots and route rider-owned gameplay through a narrow `horse_owner` sidecar.

This is not a blanket replacement of `dComIfGp_getHorseActor()`. Vanilla uses the same singleton
for campaign persistence, authored demos, player movement, horse input, reins, camera, collision,
world tags, enemies, and HUD state. Each callsite must first be classified as canonical story Epona,
slot-assigned runtime Epona, current rider, any active horse, or horse-local collision owner.

## Assumptions

- Slot 0 keeps the authored Epona actor and vanilla `dComIfGp_getHorseActor()` identity.
- Each registered additional player gets one session-only runtime Epona clone while the canonical
  Epona exists.
- Runtime clone actors never overwrite horse restart/save data and never become the campaign horse.
- Runtime clones are slot-assigned: P2 calls, mounts, controls, and renders P2's Epona rather than
  racing P1 for whichever horse was touched last.
- Authored story, rodeo, NPC, event-camera, and save paths remain canonical until individually
  classified.
- The registry is shaped for four local player slots, but the first manual proof targets P1/P2.

## Current Evidence

- `daHorse_c::create()` rejects any second horse as soon as `dComIfGp_getHorseActor()` is non-null,
  then always installs itself into that global pointer.
- Horse-local movement reads P1 through `daAlink_getAlinkActorClass()`, direct `PAD_1`, and P1's
  camera angle even after a horse actor already exists.
- Mounted ALINK code contains many `dComIfGp_getHorseActor()` reads despite already owning a rider
  relationship through `mRideAcKeep`.
- The camera ride path owns `mpPlayerActor` but still resolves the horse through the global
  singleton.
- Epona's PC frame-interpolation buffers for reins are file-static. A second horse would overwrite
  the first horse's previous/current control points unless interpolation state becomes per-horse.
- Epona installs `m_mtxcalc` onto the shared horse model-data root joint. A second horse leaves its
  calculator active for both models unless each actor rebinds its own calculator at evaluation time.
- Horse BCK archive wrappers also carry mutable frame state. Runtime clones need actor-local
  wrappers over the shared immutable animation key data.
- Horse scene-exit collection executes once per horse but writes through the global singleton.
- Special-wall background collision receives horse pass flags after the calling horse PID has been
  discarded. It needs an explicit horse-local collision context, not an iteration over every horse.
- `dMeter2Info_setHorseLifeCount()` is written from horse execution and remains one global meter
  field. Independent lash presentation is a later HUD-owner question.

## Ownership Boundary

### Canonical Campaign Epona

Keep `dComIfGp_getHorseActor()` for authored global questions:

- horse restart/save position;
- story placement tags and NPC lookups;
- authored rodeo/demo actors;
- event-camera scripts that intentionally stage the campaign horse;
- compatibility fallback when no slot-local horse exists.

### Slot-Assigned Runtime Epona

Use `dusk::coop::horse_owner` when the question is:

- which Epona belongs to this ALINK slot;
- which ALINK slot owns this Epona clone;
- which horse should a rider mount, call, control, or render alongside;
- which registered horses need per-frame rein interpolation;
- whether an actor is the canonical campaign horse or a runtime clone.

### Horse-Local Collision

Keep horse-local wall checks separate from slot assignment. The relevant question is:

> Which horse actor initiated this background line check?

The low-level polygon pass should not guess from `dComIfGp_getHorseActor()` and should not iterate
all registered horses.

## API Shape

Add `dusk::coop::horse_owner`:

```cpp
bool isAdditionalHorseSpawnRequest(const fopAc_ac_c* actor);
PlayerSlot getAdditionalHorseSpawnRequestSlot(const fopAc_ac_c* actor);

void registerHorse(PlayerSlot slot, daHorse_c* horse);
void unregisterHorse(PlayerSlot slot, const daHorse_c* horse);

daHorse_c* getHorse(PlayerSlot slot);
daHorse_c* getHorseForPlayer(const daAlink_c* player);
daAlink_c* getPlayerForHorse(const daHorse_c* horse);
PlayerSlot getSlotForHorse(const daHorse_c* horse);
bool isCanonicalHorse(const daHorse_c* horse);

void ensureHorseForSlot(PlayerSlot slot);
void ensureAdditionalHorses();
void releaseHorseForSlot(PlayerSlot slot);

J3DAnmTransform* localizeAnimationTransform(daHorse_c* horse, J3DAnmTransform* animation);
int getLocalizedAnimationCount(const daHorse_c* horse);
void copyReinSimulationState(daHorse_c* horse, const cXyz* points, int count);
void lerpRegisteredHorseReins(f32 alpha);
```

P1 forwards to the canonical horse. Additional slots use sidecar state. Missing or invalid owners
fall back to P1 only where vanilla compatibility is a valid answer.

The first implementation may keep horse-local collision context in a separate scoped helper inside
`horse_owner`; do not expose it as a broad mount API.

## Checkpoints

### 1. Add Slot-Local Epona Lifecycle

- Add `horse_owner` registry and structured `horse.owner` diagnostics.
- Encode additional horse spawn requests with negative actor arguments, mirroring ALINK's
  registration bootstrap without reusing ALINK identity.
- Allow canonical horse creation to install the vanilla pointer and then ensure clones for active
  additional player slots.
- Allow later additional-player registration to spawn a clone if canonical Epona already exists.
- Let runtime clone creation bypass the duplicate-horse rejection and canonical restart/save
  placement path.
- Unregister runtime clones without clearing the vanilla horse pointer.
- Keep per-horse rein interpolation snapshots in Dusk-owned sidecar state and interpolate every
  registered horse before 3D line refresh.
- Route enough horse-local execution, rein attachment, and draw subjectivity reads through the
  assigned ALINK to make a spawned idle clone safe and deterministic.

Manual proof:

- Enter an Epona area, enable split screen, and confirm two horses exist without a crash.
- Enable split screen before entering an Epona area and confirm the canonical spawn creates P2's
  clone.
- Confirm P1's authored Epona remains stable and both rein lines render independently.

### 2. Route Rider-Owned Epona Gameplay

- Replace horse-local P1 pad/camera reads with the assigned player's input snapshot and camera.
- Route mount acceptance, call-horse behavior, lash count, heavy-boots checks, movement, neck/rein
  positioning, and local sound through the assigned ALINK.
- Route mounted ALINK horse reads through `horse_owner::getHorseForPlayer()` or the retained
  `mRideAcKeep` actor where the rider relationship is already established.
- Route player-camera horse reads through the camera's owned ALINK.
- Decide lash HUD presentation through `hud_owner`; do not let clone execute order overwrite P1's
  global meter state.

Implementation status:

- Live ALINK horseback synchronization now prefers the rider's retained `mRideAcKeep` horse and
  falls back to `horse_owner` before mounting. Saddle position, rider animation frames, reins,
  stirrups, lash state, jump, turn, dismount, mounted item demos, scene-exit movement, shadows, and
  camera ride focus use that rider-local horse.
- Horseback boomerang, hookshot, bottle, and subject camera status bits use
  `player_camera_status` on PC.
- Runtime clones localize mutable BCK wrappers while sharing immutable archive animation data.
  Every horse model evaluation rebinds the calling actor's matrix calculator onto the shared
  model-data root joint.
- Authored Zelda-horse helpers and scene-start campaign placement intentionally remain canonical.
- Manual mounted-gameplay validation is still required before this checkpoint is considered
  complete.

Manual proof:

- P1 and P2 independently mount, dismount, call, steer, dash, stop, and use horseback items.
- Neither controller moves the other slot's horse or camera.
- Both horses preserve visible independent reins and shadows.

### 3. Route Horse-Local World Interaction

- Fix scene-exit area collection so each horse fills its own buffer.
- Route horse jump tags, horse-region switches, and physical gate interactions through registered
  horses where the rule is "any active horse."
- Preserve the initiating horse identity through special-wall background collision checks.
- Review collision callbacks that still ask P1 after already receiving the colliding ALINK.

Manual proof:

- Each horse can cross scene exits, jump fences, interact with field gates, and collide with
  horse-special walls independently.

### 4. Classify Authored Epona Systems

- Audit horse-start tags, rodeo tags, NPCs, authored event cameras, enemy horseback minigames, and
  save/restart state.
- Keep explicitly authored sequences canonical unless a real co-op behavior requires an owner-aware
  conversion.
- If scene placement moves canonical Epona, deliberately decide whether runtime clones follow,
  respawn beside their players, or remain untouched for that sequence.

Manual proof:

- Story Epona placement, saves, scene transitions, and authored horseback sequences retain vanilla
  behavior with split screen disabled and do not duplicate campaign state with split screen active.

## Diagnostics

Add a bounded `horse.owner` provider to the existing recorder.

`latest.json` should include:

- each registered horse slot, pointer, actor id, room, argument, canonical/runtime-clone flag,
  assigned ALINK pointer, horse position, speed, process, ride flag, lash count, and rein point
  count;
- canonical horse pointer;
- clone spawn/delete decisions;
- rider/horse mismatches;
- each ALINK slot's retained ride actor, retained-horse mismatch state, and whether multiple riders
  accidentally retain the same horse;
- registered rein interpolation entries;
- localized runtime-clone animation-wrapper counts and each horse's active animation indexes and
  frames.

`events.jsonl` should emit semantic changes only:

- horse register/unregister;
- clone spawn request/result;
- owner-slot change;
- mounted/unmounted transition;
- rider/horse mismatch transition.

Continuous position, speed, and rein values belong in `latest.json`, not the event key.

## Files And Hotspots

- `include/dusk/coop/horse_owner.h`
- `src/dusk/coop/horse_owner.cpp`
- `src/dusk/coop/player_slots.cpp`
- `src/d/actor/d_a_horse.cpp`
- `src/d/actor/d_a_alink.cpp`
- `src/d/actor/d_a_alink_horse.inc`
- `src/d/d_camera.cpp`
- `src/m_Do/m_Do_graphic.cpp`
- `src/d/d_bg_w.cpp`
- `src/d/d_bg_w_kcol.cpp`
- `src/d/actor/d_a_tag_hjump.cpp`
- `src/d/actor/d_a_swc00.cpp`
- `src/d/actor/d_a_obj_kgate.cpp`
- `src/d/actor/d_a_obj_rgate.cpp`
- `src/dusk/diagnostics.cpp`
- `docs/coop-player-singleton-api-map.md`
- `docs/coop-player-owner-lookup-audit.md`
- `docs/codex-hooks.md`
- `AGENTS.md`
- `files.cmake`

## Deferred Questions

- Whether players should ever be allowed to cross-mount another slot's Epona. V1 deliberately keeps
  slot assignment strict for deterministic ownership.
- Whether P2 needs a dedicated lash meter in the first mounted-gameplay pass or whether the meter
  can remain canonical until the broader HUD pass.
- Whether authored canonical-Epona reposition tags should also reposition runtime clones.
- Which enemy horseback/minigame reads should remain campaign-authoritative and which should react
  to any mounted player.
