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
- Runtime clones mirror canonical Epona's native `FLG0_NO_DRAW_WAIT` state when created. Campaign
  Epona may exist as a parked actor in eligible areas without being presented; clone registration
  must not make an additional horse visible until its own native call-horse flow releases that wait.
- When an ordinary canonical Epona summon is accepted, parked runtime clones enter their own native
  call-horse flow against their assigned players. Do not expose clones by clearing draw state
  directly, and do not pull already-present clones toward P1's summon.
- When an authored horse-initialization tag explicitly presents canonical Epona, parked runtime
  clones receive the matching placement lifecycle with deterministic slot offsets. Already-present
  clones keep their rider-local state.
- Clone creation is asynchronous. If a canonical summon or placement arrives while a requested
  runtime clone is still pending, `horse_owner` retains that presentation transition and applies it
  when the clone registers.
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
- Horse scene-exit collection originally executed once per horse but wrote through the global
  singleton. Each executor now fills its own actor buffer, and shared exit producers recognize every
  active ALINK.
- Special-wall background collision receives horse pass flags after the calling horse PID has been
  discarded. It needs an explicit horse-local collision context, not an iteration over every horse.
- `dMeter2Info_setHorseLifeCount()` remains the canonical horse's vanilla global meter field.
  Additional horses now present their horse-local lash counts through `hud_owner` while replaying
  the same native spur presenter into their viewport.
- Manual split-screen testing exposed a rein-presentation ownership leak: the visible floating reins
  disappeared when P1 entered horseback first-person mode alongside P1's normally suppressed model,
  even when observing from P2. The correct fix was not rein-specific submission filtering or local
  line-point rewriting; it was restoring the native per-viewport ribbon expansion lifecycle.
- Bounded `render.lines` diagnostics confirmed that both mounted horses submit finite, horse-local
  rein materials, but neither received a presentation-eye expansion while frame interpolation was
  disabled. Textured 3D ribbons are camera-facing geometry: split-screen replay must rebuild every
  submitted textured ribbon for the active viewport eye regardless of interpolation state. Keep
  horse control-point interpolation conditional on frame interpolation; keep viewport expansion
  conditional on split-screen replay or interpolation.

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
bool shouldPresentLashMeter(PlayerSlot slot);
bool anyHorseNeedsLashMeter();

void ensureHorseForSlot(PlayerSlot slot);
void ensureAdditionalHorses();
void callParkedAdditionalHorsesForCanonicalSummon();
void presentParkedAdditionalHorsesForCanonicalPlacement(const cXyz& pos, s16 angle);
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
  Clone teardown releases those wrappers through the JKR allocator path before clearing sidecar
  state.
  Every horse model evaluation rebinds the calling actor's matrix calculator onto the shared
  model-data root joint.
- The native spur presenter now resolves lash counts through `hud_owner` and replays one presenter
  with independent per-slot animation and pane-alpha state into each mounted player's viewport.
  P1 keeps the vanilla global count and alpha lifecycle; runtime clones present their horse-local
  lash count and slot-local visibility without creating a second shared-pane presenter.
- Authored Zelda-horse helpers and scene-start campaign placement intentionally remain canonical.
- Manual mounted-gameplay validation is still required before this checkpoint is considered
  complete.

Manual proof:

- P1 and P2 independently mount, dismount, call, steer, dash, stop, and use horseback items.
- Neither controller moves the other slot's horse or camera.
- Both horses preserve visible independent reins and shadows.

### 3. Route Horse-Local World Interaction

- Each horse now fills its own scene-exit buffer. Shared field exits offer their native transition
  handoff to every active ALINK, and shared grotto exits recognize every active ALINK entering their
  native volume, so additional riders cannot leave the map without starting the shared scene change.
- Route horse jump tags, horse-region switches, and physical gate interactions through registered
  horses where the rule is "any active horse." Horse jump tags and horse-only `SwAreaC` volumes now
  run their native trigger tests for every registered horse. Kakariko and rider-gate horse panels
  resolve the registered Epona that entered each native panel area; ordinary player and coach paths
  remain unchanged.
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

#### Specialized Follow-Up Lanes

These lanes sit outside ordinary rider-local Epona control. Do not mass-convert their remaining
`dComIfGp_getHorseActor()` and P1 reads. Before editing a callsite, identify whether the native
question is about canonical campaign Epona, the initiating rider's assigned horse, any registered
horse, a selected enemy target, a damage source, or one singular authored presentation.

##### Horse-Start Placement Tags And Canonical Repositioning

This lane covers authored stage placement, restart positioning, horse stop volumes, and warp-demo
staging. These systems can deliberately move or hide campaign Epona independently of ordinary rider
control.

Start with:

- `src/d/actor/d_a_tag_hinit.cpp`: `daTagHinit_c::execute()` directly repositions canonical Epona
  through `setHorsePosAndAngle()` after stage switches and event bits permit the placement.
- `src/d/actor/d_a_tag_hstop.cpp`: horse-stop regions, mounted bow-training switches, floating
  message flow, and canonical horse turn-stand state.
- `src/d/actor/d_a_alink.cpp`: `checkHorseStart()` and ALINK scene-start setup decide whether the
  entering player starts mounted and how canonical Epona participates.
- `src/d/actor/d_a_e_warpappear.cpp`: warp-demo staging explicitly resolves P1, camera 0, and
  canonical Epona while placing the bridge-warp sequence.
- `src/d/actor/d_a_no_chg_room.cpp`: no-room-change transitions borrow canonical horse height.
- `src/d/actor/d_a_npc_aru.cpp`, `src/d/actor/d_a_npc_besu.cpp`,
  `src/d/actor/d_a_npc_bou.cpp`, `src/d/actor/d_a_npc_kolin.cpp`,
  `src/d/actor/d_a_npc_maro.cpp`, `src/d/actor/d_a_npc_post.cpp`, and
  `src/d/actor/d_a_npc_taro.cpp`: authored NPC scenes that search for or stage canonical Epona.

Initial stance:

- Preserve canonical movement when the scene is deliberately staging campaign Epona.
- Decide per tag whether runtime clones remain untouched, respawn beside their assigned players, or
  follow a deliberate shared scene reset.
- Keep save/restart writes canonical. Runtime clones remain session-only actors.

##### Rodeo And Cattle-Herding Behavior

This lane covers Ordon's goat-herding sequence and Epona's rodeo movement mode. It mixes global
minigame progression with rider-local horse movement and cow reactions.

Start with:

- `src/d/actor/d_a_tag_event.cpp`: type `5` tag events enter rodeo mode and currently enable it on
  canonical Epona.
- `src/d/actor/d_a_tag_camera.cpp`: camera-tag conditions include horseback jumping and rodeo mode,
  but still contain canonical-horse and P1 status reads.
- `src/d/actor/d_a_horse.cpp`: `setStickRodeoMove()`, the rodeo path fields, point counters, and cow
  collision callbacks are horse-local mechanics.
- `src/d/actor/d_a_alink_horse.inc`: rider-side rodeo input, balance, fall, and completion handling.
- `src/d/actor/d_a_cow.cpp`: cow proximity, surprise, lash, and movement reactions still search P1.
- `src/d/actor/d_a_alink_damage.inc`: mounted cow-hit knockback still resolves canonical Epona.
- `src/d/d_camera.cpp`: `CAM_TYPE_RODEO` is the dedicated rodeo camera mode.

Initial stance:

- Keep minigame completion and switch progression singular unless a co-op design explicitly changes
  the rules.
- Route horse movement and balance through the rider's assigned Epona.
- Decide whether cows react to any eligible mounted player, the nearest player, or the minigame's
  retained owner before converting their searches.

##### Zelda-On-Epona Helpers

This lane covers the final horseback battle's story Zelda actor, including her attachment point,
animations, bow, damage state, shadow, and campaign-horse material state.

Start with:

- `src/d/actor/d_a_hozelda.cpp`: repeated canonical-horse reads attach Zelda to Epona, choose ride
  offsets, update bow behavior, toggle bag material, and borrow Epona's shadow ID.
- `src/d/actor/d_a_alink_horse.inc`: `checkHorseZeldaBowMode()` and `setHorseZeldaDamage()` resolve
  Zelda through canonical Epona.
- `src/d/actor/d_a_alink.cpp`: the horseback jump action checks whether canonical Epona carries
  Zelda.
- `src/d/actor/d_a_b_gnd.cpp`: the Ganondorf horseback battle reads P1 ride state and canonical
  Epona speed throughout its encounter logic.

Initial stance:

- Keep Zelda attached to canonical campaign Epona unless an authored sequence demonstrably needs a
  different presentation owner.
- Do not duplicate Zelda for runtime clones.
- Treat additional riders in the final battle as a separate encounter-design decision from
  ordinary Epona ownership.

##### Event-Camera And Scripted Demo Sequences

This lane covers camera scripts and ALINK demos that intentionally stage mounted movement. These
paths often need one singular event owner even when ordinary riding is per-player.

Start with:

- `src/d/d_ev_camera.cpp`: event-camera tactics and item-camera collision exclusions still use P1
  ride state and canonical Epona at several branches.
- `src/d/actor/d_a_alink_demo.inc`: mounted demo setup, ride-actor comparison, jump checks, and
  scripted horse placement still consult canonical Epona.
- `src/d/actor/d_a_tag_camera.cpp`: authored camera volumes include horse, horse-jump, and rodeo
  conditions.
- `src/d/actor/d_a_e_warpappear.cpp`: bridge-warp demo logic is an example of a deliberately
  singular staged sequence.

Initial stance:

- Preserve one event-camera presentation owner for singular demos.
- When a demo is rider-triggered, retain the initiating ALINK and resolve that rider's assigned
  horse where the script is asking about the participant rather than campaign Epona.
- Audit presentation collapse needs separately; do not make every event camera per-viewport by
  default.

##### Horseback Combat Set Pieces

This lane covers encounter-specific rules built around mounted Link, King Bulblin, boars, mounted
enemies, and Ganondorf. These are not ordinary enemy-awareness conversions.

Start with:

- `src/d/actor/d_a_e_wb.cpp`: King Bulblin and boar encounter logic repeatedly asks whether P1 is
  riding and how fast canonical Epona is moving.
- `src/d/actor/d_a_b_gnd.cpp`: Ganondorf horseback battle movement and attack decisions use P1 and
  canonical Epona speed.
- `src/d/actor/d_a_e_fk.cpp`: mounted or special cavalry behavior resolves canonical Epona.
- `src/d/actor/d_a_e_kr.cpp`: horseback encounter behavior contains direct P1 ride and canonical
  speed reads.
- `src/d/actor/d_a_e_rd.cpp`, `src/d/actor/d_a_e_rdy.cpp`, and
  `src/d/actor/d_a_e_rdb.cpp`: mounted enemy and boar-rider families mix awareness, combat, and
  scripted encounter behavior.
- `src/d/actor/d_a_alink_bow.inc` and `src/d/actor/d_a_alink_damage.inc`: ALINK boar-battle bow and
  damage branches still consult canonical Epona.

Initial stance:

- Classify each encounter before conversion: some set pieces are campaign-authoritative, while
  others should follow a selected target or the initiating rider.
- Keep scripted encounter progression singular unless the encounter is deliberately redesigned for
  multiple riders.
- Reuse behavior-owned enemy target state where the native question is about an enemy's current
  opponent. Do not infer an owner from actor iteration order.

##### Enemy Reactions To Mounted Speed

Several enemy damage and behavior paths still decide reaction strength by asking whether P1 rides
canonical Epona above a speed threshold. In co-op this can ignore a fast P2 impact or apply P1's
speed to another player's attack.

Start with:

- `src/d/actor/d_a_e_dn.cpp`: mounted hit reactions check P1 ride state and canonical speed.
- `src/d/actor/d_a_e_mf.cpp`: mounted hit reactions contain the same P1-speed pattern.
- `src/d/actor/d_a_e_rd.cpp`: awareness, mounted damage, and encounter branches contain several
  canonical speed checks.
- `src/d/actor/d_a_e_rdy.cpp`: mounted reaction logic checks P1 and canonical speed.
- `src/d/actor/d_a_e_yr.cpp`: behavior and speed helpers resolve canonical Epona.
- `src/d/actor/d_a_e_kr.cpp`: mounted behavior checks canonical speed.
- `src/d/actor/d_a_bd.cpp`: movement tuning borrows canonical Epona speed while P1 is mounted.

Initial stance:

- For collision or damage reactions, resolve the striking ALINK or horse from hit provenance.
- For awareness or chase behavior, resolve the enemy's behavior-owned selected target and that
  target's assigned horse.
- Keep encounter-global checks in the set-piece lane rather than forcing them through generic enemy
  targeting APIs.

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
- Whether authored canonical-Epona reposition tags should also reposition runtime clones.
- Which enemy horseback/minigame reads should remain campaign-authoritative and which should react
  to any mounted player.
