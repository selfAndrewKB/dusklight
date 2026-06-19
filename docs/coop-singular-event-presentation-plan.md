# Singular Event Presentation Plan

Status: the generic Poe ItemGet ownership, post-camera split rebuild, and Camera-1 fullscreen kankyo
slice were field-validated on June 19, 2026. Active implementation work has moved to the remaining
enemy queue in `docs/coop-enemy-audit.md`; this document remains the lifecycle source of truth for
future singular-presentation and ItemGet producer work.

## Summary

Some Twilight Princess sequences are intentionally singular: they present one authored camera,
one fullscreen message or minigame surface, and one global world-state transition. Native local
split-screen should not automatically duplicate or partition those sequences.

Howling stones are the first clear example. They remain P1/global in V1. Players can use Dusk's
seamless controller-port switching when they want to trade control briefly. Captured fullscreen
menu surfaces are the second classified family: the item ring, Start-menu collection tree, field
map, dungeon map, and Agitha's standalone insect screen now reuse the same presentation override.

This is a presentation policy, not a broad event-ownership conversion. Interactive dialogue is now
an explicit classified consumer through `message_owner`, and the generic `DEFAULT_GETITEM`
sequence is classified through `item_get_owner`. Passive message overlays, interaction prompts,
other item cameras, and gameplay events should continue to use their existing narrow ownership APIs
unless a specific sequence is classified as singular.

## Ownership Question

The API answers:

> Should an explicitly classified singular event temporarily present one fullscreen camera and
> hide additional local players without disabling co-op simulation?

It does not answer:

- which player is eligible to activate an interaction prompt;
- which player requested an accepted event;
- which controller advances dialogue outside interactive `message_owner` surfaces;
- which camera owns ordinary split-screen gameplay;
- whether the co-op feature itself is enabled.

Those remain `interaction_owner`, `event_owner`, `hud_owner` / `ui_owner`, and
`dusk::coop::camera` questions.

## Implemented API

The Dusk-owned `dusk::coop::event_presentation` family is:

```cpp
namespace dusk::coop::event_presentation {

enum class Source : u8 {
    WolfHowl,
    ItemRing,
    PauseMenu,
    FieldMap,
    DungeonMap,
    AgithaInsect,
    MidnaService,
    Dialogue,
    ItemGet,
    EnemyRetainedInteraction,
};

struct Options {
    PlayerSlot fullscreenSlot = PlayerSlot::Primary;
    bool hideNonPresenterVisuals = true;
};

void begin(Source source, const Options& options = {});
void end(Source source);
void reset();

bool isFullscreen();
bool shouldPresentSplitViewports();
bool shouldRefreshViewportOwnedWorldState();
bool shouldDrawWindow(int windowIndex);
bool shouldHideSlot(PlayerSlot slot);
PlayerSlot presenterSlot();
int presenterWindowIndex();

}  // namespace dusk::coop::event_presentation
```

The sidecar keeps a fixed-allocation stack plus retained depth per source. The most recent active
source supplies the presenter slot and hiding policy, so nested begin/end calls restore the previous
presentation instead of restoring split-screen early. P2-owned item rings present camera 1 through
its existing render window. Invalid or unsupported presenter slots fall back to P1. `reset()` clears
interrupted presentation during play-scene teardown and initialization beside the existing
split-screen camera-sidecar reset.

## Presentation Policy

While a singular fullscreen presentation is active:

- native co-op remains enabled;
- additional ALINK actors continue executing and retain their player-slot identity;
- the painter presents the selected owner's camera in a fullscreen viewport while other cameras
  remain alive;
- non-presenting ALINK models, ALINK-submitted equipment and shadows, and slot-owned runtime Epona
  are hidden through one centralized slot policy;
- shared world simulation, event progression, room state, and save state continue normally;
- ending the sequence restores the previous split layout and player presentation immediately.

Do not implement this by calling `dusk::coop::camera::setSplitScreenEnabled(false)`. That toggle
changes the underlying co-op camera capability, effective window count, HUD behavior, render-policy
checks, and secondary-camera readiness semantics. Singular events need a temporary presentation
override above the camera sidecar, not a gameplay-mode toggle.

Do not implement hiding by mutating persistent actor `NODRAW` state unless investigation proves a
central draw-policy query is insufficient. A presentation override must not be able to leave P2
invisible after an interrupted event.

## Validated Item-Get Ownership

The first field-validated generic item-get owner is P2 Poe soul collection. The implementation is
split across three ownership phases:

1. The producer retains who collected the item.
2. `PROC_GET_ITEM` promotes that pending collector into the active item-get owner.
3. Event teardown requests release, native camera recovery runs, and the renderer performs the
   final release immediately before viewport replay.

This separation is intentional. The actor that detects or produces an item, the ALINK that consumes
the singular `"Alink"` event staff, and the renderer that safely restores split presentation run at
different times.

### Producer handoff

`item_get_owner::retainForEventSource(source, player)` must run before the producer orders
`DEFAULT_GETITEM`. The pending record stores:

- the exact event-source actor pointer;
- that source actor's process ID, so pointer reuse cannot validate a stale owner;
- the collecting ALINK pointer;
- the collecting player slot.

When `PROC_GET_ITEM` starts, `item_get_owner::begin(player)` accepts the pending owner only if the
current event source still matches both pointer and process ID and the retained slot still resolves
to the same live player. This prevents a pending owner from one acquisition leaking into an
unrelated item event.

Poe `E_HP` and `E_PO` bridge their retained soul `Collect` owner into this API immediately before
ordering `DEFAULT_GETITEM`. That producer handoff is the validated path. The generic API has a
best-effort fallback for already owner-routed events, but a fallback is not proof that an unaudited
pickup, chest, NPC reward, insect, key, equipment object, or scripted item source is P2-correct.

### Active sequence ownership

After promotion:

- only the retained ALINK consumes the singular `DEFAULT_GETITEM` `"Alink"` staff track;
- `Demo_Item` follows that ALINK's live position, facing, horse state, and human/wolf form;
- item-message input uses that slot's pad;
- item-get camera/action status `0x4000000` is produced through `player_camera_status`;
- `event_presentation::Source::ItemGet` expands the retained slot's existing window fullscreen and
  hides non-presenting player visuals;
- inventory, save, and event mutation remain shared/global.

Do not infer the collector from the nearest player after the event starts. Do not give every ALINK
an independent copy of the authored staff. The owner must cross the source-to-event boundary once,
then remain retained for the sequence.

## Item-Get Teardown And Split Rebuild

The teardown order is a render-lifecycle contract, not a cosmetic delay:

```text
DEFAULT_GETITEM reaches END
  -> dEvent_manager_c::endProc() classifies event->getName()
  -> item_get_owner::requestEnd()
  -> native event manager sets cameraPlay = 2
  -> dEvt_control_c::Step() later sets cameraPlay = 0
  -> camera actors execute and consume native recovery state
  -> mDoGph_Painter() begins
  -> item_get_owner::finishPendingEnd()
  -> event_presentation::end(ItemGet)
  -> camera::refreshWindowLayout()
  -> painter samples window count and render policy
  -> restored split viewports replay with per-view world state
```

Each boundary matters:

- `dEvent_manager_c::endProc()` must classify the closing `dEvDtEvent_c` directly with
  `event->getName()`. `getRunEventName()` returns `"NOT RUNNING"` once the event enters END state,
  so using it here silently prevents `requestEnd()` from ever running.
- `endProc()` must request release, not release presentation immediately. It sets
  `mCameraPlay = 2`; native camera recovery still has work to consume.
- `dEvt_control_c::Step()` later calls `setCameraPlay(0)`, but play-scene execution invokes event
  `Step()` before the camera actors execute. Zero means recovery has been released to the cameras;
  it does not mean those cameras have already rebuilt their native state.
- Releasing presentation inside `Step()` is too early. It can reopen split layout before camera
  execution, after which camera recovery can overwrite the layout or leave renderer globals
  mismatched. Field testing showed missing split restoration plus broken ground and lighting from
  this ordering.
- `mDoGph_Painter()` is the first narrow boundary after actor/camera execution and before the
  renderer samples `dComIfGp_getWindowNum()`, chooses visible windows, primes shadows, reloads
  lights, or replays kankyo materials. Ending there lets `camera::refreshWindowLayout()` rebuild the
  two viewports before the first restored frame is configured.

Do not replace this with an arbitrary one-frame timer. Interpolated presentation may call the
painter more than once per simulation tick; the semantic condition is pending event end plus native
camera play being clear at the post-camera/pre-render handoff.

## Camera-1 Fullscreen And Kankyo

`kankyo` is the engine's environment/lighting family. A P2-owned fullscreen item-get sequence draws
only camera 1, but camera 1 is still not allowed to inherit camera-0 world-render state.

Two policy questions must remain separate:

| Policy | Meaning during P2 fullscreen |
| --- | --- |
| `shouldPresentSplitViewports()` | `false`: draw only the selected fullscreen window and suppress split-only framebuffer replay |
| `shouldRefreshViewportOwnedWorldState()` | `true`: camera 1 still needs its own culling, shadow, kankyo material, particle-culling, and GX-light setup |

The second policy keeps these paths active for camera 1:

- shared draw-culling bypass through `render_visibility`;
- real-shadow setup through `render_shadows`;
- registered kankyo/J3D material replay through `render_materials`;
- particle-creation culling policy through `render_effects`;
- `dKy_setLight_again()` after camera 1's view is installed.

The distinction was field-proven. Suppressing all split render refresh merely because one fullscreen
window was visible made room lighting move with the camera, matching the earlier camera-0/kankyo
ownership bug. Restoring only the viewport layout fixed ground rendering but not lighting. Camera 1
fullscreen must therefore be treated as a single presented window that still owns non-native
viewport world state.

P1-owned fullscreen presentation can continue using native camera-0 world state. Do not broaden
camera-1 refresh into fullscreen framebuffer ownership: bloom, captures, and other screen-sized
effects remain governed by `shouldPresentSplitViewports()` and their explicit `render_effects`
classification.

## Remaining Item-Get Audit

The generic sequence machinery is field-validated, but producer coverage is not complete. Future
ItemGet work should audit producers in families rather than mass-patching every
`"DEFAULT_GETITEM"` string:

- ordinary world item/pickup objects, including which actor owns collection radius and callback;
- treasure chests and small keys;
- life containers, equipment, swords, shields, lanterns, and dungeon-exit items;
- insects and other dedicated collection managers;
- NPC rewards and shop/scripted `fopAcM_orderChangeEventId()` paths;
- special variants such as wolf-only or authored non-default get-item events.

For each producer:

1. Find the native player-selection or collection callback before the event is ordered.
2. Retain the exact slot at that producer boundary; do not infer from event source alone when the
   source is an NPC, chest, manager, or shared object.
3. Call `retainForEventSource()` before ordering or changing into `DEFAULT_GETITEM`.
4. Verify the accepted event source pointer/process ID matches the retained source.
5. Field-test P1 and P2 item position/form, message input, camera 1 fullscreen lighting, control
   restoration, split-layout restoration, and interruption/scene-change cleanup.
6. Keep shared inventory/save mutation global unless a separate inventory ownership milestone says
   otherwise.

Also audit non-`DEFAULT_GETITEM` acquisition sequences separately. Do not route them through
`item_get_owner` merely because they display an item model.

## First Consumer: Howling Stones

Howling stones and howl tags remain P1/global in V1:

- keep their prompt eligibility on the vanilla P1/wolf path;
- keep the howl manager, audio manager, event camera, status bits, waveform screen, and completion
  state global;
- do not route the waveform minigame through `interaction_owner` or `event_owner`;
- do not duplicate the waveform screen per viewport.

The shared howl sequence hooks the presentation API at its lifecycle boundary:

- begin a primary fullscreen presentation when the global howl sequence starts;
- hide additional players while the authored sequence is visible;
- end the override when the sequence closes or aborts.

The hook should remain small and event-specific. The reusable behavior belongs in
`event_presentation`.

## Midna Service

Manual Midna service and wolf-transform requests use the same presentation layer with slot-local
runtime Midna service actors:

- `midna_owner` registers one canonical P1/global Midna plus runtime service actors for additional
  slots;
- `midna_owner` retains the requesting ALINK slot for prompt/message/transform ownership;
- `event_presentation::Source::MidnaService` presents the requester fullscreen while the service is
  active;
- the active service actor reads its registered ALINK for position/no-draw and transform setup, while
  unrelated P1/global Midna reads remain canonical.

Interactive Midna dialogue uses `message_owner` and `event_presentation::Source::Dialogue` after the
native message controller accepts the message. The retained owner supplies the
listener ALINK, speaker Midna, input pad, and talk-camera fallback actor; native global dialogue
movement locking remains in place.

Retained enemy interactions use `event_presentation::Source::EnemyRetainedInteraction` only for the
authored fullscreen camera/presentation surface. The gameplay effect owner remains in the matching
retained owner API, such as `retained_interaction_owner` for Deku Like Link swallow/eat/spit.
Localized gameplay cameras do not collapse presentation merely because their player is retained:
the Tile Worm toss keeps split-screen active and force-locks only the retained victim's camera.

## Captured Fullscreen Menus

Captured menu surfaces begin presentation before native framebuffer capture and end only after the
capture is deleted:

- The item ring presents `ui_owner::singularSlot()`, so P2 can open and steer one fullscreen vanilla
  ring against P2's camera without broadening menu ownership.
- The Start-menu collection tree stays P1/global and retains one `PauseMenu` presentation across
  save, options, letters, fishing journal, skills, and insect collection descendants.
- Field and dungeon maps stay P1/global, including scripted map opens.
- Agitha's NPC-triggered insect screen stays P1/global and uses its own explicit source.
- Menu-window teardown ends menu-only sources after native cleanup as interruption insurance.

Input and presentation remain separate. `ui_owner` retains singular item-ring input and selection;
`event_presentation` owns only temporary camera layout and visual hiding. Start-menu and map input
remain P1/global until a separate ownership audit justifies expanding them.

## Candidate Consumers

Evaluate later consumers case by case:

- Hidden Skill training;
- minigames with a single authored screen;
- selected cutscenes or scripted demonstrations;
- passive or scripted message-camera scenes that visibly assume one camera.

Interactive talk/choice messages now opt into `event_presentation::Source::Dialogue` through
`message_owner`. Generic `DEFAULT_GETITEM` acquisition sequences opt into
`event_presentation::Source::ItemGet` through `item_get_owner`. Do not infer the same policy for
every passive message overlay, unrelated item camera, boss-name/title card, shop-special surface,
or demo; those still require explicit classification.

## Implemented Slice

- `event_presentation` retains a fixed active-entry stack, source-indexed depth, presenter slot, and
  non-presenter hiding policy. Camera layout refreshes when fullscreen state or presenter changes.
- Camera layout, painter replay, framebuffer-effect policy, split-only framebuffer refresh,
  secondary HUD replay, material refresh, shadow refresh, and draw-culling bypass distinguish
  active split presentation from the underlying split-screen capability.
- ALINK and runtime-Epona draw wrappers hide non-presenting slots without changing execution or
  persistent actor flags.
- `event.presentation` diagnostics record source depths, transition, split capability, presenter
  slot/window, active presentation layout, hiding policy, and hidden slots.
- Interactive dialogue begins a `Dialogue` presentation through `message_owner` after the native
  message controller accepts the message, retaining the slot, pad, listener, speaker, and presenter
  actor while leaving native global dialogue movement locking intact. `talkStartInit()` is fallback
  insurance, not the primary presentation timing boundary.
- Generic item-get sequences promote a source-tied pending collector through `item_get_owner` when
  `PROC_GET_ITEM` begins. Only the retained ALINK consumes the singular staff track; `Demo_Item`,
  item-message input, slot-local item-get status, and fullscreen presentation follow that owner
  until `DEFAULT_GETITEM` closes, native camera recovery executes, and the renderer reaches its
  pre-viewport handoff. Inventory/save mutation remains shared.
- P2-owned fullscreen presentation continues viewport-owned world refresh for camera 1: shared
  culling is bypassed, real shadows and kankyo materials are rebuilt, and GX lights are reloaded.
  Only the second viewport and split-only framebuffer replay are suppressed.
- Wolf howl begins after the global event is accepted and ends on its explicit close, scene-change,
  Sun's Song, horse-call, and Golden Wolf handoffs. Scene lifecycle reset remains interruption
  insurance.

## Follow-Up Order

1. Select the next untouched regular enemies from `docs/coop-enemy-audit.md`. The
   `E_FK` / `E_HZ` / `E_BUG` batch is field-validated; `E_GOB` is Dangoro and remains deferred to
   the miniboss/setpiece pass.
2. Return to the producer-family ItemGet audit when a non-Poe P2 acquisition is selected for field
   testing.
3. Classify later singular sequences individually as they are encountered.

## Test Plan

- Normal split-screen gameplay is unchanged when no presentation override is active.
- Beginning a primary fullscreen presentation draws camera 0 fullscreen without destroying camera
  1 or disabling P2 simulation.
- A P2-owned item ring draws camera 1 fullscreen, hides P1 presentation, and keeps P2 input retained
  through `ui_owner`.
- The Start-menu tree, field map, dungeon map, and Agitha insect screen present P1/global
  fullscreen surfaces and restore split layout after native capture cleanup.
- Non-presenting players, their equipment, and their shadows are hidden while requested.
- Ending or aborting the presentation restores both split viewports and P2 presentation.
- Nested begin/end calls do not restore split-screen until the outermost presentation ends.
- Howling stones preserve their vanilla global behavior and present the authored fullscreen effect.
- Interactive dialogue fullscreen-presents the retained owner. Doors, HUD prompts, Hawkeye,
  boomerang, fishing rod, and passive message overlays remain unchanged unless explicitly
  classified as singular consumers.
- A P2-collected Poe soul runs the item-get pose on P2, displays the soul above P2, advances from
  P2's controller, presents camera 1 fullscreen, hides P1 visuals, and restores split-screen when
  `DEFAULT_GETITEM` closes. P1 item gets remain unchanged.

Validation result, June 19, 2026:

- P2 Poe soul ownership, item position, message input, control restoration, and fullscreen
  presentation were field-tested successfully.
- Releasing presentation at painter entry fixed ground rendering by restoring layout after native
  camera execution.
- Keeping Camera 1 viewport-owned kankyo/GX-light refresh active during P2 fullscreen fixed the
  remaining camera-relative room-lighting corruption.
- Split-screen restoration was confirmed after changing END-state classification from
  `getRunEventName()` to the closing event object's name.

## Non-Goals

- Do not make howling stones independently usable by P2 in this pass.
- Do not duplicate singular message or minigame screens.
- Do not pause, despawn, teleport, or otherwise mutate additional players merely to hide them.
- Do not treat all message-camera scenes as singular automatically; interactive dialogue is the
  explicit V1 consumer.
- Do not broaden this into a general cutscene rewrite.
