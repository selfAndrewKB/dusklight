# Singular Event Presentation Plan

## Summary

Some Twilight Princess sequences are intentionally singular: they present one authored camera,
one fullscreen message or minigame surface, and one global world-state transition. Native local
split-screen should not automatically duplicate or partition those sequences.

Howling stones are the first clear example. They remain P1/global in V1. Players can use Dusk's
seamless controller-port switching when they want to trade control briefly. The implemented
presentation override collapses split-screen while a howl event runs so the original fullscreen
presentation remains coherent.

This is a presentation policy, not a broad event-ownership conversion. Ordinary dialogue,
interaction prompts, item cameras, and gameplay events should continue to use their existing narrow
ownership APIs unless a specific sequence is classified as singular.

## Ownership Question

The API answers:

> Should an explicitly classified singular event temporarily present one fullscreen camera and
> hide additional local players without disabling co-op simulation?

It does not answer:

- which player is eligible to activate an interaction prompt;
- which player requested an accepted event;
- which controller advances ordinary dialogue;
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
};

struct Options {
    bool hideAdditionalVisuals = true;
};

void begin(Source source, const Options& options = {});
void end(Source source);
void reset();

bool isFullscreen();
bool shouldPresentSplitViewports();
bool shouldDrawWindow(int windowIndex);
bool shouldHideSlot(PlayerSlot slot);

}  // namespace dusk::coop::event_presentation
```

Each source has retained depth, so nested begin/end calls cannot restore split-screen too early.
V1 deliberately presents P1's camera only. `reset()` clears interrupted presentation during play
scene teardown and initialization beside the existing split-screen camera-sidecar reset.

## Presentation Policy

While a singular fullscreen presentation is active:

- native co-op remains enabled;
- additional ALINK actors continue executing and retain their player-slot identity;
- the painter presents camera 0 in a fullscreen viewport while camera 1 remains alive;
- additional ALINK models, ALINK-submitted equipment and shadows, and slot-owned runtime Epona are
  hidden through one centralized slot policy;
- shared world simulation, event progression, room state, and save state continue normally;
- ending the sequence restores the previous split layout and player presentation immediately.

Do not implement this by calling `dusk::coop::camera::setSplitScreenEnabled(false)`. That toggle
changes the underlying co-op camera capability, effective window count, HUD behavior, render-policy
checks, and secondary-camera readiness semantics. Singular events need a temporary presentation
override above the camera sidecar, not a gameplay-mode toggle.

Do not implement hiding by mutating persistent actor `NODRAW` state unless investigation proves a
central draw-policy query is insufficient. A presentation override must not be able to leave P2
invisible after an interrupted event.

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

## Candidate Consumers

Evaluate future consumers case by case:

- singular fullscreen item-ring, field-map, and dungeon-map surfaces, while their retained input
  owner remains a `ui_owner` concern;
- Hidden Skill training;
- minigames with a single authored screen;
- selected cutscenes or scripted demonstrations;
- message-camera scenes that visibly assume one camera.

Do not collapse split-screen for all messages, NPC conversations, signs, shops, or demos by
default. Many can remain split-screen, and some should become correctly owner-routed instead.

## Implemented Slice

- `event_presentation` retains source-indexed depth and refreshes the camera layout only when
  fullscreen presentation begins or ends.
- Camera layout, painter replay, framebuffer-effect policy, split-only framebuffer refresh,
  secondary HUD replay, material refresh, shadow refresh, and draw-culling bypass distinguish
  active split presentation from the underlying split-screen capability.
- ALINK and runtime-Epona draw wrappers hide additional slots without changing execution or
  persistent actor flags.
- `event.presentation` diagnostics record depth, transition, split capability, active presentation
  layout, and hidden slots.
- Wolf howl begins after the global event is accepted and ends on its explicit close, scene-change,
  Sun's Song, horse-call, and Golden Wolf handoffs. Scene lifecycle reset remains interruption
  insurance.

## Follow-Up Order

1. Validate the test matrix below in game.
2. Classify later singular sequences individually as they are encountered.

## Test Plan

- Normal split-screen gameplay is unchanged when no presentation override is active.
- Beginning a primary fullscreen presentation draws camera 0 fullscreen without destroying camera
  1 or disabling P2 simulation.
- P2, their equipment, and their shadow are hidden while requested.
- Ending or aborting the presentation restores both split viewports and P2 presentation.
- Nested begin/end calls do not restore split-screen until the outermost presentation ends.
- Howling stones preserve their vanilla global behavior and present the authored fullscreen effect.
- Ordinary dialogue, doors, HUD prompts, Hawkeye, boomerang, and fishing rod behavior remain
  unchanged unless explicitly classified as singular consumers.

## Non-Goals

- Do not make howling stones independently usable by P2 in this pass.
- Do not duplicate singular message or minigame screens.
- Do not pause, despawn, teleport, or otherwise mutate additional players merely to hide them.
- Do not treat all message-camera scenes as singular automatically.
- Do not broaden this into a general cutscene rewrite.
