# Testing and evidence

Completed checks include isolated single-player components, native two-player admission, shared stock/campaign-trainer loading, and **automated independent native movement with matching world positions** on one exact Windows x86 build184 executable. The two-player tests used independent copies on **one PC**, through fixed-loopback QA and exact-peer transport on an existing nonloopback interface. This is bounded trainer movement QA. Separate-PC play, aim/fire/use, campaign progression and three/four-player networking remain unverified. The offline roster/ACK policy is tested separately and has no native game integration.

## Reproduce the standalone checks

Use [build.ps1](../build.ps1) to build the project and [tests/run-tests.ps1](../tests/run-tests.ps1) for the current standalone test suite. Follow the compiler and path options documented by those scripts. From the repository root, inspect their parameters with:

```powershell
Get-Help .\build.ps1 -Detailed
Get-Help .\tests\run-tests.ps1 -Detailed
```

Record the commit, compiler version, runner options, exit code and actual summary from each new run. A successful helper suite does not reproduce a game smoke test. The historical counts below are receipts from development runs; they are not a promise that the current runner will print identical totals. A changed assertion count needs an explanation, and failures must remain failures rather than being hidden by an updated expected total.

Use an isolated test copy for any later game run. Record the executable's build identity and SHA-256 first; a different executable requires its own compatibility work. Keep game files, extracted or transformed game scripts, saves and raw diagnostic logs out of published test artifacts. Public receipts should contain sanitized results and test configuration, without personal paths, account identifiers, process identifiers or memory addresses.

## Completed standalone development receipts

These tests exercised project helpers or synthetic fixtures, without a multiplayer session.

The current native standalone runner passed **72 helper/build cases**. The original publication-stage receipt had 48 cases; 68 is a later historical total. The native suite includes pure exact-peer policy/transport fixtures and guarded identity tests. A separate public Python suite passed **128 fixtures**: 30 launcher, 66 shared-map and 32 input-reader cases. The scorer modules have **66 shared-map and 32 input fixtures**, including negative teardown/crash, stale state, missing evidence and excessive neutral excursion. The private dispatcher adds 11 fixtures; those 109 private machinery fixtures are a different total from the public native suite and do not establish a real-game result. Per-component counts below use their own assertion units and must not be added to these totals.

```powershell
python -B -m unittest discover -s tests -p 'test_*.py'
```

The portable read-only scorers and their invocation are documented under [tools/qa](../tools/qa/README.md). Raw receipts stay private; public evidence contains only selected sanitized results.

| Area | Historical result | What was exercised |
|---|---|---|
| Script patch parser and path handling | 35 checks, zero failures | Unique context matching, malformed/ambiguous patch rejection, bounded output, identifier/function handling and restricted relative paths. This is not a proof that every campaign patch compiles or behaves correctly. |
| Cache transaction/reset helper | 37 checks, zero failures | A synthetic 32-site transaction, full-instruction mismatch refusal, protection restoration, explicit rollback, injected failures, reset argument/caller/thread checks and guard corruption handling. |
| Startup/unsupported-image helpers | 26 checks, zero failures | Default forwarding behavior and refusal of unsupported helper images, dynamic loading and missing required control opt-in. These are project-owned helper processes, not game clients. |
| Additional cache/startup guard cases | 14 checks, zero failures | Dynamic cache-only, static cache-only and cache-without-control cases. These extend the startup receipts; they are not 14 additional game launches. |
| SHA-256 boundary/chunking comparisons | 14 comparisons matched an independent implementation | Irregular input lengths and chunk boundaries, in addition to the helper's known-vector checks. |
| Offline roster/ACK policy | 633,897 checks, zero failures; 200,000 mutation cases | Three bounded slots, snapshot encoding/decoding, generation/revision changes, reconnect handling, recipient binding and content-bound ACK readiness. |
| Policy mutation review | All seven deliberately broken variants were detected | Incorrect one-peer readiness, ignored freshness/content/recipient checks, fourth-peer clobbering and acceptance of the stock namespace. These mutants were expected to fail the tests. |

The policy's ACK binds the canonical snapshot content with SHA-256. It supplies consistency/freshness checks after a caller identifies the sender; it does not authenticate a peer. Native dispatch must still bind the expected host/session and sender slot, reject replayed snapshots, and distinguish retransmission from a new connection incarnation. A passing offline policy is not evidence that any of those engine connections exist.

## Completed isolated trainer component smoke

The final combined development smoke observed the trainer for approximately **45 seconds**, then requested an orderly quit. It exited normally with code **0**. The session had **one active local player**, with native co-op disabled. The test executable remained unchanged on disk.

| Component | Recorded evidence | Limit of the result |
|---|---|---|
| Early installation | Storage, cache and script-loader installation completed before the original WinMain/engine initialization; three startup barrier entries had three matching exits. | This establishes the tested early-startup phase. It is not a worker barrier for modifying a running lobby or script VM. |
| Native player storage | All 20 guarded storage sites installed; 33 valid observations reported capacity three, three distinct entity/client bindings and one local participant. Guards remained intact. | Allocation and initialization of three records do not make three live players. |
| Lobby statistics/name cache | The guarded 32-site cache transaction installed. Across 34 valid observations, capacity was three while traversal stayed at two. Reset validation retained the original 0x7C prefix clear and checked the expected caller and marker bytes. | The third cache row is reserved. Common roster access, third-member traversal and peer propagation are not integrated. |
| Script probe | The source loader installed, with 33 valid observations of a single script-visible player and the expected probe text. | The probe verifies the tested source-load path and one-player state. It does not demonstrate three script actors. |
| Two-player framework compile/load smoke | 161 source-load requests were recorded. Four project-supplied source files and five patch files were exercised; 19 patch blocks were applied with the expected coverage checks. | This exercised a trainer subset of the existing framework. It does not cover every staged file, every map or any two-/three-client network session. |

No guard failure was reported during this run. These observations belong to that specific component configuration; they should not be relabeled as multiplayer, campaign-completion or general stability results. Later changes to storage, reset logic, source loading or startup installation require a fresh combined smoke rather than reuse of this receipt.

## Completed same-PC two-player integration checks

Development now prioritizes the original two-player native session before increasing capacity. These runs used independent full test copies, the original two-player capacities, native UDP transport, explicit development opt-ins, and the same-PC `127.0.0.2` QA boundary. The client used a guarded alias of the cached native identity so the two local instances could be distinguished. The account/provider interface and identity-validation code were unchanged. This QA identity is not a supported separate-PC or shipping identity flow.

| Stage | Recorded evidence | Limit of the result |
|---|---|---|
| Native admission | Real Connect/Accept, acknowledgements and heartbeat traffic; two occupied native members with the intended local ownership. A bounded 30-second observation after readiness ended with both processes exiting normally. | Admission alone does not load a shared world or prove input routing. |
| Shared stock map | Both copies loaded `so_killspree_trainer` with `coop=1`, `specialops=1`, and advanced native simulation during a bounded 100-second run. The shared-map scorer passed using a validated observation cutoff before orderly teardown. | This is a stock-map integration baseline, not campaign coverage or an independent-input result. |
| Shared campaign trainer | Both copies loaded `trainer`, start `pit`, with `coop=1`, `specialops=0`, `gameMode=2` and two actual script players. Both reached native client state 6, with two entity/channel bindings. During the final 20-second window, level time and server/applied command times for both slots advanced by about 19.9 seconds in both copies. | The trainer start and advancing native application are verified. Distinct deliberate controls, mission progression, checkpoints and other mission modules are unverified. |

The campaign run observed **100 seconds after readiness**, with native `coopLaunch` over UDP occurring about **18.9 seconds** into that interval. Its DLL SHA-256 was `d48abb16e08bc5e857313067c40f5f2998bdea187909d2ebc1e44309f49e9aca`. The shared-map scorer passed. Both processes exited with code 0, no forced cleanup was required, all owned processes stopped, and both executables remained unchanged. These results do not activate or validate three-member networking.

### Shared-map acceptance and observation boundaries

A map appearing once is a load milestone. The stronger advancement check requires both copies to retain the expected map/mode, `sv_running=1`, `coop=1`, native world state 6, active sessions, two occupied members, stable slot/entity/channel ownership and finite positions. Over the final 20-second observation window, level time and server/applied command times must advance by at least 1,000 milliseconds across at least three samples. A full pass also requires a clean harness result, normal exits and unchanged executables. A passing tail of telemetry cannot excuse a crash or earlier harness failure.

The measurement interval ends **before either process is asked to quit**. New runs record an explicit pre-cleanup observation marker while both mailboxes are empty. The historical stock run used an explicitly requested retrospective boundary: the earliest consumed own normal-quit command, cross-checked against the clean harness command ledger. Sequential normal teardown can make the remaining peer leave its world; records after the validated boundary do not describe the measurement interval. Invalid state before that boundary remains a failure, and crash metadata prevents an overall pass.

Native command times can advance while inputs remain neutral. Shared-map advancement does not demonstrate two independent producers, correct movement ownership, or agreement under deliberate divergent actions.

## Completed independent two-player movement gate

Require fresh shared-map state, an unpaused simulation, stable bindings and advancing command times on both copies. Use game-time-bounded neutral, host-only, client-only and distinct simultaneous native movement phases, with confirmed mailbox consumption and releases on both copies. Compare actual server/applied command axes and actor positions at matching game/command times across both worlds. Check every captured neutral sample for excessive movement, not just its end position. Script teleports and QA button variables do not satisfy this gate.

Two real-game runs passed all seven phases and the stronger shared-map oracle, each observed for **100 seconds after readiness**, with 30 exact count/byte input-plan mailbox acknowledgements, consumed releases, normal exit code 0 on both processes, no forced cleanup and unchanged executables:

| Run | Native configuration | Result |
|---|---|---|
| [Loopback movement evidence](evidence/native-input-loopback-2026-10-21.json) | Separate fixed-loopback QA mode; client cached identity alias; host forward, client right, simultaneous host forward/client left | 7/7 phases passed; every compared equal-time world position matched, maximum error 0 |
| [Exact-peer movement evidence](evidence/native-input-exact-peer-2026-10-21.json) | Peer-test mode, loopback-QA flag off; one existing owned nonloopback interface, two exact native ports, role-distinct cached IDs | 7/7 phases passed; solo host/client displacement about 112.7/69.8 units; simultaneous return displacement about 80.6/84.0 units; maximum compared world error 0 |

The exact-peer run used DLL SHA-256 `0b9ae3e2da9bed02b80a5be30450e642d53a9f400d1d0ae73f34efb2bc3acb34`. Both copies had `trainer`/`pit`, `coop=1`, `specialops=0`, `game_mode=2`, native world state 6, two script players and distinct native bindings. Storage/cache expansion flags were off; capacities and wire framing stayed at two. The simultaneous exact-peer phase used host back/client left to retrace the preceding solo routes and avoid a wall. This is a separately named input pattern; the earlier forward/right expectations and failed receipts were preserved.

The movement reader requires signed native server/applied axes for each assigned player, at least three exact cross-world `(level_time, applied_cmd_time)` pairs spanning 300 ms per phase, maximum pair gap 200 ms, world-position error at most 0.5 units, active displacement between 2 and 256 units, and neutral maximum excursion at most 4 units across **all** captured interior samples, including unpaired samples. Phase edges exclude 200 ms. All four neutral phases in both passing runs had measured excursion 0. No acceptance thresholds were relaxed to pass these runs.

Earlier attempts remain failures: 20 FPS telemetry lacked exact-pair density; an initial movement route hit a wall; the first exact-peer native-open attempts rejected incorrect assumptions about pending dvars. The native-open fix validates the engine's pending `net_ip`/`net_port` values before the stock open routine, then verifies the actual socket bind afterward. It does not write raw dvar storage or assume a requested port was obtained.

Exact-peer observation cutoffs are accepted only with explicit pre-cleanup markers, verified same-PC ownership/configuration and clean teardown. A shared `GetTickCount` clock cannot be assumed between different PCs. Separate-PC tests need their own cross-machine event/time agreement and must not reuse the same-PC cutoff as proof.

These results establish native movement ownership and bounded measured position agreement. They do not validate mouse/controller input, firing, interaction, progression, packet-loss behavior or saving. The public launcher is a separate execution path; its own bounded result follows below.

## Completed public launcher lifecycle check

The [developer launcher](TWO-PLAYER.md) was exercised end to end using two fresh, independent copies created by its own `--prepare-copy` command. Both actual CLI processes became ready, the test driver confirmed the exact client READY and host prompt before supplying Enter, and the native launch loaded `trainer`/`pit` with two participants. Both slots' level/server/applied command clocks advanced by over ten seconds across 23 accepted snapshot pairs. Both launchers and games exited with code 0, no forced termination was needed, all managed files were restored, both executables were unchanged and the exclusive test lease was released. See the [sanitized launcher receipt](evidence/public-launcher-same-pc-2026-10-21.json).

An earlier launcher run failed because settings were applied before the asynchronous invite UI switched to native mode 2 and reset `specialops`. The launcher now waits for that transition before its single campaign override and retains strict fresh-state/ACK checks. Another run remains FAIL after its private observer sampled the common clock before reading concurrently appended logs. The corrected observer reads both frozen texts before sampling its clock; future/stale checks remain strict. Six pure timing/invalid-state fixtures passed before the new end-to-end run. Historical failed receipts were not relabelled.

This test covers lifecycle, shared loading and native advancement through the actual public CLI. It contains no movement plan and does not extend the separate movement evidence. All processes were on one PC; manual READY confirmation still does not implement cross-peer compatibility or settings negotiation.

## Next two-player gates

Run automated tests on two separate PCs with mirrored exact-peer endpoints and matching build/content, then cover native aim/fire/use, death/revive, checkpoint restart, disconnect/rejoin, mission triggers and transitions. The launcher currently has manual readiness confirmation; cross-peer version/seed/settings agreement is not implemented. A complete campaign claim requires recorded mission coverage and resolved progression failures.

## Gates required before claiming three-player co-op

The following gates remain open. A lobby count or three allocated entities is insufficient evidence for any later gate.

1. **Coherent native roster and lifecycle.** Establish the same three-member roster, slot assignment and generation/revision on every instance. Preserve native record ownership, reset third-member state on create/join/delete, and update already-connected clients when another peer joins. Verify one correctly assigned local slot per process.
2. **Channels, server storage and fixed-two helpers.** Review the complete operand-aware relocation manifest, initialization, embedded fragment-buffer pointers, loop bounds and unrolled helpers. Use checked allocation/protection transactions and rollback. Keep guard checks enabled across map transitions and teardown.
3. **Private protocol compatibility.** Isolate changed Connect/Accept/ACK, heartbeat, launch and participant-bit formats with a versioned private protocol. Reject stock or incompatible clients before parsing a changed payload or writing a slot. Test both directions of a mixed-version connection attempt.
4. **Full-lobby rejection.** Reject a fourth client before any roster/channel/cache write. Cover fresh connections, cached-address paths, repeated requests and reconnect races; prove that none replaces an occupied slot or changes the current roster unexpectedly.
5. **Roster ACK readiness.** Launch only after the intended peers acknowledge the same current snapshot. Test missing, delayed, duplicated, reordered, stale and wrong-recipient ACKs; generation/revision rollover policy; membership changes during acknowledgment; and reconnects with unchanged identity/name/address but a new connection incarnation.
6. **Native activation on three instances.** Run three isolated clients, then three separate PCs. Confirm three distinct active server/entity bindings, complete channel state and exactly one local input owner per instance. Distinguish an allocated slot from a connected and simulated participant.
7. **Independent real user commands.** Capture actual native user-command ownership and sequence progression for all three players. Drive different movement, view, fire and interaction actions on each client; verify the host applies each command to the intended actor. Mirrored input, scripted actor movement or a lobby animation is not a substitute.
8. **Authoritative frame delivery and recovery.** Verify host aggregation and delivery to both remote clients, participant decoding, sequence/ACK handling and consistent world state. Exercise loss, latency, duplication, reordering, timeout, disconnect and rejoin. Check that one slow or absent peer does not silently corrupt another peer's state.
9. **Engine and script lifetime barriers.** Prove the phase/thread requirements for every mutation, including map restart, reload and shutdown. Account for engine workers, script VM execution, retained source/state references and garbage collection before resetting or freeing memory. Balanced early-startup callbacks and intact canaries alone do not establish later quiescence.
10. **Campaign gameplay and progression.** Test mission triggers, all three actors, checkpoints/restarts, scripted sequences, vehicles, failure/death/revive behavior and transitions on each supported mission. Claim full-campaign support only after a recorded three-player run covers the entire campaign with no unresolved blocking progression failures.

Publish results per gate, with reproducible configuration and explicit remaining failures. The supported claim is a tested component prototype plus automated same-PC two-player admission, campaign trainer loading and independent native movement. No complete two-player campaign port or playable three-player release is claimed.

The evidence-first workflow is informed by [cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything). Credit for that playbook does not imply that it validates or endorses this prototype.
