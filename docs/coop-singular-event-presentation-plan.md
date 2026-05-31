# Singular Event Presentation Plan

## Summary

Some Twilight Princess sequences are intentionally singular: they present one authored camera,
one fullscreen message or minigame surface, and one global world-state transition. Native local
split-screen should not automatically duplicate or partition those sequences.

Howling stones are the first clear example. They remain P1/global in V1. Players can use Dusk's
seamless controller-port switching when they want to trade control briefly. A future implementation
may collapse split-screen while a howl event runs so the original fullscreen presentation remains
coherent.

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

## Proposed API

Add a Dusk-owned `dusk::coop::event_presentation` family:

```cpp
namespace dusk::coop::event_presentation {

struct Options {
    PlayerSlot fullscreenSlot = PlayerSlot::Primary;
    bool hideAdditionalPlayers = true;
};

void begin(const Options& options);
void end();

bool isFullscreen();
PlayerSlot fullscreenSlot();
bool shouldHidePlayer(const fopAc_ac_c* actor);

}  // namespace dusk::coop::event_presentation
```

The implementation should retain enough state to restore the previous presentation reliably.
Prefer a scoped token or depth-aware begin/end model so nested singular sequences cannot restore
split-screen too early.

## Presentation Policy

While a singular fullscreen presentation is active:

- native co-op remains enabled;
- additional ALINK actors continue executing and retain their player-slot identity;
- the painter presents only the selected camera in a fullscreen viewport;
- additional player models, equipment, shadows, and presentation-only attachments can be hidden
  through one centralized draw policy;
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

When the presentation API is implemented, hook the shared howl sequence at its lifecycle boundary:

- begin a primary fullscreen presentation when the global howl sequence starts;
- hide additional players while the authored sequence is visible;
- end the override when the sequence closes or aborts.

The hook should remain small and event-specific. The reusable behavior belongs in
`event_presentation`.

## Candidate Consumers

Evaluate future consumers case by case:

- Hidden Skill training;
- minigames with a single authored screen;
- selected cutscenes or scripted demonstrations;
- message-camera scenes that visibly assume one camera.

Do not collapse split-screen for all messages, NPC conversations, signs, shops, or demos by
default. Many can remain split-screen, and some should become correctly owner-routed instead.

## Implementation Order

1. Finish the current HUD/UI/interaction ownership work.
2. Add `event_presentation` storage and a painter-level fullscreen override.
3. Add centralized additional-player draw suppression without disabling simulation.
4. Validate restore behavior when a sequence ends normally and when it aborts.
5. Add the howling-stone lifecycle hook as the first consumer.
6. Classify later singular sequences individually as they are encountered.

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
