# Roadmap — 0.0.1-dev

**The goal is a three-player MW2 (2009) campaign on separate PCs, with four players a possible later expansion. Development first establishes the original two-player session and gameplay path.** Native two-player trainer movement has passed automated checks on one PC. The current version is a guarded developer prototype. This roadmap describes required work, not shipped features or a completion date.

The staged approach follows **[cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything)**: exact executable identity, isolated experiments, automated tests, and evidence before compatibility claims. The MIT campaign framework is credited to [stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop/tree/8fcb6bcf9980894a631906059558a4797f4c9752). Its native x64 support and upstream tests do not establish an x86 port or three-player support here.

## Current baseline

The [architecture document](ARCHITECTURE.md) specifies the exact supported x86 build184 hash and active opt-in modules. Earlier single-player trainer experiments exercised guarded control, three-record player storage, cache allocation/reset for three with traversal of two, and a GSC source loader. Those component runs retained one script player with co-op disabled.

That single-player framework receipt covers four own sources and five patched scripts with 19 applied blocks. It does not cover every mission module. The standalone roster/ACK prototype has offline tests but no native adapter.

The newer two-copy path uses the original two-player capacities. Native UDP admission and shared-map advancement passed on stock `so_killspree_trainer` and campaign `trainer`. The campaign run reached the `pit` start with `coop=1`, `specialops=0`, `gameMode=2` and two script players. Over 100 seconds after readiness, both copies reached native world state 6 and advanced both slots' command application by about 19.9 seconds during the final 20-second window. Both exited normally and their executables were unchanged.

Later runs passed **seven independent native movement phases** in campaign trainer through both fixed-loopback QA and exact-peer transport. The exact-peer run used two native ports on an existing nonloopback interface, with separately guarded role-distinct cached IDs. Solo inputs affected only their assigned actor, simultaneous distinct movement passed and compared equal-time world positions matched. Both copies exited normally. Every test was still on **one PC**. Separate-PC connectivity, aim/fire/use and progression remain open. Three-record capacity work is a separate future expansion baseline and stays disabled for these two-player tests.

The owner subsequently reported a trainer session on two physical PCs over Radmin VPN, with teammate labels visible but bodies absent. This is manual connectivity/loading evidence. The immediate gameplay task is to assign and verify real third-person bodies, world weapons and evaluated poses on both observers before extending mission progression. See [the current body investigation](TESTING.md#third-person-bodies--october-24-2026).

As of October 25, the separate [trainer body preview](../experiments/trainer-body/README.md) has passed genuine resource registration, normal model assignment and a controlled idle visual check on two isolated instances on one PC. The root baseline remains separate. The candidate's latest movement run completed all seven phases within the original timing limits, but one stale neutral command at the first client-right boundary keeps its strict score at FAIL. A subsequent controller gate waits for actual native producer/applied axes in both worlds before measurement, inside the same deadline; that gate is offline-tested and game-untested. See [current evidence and boundaries](BODY-PREVIEW.md).

Immediate order: run the changed controller with the unchanged strict movement oracle; verify rendered locomotion and firing on both observers; exercise repeated loading/restart and cleanup; verify the body candidate on two physical PCs; then advance interactions, death/revive, checkpoints and mission transitions. Three/four-player expansion follows these two-player gates. An idle body image or helper result does not close later gameplay gates.

## 1. Preserve the diagnostic baseline

Keep exact executable hashing and live signature checks as prerequisites for every native stage. Continue using isolated game copies and reproducible source builds. Preserve the distinction between allocated capacity, accepted participants, script entities, and local controllers.

Each new mutation needs an understood reference map, verified ABI, bounded allocation, reset behavior, protection restoration, failure rollback, and pointer-lifetime rules. Keep negative tests for wrong builds, missing opt-in files, unexpected phases, corrupted signatures, malformed input, and failed transactions.

**Exit criterion:** the new stage preserves the existing single-player component checks, exits normally when healthy, and refuses or stops cleanly when its verified contract fails. Helper success alone does not satisfy a game runtime gate.

## 2. Establish two-player input and progression first

Keep native capacity and wire framing at two while proving two independent producers in the shared stock map, then campaign trainer. Require fresh matching map/mode, native world state 6, an unpaused simulation, stable entity/channel ownership and advancing command times on both copies before dispatching actions.

Use bounded game-time phases: neutral, host-only movement, neutral, client-only movement, neutral, distinct simultaneous movement, then neutral release. Observe actual server/applied command axes and positions in both worlds at matching game/command times. Keep neutral movement bounded across every captured sample. Mailbox acceptance, advancing timestamps, script movement and player-count probes alone do not establish independence. Every failure path must release inputs, and the overall result must include clean process teardown.

The bounded native movement portion passed on one PC; preserve it as a regression. Next run the exact-peer flow on two separate PCs, then add native aim/fire/use checks, death/revive, checkpoint restart, reconnect, map transitions and mission triggers individually. Cross-machine scoring needs explicit event/time agreement; do not assume a shared Windows tick clock. The developer launcher currently requires manual readiness and does not negotiate matching versions, seed or settings. Document tested missions and failures before describing a supported host/join release.

**Exit criterion:** deliberate native inputs affect only their assigned players on two PCs, both worlds agree on measured state, and repeatable two-player progression scenarios pass. Same-PC trainer movement satisfies one bounded part of this criterion.

## 3. Implement three-member native lifecycle

Build a common native member API that can represent three participants without expanding into the original session object's neighboring platform fields. Connect actual native identities and addresses to stable member slots and connection incarnations. Audit all readers, writers, lookup/count helpers, initialization, reset, removal, and reconnect paths.

Adapt the offline roster policy only after defining the native trust boundary:

- The host owns membership changes, revisions, readiness, and launch commits.
- Clients accept snapshots only from the expected host/session and check freshness before applying them.
- ACKs bind the complete canonical roster snapshot to the known sender and current connection incarnation.
- Reconnect invalidates readiness even when identity, address, and display name are unchanged; retransmission remains idempotent.
- A fourth participant and invalid or stale slot references receive a controlled rejection.

Cache traversal stays at two until the new member API and every affected consumer can safely handle index 2. Three reserved cache records are not permission to call legacy two-member getters on that index.

**Exit criterion:** deterministic lifecycle transcripts cover join order, duplicate join, stale ACK, changed roster, reconnect, removal, slot reuse, session restart, and fourth-peer rejection. Native tests must confirm that unrelated session/platform fields are preserved.

## 4. Complete server and channel capacity together

Turn the passive **257-operand server/channel inventory** into a reviewed implementation plan. Reconcile all interior addresses, pointer-loop ends, clear sizes, and 337 deliberately preserved neighboring aliases. Do not activate the inventory as a blanket patch list.

The required scope also includes fixed-count/unrolled helpers and pointers embedded during initialization. Review server-client records, channel headers, send/receive buffers, participant metadata, count/free-slot selection, reset, and traversal as one coordinated native stage. Keep local-controller/view arrays separate: the target remains one local player per PC.

Use guarded storage and initialize every internal pointer before any engine code can retain it. Test complete reset and failure recovery, including constructor paths and pointers returned to UI or other consumers. Allocation lifetime must cover those consumers.

**Exit criterion:** three distinct accepted native participants each have the correct server/channel/storage records and buffers. No third slot aliases an existing slot, overwrites neighboring state, or enters a legacy two-record accessor. This is still a capacity/admission result, not gameplay synchronization.

## 5. Define a versioned private three-participant protocol

The existing authoritative-frame command path has a one-bit participant selector. A candidate private format uses **two bits** for participant indices 0, 1, and 2, with the remaining value explicitly invalid or reserved. This is a design proposal; both ends of every affected message must agree before it can be used.

Update authoritative-frame writers/readers, incoming command forwarding, host aggregation, all-peer broadcast, heartbeat framing, lengths, and validation together. Resolve the current heartbeat terminator collision with index 2 through explicit versioned framing. Preserve command order and timing semantics; do not infer protocol completeness from a single selector change.

Require version agreement before admission/launch. Reject mixed or stock protocol peers cleanly. Validate signed and unsigned participant indices before indexed access, and bind input to the accepted sender's slot rather than a claimed payload identity. Carry session/roster epochs through launch and runtime state transitions so delayed traffic cannot act on reused slots.

**Exit criterion:** offline codec/transcript tests cover all three valid indices, the reserved value, truncation, malformed lengths/opcodes, stale epochs, mixed versions, reordering, duplication, timeout, reconnect, and fourth-peer attempts. Then exercise the same private protocol with real game processes.

## 6. Expansion gameplay gate: three independent inputs in trainer

Use one host and two real game clients with matching code/content and one local controller each. Begin with a bounded stationary/movement test, before mission progression, save/load, vehicles, or cinematics.

1. Establish three accepted identities and distinct native entity/client records. Require actual script membership of three; do not count reserved records or lobby labels.
2. Set up separate safe positions once. During measurement, disable follow/leash and other script effects that could conceal incorrect input routing.
3. Drive distinct automated **native user-command streams** for each participant, then rotate which participant is active while the others remain neutral. Teleporting entities, applying velocity from GSC, or setting QA button flags is not evidence of independent network input.
4. Verify ownership, command sequence, movement/view/stance effects, and consistent authoritative frame progress in all three processes. Compare normalized state at matching game frames and check memory guards throughout.
5. End with neutral inputs and normal exit. Include disconnect/reconnect and controlled fourth-peer rejection after the stable three-input baseline.

**Exit criterion:** each input stream affects its own player, all processes agree on participant identity and relevant frame state, and guards remain intact. A populated lobby or one process reporting three entities is insufficient. Passing this gate still does not establish campaign support.

## 7. Generalize core GSC, revive, and checkpoints

Replace pair assumptions with explicit roles: a story leader, a participant list, all nonleader followers, a selected rescuer, all passengers, or all scene witnesses. Do not replace a singular partner helper with an arbitrary first teammate. Give per-player state and resources stable identity/epoch keys.

Core changes include per-follower regroup/leash timers, distinct safe teleport reservations, per-participant weapon/viewmodel snapshots, and an explicit policy for shared pickups and scripted loadouts. Preserve functions that already iterate over all players while testing their behavior with three actual participants.

Revive needs victim-owned use targets and timers, per-observer HUD, an exclusive rescuer/victim claim, and scoped cancellation/cleanup. A global revive target or a single friendly-icon target cannot represent two downed teammates. The existing all-downed reduction already iterates over players; preserve that behavior and test it rather than blindly changing every numeric two.

Checkpoint and mission transitions need host-authoritative revisions, idempotent requests, delivery/acknowledgement to both remote peers, and restart/next-map barriers. The GSC command mailbox alone does not implement native all-peer progression.

**Exit criterion:** test all eight downed subsets, all six directed rescuer-to-victim pairs, competing rescuers, two simultaneous victims, cancellation, bleed-out, disconnect, and terminal cleanup. Verify the agreed failure policy: two downed with one eligible living participant must not be mistaken for all downed. All three clients must restart/resume at the same committed checkpoint and map epoch.

## 8. Validate mission resources and cinematics individually

The staged upstream framework is still designed around two participants. Compile and exercise each mission module as it becomes reachable; the current two-player trainer start covers only part of the staged content and has no progression result.

| Mission system | Required three-participant work |
|---|---|
| Humvees and helicopters | Distinct passenger seats/controllers, all-passenger boarding barriers, serialized turret swaps, and cleanup for every rider. |
| Cliffhanger snowmobiles | Three safe vehicle assignments, no double mount, and an ending barrier that accounts for every eligible participant. |
| Cliffhanger climb | Participant-keyed near/far lanes and anchor caches, validated geometry, queued shared catches, per-rig arm state, and an all-player completion barrier. A third usable lane must be demonstrated. |
| Favela car scene | Distinct rear-seat placement, per-player duck/damage state, and a queue through the shared exit door. |
| Airport elevator and ending | Per-follower positions/cameras and an ordered victim sequence with idempotent shot events and completion after all intended victims. |
| Crash site and ISS | Separate rigs/cameras/view snapshots, verified cabin space, staggered exits, and per-player street hazard state. |
| Gulag fly-in | Verified distinct seats and view controllers for both followers, followed by complete ground-reference and control restoration. |
| Oilrig opening | A verified third ride position and a queue for shared helper animations, with independent swimmer/exit state. Two NPC helpers remain two NPC helpers. |
| Endgame knife scene | One validated scene lead and two independent witnesses, cameras/HUD, visibility-safe placement, and scoped cleanup. |

Avoid global singleton caches that make both followers share one second lane, seat, rig, or camera. Shared story actors and narrow geometry may require deterministic queues instead of parallel execution. Keep mission-local namespace dependencies local to the zones that provide them.

**Exit criterion:** automated real-client scenarios exercise every participant in each eligible role, simultaneous interaction, cancellation/failure, safe resource release, and progression barriers. Report supported missions and start points individually; a trainer or one-mission pass is not a campaign pass.

## 9. Establish release evidence and usable host/join flows

After the native and script gates pass, add reproducible three-client regression runs with packet delay/loss/reordering, reconnect, state/ownership comparisons, and mission transition coverage. Keep a clean single-player regression baseline.

Only then implement and document a supported host/join workflow, compatible content/protocol checks, build requirements, clear failure messages, and tested mission coverage. Publish summaries that distinguish synthetic helpers, single-process experiments, multi-process simulation, and tests across separate PCs. Keep personal data, game assets, extracted sources, and raw private logs out of release material.

**Release criterion:** claims match the real-client tests that ran. Version 0.0.1-dev currently has bounded same-PC trainer movement QA, no playable-three-player claim and no claim of a complete two-player x86 port.

## 10. Consider four players after the three-player baseline

Repeat the full capacity, lifecycle, protocol, script-resource and mission audits for four participants. Do not infer four-player support from a two-bit selector or a third allocated record. A fourth participant needs its own accepted member/channel/storage, input ownership, broadcast delivery, revive relationships, vehicle seats and cinematic handling. Preserve mixed-version rejection and test excess-player rejection at the new limit. This work starts only after repeatable three-PC gameplay and progression; it has not been implemented.
