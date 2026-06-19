# Dusk Co-op Multiplayer Roadmap

This roadmap is for adding cooperative multiplayer support to Dusk, the reverse-engineered Twilight Princess reimplementation in this repository. It is based on the current source tree, not on a generic game-networking template.

The core strategy is: preserve single-player behavior first, introduce explicit player identity, prove a second local player in a narrow vertical slice, then add host/client networking around the same command and state model. Do not build a full local co-op fork and hope to network it later.

## Assumptions

Dusk should continue to run Twilight Princess accurately in single-player. Co-op work is Dusk-specific and should follow `docs/code-conventions.md`: when original game code is modified for Dusk behavior, keep the original intent visible and fence Dusk-only changes with `#if TARGET_PC` or an equally clear local pattern already used by the project.

The first real target is two-player co-op. A design that supports more slots later is acceptable only when it costs almost nothing. The roadmap should not add a sprawling system for four-player online play before two players can stand in the same room.

The initial co-op mode should share campaign state, save data, quest flags, rupees, most inventory, scene transitions, and story progress. Per-player inventory, independent scene travel, independent cutscenes, and independent save files are later design choices, not the baseline.

The online model should be host-authoritative, effectively an authoritative server model. The first server can be the host player's Dusk process, but the boundary should stay server-shaped: clients send input or action commands, the host/server owns game truth, and clients render replicated results. This avoids relying on deterministic lockstep or peer/client-relay across machines. Deterministic lockstep means every machine simulates the same game from the same inputs and must stay bit-identical; this codebase was not built around that constraint.

## Minimal Planning Practice

Do not create a large planning directory tree yet. Keep the documentation surface small:

- This roadmap lives at `docs/co-op-roadmap.md`.
- When an implementation starts, create one self-contained plan in `docs/`, for example `docs/coop-player-slots-plan.md`.
- Each plan should include purpose, assumptions, progress, discoveries, decisions, validation, and recovery notes.
- Only introduce `docs/plans/active` and `docs/plans/completed` if there are several active co-op plans at once and the flat `docs/` directory becomes hard to scan.

The planning style should follow the spirit of the PLANS guidance without copying a large manual into the repo. Every implementation plan must be restartable from the file itself, but it should remain scoped to one milestone.

## Current Code Evidence

The repository already exposes strong single-player assumptions. The following observations were made against this tree on 2026-05-11.

- `include/d/d_com_inf_game.h` stores one player and one camera in `dComIfG_play_c`: `mCameraInfo[1]`, `mPlayerInfo[1]`, and `mPlayerStatus[1][4]`. It also has `mPlayerPtr[2]`, currently used as player and horse pointers.
- `src/d/d_com_inf_game.cpp` initializes those arrays by iterating over their current sizes.
- `include/d/actor/d_a_player.h` defines `daPy_getPlayerActorClass()` as `return (daPy_py_c*)dComIfGp_getPlayer(0);`.
- `src/d/actor/d_a_alink.cpp` registers Link globally during creation with `dComIfGp_setPlayer(0, this);` and `dComIfGp_setLinkPlayer(this);`. Its destructor clears those same globals.
- `src/d/actor/d_a_alink.cpp` reads controller port 1 directly in the movement/input path, for example `mDoCPd_c::getStickValue(PAD_1)`, `getTrigA(PAD_1)`, `getHoldB(PAD_1)`, and related calls.
- `include/m_Do/m_Do_controller_pad.h` already supports four controller ports through `PAD_1` to `PAD_4`, `m_gamePad[4]`, and `m_cpadInfo[4]`. The low-level pad wrapper is not the main blocker; the player actor is.
- `include/f_op/f_op_actor_mng.h` defines helpers such as `fopAcM_searchPlayerDistance`, `fopAcM_searchPlayerAngleY`, and `fopAcM_toPlayerShapeAngleY` directly in terms of `dComIfGp_getPlayer(0)`.
- `src/f_op/f_op_actor_mng.cpp` event helpers use the singleton player too. For example, `event_second_actor` returns `dComIfGp_getPlayer(0)`, and speak/item event ordering passes player 0 as the actor.
- `src/d/d_attention.cpp` sets `mpPlayer = dComIfGp_getPlayer(0)` and `mPadNo = PAD_1` when attention state refreshes.
- `src/d/actor/d_a_player.cpp` contains a static `daPy_py_c::m_midnaActor`, and many actors call `daPy_py_c::getMidnaActor()`. Midna ownership is global today.
- Boss and enemy actors frequently call player-singleton helpers. Examples include `src/d/actor/d_a_e_ba.cpp`, `src/d/actor/d_a_e_bg.cpp`, `src/d/actor/d_a_e_zm.cpp`, `src/d/actor/d_a_b_bq.cpp`, and `src/d/actor/d_a_b_ds.cpp`.
- There is no obvious test harness in this checkout. Searches for `enable_testing` and `add_test` did not reveal project tests. Validation must initially rely on builds, targeted debug instrumentation, and manual in-game scenarios.

Approximate singleton-touchpoint counts from `rg`:

- `dComIfGp_getPlayer(0)`: 791 matches under `src` and `include`.
- `daPy_getPlayerActorClass()`: 2,533 matches under `src` and `include`.
- `dComIfGp_getPlayerCameraID(0)`: 182 matches under `src` and `include`.
- Direct `PAD_1` reads through `mDoCPd_c`: 277 matches under `src` and `include`.

These counts are why the work should not begin with a mass replacement. It should begin by adding small, explicit player-slot and target-selection APIs, then converting call sites only when a milestone needs them.

## Current ALINK Duplication Finding

The first secondary ALINK prototype is documented in `docs/coop-secondary-player-prototype-plan.md`. It proved that a naive second `daAlink_c` is unsafe, but it did not prove that ALINK reuse should be abandoned.

Runtime evidence showed that secondary ALINK attention could be gated successfully, and that player 1 still enters `PROC_MOVE` with valid input after the secondary exists. The visible animation lock was fixed by restoring player 1's shared ALINK model-data ownership after secondary `playerInit()` / `changeLink()`. This confirms the main culprit was shared `J3DModelData` matrix-calculator ownership, not controller input, attention, action-proc selection, or late create-time animation/model calls.

The follow-up duplication audit is documented in `docs/coop-alink-duplication-audit-plan.md`. It confirmed scoped model-data ownership and shared attention lock as the first major singleton hazards. The input-routing milestone in `docs/coop-secondary-alink-input-routing-plan.md` then confirmed that secondary ALINK can consume controller 2 input for movement, rolling, and a basic combat swing.

The first item/action ownership pass is documented in `docs/coop-secondary-alink-item-ownership-plan.md`. Boomerang, fishing rod, Dominion Rod, bow/arrow, Spinner, bombs, slingshot, and Iron Boots all confirmed the same broad lesson: item actors often know their concrete owning ALINK, but still reach through P1/global helpers for matrices, counters, camera/status, sound, or lifecycle cleanup. Narrow owner-routing fixes made those item families usable for P2 without regressing P1.

The current active work surface is the remaining enemy review queue in
`docs/coop-enemy-audit.md`, beginning with `E_FK`, `E_GOB`, `E_HZ`, and `E_BUG`.
`docs/coop-p2-independent-control-plan.md` remains the control-ownership root plan rather than the
immediate queue. Split-screen is usable for co-op testing, with known V1 render/HUD limitations in
`docs/coop-native-split-screen-camera-plan.md` and validated singular-presentation/ItemGet teardown
evidence in `docs/coop-singular-event-presentation-plan.md`. The first world-acknowledgement proof
exists: Dusk-owned player-query helpers can let ordinary enemy logic react to P2 without
mass-rewriting global player helpers. Bokoblin and Tektite validated the first enemy API families:
`enemy_targeting` for combat target choice, `damage_owner` for who hit an enemy,
`selected_target_state` for facts about a known target, and `defender_owner` for who an enemy attack
touched.

The secondary Link experiment has graduated into the supported local additional-player path for current co-op testing. Runtime systems should identify player actors through the slot registry, not by inspecting ALINK's spawn argument. Spawn arguments now encode requested extra slots (`-2` for slot 1, `-3` for slot 2, `-4` for slot 3) only as a create-time bootstrap so `daAlink_c::create()` can avoid claiming vanilla player 0 before it has registered in the sidecar.

Requested additional slots and split-screen enablement are session intent, not properties of one play scene. Area loads clear scene-local actors and camera pointers normally. Once the next primary ALINK finishes creation, Dusk restores requested split-screen state and respawns requested additional slots from the new primary actor. P2 item assignments remain session-local across that reconstruction.

Reconstructed additional ALINKs are runtime joins, not second protagonists entering the area. They
keep structural `playerInit()` work, but do not register the global scene-start demo again or replay
P1's authored area-entry action proc. They enter an ordinary local wait/action state at the rebuilt
position.

The original ALINK model-data probe has also graduated into `dusk::coop::alink_model_data_owner`. Link body `J3DModelData` is shared while its installed matrix calculators are actor-local, so additional-player startup evaluation, execute, and draw temporarily install that ALINK's calculators and restore P1 afterward. This is runtime policy now, not an Actor Spawner checkbox.

Additional player spawning should not be owned by the ImGui Actor Spawner. The current debug button and `Ctrl+F12` hotkey are callers of the Dusk co-op lifecycle API. Future player-count settings, controller-port "press Start to join", and online host/client join handling should reuse the same slot-based `spawnPlayer(...)` path so local and networked co-op do not diverge. The registry, spawn request decoding, and controller-port mapping are shaped for four slots now; camera/render sidecars and diagnostics remain proven only for slot 1 until later plans extend them.

## Design Principles

Single-player remains the compatibility baseline. Slot 0 is the primary player and must behave exactly like the current game until a co-op option is enabled. Existing call sites such as `dComIfGp_getPlayer(0)` should continue to mean the primary player.

Prefer sidecar Dusk systems before resizing original game structs. For example, do not start by changing `mPlayerInfo[1]` to `mPlayerInfo[2]`. Add a small Dusk-owned slot registry first, under `src/dusk` and `include/dusk`, and hook it into Link creation. If later milestones prove that original arrays must expand, do that as a deliberate `#if TARGET_PC` change with a plan and validation.

Keep APIs narrow. The first player-slot API only needs to answer: who is the primary player, who is slot 1, which slot owns this actor, and which player is nearest to an actor. Avoid a full entity system, service locator, or abstract multiplayer framework.

Treat input as commands early. The player actor should consume a per-slot input snapshot rather than reading `PAD_1` everywhere. This is useful for local co-op immediately and becomes the same shape clients send to the host later.

Do not make enemies deterministic across machines. Enemies should be simulated by the host. Clients should receive enemy state or event results from the host once online play exists.

Network-readiness notes live in `docs/coop-network-multiplayer-readiness.md`. In particular, preserve `PlayerSlot` as the replicated player identity, treat raw actor pointers as process-local metadata, and add explicit apply-replicated-state seams before clients are allowed to render host-owned enemy or world truth.

Treat cutscenes, scene transitions, menus, and save data as global at first. These systems are deeply singleton-shaped in the current code. Early co-op should pause, park, hide, or tether secondary players during global sequences instead of trying to make every story event multiplayer-native immediately.

## Proposed Small Code Surface

When implementation begins, prefer a tiny Dusk-owned surface like this:

    include/dusk/coop/player_slots.h
    src/dusk/coop/player_slots.cpp
    include/dusk/coop/input.h
    src/dusk/coop/input.cpp

If a single pair can hold both slot and input code cleanly at first, use only:

    include/dusk/coop/coop.h
    src/dusk/coop/coop.cpp

Add files to `files.cmake`, since this project lists source files explicitly. Do not create `src/dusk/coop/net`, `src/dusk/coop/replication`, or other folders until a milestone actually needs them.

The first player-slot interface should be boring:

    namespace dusk::coop {
    enum class PlayerSlot : unsigned char { Primary = 0, Secondary = 1 };

    void registerPlayer(PlayerSlot slot, fopAc_ac_c* actor);
    void unregisterPlayer(PlayerSlot slot, fopAc_ac_c* actor);
    fopAc_ac_c* getPlayer(PlayerSlot slot);
    fopAc_ac_c* getPrimaryPlayer();
    bool isPrimaryPlayer(const fopAc_ac_c* actor);
    PlayerSlot getSlotForActor(const fopAc_ac_c* actor);
    fopAc_ac_c* findNearestPlayer(const fopAc_ac_c* actor);
    }

The first input interface should be a snapshot of existing pad state, not a new input engine:

    namespace dusk::coop {
    struct PlayerInputState {
        unsigned int holdButtons;
        unsigned int triggerButtons;
        float stickX;
        float stickY;
        float stickValue;
        short stickAngle;
        float subStickX;
        float subStickY;
        float analogL;
        float analogR;
    };

    PlayerInputState readLocalInput(PlayerSlot slot);
    }

Only add fields when a real `d_a_alink.cpp` use requires them.

## Roadmap

### Phase 0: Baseline Audit And Guardrails

Goal: make the project navigable for co-op work without changing runtime behavior.

Create a short implementation plan for the first milestone, not for the whole project. Record the current singleton facts listed above, the build command used locally, and the manual scenario used for validation. The plan should name exact files and expected behavior.

Acceptance:

- A contributor can point to the player, input, camera, attention, event, and enemy touchpoints in the current code.
- The current game builds before any co-op code changes.
- No behavior changes are made in this phase.

Recommended command on Windows, from the Dusk root:

    cmake --preset windows-msvc-debug
    cmake --build --preset windows-msvc-debug

Use the platform preset from `docs/building.md` if working on macOS or Linux.

### Phase 1: Player Slots With No Behavior Change

Goal: add explicit player identity while the game still behaves exactly like single player.

Add a Dusk-owned player-slot registry. Hook slot 0 registration into the existing Link create/delete path in `src/d/actor/d_a_alink.cpp`, next to the current calls to `dComIfGp_setPlayer(0, this)` and `dComIfGp_setLinkPlayer(this)`. Do not change what `dComIfGp_getPlayer(0)` returns. Do not resize `dComIfG_play_c` yet.

The registry should be a sidecar. It records that the primary Link actor is slot 0. It may have an empty slot 1. It should not allocate heavily, not own actor lifetime, and not change save data.

Acceptance:

- Single-player boot and control are unchanged.
- `dusk::coop::getPrimaryPlayer()` returns the same actor as `dComIfGp_getPlayer(0)` after Link is created.
- The registry is cleared when Link is deleted, but only if the actor being deleted matches the registered actor.
- A debug-only assertion or log can prove slot registration without affecting retail behavior.

Why this phase matters: it gives future work a real player identity vocabulary without touching hundreds of call sites.

### Phase 2: Input Snapshot For Primary Player

Goal: make Link's input path capable of reading from a slot-specific snapshot while still mapping primary player to `PAD_1`.

Start with the concentrated input section in `src/d/actor/d_a_alink.cpp` that computes movement and item trigger/button state. It currently reads `PAD_1` directly around the movement path. Replace only that local cluster with a helper that returns the input state for the actor's slot. Keep behavior identical for slot 0.

Do not replace every `PAD_1` in the repository. Some are debug shortcuts, menus, title-screen controls, or Dusk feature hooks in `src/d/actor/d_a_alink_dusk.cpp`. Convert them only when a player-slot behavior needs them.

Acceptance:

- Slot 0 still reads `PAD_1`.
- Movement, item buttons, mirror mode stick angle behavior, and debug movement behave as before for player 1.
- The input snapshot can be serialized later without dragging `JUTGamePad` or `mDoCPd_c` into networking code.

Why this phase matters: online play will eventually send commands resembling `PlayerInputState`; local co-op can use the same shape by reading `PAD_2`.

### Phase 3: Secondary Player Spawn Prototype

Goal: prove whether a second player actor can exist safely, or discover that a lighter proxy actor is needed.

Do not assume that `fopAcM_create(fpcNm_ALINK_e, ...)` is enough. The current `daAlink_Create` path sets `dComIfGp_setPlayer(0, this)` unconditionally, and deletion clears the primary player globals. A second ALINK instance would currently overwrite the primary singleton.

Run this as an explicit prototype. There are two viable experiments:

1. Add a Dusk-only secondary-player creation mode for ALINK. The secondary instance must not call `dComIfGp_setPlayer(0, this)`, must not call `dComIfGp_setLinkPlayer(this)`, and must not clear those globals on delete. It registers only in the sidecar slot registry.
2. If duplicating `daAlink_c` proves too entangled with static state, Midna state, heaps, or model ownership, build a temporary secondary Link proxy actor that can render and move but does not yet run the full Link state machine.

Choose experiment 1 first only if it can be done with a small parameter check and clear guards. Stop if the patch starts to require broad rewrites in `d_a_alink.cpp`.

Acceptance:

- A second local actor can be created and deleted without changing `dComIfGp_getPlayer(0)`.
- Player 1 remains fully playable.
- Slot 1 can be moved in a constrained debug scenario, or at minimum replicated beside player 1 as a visible non-authoritative actor.
- The plan records whether full `daAlink_c` duplication is viable or whether a proxy/replica actor is the safer path.

Why this phase matters: it tests the largest unknown before networking or enemy sync are attempted.

### Phase 4: Native Split-Screen Camera

Goal: keep the game playable and testable with two local actors by giving P1 and P2 real native cameras and render viewports.

The original camera path is still singleton-shaped in storage: `mWindow[1]`, `mCameraInfo[1]`, `mPlayerInfo[1]`, and many camera-0 call sites. However, the code also exposes indexed camera/window/player accessors and a camera manager table that can represent more than one camera process. The current plan is to preserve original layout for slot 0 and add a Dusk-owned extension layer for slot 1 behind the existing accessors.

Do not build a cheap camera imitation. Use native `dCamera_c`, camera process creation, and `dDlst_window_c` render windows where possible. Event cameras, boss cameras, message cameras, cutscene cameras, HUD layout, fades, and restart/save camera state are diagnostic targets first, not V1 fixes.

Acceptance:

- Split-screen disabled preserves normal P1 camera/rendering.
- Split-screen enabled creates or assigns camera 1 to P2 and render window 1 to camera 1.
- The painter can draw two active render windows during normal field gameplay.
- P1 and P2 can move apart and remain visible through their own cameras.
- HUD/fullscreen overlays may remain P1/global for the first milestone.
- Camera 0 call sites encountered during validation are classified rather than blindly replaced.

Why this phase matters: shared camera is now the test bottleneck. A native split-screen foundation makes future camera, object interaction, AI, and online-readiness work easier without pretending every special camera mode is already solved.

### Phase 5: Local Co-op Gameplay Slice

Goal: create one narrow, honest co-op slice: two local players can move, interact with basic collision, and participate in simple combat or object interaction.

Pick one simple scene and one small set of mechanics. Avoid bosses, cutscenes, horseback, Midna-heavy interactions, fishing, sumo, spinner rails, and story gates. The first slice should be ordinary movement and one or two simple enemy/object interactions.

For targeting, add helpers that select a player by policy:

- primary player,
- nearest living player,
- owning/interacting player,
- explicit slot.

Do not mass-rewrite `fopAcM_searchPlayerDistance` and friends. Add new helpers and convert one small enemy or object family. `include/f_op/f_op_actor_mng.h` shows the old helpers are inline aliases to player 0; leave them alone until a concrete actor needs a multiplayer-aware version.

Acceptance:

- Player 2 can be controlled locally.
- At least one simple enemy or hazard can react to player 2 through a multiplayer-aware target helper.
- Player 1 behavior remains unchanged when co-op is disabled.
- The implementation plan records which actors were intentionally converted and which remain primary-player-only.

Enemy slice: `docs/coop-enemy-targeting-plan.md` tracks the regular-enemy co-op API families validated on Bokoblin (`E_OC`) and Tektite (`E_TT`). The completed world-acknowledgement proof in `docs/coop-world-acknowledgement-plan.md` covers the hanging Helmasaur upward-wait proximity trigger (`e_hm.up_wait`) plus basic Bokoblin recognition/chase evidence.

Enemy coverage is tracked in `docs/coop-enemy-audit.md`. The first policy architecture lives in `docs/coop-enemy-targeting-plan.md`. The intended long-term layering is `actor patches -> enemy_targeting -> player_query`: actor files should make narrow, evidenced hooks; `enemy_targeting` should own sticky target retention, attack follow-through, recent-attacker bias, and target-pressure rules; `selected_target_state` should answer what the chosen target is doing; `player_query` should remain the raw facts provider for active-player candidates, distances, angles, and diagnostics.

Player-singleton callsites should be routed through the API-family map in `docs/coop-player-singleton-api-map.md` before any broad conversion work.

Why this phase matters: it distinguishes real co-op progress from just spawning a mannequin.

### Phase 6: Global State Policy

Goal: define rules for systems that should remain global at first.

The current save and inventory APIs under `include/d/d_com_inf_game.h` route through `g_dComIfG_gameInfo.info.getPlayer()`. That means health, items, equipment, rupees, quest flags, horse place, and last-position records are single campaign state. Treat them that way initially.

The current event code in `src/f_op/f_op_actor_mng.cpp` and many actor scripts assumes one event participant is the player. For early co-op:

- Only the primary player starts story events and room transitions.
- Secondary players are parked, hidden, tethered, or temporarily made passive during global events.
- Item pickups and quest progression are host/global actions, not per-player actions.
- Death/gameover is global at first, unless a later plan adds revive behavior.

Acceptance:

- Opening doors, entering transitions, message events, item get demos, and save/load do not corrupt slot state.
- Secondary player behavior during events is explicit and consistent.
- No per-player inventory is introduced accidentally.

Why this phase matters: global state is where co-op projects quietly become unmaintainable.

### Phase 7: Same-Machine Host/Client Skeleton

Goal: make local play use a network-shaped authority boundary before real internet networking.

Introduce a host/client loop in-process or on localhost. The host owns authoritative game state. A local client sends `PlayerInputState` commands for slot 1. The host applies commands to slot 1. This can be built without internet transport at first.

The command stream should include at least:

- frame or tick number,
- player slot,
- buttons held,
- buttons pressed this frame,
- stick values,
- camera-relative intent if needed,
- a small sequence number for debugging drops/reordering later.

Do not add prediction yet. Do not add rollback. Do not add encryption, matchmaking, NAT traversal, or lobbies.

Acceptance:

- Player 2 can be driven by commands that do not directly read `PAD_2` inside `daAlink_c`.
- The same command path can be fed from local pad input or a local loopback client.
- Host/client logging can show commands received and applied for slot 1.

Why this phase matters: it prevents local co-op from becoming a shared-memory-only design.

### Phase 8: LAN Transport Prototype

Goal: send player commands between two Dusk processes on a LAN or localhost with the minimum viable transport.

Choose a small transport only after Phase 7 has a command stream. The repository currently has HTTP code for update checks and socket code for Discord/LiveSplit, but no game networking layer. Do not force gameplay networking through the update-check HTTP abstraction. A UDP-based transport is likely appropriate for action inputs and snapshots, but the transport decision should be made in its own plan after local command authority exists.

At this phase, synchronize only player join, leave, command input, and coarse player state. Keep enemies host-owned and primary-player-only until player movement is stable.

Acceptance:

- Two processes can connect on localhost or LAN.
- Client input moves the client-owned slot on the host.
- Remote player position, facing, and animation-enough state are visible on the other process.
- Disconnect returns to a safe local state without corrupting save data.

Why this phase matters: real process separation exposes assumptions that same-process local co-op hides.

### Phase 9: Enemy And World State Sync

Goal: move from "players can connect and move" to "co-op combat and world interactions are believable."

Enemies should be simulated by the host. Convert enemy targeting through explicit target helpers rather than letting each client run AI independently. Start with simple enemies that do not own complex boss cameras, scripted demos, or unique story state. Avoid bosses until ordinary enemies, drops, and damage are reliable.

The host should own:

- enemy AI decisions,
- enemy positions and animation-relevant state,
- enemy damage and death,
- item drops and pickups,
- world switch/object state that affects progression.

Clients should render snapshots and local effects. Later, clients may predict remote player movement, but not enemy truth.

Acceptance:

- A simple enemy can choose either player as target on the host.
- Both processes agree when the enemy is damaged or killed.
- Item drops and pickups happen once.
- Converted actors are documented; unconverted actors intentionally target primary player.

Why this phase matters: enemy sync is where networked PvE either becomes coherent or collapses into divergent local simulations.

### Phase 10: Broaden Coverage Carefully

Goal: grow from a vertical slice to a game mode without turning every system into a speculative abstraction project.

Work through systems by category:

- common enemies,
- common pickups and carry objects,
- doors and room transitions,
- environmental hazards,
- mini-games,
- vehicles and mounts,
- Midna interactions,
- dungeon/boss-specific scripts,
- cutscenes and ending sequences.

Each category should have its own implementation plan and acceptance scenario. Do not convert all 2,533 `daPy_getPlayerActorClass()` call sites. Many should remain primary-player-only because they are UI, story, camera, save, debug, or single-owner mechanics.

Acceptance:

- Each broadened category has a before/after scenario and a rollback path.
- No broad category is marked done because a helper exists; it is done only when a human can observe the behavior in game.

## High-Risk Areas

Secondary `daAlink_c` duplication may fail. The class owns huge amounts of state, special heaps, item actors, Midna models, ride actors, and demo state. If a second full Link is too invasive, a replica actor may be the right intermediate for remote players while only local authoritative players use full `daAlink_c`.

Midna is global. `daPy_py_c::m_midnaActor` and many Midna APIs assume one player relationship. Early co-op should bind Midna to the primary player and make secondary Midna behavior out of scope.

Horse and vehicles are shared/special actors. `mPlayerPtr[2]` currently stores player and horse pointers, and many horse/boar/canoe/spinner paths ask the player singleton for ride state. Treat these as later categories.

Cutscenes and event demos reposition player 0 directly through `setPlayerPosAndAngle`, `changeOriginalDemo`, and `changeDemoMode`. Secondary players should be passive during these until a specific event category is converted.

Camera expansion is expensive. `mCameraInfo[1]`, `mWindow[1]`, `dComIfGp_getPlayerCameraID(0)`, and event/boss camera code indicate split-screen is not a first milestone.

Testing is weak today. Because there is no clear unit test harness, each plan must include build validation and a manual in-game scenario. Where practical, add small debug-only asserts or logs to prove internal state.

## Things Not To Do Yet

Do not mass-replace `daPy_getPlayerActorClass()` with a new helper. That creates churn without proving behavior.

Do not resize core original structs as the first step. Start with sidecar data and only alter original layout when needed.

Do not make per-player inventory, independent saves, or independent story progression part of the first playable slice.

Do not implement online transport before input commands and slot ownership work locally.

Do not implement rollback, lag compensation, matchmaking, lobbies, or reconnect support before LAN movement works.

Do not convert bosses first. Bosses are dense with camera, demo, event, and special-item assumptions.

Do not turn Dusk into a new engine architecture. The project already has an actor/process system; co-op should adapt to it.

## Suggested First Three ExecPlans

The next contributor should not start with the whole roadmap. Start with these small plans, one at a time:

1. `docs/coop-player-slots-plan.md`: add a no-behavior-change slot registry and register primary Link.
2. `docs/coop-input-snapshot-plan.md`: route one concentrated Link input path through a slot-specific input snapshot while preserving `PAD_1` behavior.
3. `docs/coop-secondary-player-prototype-plan.md`: test whether a second ALINK can exist without overwriting player 0, and record whether to continue with full Link or pivot to a replica actor.

Each plan should include:

- the exact files to touch,
- the exact build command,
- the manual scenario to run,
- what should be observed,
- the decision log,
- the discoveries made during implementation.

## Definition Of Early Success

Early success is not "online co-op works." Early success is:

- single-player behavior is unchanged when co-op is disabled,
- the code has explicit player slot identity,
- Link input can come from a slot command instead of direct `PAD_1` reads,
- a second local player or replica can exist without corrupting player 0,
- one simple local co-op scenario works,
- the path to host/client commands uses the same input abstraction.

That is the point where online work becomes a continuation of the architecture, not a rewrite.
