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
- `checkAttentionLock()` is the first confirmed singleton hazard: route ALINK gameplay through `dusk::coop::player_attention` so additional players get slot-local `dAttention_c` state while P1/global attention can still drive camera/HUD/story uses.
- Diagnostics must keep `latest.json` rich and `events.jsonl` semantic. Continuous values may appear in latest snapshots or emitted payload context, but they should not drive JSONL events unless the profile is explicitly testing frame-level churn.
- In-game co-op debug overlays are for fast visual inspection only. Keep durable evidence in structured diagnostics and avoid making overlays mutate gameplay, diagnostics, or target policy.
- Runtime co-op identity should come from the player-slot registry (`getSlotForActor`, `isPlayerInSlot`, `isAdditionalPlayer`). ALINK negative actor arguments are only spawn-time bootstraps before extra-slot registration exists.
- Additional player spawning should go through slot-based `dusk::coop::spawnPlayer(...)`; ImGui and hotkeys are debug callers, not the lifecycle owner.
- Keep durable player-slot identity and requested-session slots in `dusk::coop::player_slots`; scene-local ALINK pointers die during area loads and requested additional slots rebuild only after the new primary ALINK completes creation.
- Reconstructed additional ALINKs are runtime joins, not second protagonists entering the area. Preserve structural `playerInit()` work, but do not register the global scene-start demo again or replay P1's authored area-entry proc; initialize an ordinary local action state instead.
- Primary ALINK form on create is native save/startup state. Retained desired-form caches exist for runtime additional slots across area loads; never let cached P1 form state override the actor flag that vanilla set before `setArcName()`.
- Rebuild requested additional ALINKs only after the new primary ALINK's native camera startup has settled. Spawning P2 while camera 0 is still in its area-entry run can push P1 into the wrong event-camera orientation.
- During requested-slot reconstruction, P2 owns camera ID 1 before its sidecar camera is ready. Gate P2 execution on native camera-1 readiness; never let reconstructed P2 borrow camera 0 during asynchronous camera startup.
- Shared ALINK body `J3DModelData` installs actor-local matrix calculators. Keep create-time evaluation, additional-player execute, and additional-player draw scoped through `dusk::coop::alink_model_data_owner`; historical ALINK isolation toggles stay in `alink_probes` and must not regain ownership-policy bits.
- Scripted interaction/demo reads must classify prompt ownership separately from accepted-event ownership. Use `interaction_owner` for "who can use this prompt?" and `event_owner` for "who requested this accepted event/demo?" before replacing player singletons; prompt-time wolf/form/side checks belong to `interaction_owner`, not `event_owner`.
- HUD prompt presentation is a separate ownership question. Use `hud_owner` for "which slot is this meter/HUD pass presenting?", while prompt eligibility stays in `interaction_owner`, accepted event input stays in `event_owner`, and gameplay prompt state stays in `player_button_status`.
- Singular authored sequences and captured fullscreen menu surfaces are presentation questions, not automatically interaction-owner conversions. Keep howling stones, maps, and Start menus P1/global in V1; use `event_presentation` for explicit owner-camera collapse and non-presenter hiding without disabling co-op simulation. Retained item-ring input stays in `ui_owner`, retained interactive dialogue input/presentation stays in `message_owner`, and authored fullscreen enemy swallow/grab cameras present the retained interaction slot while the retained effect owner remains in `retained_interaction_owner`. Localized enemy force-lock cameras such as the Tile Worm toss remain split-screen and affect only the retained player's viewport.
- A P2-owned fullscreen presentation still needs viewport-owned world refresh even though only one window is drawn. Keep culling bypass, real-shadow setup, kankyo material replay, and GX light reload active for camera 1; suppress only the second viewport and split-only framebuffer replay.
- Classify viewport effects by what native update retains. Shared gameplay actors, animations, and
  particle emitters simulate once, then filter or rebuild presentation per viewport. If native visual
  update consumes a camera/player and stores positions, alpha, room ratio, or other history, P2 is
  missing a visual simulation lifecycle: keep P1 canonical and give additional slots fixed sidecar
  history with private RNG. Never replay P1's completed camera-relative packet as a substitute.
- Camera-relative replay must use the exact matrix captured at native submission, including frame
  interpolation, rather than reconstructing from the later raw camera body. Viewport refresh and
  interpolation remain separate lifecycles even when they share submitted matrix state.
- Framebuffer captures are consumer dependencies, not one fullscreen-effect switch. Preserve the
  native capture phase each sampler expects, such as water before invisible lists and heat haze
  after ordinary particles, while independently gating motion blur, depth of field, fades, and mixed
  indirect-screen passes.
- Sense reveal has separate gameplay and presentation boundaries. Use `player_sense` for slot-local
  reveal/attention eligibility; submit shared spirit actors once, then use `render_visibility` and
  `render_effects` to filter reveal models, real shadows, reveal particles, and inverse wisps per
  viewport. Keep registration frame-scoped so down/dead phases cannot inherit stale hidden state.
  Per-viewport particle presentation may override draw alpha and restore it afterward, but must
  never start, stop, or advance the shared emitter between viewport replays.
- Generic `DEFAULT_GETITEM` sequences retain their collector through `item_get_owner`: only that ALINK consumes the singular staff track, `Demo_Item` follows the owner's live form/position, item text uses the owner's pad, and `event_presentation::ItemGet` presents that slot until the post-camera render handoff after native control restoration. Keep shared inventory/save mutation global.
- ItemGet teardown is ordered: classify the closing event from `event->getName()` while it is in END state, request release, let event `Step()` clear camera play, let camera actors consume recovery, then finish release at `mDoGph_Painter()` entry before window/render-policy sampling. `getRunEventName()` cannot classify END state, and releasing immediately after `setCameraPlay(0)` is still too early.
- Poe soul collection is the validated `item_get_owner` producer. Other `DEFAULT_GETITEM` sources must retain the exact collector before ordering/changing the event; do not assume the generic fallback proves pickup, chest, NPC, insect, key, or equipment ownership.
- Midna/manual wolf-transform ownership uses slot-local Midna service actors. Keep P1's Midna as the canonical story/save/global actor, but active additional players need runtime Midna copies registered through `midna_owner`; prompt eligibility, message branch reads, transform blocking, accepted transform demo handoff, physical service setup, and slot-local talk/camera status follow the service actor's ALINK slot. Active interactive dialogue must begin presentation after the native message controller accepts the message, with `talkStartInit()` as fallback insurance, then resolve listener ALINK, speaker Midna, input pad, and talk-camera fallback actor through `message_owner`, not P1's global Midna/form state or camera `mpPlayerActor`.
- Runtime service actors that native code may delete or recreate must retain both a pointer and a process ID, then validate the live actor before message/camera use. A retained pointer alone is not an ownership boundary.
- Camera/facing bugs need a three-frame ownership trace before code changes survive: accepted event/message owner, first actor/proc frame, and camera-consumption frame. This catches cases where P2 accepts correctly but an intermediate native proc turns back toward P1/global state.
- Same-frame teardown is unsafe around messages and cameras. If native message or actor teardown can run before event-camera consumption in the same management pass, defer owner release until the camera stops consuming the retained actors.
- Form-dependent runtime paths must ask the owning live actor (`link->checkWolf()`, owner Midna, owner camera status), not global saved/canonical form helpers such as P1 `daPy_py_c::checkNowWolf()` unless the path is deliberately story/save/global.
- Do not fix talk-camera framing with offsets first. The durable order is retained listener/speaker, correct native branch, owner-local form state, and teardown timing; camera constants or manual offsets come only after those are proven correct.
- Wolf combat/HUD ownership is split by domain: wolf lock targets stay actor-local on the acting ALINK, wolf charge/dome/lock camera bits use `player_camera_status::*ForPlayer()` across their full set/clear lifecycle, gameplay prompt writes including wolf X/Y use `player_button_status`, and upper-right wolf HUD presentation reads the native human/wolf meter branch through `hud_owner`.
- Use `player_item_selection` for runtime per-slot X/Y assignments and shared consumable-pool reads. Use `ui_owner` for transient UI presentation context, singular item-wheel ownership, and viewport-local 2D projection; `hud_owner` delegates its meter-presentation slot stack to that API.
- Hawkeye scope, ALINK live reticles, boomerang lock markers, and fishing rod forced-wheel entry are absorbed into `ui_owner`. Fishing line/bobber geometry remains world-render ownership; do not misclassify it as HUD.
- Epona ownership is its own `horse_owner` family. Keep the authored Epona as the canonical campaign/save actor, spawn slot-assigned runtime clones for additional players, and classify horse reads as canonical story Epona, slot-assigned horse, current rider, any active horse, or horse-local collision owner before patching. A canonical horse actor can exist while vanilla presentation keeps it parked in `FLG0_NO_DRAW_WAIT`; runtime clones must mirror that native lifecycle instead of treating registration as visibility. When vanilla requires canonical Epona to be presented, present parked runtime clones too: ordinary summons use each clone's native call-horse flow, while authored initialization tags apply the matching clone placement lifecycle.
- Enemy co-op work must classify touched singleton reads before patching: targeting, selected-target state, primary/global state, damage-owner, caught/grab-owner, or collision-owner. Do not do naive global replacements; use `enemy_targeting` policy for repeated search/chase/attack patterns.
- Enemy targeting conversions must use an actor-local helper near the top of each enemy file. The helper sets a reusable behavior scope such as `EnemyTargetScope::Combat`; callsite strings such as `e_oc.find` are diagnostic labels only and must not create independent retention state. Use target modes for callsite policy, e.g. awareness/wake reads may use immediate acquisition while chase/attack reads use sticky combat. Actor deletion must clear target sidecar state with `clearAllEnemyTargets`.
- Keep target producers and consumers distinct. Awareness/chase/attack state code may select or update a behavior-owned target; passive beam/model/presentation geometry must read retained target state without acquiring one or advancing retention before the native wake boundary.
- Damage-owner reads must use `dusk::coop::damage_owner`, not nearest-player or current enemy target. Use it for cut type/count, weapon owner, hit direction, and hit-reaction ownership questions where the real question is "who hit me?"
- Bespoke enemy damage counters are damage/collision identity too. Big Freezard proved that some enemies reset HP and count special hits manually; when a branch gives P1 direct-hit bonuses or special thresholds, route active-player/item identity deliberately instead of assuming shared `cc_at_check()` damage already covers it.
- Selected-target state reads must use `dusk::coop::selected_target_state` once a target identity is known. Use it for target speed, facing, position, form, wolf-sense, wolf bark/threat, horse, swim, guard, or damage-state facts; do not answer those with fresh nearest-player guesses or P1 globals.
- Enemy wake/read conversion is not complete until the follow-up producer state consumes the same owner. Shadow Insect proved that P2 lock-on/wake can work while surprise/fly/attack continuation still uses P1; advanced Poe proved active formation/orbit code can be separate from ordinary search. Trace the full wake -> state transition -> chase/attack producer chain before marking an enemy converted.
- Lock-on ownership has two questions: whether any player is locking an actor, and which player/mode owns that lock. Shadow Insect proved ordinary lock-on can wake an enemy while wolf lock has distinct movement-freeze behavior; use `player_attention` to identify the locking slot and owner-local wolf-lock state before broadening P1 lock branches.
- Pathing/steering after an enemy has selected a target is selected-target state too: obstacle checks, detour angles, chase line probes, and home-range checks should sample the retained target's facts. Group participation or "any player near this teammate" checks are different and should use `player_query`.
- Preserve the native distance dimension when converting enemy gates. XZ-only wake/drop/ground-range checks, such as ceiling drops and horizontal proximity tests, should stay XZ-only; use full 3D distance only when the original behavior actually depends on height, pitch, aerial eligibility, or projectile/attachment geometry.
- Awareness acquisition must choose among candidates that already pass the actor's native eligibility rules. Apply range, height, facing-cone, LOS, form, and status predicates before nearest-player selection; selecting nearest first and filtering afterward can let an ineligible P1 mask an eligible added player.
- Enemies that return before action code because an authored room switch is off are not targeting failures. Use `ghost_rat.state`/`world.switch`-style probes to distinguish upstream trigger producers from enemy-local authored wake gates. Patch known producers at the trigger; for enemy-local wake gates, preserve native flow by opening the same room switch from the active-player predicate rather than skipping the gate.
- Enemy-spawned weapons, child attacks, and child swarms must inherit the parent/master owner at the producer boundary. Chilfos thrown spear proved child projectile launch math should inherit the parent Combat target; Beehive/Bees proved parent hit-owner fields such as `mHitActorID` must store the damage/item owner before child swarms chase. Keep nontrivial target snapshots in helpers so native switch/fallthrough/goto layouts stay build-safe.
- Enemy/item-spawned objects need owner identity at creation time when native create() immediately consumes player state. Bomb Bug proved hookshot/boomerang-created NBOMB actors must inherit the damage owner before NBOMB setup chooses owner-local carry, boomerang, lifetime, or later damage-owner behavior; setting ownership after create may be too late.
- Boomerang-carried object movement is retained item ownership, not targeting. Shared helpers such as `daPy_boomerangMove_c` must retain the throwing slot from the hit collider or spawned object's owner so carried actors return to that player instead of P1.
- Defender/collision-owner reads must use `dusk::coop::defender_owner` when the real question is "who did my attack touch?" Use it for enemy-attack contact, guard/block, and defender-state checks; do not answer those with damage-owner, nearest-player, selected-target, or P1 globals.
- Retained stun/grab/carry/hang effects need a retained owner slot. Use `dusk::coop::caught_stun_owner` for Gibdo-style scream stun ownership, `dusk::coop::wolf_catch_owner` for wolf-bite enemy ownership, and `dusk::coop::retained_interaction_owner` for generic attach/carry/hang lifetimes such as Ghost Rat body attach or Peahat hookshot carry. Nearby players may be affected by the same retained effect, but do not recompute the owner from nearest player, current enemy target, or damage owner after the effect begins.
- Collectible draw-in/pickup effects are retained interactions too. Tears of Light proved the enemy death actor may only spawn effects while the separate pickup object owns collection radius, draw-in lines, and player effect callbacks. Retain the collecting slot with `retained_interaction_owner::Collect` at the pickup object, then clear it on object deletion.
- Retained enemy swallow/grab/carry demos split gameplay ownership from presentation. Retain the caught slot in the matching retained-owner API and choose that slot's player camera. Use `event_presentation::Source::EnemyRetainedInteraction` only when the native surface is intentionally fullscreen; begin it after native demo acceptance and end it when native camera state resets or the retained owner clears. Localized force-lock reactions keep split-screen active. If detector and demo owner are separate actors, key the retained owner to the actor that consumes the demo/camera state.
- Authored P1 enemy demos may collapse presentation without becoming retained-player interactions. Begin `event_presentation::Source::EnemyAuthoredDemo` only after native demo acceptance, keep trigger and choreography ownership unchanged, and release only after the actor resets/starts the native camera; actor deletion must also clear the source.
- Item/tool awareness is separate from combat targeting. Use `dusk::coop::item_awareness` or a narrow active-player owner scan for boomerang, hookshot, bomb, bait, and similar immediate reactions; do not force item reactions through sticky combat targeting or P1 static helpers.
- Flying enemies need vertical selected-target facts, not just XZ nearest-player checks. Keese, Shadow Keese, and Bubble proved that compact flyer hover/orbit/dive positioning can move through quick batches when combat targeting, selected-target state, retained wolf bite, damage ownership, and item/tool awareness stay in their separate API families. Do not apply this shortcut to flyers with riders, camera presentation, authored spawning, boss/setpiece phases, or new ownership surfaces.
- Mixed boss files can expose ordinary shared combat without surrendering authored encounter ownership. Aeralfos proved that the native per-frame player cache can come from one Combat owner outside demo/phase/death-camera states while camera 0, player placement, stage/BGM/switch flow, and other authored presentation remain P1/global.
- One actor profile may contain different co-op policy domains by subtype and state. Goron soldiers proved that later ordinary rollers, friendly NPC interaction, retained rider/carry behavior, and the first scripted retained-subject duel cannot inherit one blanket player policy; classify the active native branch before selecting, retaining, or presenting a player.
- Static/ranged enemies need the same scope discipline: wake/LOS may acquire immediately, but breath, bullet, and spawned-child continuation should read the retained Combat target through `selected_target_state`. Big Freezard proved vertical eligibility, head pitch, side gates, selected-player heavy-boots/status0 gates, child spawn facing, active-player projectile-hit counting, and bespoke special-hit counters fit this pattern.
- Mounted enemy checks split by ownership question. Ordinary selected-target horse state uses `selected_target_state` plus `horse_owner`, boar-mounted player state uses `selected_target_state`, and observed lock-on uses `player_attention`; rider-owned mount actors should inherit the rider's retained Combat target for steering instead of sampling P1 or owning an independent target. King Bulblin/bridge/castle actor-set paths, boar master/child setup, and demo cameras are authored setpiece/presentation surfaces until a dedicated encounter-owner plan owns them.
- Enemy batch conversions must leave an audit breadcrumb for every deferred or intentionally untouched player-singleton surface in the enemy's audit row. Include camera/presentation ownership, master/child or spawned-enemy ownership, hookshot/item awareness, caught/grab/swallow/hang ownership, culling/render visibility, and any remaining P1/global reads that are not converted in that pass.
- Enemy-side consumers are only half the audit. If converted behavior depends on player status, camera/status bits, equipment/form facts, mode flags, actor status, or cached action fields, grep the vanilla producer and clear/reset sites and mirror that lifecycle for the owning slot too. Bombfish proved that patching the enemy read without the ALINK swim-status producer still leaves P2 behavior broken.
- Split-screen rendering fixes must classify the render ownership question before touching original draw code. Use the Dusk render families: `render_visibility` for shared draw-culling decisions, `render_materials` for viewport-owned kankyo/J3D material state, `render_effects` for late world/effect versus fullscreen framebuffer ownership, and `render_shadows` for real-shadow culling or baked shadow matrix ownership.
- Do not repair added-player presentation by locally rewriting interpolated points, pane positions, draw-list contents, or shared render buffers unless diagnostics prove that exact native boundary owns the defect. First trace the vanilla producer, retained owner, submission point, viewport replay, and restore path; then extend the native ownership model at the narrowest durable API boundary. Treat symptom-only presentation patches as disposable experiments requiring explicit user approval.
- Viewport-dependent presentation refresh is not frame interpolation. Camera-facing ribbons, projections, and similar geometry replayed for multiple views must rebuild from submitted native state for each active viewport even when interpolation is disabled. Keep simulation interpolation and viewport presentation refresh as separate lifecycles.
- Native framebuffer consumers keep native texture identity. Water, refraction models, and projection-particle `fbtex_dummy` / `dummy` resources permanently bind the canonical framebuffer `ResTIMG`; refresh that canonical capture sequentially so Aurora updates the same resolved handle in place. Reset/rebuild only direct sampler objects proven to own the stale binding; do not substitute slot tokens, private addresses, or evict the canonical copy beneath shared native resources.
- For every added-player defect, begin with the native-first question: "What does P1 get from the vanilla lifecycle that P2 is missing?" Restore the missing lifecycle, state write, replay, actor setup, or presentation pass at its ownership boundary before considering local correction code. Apply this principle to gameplay, UI, camera, rendering, audio, physics, and actor services.
- Native-first includes producer/consumer tracing. A converted P2 consumer is not complete until the corresponding P1 producer, updater, and clear/reset path have been found and either proven owner-local already or patched/mirrored through the correct ownership API.
- Do not propose or implement "temporary now, proper later" co-op fixes unless the user explicitly asks for a disposable experiment. Measure the engine behavior as much as needed, then choose the durable architecture first so prototype debt does not become the project foundation.
- Switch-gated enemies may never reach their own awareness code until a generic authored area actor runs. Trace the switch producer; `SwAreaC`/`SwAreaS` form/equipment conditions and volume membership are any-eligible-active-player world rules, while their native switch type still owns latch/reset behavior.
- World triggers have three independent ownership axes: who may activate, who remains the scripted subject, and which view presents an accepted demo. Use `world_trigger` to retain the eligible slot at the native transition; do not override canonical Link getters or replay trigger actors. Generic `SwAreaC/S`, `TAG_EVENT`, and `TAG_EVT` activation stays P1-subject/split by default, while classified authored cameras opt into fullscreen only after native acceptance and release after camera reset/start.
- A broadened actor-local trigger cannot remain P1-subject when its camera seed and later continuation/termination checks consume the same Link. The first rolling Goron proved that doing so starts from remote Camera 0 and can abort/retrigger forever; retain the triggering subject across the complete native interaction and release at its real one-shot completion boundary.
- When an encounter changes from P1-only to triggering-player subject, audit earlier safety refusals in the player-side action producer as well as actor-side consumers. The first Goron still exposed a Grab prompt to P2 while `procGoatCatchInit()` immediately rejected additional ALINKs, producing a one-frame catch; remove obsolete admission guards once the complete native lifecycle is owner-local.

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
- `docs/coop-viewport-effects-plan.md`: per-viewport Gale Boomerang/magnetic projection refresh,
  Wolf Sense, Twilight camera lights, bloom, diagnostics, and pending field-validation matrix.
- `docs/coop-split-screen-api-audit.md`: reopened split-screen ownership audit for camera, viewport, render-state, culling, HUD, audio, and interaction API families.
- `docs/coop-singular-event-presentation-plan.md`: validated singular-presentation and generic ItemGet lifecycle evidence, including post-camera split rebuild and Camera-1 kankyo refresh rules.
- `docs/coop-p2-independent-control-plan.md`: control-ownership root plan for replacing the secondary ALINK shared-attention probe with slot-local attention, button status, item aiming, interactions, and deferred Hidden Skill training ownership.
- `docs/coop-player-horse-ownership-plan.md`: Epona ownership checkpoint and follow-up audit: canonical campaign horse compatibility, additional-player runtime clones, rider-local control, rein interpolation, collision, and authored-system classification.
- `docs/coop-network-multiplayer-readiness.md`: host-authoritative multiplayer readiness notes, especially player-slot identity, enemy-target replication seams, and pointer-vs-slot rules.
- `docs/coop-player-singleton-api-map.md`: central routing guide for `daPy_getPlayerActorClass()`, `dComIfGp_getPlayer(0)`, and which co-op API family should answer each player-identity question.
- `docs/coop-world-acknowledgement-plan.md`: completed world-acknowledgement proof: player-query helpers, diagnostics, Hanging Helmasaur proof, and Bokoblin raw-query evidence.
- `docs/coop-world-trigger-participation-plan.md`: current active milestone; central trigger participation, generic trigger families, actor-local proof encounters, diagnostics, scanner, and pending field-validation matrix.
- `docs/coop-player-owner-lookup-audit.md`: reusable audit table for item/weapon actors that still ask global P1 when they should ask the owning ALINK slot.
- `docs/coop-enemy-audit.md`: enemy/world acknowledgement map; world-trigger eligibility and authored fullscreen proofs are implemented for the first Goron, Darknut opening, and Shadow Kargarok intro pending field validation.
- `docs/coop-enemy-targeting-plan.md`: completed enemy-targeting architecture plan: policy-backed enemy targeting over `player_query`, with validated owner API families and enemy audit follow-up in `docs/coop-enemy-audit.md`.
- `../.codex/config.toml`: workspace-root Codex hook wiring. Keep hook behavior narrow and documented in `docs/codex-hooks.md`; copy it into the repo only if a future session opens `dusk-coop` directly.
- `files.cmake`: explicit source-file list. Update it when adding C++ source/header files that must be built.

The current active co-op plan is `docs/coop-world-trigger-participation-plan.md`. Keep milestone evidence there until build and field validation are complete; keep broader queues in the roadmap and audits.

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
