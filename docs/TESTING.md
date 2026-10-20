# Testing and evidence

The completed game checks establish isolated **single-player component behavior** on one matching Windows x86 build 184 executable. They do not establish working three-player networking or a playable co-op campaign. The offline roster/ACK policy is tested separately and has no native game integration.

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

Publish results per gate, with a reproducible configuration and explicit remaining failures. At present, the supported claim is a tested component prototype plus an offline policy test suite.

The evidence-first workflow is informed by [cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything). Credit for that playbook does not imply that it validates or endorses this prototype.
