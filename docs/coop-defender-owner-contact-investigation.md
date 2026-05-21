# Co-op Defender Owner Contact Investigation

This note preserves the current Bokoblin guard-collision investigation so future sessions can resume
without re-deriving what the overlay and diagnostics proved.

## Question

After adding `dusk::coop::defender_owner`, the debug overlay sometimes showed two labels for one
Bokoblin swing:

```text
atk-sphere1 P2 hit
atk-sphere0 P2 hit
```

or:

```text
atk-sphere1 P1 guard
atk-sphere0 P1 guard
```

The main question was whether these labels were overlay duplication, incorrect ownership sharing,
or real contact facts from the original attack geometry.

## Current Evidence

The labels are backed by `defender.owner` JSONL events. The latest investigation found multiple
frames where both Bokoblin attack spheres resolved the same defender on the same frame:

- frame `1233`: `e_oc.attack_guard.1` and `e_oc.attack_guard.0` both resolved `P1 guard`.
- frame `1519`: `e_oc.attack_guard.1` and `e_oc.attack_guard.0` both resolved `P2 hit`.
- frames `2048` and `2171`: both attack spheres resolved `P1 guard`.
- frames `1927`, `2107`, and `2230`: both attack spheres resolved `P2 hit`.

That means the `atk-sphere0` / `atk-sphere1` labels are not merely duplicated text. They represent
two separate Bokoblin attack colliders being sampled by the defender-owner proof hook.

Earlier evidence also showed that the two spheres can resolve different players during the same
swing window, such as one contact moving through P2 while another resolves P1 shortly after. This is
expected when both players stand inside the swing volume. The overlay keeps labels alive briefly, so
mixed labels can visually overlap even when the JSONL events landed on adjacent frames rather than
the exact same frame. Treat JSONL `frame` as the exact timing source.

## Current Interpretation

Bokoblin has multiple attack spheres. A single visible swing can therefore create multiple
defender-owner contacts. The correct V1 gameplay rule is:

- if any attack sphere touches a guarding player, the enemy attack should bounce from that actual
  defender;
- if an attack sphere touches an unguarding player, that contact should be treated as a hit against
  that actual defender;
- P1 guarding somewhere else must not answer for a P2 contact unless a sphere truly touched P1.

The current direct-player `defender_owner` proof follows that rule. The overlay now exposes attack
geometry, not just a single abstract "who got hit" answer.

## Why A Contact Can Look Shared

The current hook records one resolved hit object per attack sphere by reading the sphere's current
`AtHitObj`. If both spheres currently report P2, both labels correctly say `P2 hit`. If both report
P1 and P1 is guarding, both labels correctly say `P1 guard`.

The important limitation is that one collider's current hit object is not a full history of every
player it overlapped during the swing. If one sphere visibly contacted both players during a broad
swing, the current hook can only report the hit object the engine has retained for that sphere at
the time the hook runs. To report every overlapping player/contact for one sphere, we would need a
deeper collision-owner pass closer to collision iteration, before the engine collapses or stores the
current `AtHitObj`.

This likely explains the user's open question: if P2 was hit while P1 guarded, why might a label
only say `P2 guard` or only `P1 guard`? The V1 hook is not a full multi-contact trace. It reports
each attack sphere's resolved defender at the sampled point, plus that defender's guard state. It
does not yet enumerate all defenders that the same sphere may have contacted during that attack
window.

## Shield Flag Notes

Some P2 contacts showed `at_shield_hit: true` while `guarded: false` and `target_shield_hit: false`.
Treat `guarded` as the behavior-driving fact. The shield flags are useful context, but can indicate
shield-capable collision state without proving the player was actually guarding at that moment.

P2 guard state is also still affected by the current ALINK/input/attention limitations. If P2 can
only guard under specific debug settings, a `P2 hit` record may reflect the local player state rather
than defender-owner resolution being wrong.

## What V1 Implements

- Direct player defender ownership for Bokoblin attack spheres.
- Per-sphere labels: `atk-sphere0` and `atk-sphere1`.
- Structured `defender.owner` diagnostics for contact source, hit actor, defender slot, guard state,
  shield-hit context, and hit position.
- Short-lived overlay labels for guard/hit contacts.

## What Remains Outside V1

These are related to the same broader API family, but are not implemented by the current Bokoblin
proof:

- broader collision-owner: who pushed, rode, stood on, picked up, carried, triggered, or physically
  interacted with an object;
- mount/object defender cases: Epona, boar, Spinner, carried objects, shield-like object actors, or
  mounts that may be the immediate collision actor instead of ALINK;
- multi-contact tracing: every player/object overlapped by one attack sphere during a swing, rather
  than the current retained `AtHitObj`;
- collision debouncing if later tests prove multiple attack spheres can apply duplicate damage in
  gameplay, not just duplicate diagnostics.

## Practical Status

There is no current evidence that the two-sphere labels are a major gameplay concern. They look like
real Bokoblin attack geometry becoming visible through diagnostics. Keep watching for:

- double damage from one visible swing;
- P1 guard affecting P2 when no sphere resolved P1;
- a sphere label whose `defender_slot` disagrees with the visibly contacted player;
- P2 guard records that contradict known P2 guard-state/input behavior.

Repeated `enemy.targeting` entries with `RetainCommitted` also do not prove that guard bounce
occurred. They only prove the enemy policy is preserving the attack target. A later investigation
added `bokoblin.attack` diagnostics to distinguish target commitment from visible attack animation,
attack-sphere contact, guard bounce, speed deceleration, and find/attack re-entry.

If those appear, the next step is not to change `enemy_targeting` or `damage_owner`; it is to inspect
`bokoblin.attack` and then deepen the collision/defender-owner family only if the evidence points
there.

## Attack-Commit Loop Finding

A later guard test exposed a separate Bokoblin issue: with P1 holding guard and P2 near the attack
target, Bokoblin could repeatedly enter attack commitment without a visible attack until P1 released
guard.

The added `bokoblin.attack` probe showed this was not an enemy-targeting failure. In the failing run,
Bokoblin actor `231` targeted P2, started attack BCK `6`, and then immediately consumed a guarded
contact from attack sphere `0` against P1 at animation frame `1.0`. That contact occurred before the
normal attack contact window at frames `14.0` through `22.0`. Because the co-op guard path accepted
the guarded contact immediately, Bokoblin reversed into guard-bounce state before the visible swing
could play. After P1 released guard, the same early contact could still appear, but it no longer
caused a guard bounce, and the attack continued into the real active window.

The durable fix was to keep `defender_owner` generic and leave animation timing local to Bokoblin:

- `defender_owner` still records every attack-sphere contact as soon as the engine reports it.
- Bokoblin only consumes a guarded contact as a bounce during BCK `5` or `6` frames `14.0` through
  `22.0`.
- `bokoblin.attack` still records `pre_active_window_hit` and `pre_active_window_guarded` so future
  tests can prove whether early contacts still exist without letting them drive behavior.

The first post-fix validation run did not reproduce the loop. The latest snapshot for Bokoblin actor
`158` showed active-window contact at animation frame `17.0`, `first_hit_before_active_window=false`,
`pre_active_window_hit=false`, and `loop_suspect=false`, while `defender.owner` continued to report
real P1/P2 contacts. That supports the current interpretation: early/stale contact was real, but it
should not be allowed to trigger guard bounce before the attack animation is active.

## Future Enemy Guidance

Other enemies may have the same broad pattern: collider contact can exist outside the animation
frames where the actor should consume that contact as gameplay. Do not put actor animation timing
inside `defender_owner`. For future enemy conversions:

- use `defender_owner` to answer "who did my attack touch?";
- keep the enemy-specific consume window in an actor-local helper near the enemy hook;
- continue recording pre-active or suspicious contact facts when diagnosing loops;
- only gate gameplay consumption, not diagnostics collection.
