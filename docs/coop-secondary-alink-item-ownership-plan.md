# Co-op Secondary ALINK Item Ownership Plan

This is the active co-op milestone after `docs/coop-secondary-alink-input-routing-plan.md`.

## Purpose

Keep P2 basic input working while classifying and fixing the first item/action ownership hazards. The immediate goal is not "all items work." The immediate goal is to prove a small ownership pattern for item actors that currently bind visible state, return state, or availability to P1/global Link.

## Current Evidence

- P2 controller input works for the spawned secondary ALINK prototype.
- P2 can move from controller 2 with secondary execute enabled.
- P2 rolling worked.
- P2 basic combat swing worked.
- Fishing hook ownership is wrong: pulling it out for P2 made it invisible in P2's hands and visible on P1.
- Boomerang ownership is wrong: P2 could throw it, but P1 caught it and P2 could not throw it again afterward.
- The remaining failures are item/action ownership failures, not the original "can P2 receive input?" problem.

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
- [ ] Audit boomerang owner/catch/availability path.
- [ ] Audit fishing hook model/hand attachment path.
- [ ] Choose the smaller first item ownership fix.
- [ ] Land one narrow item ownership helper or local fix.
- [ ] Validate P1 unchanged and P2 ownership improved.

## Test Plan

The user owns Visual Studio/CMake builds unless explicitly delegated to Codex.

Manual test sequence:

1. Build with Visual Studio MSVC debug.
2. Enable diagnostics profile if capture is needed.
3. Spawn Secondary Link Prototype.
4. Uncheck `Skip execute`.
5. Confirm P2 still moves from controller 2.
6. Test the chosen item with P1.
7. Test the chosen item with P2.
8. Flush diagnostics if ownership is still wrong.

Expected next big win:

- One P2-owned item no longer redirects visible held state, return/catch state, or availability restoration to P1.

Failure conditions:

- P1 item behavior regresses.
- P2 movement/input regresses.
- The fix depends on broad replacement of all player-singleton helpers.
- The chosen item works only by disabling P1/global item state that the game still needs.

## Cleanup Notes

- Once an item ownership pattern is confirmed, decide whether it belongs in a reusable helper.
- Reduce old action-mirror human logs once structured diagnostics cover the same evidence.
- Decide which secondary ALINK probe flags should become default co-op containment rather than debug checkboxes.
