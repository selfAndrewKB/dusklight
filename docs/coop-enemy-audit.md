# Co-op Enemy Audit

This audit tracks enemy and enemy-like actor files that may need co-op-aware player targeting. It is separate from `docs/coop-player-owner-lookup-audit.md`: item actors usually need owner lookup, while enemies need target selection policy.

## Targeting Architecture

The intended layering is:

```text
actor patches -> enemy_targeting -> player_query
```

- `player_query`: raw facts about active players: candidates, distances, angles, slots, and diagnostics.
- `enemy_targeting`: policy for choosing and retaining a target through reusable behavior scopes.
- actor patches: narrow conversions at concrete enemy callsites, preserving vanilla behavior outside the scoped PC/co-op hook.

The current Bokoblin, Tektite, Stalhound, Stalchild, and Gibdo proofs now use `enemy_targeting`
over `player_query`. Bokoblin remains the richer melee validation surface; Tektite is the first
compact non-Bokoblin port, Stalhound and Stalchild are breadth proofs, and Gibdo is the first
target-state-sensitive humanoid/undead proof. Choose the next enemy from the audit queue rather
than widening any one proof surface by default.

## Target Policy Requirements

Nearest-player selection is a useful primitive, but it is too twitchy as a universal enemy policy. The policy layer should support at least:

- target acquisition from nearest/visible active player candidates,
- sticky target retention for simulation seconds, not render frames,
- attack follow-through that does not retarget mid-attack unless the target disappears or becomes invalid,
- recent attacker bias so damage can pull attention,
- optional target-pressure weighting so one player does not receive every enemy in a crowd,
- diagnostics that show why a target was selected, retained, or changed.

Do not build all of this before the audit is useful. Start with sticky retention and attack follow-through, then add bias/pressure only when a tested enemy needs it.

Every converted enemy must use an actor-local helper near the top of the enemy file. The helper builds `EnemyTargetContext`, sets a reusable behavior scope such as `EnemyTargetScope::Combat`, and accepts a manual diagnostic label from the original callsite. The scope owns target state; labels explain which original callsite consumed that state. Do not key retention by labels or create independent per-callsite retention machines.

## Conversion Checklist

For each enemy family, classify and test these layers separately:

| Layer | What To Look For | Status Values |
| --- | --- | --- |
| Search/wake | Idle recognition, proximity checks, line-of-sight checks | P1-only / raw-query / policy-backed / validated |
| Chase/turn | Movement target position, face-player angle, home-distance checks | P1-only / raw-query / policy-backed / validated |
| Attack gate | Distance and angle checks that decide whether an attack starts | P1-only / raw-query / policy-backed / validated |
| Follow-through | Attack animation/facing after startup | P1-only / raw-query / policy-backed / validated |
| Damage/guard/cut | Hit reactions, shield checks, cut-type checks, special player states | P1-only / owner-aware / global-by-design / needs audit |
| Projectile/grab/spawn | Spawned enemy weapons, grabs, catches, thrown objects | P1-only / owner-aware / host-owned / needs audit |
| Item awareness | Hookshot, boomerang, bomb, bait, tool, or owned-item reactions | P1-only / active-player scan / owner-aware / deferred |
| Spawn/presentation | Master/child spawning, child facing, intro/fanfare angles, camera-facing presentation | P1-only / target-slot-aware / global-by-design / deferred |
| Demo/event/camera | Special cameras, scripted scenes, boss/event behavior | global-by-design / deferred / needs audit |

Before moving from one enemy to the next, update that enemy's row with every left-out or deferred API
hook discovered during implementation. Do not rely on memory or on "mostly works in-game." Name the
missing family explicitly: damage-owner, defender-owner, selected-target state, item awareness,
master/child ownership, presentation/camera ownership, caught/grab/swallow/hang ownership, broader
collision-owner, render/visibility culling, or story/demo/global state.

## Current Proofs

| Actor | Name | File | Profile | Current State | Notes |
| --- | --- | --- | --- | --- | --- |
| Hanging Helmasaur | Hanging Helmasaur | `src/d/actor/d_a_e_hm.cpp` | `E_HM` | raw-query proof | Only `e_hm.up_wait` wake/proximity is converted. Other combat and damage behavior remains P1/global. |
| Basic Bokoblin | Bokoblin | `src/d/actor/d_a_e_oc.cpp` | `E_OC` | policy-backed targeting, owner APIs validated | Search, head-search, find/chase, move-out, attack gates, and follow-through use `enemy_targeting`; sword-sound awareness uses `selected_target_state`; sword hit reactions use `damage_owner`; guard collision uses `defender_owner`. |
| Tektite | Tektite | `src/d/actor/d_a_e_tt.cpp` | `E_TT` | policy-backed targeting, owner APIs validated | Search/chase/attack/out-range use `enemy_targeting`; ordinary target facts and first-attack prediction use `selected_target_state`; cut reactions use `damage_owner`. Culling remains render/visibility work. |
| Stalhound | Stalhound | `src/d/actor/d_a_e_sh.cpp` | `E_SH` | policy-backed targeting, first-pass validated | Central target metrics, movement speed, attack commitment, head tracking, and damage knockback angle route through the co-op API families. No P1 guard-state read was found; attack shield response currently uses the collision `ChkAtShieldHit()` flag. First surface test looked good. |
| Stalchild | Stalchild | `src/d/actor/d_a_e_bs.cpp` | `E_BS` | policy-backed targeting, first-pass validated | Swarm-style ground melee proof. Recognition, chase/attack target metrics, attack-facing checks, head tracking, and attack guard response route through the co-op API families. |
| Gibdo | Gibdo | `src/d/actor/d_a_e_gi.cpp` | `E_GI` | policy-backed targeting, owner APIs validated | Sleep/wait/chase/attack/damage recovery and head tracking route through `enemy_targeting` plus `selected_target_state`; the close-range attack/scream gate uses immediate acquisition so stale chase retention cannot suppress an in-range player; ordinary sword cut reactions use `damage_owner`; scream stun uses `caught_stun_owner` so the retained slot owns release input and best-effort camera lock while nearby affected slots share the scream animation timer with an expanded co-op AoE. Wolf-bite ownership remains intentionally deferred to a later caught/grab-owner proof enemy. `gibdo.state` remains available for follow-up debugging and records native range/angle/LOS/delay/scream-owner gates. Some placed Gibdos use `mSwbit2` switch gating before sight checks, so apparent P1-only activation may also involve room script state. Simultaneous per-player scream ownership is documented as deferred because vanilla `m_cry_gi` also coordinates follow-up attacks. |
| Young Gohma | Young Gohma | `src/d/actor/d_a_e_kg.cpp` | `E_KG` | policy-backed targeting, first-pass validated | Central action target metrics route through `enemy_targeting` plus `selected_target_state`; move/search and attack gates share the selected target's distance, angle, and line-of-sight actor. Roof/drop front-roll awareness uses the selected player's state instead of P1. Damage remains routed through the shared `cc_at_check()` owner path. `young_gohma.state` records range/cone/LOS and `pl_check` gate facts. The post-attack wander gap was confirmed to match vanilla Young Gohma behavior rather than co-op target loss. |
| White Wolfos | White Wolfos | `src/d/actor/d_a_e_ww.cpp` | `E_WW` | policy-backed targeting, first-pass validated | Combat chase/attack/walk/move-out paths use one Combat owner plus `selected_target_state`; guard contact uses `defender_owner`; hookshot side-step awareness scans active players for live hookshot top positions instead of asking P1; master spawn staging can wake on P2; child facing follows the encounter anchor; presentation angles use the selected slot's camera when available. Remaining special/demo presentation and any broader spawn ownership should be documented as presentation/camera or master/child ownership if encountered in later passes. |
| Amos | Armos | `src/d/actor/d_a_e_ai.cpp` | `E_AI` | policy-backed targeting, pending validation | Wake, movement, attack gates, line-of-sight checks, and shield-facing state route through one Combat owner plus `selected_target_state`; sword cut reactions use `damage_owner`. No defender-owner or caught/grab/spawn surfaces were found in this first pass. |

## Reviewed Evidence

These rows have been inspected beyond the machine count. "Label evidence" is taken from strings/resource names/action names in the actor file; keep the original label when it is more honest than guessing an English name.

| Actor/File | Profile | Label Evidence | Classification | Targeting Notes |
| --- | --- | --- | --- | --- |
| `d_a_e_ai.cpp` | `E_AI` | HIO label `アモス`; actions wait/move/attack/damage/return | regular enemy candidate | Small action table; likely a good early audit target after names/locations are confirmed. |
| `d_a_e_ba.cpp` | `E_BA` | enemy name `E_ba`; attack and wolf-bite actions | regular enemy candidate | Has direct wolf-bite state, so damage/wolf reaction may be separate from search/chase. |
| `d_a_e_bi.cpp` | `E_BI` | enemy name `E_bi`; wait/up/move/water/disappear actions | regular/proximity candidate | Simple action table; likely useful for a low-risk wake/chase audit. |
| `d_a_e_bs.cpp` | `E_BS` | HIO label `ベビースタル`; normal/fight-run/attack/damage actions | regular melee candidate | Looks like a simple melee enemy shape with weapon model and guard/damage checks. |
| `d_a_e_bu.cpp` | `E_BU` | HIO label `バブル`; fly/fight/attack/chance/head actions | flying enemy candidate | Needs vertical targeting policy, not just XZ nearest. |
| `d_a_e_cr.cpp` | `E_CR` | HIO label `クレイジーランナー`; move/damage actions | simple movement enemy candidate | Small file, likely good for movement/proximity conversion after identification. |
| `d_a_e_fk.cpp` | `E_FK` | HIO label `ファントム騎馬兵` | mounted/special enemy | Mounted/special; defer until ordinary enemies and mount-related targeting are understood. |
| `d_a_e_fz.cpp` | `E_FZ` | enemy name `E_fz`; wait/move/attack/damage actions | regular enemy candidate | Moderate action table; no HIO label found in first pass. |
| `d_a_e_gb.cpp` | `E_GB` | HIO label `デカババ`; head/flower actions, bomb-eat, drop-key demo | plant/special enemy | Has key/demo and bomb-specific behavior; not a first-wave target. |
| `d_a_e_gob.cpp` | `E_GOB` | HIO label `マグネゴロン`; fight/attack/defence/grab/jump/message actions | special/NPC-like combat actor | Despite enemy prefix, message/grab/special actions make it a poor first-wave target. |
| `d_a_e_hb.cpp` | `E_HB` | HIO label `デグババ（ボックリ）`; stay/appear/wait/attack/chance actions | plant enemy candidate | Plant enemy with leaf/helper model; audit after ordinary mobile enemies. |
| `d_a_e_kk.cpp` | `E_KK` | HIO label `氷の剣士`; walk/spear throw/guard/attack/weaponmove actions | regular humanoid/ranged candidate | Good later stress test for melee plus thrown weapon policy. |
| `d_a_e_pm.cpp` | `E_PM` | demo action and trumpet/glow models | demo/special enemy | Many demo and player demo calls; defer. |
| `d_a_e_sf.cpp` | `E_SF` | op-demo, guard, sitwait/crashwait/getup actions | regular humanoid with demo intro | Possible regular enemy, but first-contact demo paths need care. |
| `d_a_e_st.cpp` | `E_ST` | HIO label `スタルチュラ`; search/shoot/jump/hang phases | multi-form regular enemy | Good policy stress test after simple ground enemies. |
| `d_a_e_sw.cpp` | `E_SW` | chase/attack/bomb/hook/dive/catch actions | special mobile enemy | Hook/bomb/catch paths make it a later audit target. |
| `d_a_e_tt.cpp` | `E_TT` | HIO label `テクタイト`; wait/chase/attack/out-range actions | regular enemy candidate | Compact chase/attack state; promising first-wave candidate after audit. |

### Reviewed Batch: Swarms, Ghosts, Flyers, And Specials

These rows are from a second source pass. They are mostly classification gates: the point is to avoid accidentally choosing a boss helper, swarm, grab plant, or mounted/path actor as the first policy specimen.

| Actor/File | Profile | Label Evidence | Classification | Targeting Notes |
| --- | --- | --- | --- | --- |
| `d_a_e_bee.cpp` | `E_BEE` | bee sub-actions `ACT_HOME`, `ACT_FLY`, `ACT_FLY_HOME_A/B`; parent nest lookup | helper/swarm enemy | Tied to `E_NEST` and hit-actor state; classify with nest/spawner behavior, not standalone targeting. |
| `d_a_e_bg.cpp` | `E_BG` | swim/attack/bomb/birth/hook/eat actions; fishing rod bait lookup | water/special enemy | Uses fishing rod bait and water movement; useful later, not first ground-policy wave. |
| `d_a_e_bug.cpp` | `E_BUG` | group insect simple-model setup; boomerang and bomb searches | swarm/special enemy | Group behavior and item reactions make it a later swarm/hazard category. |
| `d_a_e_dd.cpp` | `E_DD` | search, attack, flame, bomb/tail/arrow damage actions | complex regular fire enemy | Good later stress target because search/attack are compact but damage has several special reaction branches. |
| `d_a_e_df.cpp` | `E_DF` | LinkEat/BombEat/ObjEat/Search/Wait actions | plant/grab special | Eat/grab ownership makes this later than ordinary chase enemies. |
| `d_a_e_dk.cpp` | `E_DK` | wait/chase/attack/damage/death actions; separate core model | regular/complex enemy candidate | Looks policy-relevant but has a core/body split; audit after simpler ground enemies. |
| `d_a_e_fb.cpp` | `E_FB` | wait/attack/damage/bullet actions; vertical player-distance gates | static/ranged special enemy | Good later ranged/static test; vertical checks should not drive first ground policy. |
| `d_a_e_ge.cpp` | `E_GE` | wait/fly/attack/back/caw/wind/shield actions | flying enemy candidate | Needs vertical/circle-flight policy; good second-wave airborne specimen. |
| `d_a_e_gi.cpp` | `E_GI` | sleep/wait and sword model evidence | regular humanoid/undead candidate | Gibdo. First pass covers combat targeting, ordinary damage-owner reads, and scream stun ownership; wolf-bite ownership remains deferred caught/grab work. |
| `d_a_e_gm.cpp` | `E_GM` | egg/core/wait/damage/rebound actions and statue checks | boss/miniboss special | Core/egg state and statue interactions make it a deferred special actor. |
| `d_a_e_gs.cpp` | `E_GS` | wait/appear/disappear alpha behavior | ghost/proximity special | Simple state shape but invisibility/proximity presentation make it a later ghost policy test. |
| `d_a_e_hp.cpp` | `E_HP` | wait/move/retreat/attack/down/dead actions | ghost enemy candidate | Good later policy test once target retention exists, but wolf/pull-out/down states need damage ownership audit. |
| `d_a_e_hz.cpp` | `E_HZ` | hide/attack/away/wind/chance/water-death actions | complex regular/special enemy | Rich action table; defer until first-wave policy and damage ownership are stable. |
| `d_a_e_is.cpp` | `E_IS` | wait/move/attack/trap/poweroff/break/damage actions | regular/proximity candidate | Compact and promising for first-wave consideration if location/name are confirmed. |
| `d_a_e_kg.cpp` | `E_KG` | file header `Young Gohma`; move/attack/small-damage/damage actions; central `pl_check` gates | regular melee candidate | Young Gohma. Compact policy-friendly actor; first pass routes central targeting, LOS, and roof front-roll awareness through the co-op API families. |
| `d_a_e_kr.cpp` | `E_KR` | path/auto/attack/horse/wait/su-wait actions; coach and bomb references | mounted/path special enemy | Vehicle/path/coach coupling makes it a later category. |
| `d_a_e_mb.cpp` | `E_MB` | parent boss lookup and first-demo actions | boss helper | Defer with boss/miniboss work. |
| `d_a_e_md.cpp` | `E_MD` | dummy/real action split, half-break/break/vibration, spear models | destructible/decoy special | More object-like than AI target selection; not part of first enemy policy wave. |
| `d_a_e_ms.cpp` | `E_MS` | skull search, move/attack/carry-object interactions | regular/helper hybrid | Potentially useful later, but skull/carry-object coupling makes it poor for first policy pass. |
| `d_a_e_nz.cpp` | `E_NZ` | normal/attack/stick/damage actions | regular small enemy candidate | Promising compact target after sticky policy exists; stick-to-player behavior needs owner audit. |
| `d_a_e_rb.cpp` | `E_RB` | appear/move/attack/disappear plus child coordination | spawner/child swarm enemy | Parent/child group behavior should wait until target policy can represent shared or inherited targets. |

### Reviewed Batch: Name Identification Pass

This batch resolves the "unclassified" rows from the machine inventory. Sources: genLabel strings, file header comments, achievement signal strings, resource names, and action tables. Confidence level noted per row.

| Actor/File | Profile | Label Evidence | Classification | Targeting Notes |
| --- | --- | --- | --- | --- |
| `d_a_e_fs.cpp` | `E_FS` | file comment "Enemy - Puppet"; appear/attack/move/damage actions | demo/special enemy | Possessed puppet enemy; demo paths present. Defer. |
| `d_a_e_ga.cpp` | `E_GA` | error string `蛾：シンプルモデル登録失敗しました`; model "E_Ga"; ga_fly/bt_fly | ambient/non-targeting | Simple floating moth swarm; no targeting logic needed. |
| `d_a_e_mm.cpp` | `E_MM` | internal HIO field `donketsu_*`; actions: normal/dash/defence/magne_wait/turn; `reflect_chance_time` | regular melee candidate | Shield-reflect enemy; two HIO size variants. Needs identity confirmation before first wave. |
| `d_a_e_oct_bg.cpp` | `E_OctBg` | class `daE_OctBg_c`; actions: born_swim/swim/chase_core/normal_attack | water/special enemy | Large aquatic predator; water policy needed first. |
| `d_a_e_ot.cpp` | `E_OT` | 20 egg spawn positions; animations: born/swim/damage; `dCcD_SE_SOFT_BODY` | swarm/spawner likely | Egg-hatching water creature; likely spawns from a parent actor. |
| `d_a_e_ph.cpp` | `E_PH` | file comment "Peahat Enemy"; appear/wait/fly/hang/damage | regular enemy candidate | Peahat; promising audit candidate after simpler ground enemies. |
| `d_a_e_s1.cpp` | `E_S1` | loads "E_S2" resource; `HAIR_STRAND_COUNT`/`HAIR_SEGMENT_COUNT` physics web; ACT_ROOF/ACT_WOLFBITE/ACT_SHOUT | regular enemy candidate | Ceiling-hanging spider with physics silk strands; Skulltula variant distinct from E_ST (`スタルチュラ`). Hang mechanics and wolfbite need careful targeting audit. |
| `d_a_e_sb.cpp` | `E_SB` | genLabel `シェルブレイド`; English comment "Shell Blade"; jump/attack/bomb-search/shield | regular melee candidate | Shell Blade; bomb-search behavior needs owner audit before policy targeting. |
| `d_a_e_sg.cpp` | `E_SG` | includes `d_a_mg_rod.h`; ACT_ESA_SEARCH (bait search); ACT_KAMU (bite); 19-joint segmented body | water/special enemy | Fish-type bait searcher; fishing-rod coupling makes it a later water-policy target. |
| `d_a_e_sh.cpp` | `E_SH` | genLabel `スタルハウンド`; English comment "Stalhound"; appear/move/stop/damage/dead | regular melee candidate | Stalhound (skeletal dog); compact state shape, promising for regular-wave audit. |
| `d_a_e_sm.cpp` | `E_SM` | HIO comment `スライム - Slime`; actions: normal/move/damage/attack/core_control | regular proximity candidate | Slime; contact/proximity attacker. |
| `d_a_e_sm2.cpp` | `E_SM2` | merge/split/roof actions; shared resource structure with E_SM | regular proximity candidate | Slime merge variant; audit with E_SM. |
| `d_a_e_th.cpp` | `E_TH` | achievement signal `dark_hammer_one_hit`; includes `d_a_e_th_ball.h`; spiked chain ball tracking | special/miniboss-class enemy | Darkhammer (Palace of Twilight); chain-ball weapon is a separate linked actor. Defer. |
| `d_a_e_tk.cpp` | `E_TK` | genLabel `タドポール`; wait/find/attack/damage/swim/hide | regular enemy candidate | Tadpole; water-area regular enemy. |
| `d_a_e_tk2.cpp` | `E_TK2` | genLabel `タドポール` (larger variant); same action shape as E_TK | regular enemy candidate | Tadpole large variant; audit with E_TK. |
| `d_a_e_vt.cpp` | `E_VT` | file comment "Variant Enemy (Death Sword)"; daE_VA_HIO_c; cloth/arm/finger/sword joints | miniboss/special enemy | Death Sword (Arbiter's Grounds mini-boss); many combat and demo states. Defer. |
| `d_a_e_wb.cpp` | `E_WB` | genLabel `イノシシ`; English comment "Wild Boar"; `LEADER_B_IKKI` / `LEADER_B_IKKI2` / `LEADER_B_LV9` defines; player-mount paths | mounted/boss/setpiece | King Bulblin's boar; multiple cavalry-battle setpiece states and player-mount logic. Defer. |
| `d_a_e_ws.cpp` | `E_WS` | genLabel `スタルウォーーーーーーーール`; wait/attack/move/climb | proximity/special enemy | Stalhound Wall (wall-crawling skeletal variant); very low lookup count. |
| `d_a_e_ww.cpp` | `E_WW` | genLabel `ホワイトウルフォス`; wait/attack/chase/walk/run/damage | regular melee candidate | White Wolfos; chase/attack shape similar to E_OC. Good second-wave candidate. |
| `d_a_e_yc.cpp` | `E_YC` | file comment "Twilit Carrier Kargarok"; fly/rider-carry/wolfbite/damage | Twilight flying enemy | Twilit Carrier Kargarok; rider-carry paths complicate targeting. Defer. |
| `d_a_e_yd.cpp` | `E_YD` | spawns `e_yd_leaf_class` sub-actor; `Z2SE_DARK_VANISH` on death; appears/moves/vanishes | Twilight enemy | Twilight plant/vine enemy with leaf sub-actor. Defer with Twilight category. |
| `d_a_e_yg.cpp` | `E_YG` | genLabel `グース`; normal/attack/swim/dokuro/damage/wolfbite | Twilight enemy | Goose; water/Twilight creature. |
| `d_a_e_yk.cpp` | `E_YK` | file header "Shadow Keese"; genLabel `闇キース`; wind/cruise/charge/damage/disappear | Twilight flying enemy | Shadow Keese; Twilight bat; vertical/flying policy needed. |
| `d_a_e_yr.cpp` | `E_YR` | genLabel `闇カーゴロック`; English comment "Dark Kargarok" at line 2548; wait/hover/attack/fly/damage | Twilight flying enemy | Dark Kargarok; audit with E_YC family. |
| `d_a_e_zh.cpp` | `E_ZH` | no HIO label; searches for `daObjCarry_c` lightball objects; `BCK_ZH_CATCH*/FLY_DELETE`; entrance model `BMDV_ZH_ENTRANCE` | boss-encounter special | Likely Palace of Twilight Zant's Hands (catches Sols/light orbs). Defer. |
| `d_a_e_zm.cpp` | `E_ZM` | HIO comment `ザントの首 Zant's Head`; tongue animations; search/move/attack/bullet | boss/story special | Zant's Head (from Zant boss encounter). Defer. |
| `d_a_e_zs.cpp` | `E_ZS` | includes `d_a_b_ds.h`; `daB_DS_c` boss lookup; killed by `AT_TYPE_SPINNER` | boss helper | Stallord encounter helper; Spinner-weapon dependent. Defer with Stallord boss work. |

## Callsite Classification Pass

This pass reads context around every player-singleton callsite in priority regular-enemy candidates to separate targeting, selected-target state, true primary/global state, damage ownership, and caught/grab ownership. The goal is not bespoke AI per enemy. The goal is to classify each callsite well enough that repeated enemy shapes can use one policy API without blindly redirecting story, damage-owner, or protagonist-specific state.

The central routing guide for these categories is `docs/coop-player-singleton-api-map.md`.

Boss files received a lighter pass. Treat boss files as protagonist-locked/deferred until a dedicated boss co-op audit proves otherwise.

### Classification criteria

**Targeting** - redirect to the co-op-selected target:
- Distance check that decides whether to search, chase, or start an attack against a player
- Angle check used to rotate toward or face a player
- Player pointer used to move toward, attack, or aim a projectile at a player

**Selected-target state** - should eventually read from the selected target, but requires explicit API support before conversion:
- Form checks that affect an enemy's reaction to the target, such as `checkNowWolf()`
- Target movement/combat state that affects pursuit or attack choice, such as speed, guarding, swimming, or damage wait
- Target pose/direction checks used as part of an attack decision

Do not leave these permanently primary-player-only just because V1 is cautious. Also do not redirect them blindly. They should move through `enemy_targeting` or a small target-state helper once the owning target is known.

**Primary/global state** - must remain unchanged unless a dedicated milestone says otherwise:
- Story-protagonist checks such as Midna presence, cutscene/demo participation, and scripted player placement
- P1-only HUD/camera/story state
- Equipment/combat checks that intentionally ask "what is the protagonist doing?" rather than "what is my selected target doing?"
- Demo/cutscene paths that lock to the story protagonist by design
- LOD/culling distance checks that use primary-player distance as an optimization proxy

**Damage-owner** - neither targeting nor primary/global state; belongs to a separate co-op damage-ownership pass:
- `dComIfGp_getPlayer(0)` fetched only to cast to `daPy_py_c*` inside a hit-reaction function, where the real question is "which player struck me?" not "which player should I target next?"
- Use `dusk::coop::damage_owner` for cut type/count, hit direction, weapon-owner, and hit-reaction ownership. Do not substitute nearest-player or current enemy target for attacker identity.

**Defender-owner** - enemy attack contact reads where the enemy is the attacker and the player is the defender:

- Guard/block checks after an enemy attack collider hits a player
- Reads such as "which player blocked this swing?" or "which player should receive guard reaction?"
- Use `dusk::coop::defender_owner`. Do not substitute `damage_owner`, nearest-player, current target, or P1 globals for defender identity.

**Caught/grab-owner** - neither targeting nor nearest-player policy; belongs to a separate caught-state ownership pass:
- Enemy is carrying, eating, restraining, hanging, or otherwise tracking a specific captured player
- Redirecting to nearest player is wrong after the grab begins; the actor must retain the captured owner until release

**Collision-owner** - no explicit search callsite; belongs to collision/world-interaction ownership:
- The enemy detects players through collision categories or contact callbacks rather than `fopAcM_searchPlayer*`
- These actors may not need `selectEnemyTarget()` at all, but they may still need owner-aware hit/contact handling

### Per-file findings

| File | Profile | Name | Targeting | Non-target state | Ambiguous | First-wave verdict |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `d_a_e_tt.cpp` | `E_TT` | Tektite | 8 | 5 | 1 | **Safe** — chase/attack calls isolated in `executeChase`/`executeAttack`; state calls are cut-type and culling distance |
| `d_a_e_kg.cpp` | `E_KG` | Young Gohma | 3 | 2 | 0 | **Safe** — all distance/angle in `action()` helper; roof front-roll read is selected-target state; damage-check pointer fetch is a stub |
| `d_a_e_bs.cpp` | `E_BS` | Baby Stal | 5 | 3 | 0 | **Safe** — distance/angle clearly targeting; player cast in damage path is type-context only |
| `d_a_e_sh.cpp` | `E_SH` | Stalhound | 4 | 3 | 0 | **Safe** — targeting in move/attack; local P1 pointer reads at move/attack entry are target-position candidates, not story/demo state |
| `d_a_e_ai.cpp` | `E_AI` | Amos | 4 | 2 | 0 | **Safe** — angle/distance in `executeSearch`; directional check in `player_way_check` is primary-state |
| `d_a_e_fz.cpp` | `E_FZ` | Freezard | 5 | 3 | 0 | **Safe** — bounce/rotation targeting clear; `otherBgCheck` is line-of-sight to the target and should follow the selected target when this actor is converted |
| `d_a_e_dk.cpp` | `E_DK` | (unknown) | 2 | 3 | 0 | **Safe** — distance gates in `checkPlayerAttack`; home-distance and `otherBgCheck` are state |
| `d_a_e_sm.cpp` | `E_SM` | Slime | 3 | 2 | 0 | **Safe** — distance/angle in `initAction`; pointer fetch is a helper |
| `d_a_e_bi.cpp` | `E_BI` | (unknown) | 2 | 3 | 0 | **Safe** — minimal distance search; relies mostly on collision detection |
| `d_a_e_mm.cpp` | `E_MM` | Donketsu | 2 | 2 | 0 | **Safe** — very low singleton use; minimal co-op targeting impact |
| `d_a_e_cr.cpp` | `E_CR` | Crazy Runner | 2 | 3 | 0 | **Safe** — no `fopAcM_searchPlayer*` pattern found; low co-op impact |
| `d_a_e_nz.cpp` | `E_NZ` | (unknown) | 0 | 2 | 0 | **Safe** — no targeting search callsites; possibly collision-driven |
| `d_a_e_is.cpp` | `E_IS` | (unknown) | 0 | 4 | 0 | **Safe** — no targeting search; state pointer fetches only |
| `d_a_e_dd.cpp` | `E_DD` | (fire enemy) | 2 | 4 | 0 | **Safe** — two targeting calls; damage branch has cut-type state |
| `d_a_e_ba.cpp` | `E_BA` | (unknown) | 2 | 5 | 0 | **Conditional** — distance/angle clean, but `checkSwimUp()` guards a targeting branch and should wait for selected-target state support |
| `d_a_e_ww.cpp` | `E_WW` | White Wolfos | ~30 | ~15 | 0 | **Second-wave** — 45 total callsites; wolf-form checks and demo logic mixed in; needs per-callsite read before any redirect |
| `d_a_e_sf.cpp` | `E_SF` | (humanoid) | ~8 | ~12 | 0 | **Defer** — story intro calls `changeOriginalDemo()`/`setPlayerPosAndAngle()` are protagonist-locked |
| `d_a_e_kk.cpp` | `E_KK` | Ice Swordsman | ~20 | ~16 | 0 | **Second-wave** — most calls targeting but `getDamageWaitTimer()` state checks intermixed |
| `d_a_e_gi.cpp` | `E_GI` | Gibdo | ~10 | ~12 | 0 | **Current proof** — combat targeting, ordinary damage-owner reads, and scream stun ownership converted; wolf-bite ownership remains deferred caught/grab work |
| `d_a_e_hz.cpp` | `E_HZ` | (hazard enemy) | ~12 | ~7 | 0 | **Defer** — boots/armor/throw-damage checks are primary-player-specific equipment state |
| `d_a_e_st.cpp` | `E_ST` | Skulltula | ~15 | ~36 | 0 | **Protagonist-locked** — `getStCaught()` grab state is protagonist-specific; no callsites are safe to redirect without a co-op caught-state ownership model |

### Boss files — protagonist lock status

All `d_a_b_*` boss files are treated as protagonist-locked/deferred for now. A targeted read of `d_a_b_yo.cpp` and `d_a_b_zant.cpp` confirmed that boss-shaped player lookups are embedded in story-fight state machines and should wait for a dedicated boss co-op audit:

- **d_a_b_yo.cpp** — mixed: angle/distance attack-pattern calls could technically be re-targeted, but position-capture logic is tightly coupled to P1 throughout. Protagonist-locked until boss co-op work begins.
- **d_a_b_zant.cpp** — many targeting-shaped calls (~18) embedded in story-fight state machines. Protagonist-locked/deferred until boss co-op work begins.

Remaining boss files (`d_a_b_bh`, `d_a_b_bq`, `d_a_b_dr`, `d_a_b_ds`, `d_a_b_gg`, `d_a_b_gm`, `d_a_b_go`, `d_a_b_oh`, `d_a_b_oh2`, `d_a_b_tn`) were not read in this pass. Mark all as protagonist-locked until a dedicated boss co-op audit.

### Key findings from this pass

**Wolf-form and similar checks are selected-target state, not permanently primary-player state.** E_WW, E_GI, and likely others check `checkNowWolf()` to alter enemy reactions. V1 should avoid redirecting those calls blindly, but the long-term target is not "always ask P1". Once `enemy_targeting` has selected a target, these branches should use a target-state helper that asks the selected player's form/state when the branch is truly about that enemy's target.

**Grab/caught state needs ownership, not nearest-player targeting.** E_ST's `getStCaught()` checks are the clearest example: an enemy actively carrying a player must track that specific player, not whichever player happens to be nearest. Avoid grab-heavy files in the first policy wave unless state boundaries are proven clear. Later work should add a caught/grab owner model and convert only the grab paths that can retain a concrete captured player.

**Story intros and boss/setpiece scenes stay protagonist/global until a dedicated milestone.** E_SF calls `changeOriginalDemo()` and `setPlayerPosAndAngle()` during a scripted encounter intro. Those paths cannot be redirected by `enemy_targeting`. The same pattern likely appears in E_FS, E_PM, and any enemy with `op_demo`, `demo_wait`, or `changeDemoMode()` action states. Boss files are deferred/protagonist-locked for now, not because they can never support co-op targeting, but because boss cameras, phase scripts, and arena state need their own audit.

**`dComIfGp_getPlayer(0)` as a type-only cast is damage-owner work.** Several damage-check functions fetch the player pointer only to cast it to `daPy_py_c*` for a hit-reaction call, not to locate a target. These look like `daPy_py_c* pPy = (daPy_py_c*)dComIfGp_getPlayer(0); someHitCall(pPy->someField)`. If the hit reaction is owner-agnostic, the correct actor is the attacker, not P0. Flag these as **damage-owner callsites** rather than targeting or primary/global state.

**E_IS, E_NZ, and E_CR have no obvious `selectEnemyTarget()` surface.** These enemies likely detect players via collision rather than explicit distance/angle search. That makes them poor proof targets for `enemy_targeting`, not necessarily easy enemies overall. Handle them in a later collision/world-interaction ownership pass.

**The desired conversion shape is repeated, not bespoke.** For regular enemies, the target should be owned once per actor behavior scope, then read through search/chase/attack/follow-through callsites that used to re-query P1. Per-file edits are still needed to hook each state machine, but the policy, retention, diagnostics, and target-state accessors should live in Dusk-owned reusable modules.

## Initial Actor Map

The table below is machine-assisted from `src/d/actor/d_a_e_*.cpp` and profile symbols. The lookup count is the number of direct matches for `fopAcM_searchPlayerDistance*`, `fopAcM_searchPlayerAngleY`, `dComIfGp_getPlayer(0)`, and `daPy_getPlayerActorClass()`. It is a triage signal, not a complexity score.

| Priority | Actor/File | Profile | Lookup Count | Initial Classification | Notes |
| --- | --- | --- | ---: | --- | --- |
| Done proof | `d_a_e_oc.cpp` | `E_OC` | 48 | regular melee | First regular-enemy proof; targeting, damage-owner, selected-target-state, and defender-owner paths are validated. |
| Done proof | `d_a_e_hm.cpp` | `E_HM` | low | proximity enemy | One wake trigger converted; full combat not audited. |
| High risk | `d_a_e_wb.cpp` | `E_WB` | 75 | mounted/boss/setpiece likely | Very high singleton density; defer until ordinary enemies are stable. |
| High risk | `d_a_e_po.cpp` | `E_PO` | 63 | special/ghost-like | High singleton density; classify before patching. |
| High risk | `d_a_e_pz.cpp` | `E_PZ` | 60 | special enemy | High singleton density; classify before patching. |
| High risk | `d_a_e_rd.cpp` | `E_RD` | 56 | ranged/mounted/complex humanoid | Many weapon, boar, horn, and demo paths; good later family, not next. |
| High risk | `d_a_e_vt.cpp` | `E_VT` | 50 | miniboss/special enemy | Confirmed: Death Sword (Arbiter's Grounds mini-boss). Many combat states and demo/camera calls; defer. |
| Defer | `d_a_e_st.cpp` | `E_ST` | 48 | protagonist-locked — grab/catch | HIO label `スタルチュラ`; `getStCaught()` grab-state is protagonist-specific; cannot redirect targeting callsites without a co-op caught-state ownership model. |
| Current proof | `d_a_e_ww.cpp` | `E_WW` | 45 | regular melee candidate | Confirmed: ホワイトウルフォス (White Wolfos). Combat chase/attack/walk/move-out paths route through one Combat owner and selected-target state; master spawn staging can wake on P2, uses target-slot presentation angle, and hookshot side-step awareness scans active players. |
| Candidate | `d_a_e_ymb.cpp` | `E_YMB` | 42 | boss/miniboss likely | High density and many camera/player-state calls; defer. |
| Candidate | `d_a_e_fm.cpp` | `E_FM` | 41 | special/grab/chain likely | Has demo/grab-heavy paths; defer until targeting policy exists. |
| Candidate | `d_a_e_rdy.cpp` | `E_RDY` | 36 | related humanoid | Likely related to `E_RD`; audit with that family. |
| Candidate | `d_a_e_kk.cpp` | `E_KK` | 36 | regular humanoid/ranged candidate | HIO label `氷の剣士` (Ice Swordsman); spear throw and guard; good later stress test. |
| Candidate | `d_a_e_ym.cpp` | `E_YM` | 31 | wolf/Midna-sensitive likely | Many wolf-specific checks; defer until damage/guard policy improves. |
| Candidate | `d_a_e_yh.cpp` | `E_YH` | 28 | grab/catch likely | Has caught-state style calls; likely needs player-state ownership audit. |
| Candidate | `d_a_e_db.cpp` | `E_DB` | 28 | grab/catch likely | Has caught-state style calls; likely needs player-state ownership audit. |
| Candidate | `d_a_e_sw.cpp` | `E_SW` | 27 | unknown regular/special | Needs name/location identification. |
| Candidate | `d_a_e_mf.cpp` | `E_MF` | 27 | unknown regular/special | Needs name/location identification. |
| Candidate | `d_a_e_gob.cpp` | `E_GOB` | 25 | special/NPC-like combat actor | HIO label `マグネゴロン`; message/grab/special actions, not first wave. |
| Candidate | `d_a_e_dn.cpp` | `E_DN` | 25 | complex enemy | Guard/hookshot/wolf/hit reactions appear in code; defer until policy and damage checks exist. |
| Candidate | `d_a_e_s1.cpp` | `E_S1` | 24 | regular enemy candidate | Ceiling-hanging spider with physics web; loads "E_S2" resource; Skulltula variant. Hang/wolfbite paths need audit before policy targeting. |
| Candidate | `d_a_e_dt.cpp` | `E_DT` | 24 | boss/setpiece likely | Many press/demo/special-position calls; defer. |
| Current proof | `d_a_e_gi.cpp` | `E_GI` | 22 | regular humanoid/undead candidate | Gibdo. Combat targeting, ordinary damage-owner reads, and scream stun ownership are policy-backed; wolf-bite ownership is deferred. |
| Done proof | `d_a_e_tt.cpp` | `E_TT` | 21 | regular enemy | HIO label `テクタイト`; compact chase/attack state; first non-Bokoblin port validated for targeting, damage-owner, and selected-target-state surfaces. |
| Candidate | `d_a_e_th.cpp` | `E_TH` | 20 | special/miniboss-class enemy | Confirmed: Darkhammer; `dark_hammer_one_hit` achievement signal; chain-ball weapon. Defer. |
| Candidate | `d_a_e_sf.cpp` | `E_SF` | 20 | regular humanoid with demo intro | Guard/sitwait/op-demo paths need care. |

## Reusable Conversion Groups

Use these groups to minimize manual per-enemy work. Each group should map to reusable `enemy_targeting` policy or helper APIs, while actor files supply only the local state-machine hook points.

| Group | Pattern | Reusable API Direction | Good Candidates | Deferred Hazards |
| --- | --- | --- | --- | --- |
| Ground search/chase/attack | Enemy wakes, turns, chases, and gates an attack by player distance/angle | Actor-local helper over `EnemyTargetScope::Combat`; callsite labels are diagnostics only | `E_OC`, `E_TT`, `E_KG`, `E_BS`, `E_SH`, `E_AI` | Demo intros, guard/damage-owner paths |
| Proximity/contact | Enemy reacts mostly through collision or a small wake radius | Collision-owner pass plus small query helpers where explicit search exists | `E_HM`, `E_BI`, `E_SM`, `E_SM2` | Hookshot/carry interactions, contact owner attribution |
| Vertical/flying/ranged | Enemy needs height, line-of-sight, projectile aim, or flight behavior | Later policy profile with vertical scoring and target-state helpers | `E_BU`, `E_GE`, `E_PH`, `E_YK`, `E_YR`, `E_FB` | Camera/story flyers, rider-carry paths |
| Target-state-sensitive | Enemy decision depends on target form/speed/guard/swim/damage state | Use `dusk::coop::selected_target_state` after the target identity is known | `E_WW`, `E_GI`, `E_KK`, `E_BA` | Accidentally reading P1 state for P2, or replacing protagonist-only state |
| Enemy-attack defender contact | Enemy attack collider hits a player, then code checks guard/block/defender state | Use `dusk::coop::defender_owner`; direct-player V1 proof surface is Bokoblin guard collision | `E_OC`, later humanoid melee enemies | Confusing defender identity with damage-owner or current target |
| Damage-owner | Enemy reaction depends on who hit it | Separate damage ownership API, later aggro/threat bias | Many humanoids and item-reactive enemies | Treating attacker identity as nearest target |
| Caught/grab-owner | Enemy captures or carries a specific player | Separate caught/grab ownership model | `E_ST`, `E_DF`, `E_SW`, grab-heavy files | Nearest-player retarget during a grab |
| Boss/setpiece/demo | Encounter state owns camera, script, phase, or protagonist placement | Dedicated boss co-op audit | `d_a_b_*`, `E_SF`, `E_FS`, `E_PM`, `E_VT` | Breaking story/camera/phase assumptions |

## First Policy-Backed Wave

The audit was sufficient to begin the `enemy_targeting` V1 module without broad enemy conversion.
Bokoblin now has the reusable policy spine plus damage-owner, selected-target-state, and
defender-owner proof surfaces. Tektite has been ported and validated as the first compact
non-Bokoblin specimen.

After Bokoblin, Tektite, Stalhound, Stalchild, Gibdo, Young Gohma, and the current White Wolfos first pass, the next choices are:

1. **Finish validating White Wolfos (`E_WW`)** - confirm P2 chase/attack, selected target speed/form behavior, and guard contact before broadening the pattern.
2. **Next target-state-sensitive enemy (`E_KK` or `E_BA`)** - now reasonable because `selected_target_state` exists, but classify form/guard/damage reads before patching.
3. **A future caught/grab-owner proof enemy** - needed for wolf-bite hang ownership and similar retained physical interactions; Gibdo's wolf-bite path is explicitly deferred.
4. **Avoid grab-heavy or setpiece enemies** until caught/grab-owner and event/camera policies exist.

This sequence keeps the manual work small: build one policy API, convert one already validated actor, then port the same shape to one compact enemy before touching target-state-sensitive families.

## Audit Queue

Use this queue before writing more enemy behavior code:

1. ~~Finish label/resource extraction for unclassified actors.~~ Done — name identification pass complete. All previously blank rows in the machine inventory now have label evidence and a classification. A few remain uncertain (E_MM identity, E_SG exact species, E_ZH confirmation) but are classified well enough to defer safely.
2. Split the full inventory into buckets:
   - regular ground melee,
   - regular flying/ranged,
   - proximity/hazard,
   - helper/projectile/spawner,
   - plant/static special,
   - mounted/vehicle,
   - grab/caught-state,
   - miniboss/boss/story/demo.
3. ~~For each likely regular enemy, inspect only enough code to mark search/chase/attack/follow-through/damage risk. Do not patch during this pass.~~ Priority 1 regular-enemy callsite classification is complete enough for the first policy wave. Continue classification opportunistically for candidates outside that wave.
4. ~~Pick the first policy-backed wave from the best understood regular enemies, not necessarily from the highest lookup counts.~~ First wave chosen and validated: policy-backed `E_OC`, then `E_TT`. `E_SH` and `E_BS` passed first surface testing; `E_GI` is the current target-state-sensitive proof.
5. Keep target choice in `enemy_targeting`, selected target facts in `selected_target_state`, hit ownership in `damage_owner`, enemy-attack contact in `defender_owner`, and diagnostics quiet while selecting the next actor.

## Full Machine Inventory

This inventory is generated from `src/d/actor/d_a_e_*.cpp` file names and `g_profile_*` symbols. Lookup count uses the same four player-singleton patterns as the triage table. Classification is intentionally blank until reviewed against code/resource evidence.

| File | Profile | Lookup Count | Classification | Notes |
| --- | --- | ---: | --- | --- |
| `d_a_e_ai.cpp` | `E_AI` | 6 | regular enemy candidate | HIO label `アモス`; compact wait/move/attack/damage table |
| `d_a_e_arrow.cpp` | `E_ARROW` | 3 | helper/projectile likely |  |
| `d_a_e_ba.cpp` | `E_BA` | 10 | regular enemy candidate | attack and wolf-bite actions |
| `d_a_e_bee.cpp` | `E_BEE` | 5 | helper/swarm enemy | tied to `E_NEST`; not standalone targeting |
| `d_a_e_bg.cpp` | `E_BG` | 16 | water/special enemy | fishing rod bait/hook/eat paths |
| `d_a_e_bi.cpp` | `E_BI` | 5 | regular/proximity candidate | compact wait/up/move/water/disappear action table |
| `d_a_e_bi_leaf.cpp` | `E_BI_LEAF` | 0 | helper/projectile likely |  |
| `d_a_e_bs.cpp` | `E_BS` | 8 | regular melee candidate | HIO label `ベビースタル` |
| `d_a_e_bu.cpp` | `E_BU` | 10 | flying enemy candidate | HIO label `バブル`; needs vertical targeting consideration |
| `d_a_e_bug.cpp` | `E_BUG` | 15 | swarm/special enemy | group insect/simple-model behavior; boomerang/bomb searches seen |
| `d_a_e_cr.cpp` | `E_CR` | 6 | simple movement enemy candidate | HIO label `クレイジーランナー` |
| `d_a_e_cr_egg.cpp` | `E_CR_EGG` | 0 | helper/projectile likely |  |
| `d_a_e_db.cpp` | `E_DB` | 32 | special/grab likely | caught-state style calls seen in scan |
| `d_a_e_db_leaf.cpp` | `E_DB_LEAF` | 0 | helper/projectile likely |  |
| `d_a_e_dd.cpp` | `E_DD` | 6 | complex regular fire enemy | flame plus bomb/tail/arrow damage branches |
| `d_a_e_df.cpp` | `E_DF` | 4 | plant/grab special | LinkEat/BombEat/ObjEat actions |
| `d_a_e_dk.cpp` | `E_DK` | 8 | regular/complex enemy candidate | wait/chase/attack/damage/death; body/core split |
| `d_a_e_dn.cpp` | `E_DN` | 26 | complex enemy likely | guard/hookshot/wolf/hit-reaction calls seen in scan |
| `d_a_e_dt.cpp` | `E_DT` | 24 | boss/setpiece likely | press/demo/special-position calls seen in scan |
| `d_a_e_fb.cpp` | `E_FB` | 21 | static/ranged special enemy | vertical/bullet checks |
| `d_a_e_fk.cpp` | `E_FK` | 9 | mounted/special enemy | HIO label `ファントム騎馬兵`; defer |
| `d_a_e_fm.cpp` | `E_FM` | 42 | special/grab likely | demo/grab-heavy calls seen in scan |
| `d_a_e_fs.cpp` | `E_FS` | 11 | demo/special enemy | file comment "Enemy - Puppet"; appear/attack/move/damage; demo/possessed enemy, defer |
| `d_a_e_fz.cpp` | `E_FZ` | 15 | regular enemy candidate | wait/move/attack/damage actions |
| `d_a_e_ga.cpp` | `E_GA` | 0 | ambient/non-targeting | 蛾 (Moth); simple floating swarm with no player targeting; ambient/environmental only |
| `d_a_e_gb.cpp` | `E_GB` | 17 | plant/special enemy | HIO label `デカババ`; bomb/key/demo behavior |
| `d_a_e_ge.cpp` | `E_GE` | 9 | flying enemy candidate | circle-flight/attack behavior |
| `d_a_e_gi.cpp` | `E_GI` | 28 | regular humanoid/undead candidate | sleep/wait and sword model evidence |
| `d_a_e_gm.cpp` | `E_GM` | 12 | boss/miniboss special | egg/core/rebound/statue checks |
| `d_a_e_gob.cpp` | `E_GOB` | 26 | special/NPC-like combat actor | HIO label `マグネゴロン`; message/grab/special actions |
| `d_a_e_gs.cpp` | `E_GS` | 2 | ghost/proximity special | appear/disappear alpha behavior |
| `d_a_e_hb.cpp` | `E_HB` | 17 | plant enemy candidate | HIO label `デグババ（ボックリ）` |
| `d_a_e_hb_leaf.cpp` | `E_HB_LEAF` | 0 | helper/projectile likely |  |
| `d_a_e_hm.cpp` | `E_HM` | 6 | proximity enemy | `e_hm.up_wait` proof converted |
| `d_a_e_hp.cpp` | `E_HP` | 12 | ghost enemy candidate | wolf/down states need care |
| `d_a_e_hz.cpp` | `E_HZ` | 19 | complex regular/special enemy | hide/attack/away/wind/chance states |
| `d_a_e_hzelda.cpp` | `E_HZELDA` | 20 | boss/story likely |  |
| `d_a_e_is.cpp` | `E_IS` | 4 | regular/proximity candidate | compact wait/move/attack/trap/poweroff/break states |
| `d_a_e_kg.cpp` | `E_KG` | 5 | regular melee candidate | move/attack/damage around `pl_check` |
| `d_a_e_kk.cpp` | `E_KK` | 36 | regular humanoid/ranged candidate | HIO label `氷の剣士`; spear throw and guard |
| `d_a_e_kr.cpp` | `E_KR` | 15 | mounted/path special enemy | horse/coach/path/bomb coupling |
| `d_a_e_mb.cpp` | `E_MB` | 1 | boss helper | boss monkey helper path |
| `d_a_e_md.cpp` | `E_MD` | 3 | destructible/decoy special | dummy/real half-break/break behavior |
| `d_a_e_mf.cpp` | `E_MF` | 28 | special/grab likely | previously scanned as demo/grab-heavy |
| `d_a_e_mk.cpp` | `E_MK` | 17 | miniboss/story special | boomerang monkey, demo-camera heavy |
| `d_a_e_mk_bo.cpp` | `E_MK_BO` | 10 | helper/projectile likely |  |
| `d_a_e_mm.cpp` | `E_MM` | 5 | regular melee candidate | internal codename `donketsu`; normal/dash/defence/magne_wait/turn/damage; shield-reflect mechanic; two size variants via HIO |
| `d_a_e_mm_mt.cpp` | `E_MM_MT` | 15 | helper/mounted variant | sub-actor or mounted variant for `E_MM`; audit with E_MM family |
| `d_a_e_ms.cpp` | `E_MS` | 7 | regular/helper hybrid | skull/carry-object interaction paths |
| `d_a_e_nest.cpp` | `E_NEST` | 11 | spawner/nest likely |  |
| `d_a_e_nz.cpp` | `E_NZ` | 4 | regular small enemy candidate | stick behavior needs owner audit |
| `d_a_e_oc.cpp` | `E_OC` | 49 | regular melee | raw-query proof validated; policy-backed targeting V1 implemented for existing proof systems |
| `d_a_e_oct_bg.cpp` | `E_OctBg` | 8 | water/special enemy | large aquatic predator (Oct = Octorok-like, Bg = Big); born_swim/swim/chase_core/normal_attack; water enemy, defer until water-targeting policy exists |
| `d_a_e_ot.cpp` | `E_OT` | 8 | swarm/spawner likely | water egg-hatcher; born/swim/damage animations; 20 egg spawn positions; soft-body collision; likely spawns from a parent |
| `d_a_e_ph.cpp` | `E_PH` | 14 | regular enemy candidate | Peahat; file comment "Peahat Enemy"; appear/wait/fly/hang/damage; good audit candidate after ground-enemy wave |
| `d_a_e_pm.cpp` | `E_PM` | 18 | demo/special enemy | demo action and player demo calls |
| `d_a_e_po.cpp` | `E_PO` | 63 | special/ghost-like likely | high singleton density |
| `d_a_e_pz.cpp` | `E_PZ` | 60 | special enemy likely | high singleton density |
| `d_a_e_rb.cpp` | `E_RB` | 9 | spawner/child swarm enemy | child coordination and parent tracking |
| `d_a_e_rd.cpp` | `E_RD` | 57 | ranged/mounted/complex humanoid | weapon, boar, horn, and demo paths seen in scan |
| `d_a_e_rdb.cpp` | `E_RDB` | 18 | related humanoid/mounted likely | audit with `E_RD` family |
| `d_a_e_rdy.cpp` | `E_RDY` | 36 | related humanoid likely | audit with `E_RD` family |
| `d_a_e_s1.cpp` | `E_S1` | 25 | regular enemy candidate | ceiling-hanging spider with physics-simulated web strands (HAIR_STRAND/HAIR_SEGMENT counts); loads "E_S2" resource; hang/search/fight/wolfbite/shout; Skulltula variant distinct from E_ST |
| `d_a_e_sb.cpp` | `E_SB` | 10 | regular melee candidate | シェルブレイド (Shell Blade); jump/attack/bomb-search/shield; good audit candidate |
| `d_a_e_sf.cpp` | `E_SF` | 20 | regular humanoid with demo intro | op-demo/guard/sitwait paths |
| `d_a_e_sg.cpp` | `E_SG` | 8 | water/special enemy | fishing-bait searcher (`d_a_mg_rod.h`, ACT_ESA_SEARCH); bite (ACT_KAMU); 19-joint segmented body; water-area enemy, defer until water policy exists |
| `d_a_e_sh.cpp` | `E_SH` | 7 | regular melee candidate | スタルハウンド (Stalhound); appear/move/stop/damage/dead; skeletal dog; good audit candidate |
| `d_a_e_sm.cpp` | `E_SM` | 13 | regular proximity candidate | スライム (Slime); normal/move/damage/attack/core_control; contact/proximity enemy |
| `d_a_e_sm2.cpp` | `E_SM2` | 10 | regular proximity candidate | Slime variant; merge/split/roof actions; audit with E_SM |
| `d_a_e_st.cpp` | `E_ST` | 51 | multi-form regular enemy | HIO label `スタルチュラ`; many search/shoot/jump/hang phases |
| `d_a_e_st_line.cpp` | `E_ST_LINE` | 0 | helper/projectile likely |  |
| `d_a_e_sw.cpp` | `E_SW` | 27 | special mobile enemy | hook/bomb/catch/dive actions |
| `d_a_e_th.cpp` | `E_TH` | 20 | special/miniboss-class enemy | Darkhammer; `dark_hammer_one_hit` achievement signal; carries spiked chain ball (E_TH_BALL); Palace of Twilight; defer |
| `d_a_e_th_ball.cpp` | `E_TH_BALL` | 2 | helper/projectile | Darkhammer's chain ball; audit with E_TH |
| `d_a_e_tk.cpp` | `E_TK` | 6 | regular enemy candidate | タドポール (Tadpole); wait/find/attack/damage/swim/hide; water-area regular enemy |
| `d_a_e_tk2.cpp` | `E_TK2` | 4 | regular enemy candidate | タドポール large variant; same action shape as E_TK; audit with E_TK |
| `d_a_e_tk_ball.cpp` | `E_TK_BALL` | 3 | helper/projectile | Tadpole projectile; audit with E_TK |
| `d_a_e_tt.cpp` | `E_TT` | 21 | regular enemy candidate | HIO label `テクタイト`; compact chase/attack state |
| `d_a_e_vt.cpp` | `E_VT` | 50 | miniboss/special enemy | Death Sword; file comment confirmed; daE_VA_HIO_c; cloth/arm/finger joints; mini-boss in Arbiter's Grounds; defer |
| `d_a_e_warpappear.cpp` | `E_WAP` | 11 | helper/effect likely |  |
| `d_a_e_wb.cpp` | `E_WB` | 75 | mounted/boss/setpiece | confirmed: イノシシ (Wild Boar); King Bulblin's mount; LEADER_B_IKKI variants = cavalry battle setpieces; player-mountable; complex boss/setpiece, defer |
| `d_a_e_ws.cpp` | `E_WS` | 2 | proximity/special enemy | スタルウォーーーーーーーール (Stalhound Wall); wall-crawling skeletal enemy; wait/attack/move/climb; low lookup count, low priority |
| `d_a_e_ww.cpp` | `E_WW` | 45 | regular melee candidate | ホワイトウルフォス (White Wolfos); HIO genLabel confirmed; wait/attack/chase/walk/run/damage; good second-wave candidate |
| `d_a_e_yc.cpp` | `E_YC` | 8 | Twilight flying enemy | Twilit Carrier Kargarok; file comment confirmed; fly/rider-carry/wolfbite; rider-carry paths complicate targeting, defer |
| `d_a_e_yd.cpp` | `E_YD` | 15 | Twilight enemy | Twilight plant/vine enemy; spawns E_YD_LEAF sub-actor; Z2SE_DARK_VANISH on death; defer with Twilight category |
| `d_a_e_yd_leaf.cpp` | `E_YD_LEAF` | 0 | helper/projectile | E_YD leaf sub-actor; audit with E_YD |
| `d_a_e_yg.cpp` | `E_YG` | 8 | Twilight enemy | グース (Goose); HIO genLabel confirmed; normal/attack/swim/dokuro/damage/wolfbite; water/Twilight enemy |
| `d_a_e_yh.cpp` | `E_YH` | 29 | special/grab likely | caught-state style calls seen in scan |
| `d_a_e_yk.cpp` | `E_YK` | 10 | Twilight flying enemy | 闇キース (Shadow Keese); file header + genLabel confirmed; wind/cruise/charge/damage/disappear; Twilight bat; flying policy needed |
| `d_a_e_ym.cpp` | `E_YM` | 32 | wolf/Midna-sensitive likely | many wolf-specific checks seen in scan |
| `d_a_e_ym_tag.cpp` | `E_YM_TAG` | 0 | tag/helper likely |  |
| `d_a_e_ymb.cpp` | `E_YMB` | 42 | boss/miniboss likely | high density and camera/player-state calls seen in scan |
| `d_a_e_yr.cpp` | `E_YR` | 12 | Twilight flying enemy | 闇カーゴロック (Dark Kargarok); genLabel + English comment confirmed; wait/hover/attack/fly/damage; audit with E_YC family |
| `d_a_e_zh.cpp` | `E_ZH` | 2 | boss-encounter special | no HIO label; catches `daObjCarry_c` lightball objects (Sol/light orb); catch/fly/entrance mechanics; likely Palace of Twilight Zant's Hands; defer |
| `d_a_e_zm.cpp` | `E_ZM` | 9 | boss/story special | ザントの首 (Zant's Head); genLabel confirmed; flying boss head from Zant encounter; defer |
| `d_a_e_zs.cpp` | `E_ZS` | 7 | boss helper | Stallord encounter helper; references `daB_DS_c` (Boss Stallord); killed by Spinner (`AT_TYPE_SPINNER`); defer with Stallord |

## Next Steps

1. Choose the next regular enemy from an accessible test location, preferably a target-state-sensitive second-wave enemy (`E_WW`, `E_KK`, or `E_BA`) or another compact non-flying ground enemy.
2. Before patching, classify its singleton reads into targeting, selected-target state, damage-owner, defender/collision-owner, caught/grab-owner, primary/global, and render/culling.
3. Convert only the smallest coherent behavior slice, using actor-local helpers over the API families proven by Bokoblin and Tektite.
4. Continue classifying/test-locating target-state-sensitive second-wave enemies (`E_WW`, `E_KK`, `E_BA`) in parallel.

## Multiplayer AI Notes

General multiplayer PvE practice supports the policy direction here: enemies should usually avoid instantaneous target flicker, preserve committed attacks, react to recent damage/threat, and distribute attention when many enemies are present. For this project, treat those as design heuristics until they are backed by local diagnostics and in-game behavior. Host-authoritative online play should make the host own enemy target decisions; clients can render or predict presentation, but should not independently decide enemy truth.
