# Co-op Secondary ALINK Item Ownership Plan

This was the active co-op milestone after `docs/coop-secondary-alink-input-routing-plan.md`. The main item/action ownership pass is now complete enough to move on; the current active milestone is `docs/coop-native-split-screen-camera-plan.md`.

## Purpose

Keep P2 basic input working while classifying and fixing the first item/action ownership hazards. The immediate goal is not "all items work." The immediate goal is to prove a small ownership pattern for item actors that currently bind visible state, return state, or availability to P1/global Link.

## Current Evidence

- P2 controller input works for the spawned secondary ALINK prototype.
- P2 can move from controller 2 with secondary execute enabled.
- P2 rolling worked.
- P2 basic combat swing worked.
- The default secondary ALINK probe set now runs secondary execute. `Skip execute` remains available only as a recovery/debug checkbox.
- `Ctrl+F12` is the fast test hotkey: it enables the co-op diagnostics profile, resets secondary ALINK probes to default, spawns P2 if the secondary slot is free, and shows a Dusk toast with the result.
- Fishing hook ownership is wrong: pulling it out for P2 made it invisible in P2's hands and visible on P1.
- Boomerang ownership was wrong: P2 could throw it, but P1 caught it and P2 could not throw it again afterward.
- Structured diagnostics showed the P2 boomerang flow enters `PROC_BOOMERANG_MOVE`, moves the item actor from P2 `mItemAcKeep` to P2 `mThrowBoomerangAcKeep`, then remains associated with that thrown actor while P2 returns to wait.
- The first boomerang owner bug is in `daBoomerang_c`: the boomerang actor asks `daAlink_getAlinkActorClass()` for held matrices, speed/range, aim/catch position, lock state, and `returnBoomerang()`, which always resolves global P1.
- The local boomerang owner fix routes those lookups through the ALINK slot whose `mThrowBoomerangAcKeep` owns the boomerang actor, falling back to global P1 for vanilla behavior. User validation confirmed P2's boomerang now returns to P2 and can be repeatedly rethrown, while P1's boomerang still works.
- The remaining P2 boomerang reticle/camera perspective is still P1-centered. Treat that as camera/HUD/attention ownership, not boomerang actor ownership.
- Fishing rod has a clean owner relationship for the first pass: `daAlink_c::checkFishingRodGrab(actor)` checks whether a rod actor is the ALINK's kept item actor. The first fishing patch should use that relationship only for hand attachment/ready-state owner facts, leaving camera/HUD/minigame policy alone.
- First fishing test result: the rod appeared in P2's hand and P2 could cast it, which confirms the hand attachment owner path. After casting, P2 got stuck and rod control followed P1's controller, exposing a second owner-specific path: `MG_ROD` samples raw `PAD_1` into its rod stick/substick/reel fields and those values feed ALINK fishing control.
- The second fishing patch routes the bobber rod input fields and immediate owner arm/cast callbacks through the owning ALINK slot. User validation confirmed P2 regained control after casting, and P1/P2 can use fishing rods simultaneously. Camera/HUD/minigame policy remains intentionally untouched.
- Broader item/weapon candidates are tracked in `docs/coop-player-owner-lookup-audit.md` so future fixes can classify owner-specific lookups without rediscovering the boomerang/fishing pattern each time.
- Dominion Rod showed the same characteristics as the boomerang. The first CROD patch routes held/thrown CROD actor matrix, light/top-use, speed/return, control-hit, catch/return, and draw visibility checks through the ALINK slot that owns either `mItemAcKeep` or `mCopyRodAcKeep`. User validation confirmed this fixes P2 Dominion Rod behavior.
- `alink.secondary` now includes copy-rod control/camera actor pointers and top-use state so future item fixes leave structured evidence behind even when the symptom and fix are already understood.
- Bow/arrow showed the projectile version of the same owner problem. P2 could enter the bow proc, but global player status forced P1 first-person camera and `daArrow_c` used global P1 for held matrix, shot origin, HIO values, and hit sounds. The local arrow patch stores the spawning ALINK owner on the arrow actor, routes owner-specific arrow work through that owner, and skips setting global bow/sling camera status for secondary ALINK prototypes. User validation confirmed P2 no longer forces P1 first-person, arrows fire from each owning player, and sound follows the shot correctly.
- Spinner is the ride-action version of the same owner problem. Its actor is kept in `mRideAcKeep`, and the first patch routes spinner lifecycle, draw visibility, movement constants, sounds, and raw local pad input through the ALINK slot whose ride keep owns the spinner actor. User validation confirmed this fixes P2's immediate Spinner despawn while P1 Spinner still works. The next Spinner issue was rail/slot detection: `Tag_Sppath` followed only global P1's position, so the rail patch makes the tag use the nearest registered ALINK that is currently riding Spinner. User validation confirmed P2 can now enter Spinner slots/rails.
- Bombs are the first counter-lifetime owner problem. P2 can place bombs, but each created bomb increments P2's `mActiveBombNum` and deletion decremented global P1, leaving P2's three-bomb limit stuck until respawn or area reload. The first bomb patch preserves the creating ALINK on normal, water, and bombling bombs so `daNbomb_c` deletion decrements the same slot that incremented the counter. User validation confirmed P2 can place more bombs after earlier bombs explode.
- P2 cannot pick bombs back up yet. Treat that as part of the broader object interaction/carry lane unless the counter patch exposes a bomb-specific pickup owner bug.
- Slingshot showed a create-time owner variant of the bow fix. P2 sling stones were visible only when P1 also had slingshot equipped. Letting secondary slingshot set the legacy global slingshot status was rejected because it reintroduced P1 first-person/facing leakage. The current containment keeps secondary bow/sling status suppressed and gives `daArrow_c` a pending owner during `fopAcM_fastCreate()` so sling-stone launch math can resolve P2 before `makeSlingStone()` returns. User validation confirmed P2 slingshot visibility, direction, and camera behavior are fixed.
- Iron Boots showed an equipment-model visibility variant. Equipping P2 boots hid P1's feet, then skipping only secondary shape changes let P1 boots hide P2's regular legs. The current containment skips the shared feet/leg `J3DShape::show()/hide()` calls for all PC ALINKs until per-player equipment model data exists. The follow-up audio patch routes heavy-boot footstep selection through the owning `Z2CreatureLink` instead of global `Z2GetLink()`. User validation confirmed cross-player leg visibility and P1/P2 heavy boot sounds are fixed.
- Item-specific `alink.secondary` blocks are gated to actual item context. `copy_rod`, `bomb`, and any future `bow` or `arrow` block should be absent during unrelated item tests; generic `equip_item`, `item_actor`, `ride_actor`, and `proc_name` remain always available for current ALINK state.
- The remaining known item-adjacent failure is P2 bomb pickup/object pickup, which is likely part of the broader object interaction/carry lane rather than this first item/action owner-routing pass.

## Working Assumptions

- P1 remains the compatibility baseline.
- Fishing and boomerang are useful probes because they exercise visible held-item attachment, spawned item actors, return-to-owner logic, and item availability state.
- Do not broaden to every item yet.
- Prefer one item family at a time.
- If two item families fail through the same owner lookup pattern, introduce one small helper; otherwise keep the first fix local.
- Co-op edits in original/decomp code need concise `Co-op:` why-comments.

## Implementation Plan

1. Audit the fishing hook and boomerang owner paths.
   - Find where item actors bind to Link hands/models.
   - Find where item actor return/catch ownership is resolved.
   - Find where Link item availability is cleared/restored.
2. Use diagnostics first if the owner path is unclear.
   - Prefer `player.slots`, `alink.secondary`, `input.pad`, and targeted low-volume item owner fields.
   - Do not add per-frame item logs.
3. Pick the smaller first fix.
   - If boomerang ownership has a clean owner actor ID/pointer path, start there.
   - If fishing hook attachment is simpler, start there.
4. Route only the proven owner lookup away from unconditional P1/global state.
   - Keep P1 behavior identical.
   - Keep camera/HUD/story state P1-owned.
5. Validate with the same secondary ALINK harness.
   - P2 movement still works.
   - The chosen item remains visible on/near P2 when P2 owns it.
   - The chosen item returns ownership/availability to P2 rather than P1.
   - P1 can still use the same item normally.

## Progress

- [x] P2 input routing milestone completed.
- [x] Fishing hook and boomerang identified as first item ownership failures.
- [x] Audit boomerang owner/catch/availability path.
- [x] Choose the smaller first item ownership fix.
- [x] Land one narrow item ownership helper or local fix.
- [x] Validate P1 unchanged and P2 ownership improved for boomerang.
- [x] Add reusable owner-lookup audit lane for item/weapon actors.
- [x] Validate fishing rod owner-scoped hand attachment.
- [x] Validate fishing rod owner-scoped input/cast recovery.
- [x] Pick the next item/weapon owner-lookup target from `docs/coop-player-owner-lookup-audit.md`.
- [x] Land first Dominion Rod owner-routing patch.
- [x] Validate Dominion Rod owner-scoped throw/return behavior.
- [x] Land first bow/arrow owner-routing patch.
- [x] Validate P2 bow shots use P2 origin without forcing P1 first-person.
- [x] Pick Spinner as the next item/action owner-lookup target from `docs/coop-player-owner-lookup-audit.md`.
- [x] Land first Spinner ride-owner routing patch.
- [x] Validate P2 Spinner no longer despawns immediately and still lets P1 use Spinner normally.
- [x] Land first Spinner rail/slot tag patch.
- [x] Validate P2 Spinner can enter rail/slot paths.
- [x] Identify bomb counter ownership failure.
- [x] Validate P2 bomb count resets after placed bombs explode.
- [x] Add gated bomb count diagnostics to `alink.secondary`.
- [ ] Classify P2 bomb pickup failure as bomb-specific or broader object interaction/carry ownership.
- [x] Validate P2 slingshot stones remain visible and use P2 facing when P1 does not have slingshot equipped.
- [x] Validate P1/P2 Iron Boots no longer hide the other player's regular legs.
- [x] Validate P1/P2 Iron Boots both emit heavy boot footstep sounds.

## Test Plan

The user owns Visual Studio/CMake builds unless explicitly delegated to Codex.

Manual test sequence:

1. Build with Visual Studio MSVC debug.
2. Press `Ctrl+F12` to enable diagnostics, reset default probes, and spawn Secondary Link Prototype.
3. Confirm the Dusk toast reports that diagnostics are enabled and P2 spawned.
4. If using the UI instead, confirm `Skip execute` is unchecked under the default probe set.
5. Confirm P2 still moves from controller 2.
6. Test the chosen item with P1.
7. Test the chosen item with P2.
8. Flush diagnostics if ownership is still wrong.

Expected next big win:

- One P2-owned item no longer redirects visible held state, return/catch state, or availability restoration to P1.
- Boomerang has reached this win for return/catch and availability restoration.
- Fishing rod has reached this win for visible hand attachment and owner-routed rod input/cast recovery, including simultaneous P1/P2 use.
- Dominion Rod has reached this win for the first copy-rod actor ownership cluster.
- Bow/arrow has reached this win for shot origin, held-arrow matrix, owner HIO values, and hit sounds.
- Spinner has reached this win for item lifecycle, owner-local input/sound, and rail/slot detection.
- Bombs have reached this win for active bomb count release after explosion.

Failure conditions:

- P1 item behavior regresses.
- P2 movement/input regresses.
- The fix depends on broad replacement of all player-singleton helpers.
- The chosen item works only by disabling P1/global item state that the game still needs.

## Cleanup Notes

- Once an item ownership pattern is confirmed, decide whether it belongs in a reusable helper.
- Reduce old action-mirror human logs once structured diagnostics cover the same evidence.
- Decide which secondary ALINK probe flags should become default co-op containment rather than debug checkboxes.
- If P2 execute becomes unstable again, temporarily re-enable `Skip execute` only to isolate the regression; do not treat it as the normal test path.
