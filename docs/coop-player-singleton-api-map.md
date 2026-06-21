# Co-op Player Singleton API Map

This is the central routing guide for original-game code that asks for "the player" through
`dComIfGp_getPlayer(0)`, `daPy_getPlayerActorClass()`, `daAlink_getAlinkActorClass()`, or nearby
singleton helpers.

Do not mass-replace these calls. Classify the question first, then route it to the narrow co-op API
that owns that kind of player identity.

## Vanilla Meaning

`daPy_getPlayerActorClass()` is the common vanilla shortcut for "Link as a `daPy_py_c*`." In this
codebase it ultimately means P1/the story protagonist because vanilla Twilight Princess has one
player actor.

In co-op, that helper should be read as **primary/global protagonist** unless a callsite is
deliberately converted. Some callsites should remain P1/global forever or until a dedicated story,
camera, HUD, or save-system milestone exists.

## Why These Questions Split Apart

Vanilla code often makes different gameplay decisions through the same P1 singleton helpers. That
was fine when the game only had one meaningful player, because "the target," "the attacker," "the
defender," and "the story protagonist" were all Link. In co-op, those identities can be different
actors in the same frame, so each callsite must be classified by the gameplay question it is asking.

`enemy_targeting` answers **"who am I fighting?"** It owns search, chase, facing, attack-range, and
follow-through target selection. This is where sticky combat targets and committed attack retention
belong.

`damage_owner` answers **"who hit me?"** It owns hit reactions, cut type/count, weapon owner, and
attacker-state reads tied to the collider that caused damage. This must not use nearest player or
current enemy target, because an enemy can be fighting P1 while P2 hits it from behind.

Selected-target state helpers answer **"what is my current target doing?"** These are behavior
reads after the enemy already has a target: target speed, facing, form, horse state, swim state,
guard state, damage state, or similar facts. These should follow the selected target, not P1 and not
a fresh nearest-player query.

Pathing and obstacle steering after an enemy has selected a combat target are selected-target state
too. If a chase state has already retained P2, detour angles, obstacle line checks, and move-out
home-range decisions should sample that selected player's position/angle rather than asking P1 or
running a new nearest-player selection. Group wake-up or battle-participation checks are different:
when the vanilla question is "is any active player close enough to this teammate?", use
`player_query` directly instead of sticky combat targeting.

`world_trigger` / switch activation answers **"which active player can trip this authored room
trigger?"** These are wake or event gates that may run before an enemy's own AI. First distinguish
an upstream trigger producer from an enemy-local authored wake gate. Patch known producers at the
trigger. For an enemy-local gate, preserve native flow by opening the same room switch from the
active-player predicate rather than skipping the gate. `world.switch` diagnostics record switch
activations so room gates can be traced back to their source or to direct switch plumbing.

`defender_owner` answers **"who did my attack touch?"** These are enemy-attack contact reads such as
"which player blocked this swing?" They are separate from `damage_owner` because the enemy is the
attacker and the player is the defender.

The first proof surface is Bokoblin guard collision. See
`docs/coop-defender-owner-contact-investigation.md` for the current attack-sphere findings and the
known V1 limitation that a sphere's retained hit object is not a full multi-contact trace.

`caught_stun_owner` answers **"which player is retained by this enemy effect?"** These are not
ordinary target or damage reads: once a grab, scream, stun, carry, or hang begins, the enemy must
keep talking to that same player slot until release. The first proof surface is Gibdo scream stun,
which now binds release input and best-effort camera lock to the retained slot, while also allowing
the same scream timer to animate nearby affected player slots.

`wolf_catch_owner` answers the wolf-specific retained physical version of that question: **"which
wolf player currently owns this bitten enemy?"** The hit that starts the bite is still resolved
through `damage_owner`, but once the bite begins, release checks, left/right throw state, and
mouth-matrix attachment must follow the retained wolf slot. Keese is the first proof surface.

`retained_interaction_owner` answers the generic retained-object version of the same family:
**"which player owns this ongoing attach/carry/hang interaction?"** Use it when the interaction is
not the specialized Gibdo scream-stun or wolf-bite mouth hold. Ghost Rat uses it for body attachment,
heavy-state counting, Midna rat-body routing, and joint attachment; Peahat uses it for hookshot
carry/hang status, heavy-boots checks, and carry-offset writes. Deku Baba uses the same retained
family for bite/caught lifetime after defender contact establishes the caught player; Slime Chu uses
it for hookshot core carry after `damage_owner` establishes the hookshot owner.

Gibdo currently preserves the vanilla single global scream owner (`m_cry_gi`) because that pointer
also coordinates follow-up attacks between Gibdos. Dusk broadens the affected-player range for the
single owned scream instead of allowing simultaneous per-player screams. A future retained-effect
policy may allow one active scream per player slot, but that must explicitly preserve or replace
the native group choreography.

White Wolfos added two adjacent ownership lessons that should be remembered during batch enemy
work. First, master/child or spawned-enemy logic is not automatically P1-owned: the parent/master
may need active-player awareness to decide when to spawn or wake children, and child presentation
may need to face an encounter anchor or selected target rather than P1. Second, camera presentation
reads are not combat targeting reads. If an enemy chooses an angle for a spawn intro, flourish, or
camera-facing presentation, route it through a presentation-aware helper that can use the selected
slot's camera when available, and leave a documented deferred hook when the proper presentation API
does not exist yet.

Chilfos added the attack-owned child-weapon version of the same rule. A spawned spear is not an
independent enemy choosing a target from scratch; it is a child attack launched by the parent
Chilfos. Its launch angle, vertical aim, and distance math should inherit the parent's retained
Combat target when the parent is live, then fall back locally only if the parent is gone. When
patching old switch/fallthrough/goto-heavy enemy code, hide nontrivial selected-target snapshots in
file-scope helpers and keep switch cases to primitive assignments so C++ lifetime rules do not fight
the decompiled control flow.

Bomb Bug added the item-hit-spawned object version: when a player-owned hookshot or boomerang hit
creates an enemy bomb, the spawned NBOMB needs the damage owner before its `create()` method runs,
because create-time logic immediately chooses owner-local hookshot carry, boomerang movement,
lifetime, and later damage-owner behavior. Use owner-aware creation or parent/owner plumbing at the
spawn boundary; setting ownership after create may be too late for native setup.

Boomerang-carried object movement is a retained item-owner path. The vanilla shared
`daPy_boomerangMove_c` helper asks P1's thrown boomerang and P1's catch/carry state when it moves
an actor around the Gale Boomerang. In co-op, the helper must retain the throwing slot from the hit
collider or from the spawned object's owner, then use that slot's thrown boomerang and
`checkBoomerangCarry()` lifecycle until the carried object is released. This is why a P2-owned
Bomb Bug enemy bomb needs both owner-aware NBOMB creation and owner-aware boomerang movement.

Hookshot, boomerang, bomb, bait, and similar item-awareness checks are also their own question:
"which active player or owned item should this enemy react to?" White Wolfos side-step awareness
proved that these can be active-player scans without changing combat target ownership. Do not answer
item-awareness with P1 globals by habit, but also do not force it through sticky combat targeting if
the vanilla behavior is an immediate reaction to an item/tool state.

`item_awareness` is the first Dusk-owned helper for that family. Keese and Guay wind now scan active
owner-local boomerang actor keeps so a P2 boomerang can drive the same wind/follow behavior without
becoming the enemy's retained combat target.

Keese, Shadow Keese, Bubble, and Guay define the batchable compact-flyer pattern: use
`enemy_targeting` for combat identity, `selected_target_state` for vertical
position/height/orbit/dive facts, `damage_owner` for owner-sensitive hit reactions or the hit that
starts a wolf bite, `wolf_catch_owner` for retained mouth/throw/release lifetime when the enemy has
that state, and `item_awareness` for immediate tool reactions such as boomerang wind when the enemy
has that state. Similar compact flyers can move through quick batches when they expose the same API
families and no rider, authored spawn, boss/setpiece, or new ownership surface. Guay adds one
presentation wrinkle: if a dive/staging gate compares target angle to camera angle, use the selected
slot's presentation camera when available instead of P1's camera. Do not collapse those families
into one broader "flying enemy" helper unless more validated enemies prove the same narrower shape.

Kargarok (`E_KR`) confirms that ordinary flyer combat can share the compact-flyer conversion shape
even when the file also contains mounted/path/setpiece code. View/wake, auto movement, dive attack,
horse pursuit, dispatcher-cache distance/angle/pitch, and attack guard contact belong to
`enemy_targeting`, `selected_target_state`, `horse_owner`, and `defender_owner`. Coach/path,
cargo-bomb, boomerang lock-cancel, and authored setpiece targets remain separate mounted/setpiece or
item-awareness questions and should not be flattened into the ordinary Combat owner.

Rat (`E_MS`) and Goose (`E_YG`) define the batchable small-ground wolf-bite pattern: fill the native
angle/distance dispatcher cache from the Combat owner, answer wolf-bark fear with an active-player
`selected_target_state` predicate scan, start wolf-bite ownership from `damage_owner`, then keep
release, throw direction, and mouth-matrix attachment in `wolf_catch_owner`. Their skull/dokuro
searches are object-owned behavior and should not be converted as player targeting.

Deku Like (`E_DF`) adds the swallow/eat version of retained interaction ownership. The wake question
is "which active falling player entered the mouth volume?", so it uses selected/player-state facts
such as slot-local `status0_0x100`, height, and distance. Once the actor starts eating Link, the
question changes to "which player is currently swallowed?", so `retained_interaction_owner` owns
the ALINK through hold, repositioning, spit-out, throw damage, cleanup, and delete. Bomb/object
eating remains object-owned behavior. Link-swallow camera/demo presentation is then a separate
`event_presentation::EnemyRetainedInteraction` surface that presents the retained swallowed slot
fullscreen; it does not change the retained gameplay owner or become combat targeting.

Twilit Carrier Kargarok (`E_YC`) and Rider (`E_RDY`) add the split-actor carry version of the same
rule. The carrier detects the caught selected target, but the rider owns the native carry demo and
camera state machine. Key the retained `Carry` owner to the actor that consumes the demo/camera
state, then let that actor choose the retained slot's player and player camera for `changeDemoMode`,
throw damage, camera stop/reset, and `event_presentation::EnemyRetainedInteraction`. Shadow Bulblin's
ordinary wake/LOS, combat cache, guard, damage, mounted-hit, mobile/stationary bow steering and
arrows, bark/wolf/down, and finishing-blow paths can use normal combat/damage/defender/horse
ownership while carry demo modes 1-4 continue to use the retained caught slot. Bridge, field,
Kargarok, `BOW_IKKI2`, and later demo modes stay authored
until a dedicated setpiece-owner policy classifies them.

Bulblin Rider (`E_RD`) is the mounted/setpiece boundary for regular-enemy batching. Ordinary Link
combat can still use the usual dispatcher-cache pattern: fill native distance/angle fields from one
Combat owner, route guard contact through `defender_owner`, and route hit reactions through
`damage_owner`. Ordinary mounted run/attack gates add three ownership questions: selected-target
horse facts should come from `selected_target_state` plus `horse_owner`, selected-target boar state
should come from `selected_target_state`, and "is the mounted opponent locking onto me?" should
observe slot-local `player_attention`. But Rider also deliberately targets non-player actors such as
the coach/wagon; ordinary bow aim and spawned arrow launch math should inherit the retained Combat
target while preserving native coach-arrow homing only for those authored non-player targets. Rider
also owns a separate boar actor in ordinary mounted combat. That mount should not select a target of
its own; it should derive steering, pass checks, range checks, and selected-slot horse-speed gates
from the mounted Rider's retained Combat target. Rider also contains bridge, King Bulblin-style,
horn, death notification, and demo-camera paths. Preserve those authored targets until a
mounted/setpiece or presentation API specifically owns the question; do not flatten them into
nearest-player targeting.

King Bulblin (`E_RDB`) applies the same boundary inside one actor. Ordinary on-foot combat can retain
a sticky player target, resolve cut/facing/death reactions from `damage_owner`, and notice owner-local
hookshot or Ball-and-Chain state. LV9/start/end progression, mount behavior, and demo camera/player
placement remain encounter-authored P1/global state rather than nearest-player combat.

Poe (`E_HP`) shows that ghost visibility and vulnerability can be selected/active-player state
without becoming combat targeting. Wake, move, attack, and down-state steering use the Combat owner;
wolf-form and wolf-sense reveal gates scan active players. Once a wolf's native proc actor points at
the Poe and enters pull-out, retain that exact ALINK as a `Collect` owner for soul draw suppression
and item-event conditions. Bridge that retained collector into `item_get_owner` before
`DEFAULT_GETITEM` is ordered so the owner-only ALINK staff, `Demo_Item` position/form, item-message
pad, slot-local item-get status, and fullscreen presentation all agree. Camera-facing soul
billboards remain a separate per-viewport presentation question.
The larger Poe variant (`E_PO`) reinforces that split: ordinary action caches, wolf-sense wake and
shared reveal palette, advanced rolling/circling formation, and down-position geometry can share the ghost targeting shape,
while the exact wolf-down proc owner is retained through soul draw suppression, dead-state
camera/player selection and hang angle, lock cleanup, and the `item_get_owner` handoff.
Opening/limbering/roll/holl/formation demos and billboard replay remain authored presentation
surfaces. Do not classify them as combat targeting just because the same file also has search and
attack movement.

Shadow Insects (`E_YM`) use the same selected-target cache pattern for ordinary movement/attack
distance and angle. Their surprise-lock, surprise-near, surprise recovery, and fly/orbit states also
consume those facts, so converting only the wake/read path is incomplete if the follow-up producer
still points at P1. They also prove that "is this actor locked?" and "which player/mode locked it?"
are different questions. Surprise wake can observe slot-local `player_attention`, but branches that
sample player position or distinguish ordinary lock-on from wolf lock must use the locking player
and the owner-local wolf-lock state, not a stale Combat target and not a generic any-lock boolean.
Wolf-sense reveal, wolf-bark reaction, crash/front-roll triggers, rail-surprise status, down wall
trace, attack-wall/end movement, wolf-lock-cut observation/cancellation, tear collection, river/rail
scripting, Midna-sensitive paths, and camera visibility are separate player-state or presentation
questions. The first group now uses active-player/owner APIs; authored river/rail movement, Midna,
and camera presentation remain explicit deferred surfaces.

Tears of Light (`Obj_Drop`) are the retained collection version of this split. Shadow Insect death
plays disappear effects, but the actual pickup object owns the native collection radius, draw-in
lines, and `onWolfLightDropGet()` call. The active player who enters that native radius should be
retained as the `Collect` owner until the drop deletes. Do not solve Tear pickup by retargeting
Shadow Insect combat AI.

Beehive/Bees (`E_NEST`/`E_BEE`) are the parent-child swarm version of the same ownership rule. The
beehive stores the actor that disturbed it in `mHitActorID`, and child bees chase that actor later.
That parent producer must store the damage/item owner before the bees spawn or fly; fixing only the
bee contact/follow consumer still lets a P2 hit create a P1-targeted swarm.

Stalfos (`E_SF`) is the regular-humanoid-with-authored-demo boundary. Combat, guard contact, and
damage reactions can use the existing targeting, defender, and damage-owner families, but first
contact intro cameras, `changeOriginalDemo()`, and `setPlayerPosAndAngle()` are authored
presentation/protagonist paths. Do not route those through enemy targeting just because the same file
also has ordinary combat AI.

Static/ranged enemies follow the same identity split but need stricter callsite policy: wake/LOS
may acquire immediately, while breath, bullet, or spawned-child attack continuation should read the
retained Combat target through `selected_target_state`. Big Freezard (`E_FB`) uses this for vertical
eligibility, head pitch, caged/sweep side gates, Mini Freezard spawn facing, and active-player
bullet hit counting. Big Freezard and Bombfish split two similar-looking ownership families:
equipment facts such as heavy boots (`FLG0_EQUIP_HVY_BOOTS`) must come from the selected player
actor through `selected_target_state`, while true camera/status flags remain slot-local
`player_camera_status` checks once the enemy already knows which player it means.
Shell Blade (`E_SB`) added another selected-target status proof: status0 `0x4000` is sampled from
the selected slot for hookshot/player-camera-state shell reaction gates instead of asking P1's
global status.
Bombfish (`E_BG`) and Skullfish (`E_SG`) add the first quick water-enemy batch pattern. Ordinary
water wake, swim/charge/follow, selected-player heavy-boots checks, selected-slot hook-carry gates,
camera-relative movement, retained bite release/status, and damage knockback ownership can use the
existing Combat, selected-target-state, player-camera-status, and damage-owner APIs. Fishing-rod
bait/eat searches, bomb/cargo state, hook-carry object behavior, and broader water/fishing ownership
are object-owned or item-awareness surfaces; leave them documented instead of routing them through
sticky Combat.
Bombfish also proved the producer side of the native-first rule: an enemy-side read can be converted
correctly and still fail if P2 never receives the vanilla player state that P1 gets. `E_BG` used
status0 `0x100000` as a water/swim eligibility gate, but that bit was still produced by ALINK swim
lifecycles only through global P1 status. Future conversions that read status bits, mode flags,
equipment/form facts, or cached action fields must grep and audit the matching set/update plus
clear/reset producers before the behavior is considered covered.
Dodongo (`E_DD`) confirmed the dispatcher-cache pattern: some enemies compute one native
"player angle/distance" pair at the top of the action dispatcher and let every state consume those
fields. Convert that cache from the Combat owner once per tick, then leave search, chase, attack,
flame, and reaction state code using the native fields. Only split out separate families when the
question changes, such as `damage_owner` for the player who caused a tail/jump-cancel reaction or
object-owned bomb suction/search.

Split-screen follows the same classification rule outside enemy code. Camera, viewport, HUD,
lighting, audio, and render-culling questions should not be patched as generic "P2 fixes." Route
them through the split-screen ownership families recorded in
`docs/coop-split-screen-api-audit.md`. In particular, draw frustum culling is a
`render_visibility` question, not a combat, selected-target, or player-query question.

P2 control independence follows the same rule inside ALINK. The old secondary-only
`Ignore shared attention lock` probe was a containment flag, not the final design. Player lock-on,
guard/block availability, first-person item camera modes, prompts, and Hidden Skill training need
slot-local ownership APIs so P2 can answer "what am I targeting or doing?" without consuming P1's
global attention/status state. See `docs/coop-p2-independent-control-plan.md`.

Accepted events and demos are also not automatically primary-player state. The event manager already
retains the requester in `Pt1`, so scripted interaction code should ask `event_owner` when the
question is "which player started this accepted event?" Door demos are the first proof surface:
prompt eligibility may still be interaction-owned, but the accepted door animation/placement belongs
to the requesting player slot.

Accepted event/catch presentation details can show up inside enemy files too. Chuchu 2's bottle
catch effect matrix is not targeting, damage ownership, or nearest-player state; it belongs to the
player who owns the accepted catch event, so it routes through `event_owner`.

## Routing Table

| Question the callsite is asking | Use | Current status |
| --- | --- | --- |
| "Which co-op slot owns this actor?" | `dusk::coop::player_slots` | Implemented |
| "Which additional slots should be rebuilt after an area load?" | requested-session state in `dusk::coop::player_slots` | Implemented; scene-local actors unregister normally and the completed new primary ALINK respawns requested slots |
| "Which ALINK temporarily owns the shared body model-data calculators?" | `dusk::coop::alink_model_data_owner` | Implemented for additional-player startup model evaluation, execute, and draw; each scope restores P1 afterward |
| "Which active player is nearest or eligible by raw distance/angle facts?" | `dusk::coop::player_query` | Implemented |
| "Who is this enemy fighting right now?" | `dusk::coop::enemy_targeting` | Implemented for scoped combat targeting |
| "Who caused this hit?" | `dusk::coop::damage_owner` / narrow actor-local collision owner checks | Implemented for direct players and known owned items; Big Freezard added a bespoke direct-hit counter proof for enemies that bypass normal HP |
| "What is the selected target's form/speed/guard/horse/boar/swim/damage/status state?" | `dusk::coop::selected_target_state` | Initial implementation for target speed/facing/position/cut/horse facts; Bulblin Rider added selected-target boar-ride state for mounted attack volume; Chilfos added selected-target damage-wait and slot-local status bits such as `0x100` and iron-ball subject mode; Bomb Bug added selected-target collision-status band checks through `player_camera_status`; Ghost Rat added owner-local wolf-sense visibility facts; Shadow Beast added owner-local wolf bark/threat facts |
| "What position/angle should this enemy use for chase detours after it already selected a target?" | `dusk::coop::selected_target_state` | Bokoblin obstacle steering proof uses selected target facts instead of P1 globals |
| "Is any active player near this enemy/teammate for group wake-up?" | `dusk::coop::player_query` | Bokoblin group battle participation uses nearest active-player facts |
| "Which active player can trip this authored trigger or room switch?" | `dusk::coop::world_trigger` plus `world.trigger` / `world.switch` diagnostics | Implemented for `SwAreaC/S`, `TAG_EVENT`, `TAG_EVT`, first rolling Goron, Darknut opening, and Shadow Kargarok `ACT_TKUSA` intro pending field validation. Activation, scripted subject, and presentation are independent policies; generic families retain the accepted slot while keeping P1 as subject, while the first Goron retains its triggering subject because camera seed and continuation checks consume the same Link. |
| "Who did this enemy attack touch, and was that player guarding/blocking?" | `dusk::coop::defender_owner` | Initial direct-player implementation for Bokoblin guard collision |
| "Which player collided, rode, pushed, stood on, or picked this up?" | broader collision-owner helpers | Not implemented yet |
| "Which player is caught, stunned, grabbed, carried, swallowed, or retained by this actor?" | `dusk::coop::caught_stun_owner` / `dusk::coop::wolf_catch_owner` / `dusk::coop::retained_interaction_owner` / future caught-grab helpers | Gibdo scream stun uses `caught_stun_owner`; Keese wolf bites use `wolf_catch_owner`; Skulltula `StCaught`, Ghost Rat attach, Peahat hookshot carry, Deku Like swallow, Twilit Carrier Rider-carry, and Poe soul pull use `retained_interaction_owner` |
| "Which active player or owned item should this enemy notice immediately?" | `dusk::coop::item_awareness` / narrow active-player scans | Initial hookshot-awareness proof in White Wolfos; Keese boomerang wind uses `item_awareness` |
| "Which player/camera owns this spawn intro, child facing, or presentation angle?" | future presentation/camera-owner helpers | White Wolfos uses a narrow helper; broader API deferred |
| "Which target or owner should this enemy-spawned weapon/object inherit?" | parent/master `enemy_targeting` scope, `damage_owner`, or retained interaction owner depending on the source | Chilfos thrown spear launch math inherits the parent Combat target when the parent is live; Bomb Bug hookshot/boomerang-created bombs inherit the player/item damage owner before NBOMB create-time setup |
| "Which player owns this boomerang-carried object until it is released?" | retained item-owner plumbing on `daPy_boomerangMove_c` / item movement helpers | Bomb Bug NBOMB boomerang movement retains the throwing player so carry/catch/drop position uses P2 when P2 threw the boomerang |
| "Which player owns this collectible draw-in/pickup effect?" | `dusk::coop::retained_interaction_owner::Collect` plus active-player pickup eligibility | Tears of Light retain the player who entered the native pickup radius so draw-in lines and `onWolfLightDropGet()` use that slot |
| "Which player owns this item/tool instance?" | item-owner helpers / owner keeps | Partially implemented by item ownership patches |
| "What is this player slot locked onto or allowed to target?" | `dusk::coop::player_attention` | V1 gives additional ALINK actors their own `dAttention_c`; lock acquisition/status gating, owner target-capability masks, owner camera gameplay, cursor drawing, and actor-observed "am I locked-on?" checks use the same attention owner while P1 remains on global attention for HUD/story compatibility |
| "What Do/A/R/Z, wolf X/Y, or 3D action status should this ALINK consume?" | `dusk::coop::player_button_status` | Implemented for ALINK gameplay prompt state; P1 forwards to vanilla globals and additional slots store sidecar values |
| "Which X/Y items has this player assigned?" | `dusk::coop::player_item_selection` | Implemented as runtime sidecar assignments for additional slots with P1 forwarding to vanilla globals; inventory and consumable pools remain shared |
| "Which runtime Epona belongs to this player slot or rider?" | `dusk::coop::horse_owner` | Authored Epona remains canonical for story/save compatibility and additional slots receive slot-assigned runtime clones; rider-local mounted gameplay, per-viewport spur presentation, and any-active-horse fence-jump tags are routed, while remaining collision and authored world-tag families stay active audit work |
| "Which selected enemy target's horse speed or mounted state should drive an enemy gate?" | `selected_target_state` plus `horse_owner` | Bulblin Rider's ordinary mounted run/attack spacing and fast-horse gates use the retained Combat target's slot-assigned horse; authored bridge/King Bulblin progression remains a separate setpiece-owner question |
| "Which player should a rider-owned mount steer around?" | Parent/rider `enemy_targeting` plus mount-local `selected_target_state` snapshots | Ordinary Bulblin Rider boars inherit the Rider's retained Combat target for steering and speed gates; leader/King Bulblin and player-mounted boar paths remain authored/setpiece or horse/mount ownership questions |
| "Which player's prompt/item state is this HUD/meter pass presenting?" | `dusk::coop::hud_owner` | Split-screen prompt and assigned-item HUD replay reads slot-local button state and item snapshots; the Epona spur presenter also resolves horse-local lash counts while broader independent inventory/menu/meter duplication remains deferred |
| "Which slot owns this transient overlay, delayed reticle packet, or singular item wheel?" | `dusk::coop::ui_owner` | Implemented for scoped presentation slots, retained singular UI ownership, viewport-local projection/draw helpers, Hawkeye scope, ALINK live reticles, boomerang lock markers, and fishing forced-wheel entry |
| "Which player owns first-person/item/camera-action status?" | `dusk::coop::player_camera_status` over `dusk::coop::camera` | First pass implemented for slot-local status 0/1 bits, camera attention bits, subject zoom/focus, bow/slingshot, Hawkeye, iron ball subject mode, hookshot subject/hang/flight status, MG_ROD camera/cast status, and wolf AOE charge/dome/lock camera status lifecycle |
| "Which player owns this prompt/object interaction?" | `dusk::coop::interaction_owner` | Selects active-player prompt owners for knob/shutter door side fields. Generic ALINK talk/check/pickup and carried-item actions already work through slot-local attention/status; use this API for remaining world actors with their own P1-only eligibility scans. Howling stones intentionally remain P1/global |
| "Which player requested this accepted event/demo?" | `dusk::coop::event_owner` | Initial implementation derives from event `Pt1`; message input, ALINK door-demo staff consumption, and knob/shutter door demos use it so P2-started scripted interactions do not animate or move P1 |
| "Which player owns the transient Midna service and manual wolf-transform request?" | `dusk::coop::midna_owner` | P1's Midna remains canonical for story/save/global paths, while active additional slots get runtime Midna service actors; active-service position/no-draw setup, prompt eligibility, message branch reads, transform blocking, accepted transform demo handoff, and the talk/camera status bit follow the service actor's ALINK slot |
| "Which player owns this active interactive dialogue/message surface?" | `dusk::coop::message_owner` | Retains the dialogue slot, pad, listener, speaker, and presenter actor after the native message controller accepts the message, with `talkStartInit()` as fallback insurance; prefers active `midna_owner` service, otherwise falls back to `event_owner`; A/B and choice input read the retained pad while native global movement/input locking remains intact |
| "Should this explicitly classified singular event, interactive dialogue, retained enemy interaction, or captured menu surface temporarily present one fullscreen camera and hide non-presenting players?" | `dusk::coop::event_presentation` | Implemented as an opt-in presentation override above the camera sidecar; P1/global howling stones, Midna service, interactive dialogue, retained enemy Link-swallow/Rider-carry presentation, item ring, Start-menu tree, field/dungeon maps, and Agitha's insect screen are classified consumers |
| "Which player is retained by this training sequence?" | future `training_owner` | Deferred unless Hidden Skill / `NPC_KN` playtesting exposes a concrete P2 ownership failure |
| "Which player owns camera/HUD/message/story/save state?" | camera/HUD/story-specific APIs | Partially implemented for split-screen camera only |
| "Which viewport owns this render pass, post effect, lighting, fog, or culling decision?" | split-screen viewport/render ownership APIs | Initial audit in `coop-split-screen-api-audit.md`; `render_visibility` implemented for known draw-culling paths |

## Classification Rules

- **Targeting:** distance, angle, position, or pointer reads used to search, chase, face, aim at, or
  attack a player. Route through actor-local helper wrappers over `enemy_targeting` or raw
  `player_query` only for narrow world-acknowledgement proofs.
- **Damage-owner:** cut type/count, weapon owner, hit direction, attacker equipment, and hit reaction
  ownership. Route through `damage_owner`. Never use nearest player or current enemy target to answer
  "who hit me?"
- **Bespoke damage counters:** some enemies reset HP or keep manual hit counters for specific tools
  rather than relying on `cc_at_check()`. Classify direct-hit bonuses, object thresholds, cannon or
  carried-object exceptions, and special death counters as damage/collision identity. Big Freezard's
  iron-ball counter is the proof case: direct active-player hits must match vanilla P1 behavior, while
  cannon-carry instant kill remains object-owned.
- **Selected-target state:** form, speed, position, guard, swim, horse, damage-wait, wolf-sense, camera/status
  bits, or facing
  checks that modify behavior toward a known target. Route these through
  `dusk::coop::selected_target_state` once the target identity is known. Do not leave them
  permanently P1-only by accident, but do not fake them with fresh nearest-player guesses.
- **Defender/collision-owner:** enemy-attack contact reads such as guard/block/defender state.
  Route through `dusk::coop::defender_owner`. Never use `damage_owner`, nearest-player, selected
  target, or P1 globals to answer "who did my attack touch?"
- **Broader collision-owner:** contact-driven logic with no explicit search/chase surface, such as
  ride, push, stand-on, pickup, and object interaction. Keep it out of `enemy_targeting`; it needs
  its own ownership model.
- **Caught/grab-owner / caught-stun-owner / wolf-catch-owner / retained-interaction-owner:** a retained interaction with one specific player. It
  must not retarget to the nearest player while the grab/stun is active, and it must not borrow P1
  camera/body/controller state for P2. Gibdo scream stun uses `caught_stun_owner`; Keese wolf-bite
  holds use `wolf_catch_owner`; Skulltula `StCaught`, Ghost Rat attach, Peahat hookshot carry, and
  Poe soul pull use `retained_interaction_owner`; remaining swallow/grab files need their own
  retained-owner proof before conversion.
- **Item awareness:** immediate item/tool reactions should ask `item_awareness` or a similarly
  scoped owner scan. Do not route boomerang, hookshot, bomb, or bait reaction checks through sticky
  combat targeting just because an enemy also has a combat target.
- **World trigger/switch activation:** if an enemy is blocked before its action function by a room
  switch, use `world.switch` diagnostics and enemy-local probes to identify whether the missing P2
  path is an upstream producer or an enemy-local authored wake gate. Patch known producers at the
  producer; for enemy-local gates, open the same native switch from the active-player predicate.
- **Player attention:** ALINK lock-on, target actor, attention truth/release, and slot-local prompt
  candidates. Do not let P2 consume P1's `dAttention_c::Lockon()` as its own gameplay lock state.
- **Player button status:** Do/A/R/Z, wolf X/Y, and 3D action availability consumed by ALINK
  gameplay. P1 forwards to the vanilla meter globals; additional slots keep sidecar prompt state for
  gameplay and HUD presentation.
- **Player item selection:** runtime X/Y assignment and mix-item indexes for each player slot. P1
  forwards to vanilla globals; additional slots retain session-local choices while inventory and
  consumable counts remain shared.
- **Horse owner:** slot-assigned runtime Epona identity. Keep the authored horse canonical for
  story/save paths, then ask `horse_owner` for rider-local movement, call, camera, rein, render, and
  clone-lifecycle questions. Runtime clones also localize mutable BCK wrappers, and horse model
  evaluation rebinds the actor-local matrix calculator before using shared model data. Physical
  background checks need the initiating horse actor, not a guess from the canonical horse and not
  an iteration over every registered horse.
- **HUD owner:** meter/prompt presentation ownership. It decides which slot's prompt state and
  native human/wolf meter branch are being drawn for the active HUD viewport, not who is eligible to
  interact or who accepted an event. Keep prompt eligibility in `interaction_owner` and accepted
  event input in `event_owner`.
- **UI owner:** transient presentation-slot ownership, retained singular UI ownership, and
  viewport-local 2D projection/draw setup. Use it for item wheel ownership and viewport overlays,
  not world-rendered fishing line/bobber geometry.
- **Player camera status:** first-person and item-aiming states such as bow, slingshot, Hawkeye,
  hookshot, and iron ball subject mode. Route through slot camera ownership rather than global
  player-status bits.
- **Interaction owner:** prompt-driven actions such as talk, check, pickup, and object use.
  Keep it separate from enemy targeting, item owner lookup, and accepted event ownership. Prompt
  code should return the selected slot/actor before the event manager accepts an order.
- **Event owner:** accepted event/demo ownership after the event manager chooses an order. Derive
  from `dComIfGp_event_getPt1()` where possible so scripted interaction placement, input, and
  animation follow the requesting player. Do not use it for raw prompt eligibility before an event is
  accepted; that is `interaction_owner`.
- **Message owner:** active interactive dialogue/message ownership after native message acceptance. It
  retains the presenter slot, pad, listener, speaker, and presenter actor for
  talk-camera/message presentation; Midna service conversations prefer
  `midna_owner`, while ordinary accepted messages fall back to `event_owner`. Use it for message
  input and talk-camera presentation, including fallback actor selection instead of camera
  `mpPlayerActor`. Keep the native global dialogue movement/input lock unless a concrete co-op bug
  proves it must be split.
- **Training owner:** retained instructional/event combat sequences such as Hidden Skills. Once a
  trainer binds to a slot, required move checks and forced placement should follow that slot.
- **Item-get owner:** retained collector for generic `DEFAULT_GETITEM`. A producer must hand off the
  exact collecting slot before ordering the event; the pending record is keyed to source actor plus
  process ID. Once `PROC_GET_ITEM` begins, it owns the singular ALINK staff, `Demo_Item`,
  item-message pad, slot-local item-get status, and `event_presentation::ItemGet`. Event END requests
  release, but actual release waits until painter entry after camera recovery has executed. Poe soul
  collection is validated; other pickup/chest/NPC/insect/equipment producers remain unaudited.
- **Singular event presentation:** opt-in fullscreen presentation for authored sequences and
  captured menu surfaces. It expands the retained presenter's existing render window and hides
  non-presenting player visuals without disabling co-op simulation. Howling stones remain P1/global;
  the item ring presents its retained `ui_owner` slot; Start menus, maps, and Agitha's insect screen
  remain P1/global. Interactive dialogue opts in through `message_owner`, and generic
  `DEFAULT_GETITEM` acquisition opts in through `item_get_owner`; passive overlays, unrelated item
  cameras, boss names, stage titles, and unaudited message-camera scenes must not silently become
  singular consumers.
- **Viewport/render ownership:** split-screen render passes, post effects, lighting, fog, HUD
  projection, draw-time visibility culling, and shadows should be owned by viewport/render policy.
  Use `render_visibility` for shared draw-culling decisions, `render_materials` for viewport-owned
  kankyo/J3D material state, `render_effects` for late world/effect versus fullscreen framebuffer
  ownership, and `render_shadows` for real-shadow culling or baked shadow matrix ownership. A P2
  fullscreen surface draws one window but still needs camera-1 viewport-owned world refresh; do not
  equate “not presenting both split windows” with “camera-0 render state is sufficient.” Do not
  scatter actor-specific render fixes when a central PC split-screen policy can answer the question.
- **Per-viewport effects:** `render_effects` retains the active slot/window/camera/view context,
  Base/Sense environment and bloom snapshots, slot-local Sense fade/emitter state, and slot-local
  Twilight camera lights. `render_materials` owns submitted J3D `viewCalc()` and projected-material
  replay. `player_sense` owns per-slot reveal eligibility, while `render_visibility` and
  `render_effects` filter shared reveal model packets, real shadows, reveal particles, and inverse
  spirit wisps during viewport replay. Actor draw submission supplies native per-camera culling,
  and particle replay only overrides/restores draw alpha without mutating emitter simulation.
  Shared gameplay actors and emitters stay singular. Camera-retained visual packets are a distinct
  case: if native update consumes a player/camera and stores positions, alpha, room ratio, or other
  history, additional slots need fixed per-slot visual simulation sidecars with private RNG. P1
  remains canonical. Camera-relative replay uses the exact submitted matrix, independent of frame
  interpolation enablement. Viewport framebuffer consumers refresh the canonical native capture at
  the same native phase the consumer expects because water and projection-particle resources retain
  that address; motion blur, depth of field, mixed indirect-screen passes, generic fullscreen 2D,
  and fades remain global/gated.
- **Primary/global state:** story protagonist, demo/cutscene, save/restart, HUD, passive message, or
  single-camera state. Keep P1/global until a dedicated milestone proves otherwise.

## Audit Breadcrumb Rule

When an enemy conversion intentionally leaves a player-singleton read unconverted, record that fact
in `docs/coop-enemy-audit.md` before moving to the next enemy. The audit row should name the
deferred API family, not just say "left vanilla." This is especially important for:

- camera/presentation ownership;
- master/child spawning or inherited target ownership;
- hookshot/item awareness;
- caught/grab/swallow/hang ownership;
- broader collision-owner contact such as push, ride, stand-on, pickup, and trigger ownership;
- render/visibility or split-screen culling;
- story/demo/HUD/save reads that remain deliberately primary/global.

## Enemy Conversion Rule

Every converted enemy should have an actor-local helper near the top of the file. The helper should
translate that actor's state-machine vocabulary into the shared API:

- reusable behavior scope, such as `EnemyTargetScope::Combat`;
- diagnostic label, such as `e_oc.find`;
- callsite policy mode, such as immediate awareness or sticky combat;
- committed/follow-through hints supplied by the actor.

The label is diagnostics only. It must not create a separate target-retention machine.

## Diagnostics Rule

When adding a new API family or converting a new singleton category, add structured diagnostics at
the same time. Keep `latest.json` rich and `events.jsonl` semantic: continuous values can appear as
context, but should not drive event spam.
