# Architecture — 0.0.1-dev

**MW2 Trio is an experimental x86 development project, not a playable three-player campaign mod. Cooperative gameplay QA: none.** No second or third game client has been connected in this project, and a complete two-player port has not been demonstrated either.

The development method and handoff structure are guided by **[cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything)**: identify the exact executable, isolate experiments, verify changes, and report the actual QA level. Native proxy design and the staged MIT GSC framework also build on [stxlvn/mw2_coop at commit 8fcb6bc](https://github.com/stxlvn/mw2_coop/tree/8fcb6bcf9980894a631906059558a4797f4c9752). The upstream native implementation targets a different x64 build; it is not binary compatible with this x86 target. Its licenses and attribution are retained in this repository.

## Exact target

The guarded runtime experiments support one inspected executable identity:

| Property | Required value |
|---|---|
| Game | Call of Duty: Modern Warfare 2 (2009), original PC single-player executable |
| Executable | `iw4sp.exe` |
| Architecture | x86, PE32 |
| Build marker | `184` |
| PE timestamp | `0x4B7215E1` |
| Expected image base | `0x00400000` |
| SHA-256 | `4DD53FABF9677980AAF081009584ADE99D6BF440330046E5C8950E87EF0547D9` |

The build marker alone is insufficient. A different hash requires a new identity review and address map. The guard checks executable structure and the complete file hash; each mutation module additionally checks its expected live instruction bytes. The project does not distribute the executable, game assets, extracted game scripts, or replacement game dependencies.

## Runtime layers

The compiled runtime contains optional diagnostic experiments. They remain opt-in and require the exact identity and installation phase; their presence in the source tree does not enable networking.

| Layer | Source | Current role and boundary |
|---|---|---|
| System proxy and identity | [`bootstrap.c`](../src/bootstrap.c), `winmm32_*`, `pure_sha256.*` | Forwards the system WinMM interface and exposes read-only executable identity checks. SHA-256 uses the project's portable implementation. |
| Startup barrier | [`startup184.c`](../src/startup184.c) | Verifies the first static-load startup call and installs experiments before the original WinMain, outside DllMain. Provides the shared pre-engine quiescence callbacks. |
| Main-thread control | [`control184.c`](../src/control184.c) | Runs diagnostic observation and a bounded local command mailbox on the initial game thread. Queued commands are not proof that an engine action completed. |
| Player storage | [`storage184.c`](../src/storage184.c) | Guards 20 sites and reserves three gclient/metadata records with canaries and guard pages. It does not expand native server admission or establish three active players. |
| Member cache | [`cache184.c`](../src/cache184.c) | Relocates 31 classified cache operands and one constructor reset call. Allocation and reset cover three records; native traversal remains two. |
| GSC source loader | [`scripts184.c`](../src/scripts184.c) | Loads bounded own script files and applies strict text patches to sources obtained from the player's game. It does not enable campaign co-op. |

The startup order is storage, cache, then scripts, followed by the original WinMain. Installation uses exact-byte checks, verified pre-engine callbacks, protection restoration, and checked failure handling. Installed code and relocated storage must outlive any engine or UI pointer that can reference them. Rollback is restricted to the phase before those pointers can escape; runtime unloading is not supported.

An active control callback observes storage and cache guards every owner-thread frame, including when mailbox polling or ordinary telemetry is paused. A fatal restore failure stops the owned test process before WinMain; runtime cache contract failures or detected corruption stop that test process. The cache flag alone does not create an observer: the control callback must also be active.

## Storage capacity is not participant membership

These values describe different things:

- **Allocated records:** memory reserved for possible participant state.
- **Native accepted participants:** connections admitted and bound to distinct identities and server/channel records.
- **Script players:** actual entities in `level.players`.
- **Local players:** controllers and views owned by one process.

The intended topology is one host and two remote clients, with one local player on each PC. The completed single-player experiments have one local player, `coop=0`, and one actual script player even when three records are allocated. A presence mask for three distinct record pointers does not show three connections or independent inputs.

The cache distinction is deliberate: each cache record is `0x38` bytes; allocation/reset reserve `3 * 0x38 = 0xA8` bytes. All four relocated traversal endpoints still use **two** records. Expanding those loops alone would call original two-member session accessors with index 2 and reach unrelated session state. The original constructor's `0x7C` clear remains intact; a separate guarded clear resets the relocated three-record cache. Neighboring lobby and dvar fields retain their layout.

## Script loading and staged content

[`content/probe`](../content/probe/) contains a small own diagnostic script and its startup patch. The probe reports the actual script array size; in the completed trainer checks it reported `players=1`.

[`content/framework`](../content/framework/) stages 14 upstream own mod scripts, the additional probe, and 26 patch files. Its provenance records the pinned MIT source. The patches retain small context fragments needed to locate changes in the player's own scripts; no complete extracted game script is included.

The x86 loader uses the target engine's source-buffer interface and native rawfile allocation/read functions. Relative paths and file sizes are bounded, traversal paths are rejected, and patch operations fail closed on malformed, missing, or ambiguous matches. Patch acceptance does not establish that every referenced GSC builtin has the required x86 behavior.

The completed trainer single-player load requested **four own sources and five patched scripts, applying 19 blocks**:

| Requested own sources | Requested patched scripts |
|---|---|
| `_mw2coop`, `_mw2coop_qa`, `_mw2coop_roadkill`, `_mw2trio_probe` | `_utility`, `_load`, `_coop`, `_intelligence`, `trainer` |

This is partial loading/compilation coverage: **4 of 15 staged own sources and 5 of 26 patch files**. The separate parser comparison covered all 26 patch files and 343 operations, including the probe, but does not extend the runtime coverage. With `coop=0`, campaign co-op framework threads remained gated off. The other mission modules and the upstream co-op scenarios were not exercised by this trainer load.

The staged framework still contains pair assumptions: `level.player2`, a singular partner helper, one passenger seat or witness camera, shared second-climb-lane caches, and QA checks for exactly two participants. Some loops already handle an array of players; that does not resolve shared resources, revive relationships, or mission geometry. See the [roadmap](ROADMAP.md) for the dependency order.

## Offline roster policy

[`tests/trio_policy.c`](../tests/trio_policy.c) and its tests are an **offline prototype**, not a runtime networking module. They model a bounded three-member roster, stable slots, session generation, revision, per-peer acknowledgements, reconnect invalidation, and fourth-member rejection. ACKs include SHA-256 of the complete canonical recipient snapshot.

The model treats native identity and address data as opaque. A future adapter must bind the actual packet sender to its connection, slot, identity, and incarnation before accepting an ACK. It must also authenticate the expected host, reject stale snapshot application, and separate host mutation/readiness from client snapshot/ACK behavior. A content hash does not provide sender authentication.

The prototype does not modify native session objects, open sockets, admit peers, change packet framing, or submit user commands to the game. Its tests establish properties of that model only.

## Unfinished native network layer

Static work has classified **257 candidate server/channel relocation operands** and explicitly preserved 337 neighboring aliases. That inventory remains an inactive implementation aid. Fixed-count and unrolled helpers, embedded pointers initialized before engine startup, native session lifetime, and wire-format consumers require coordinated work beyond operand relocation.

The authoritative-frame participant path includes a one-bit selector, and existing heartbeat framing uses a value that conflicts with a third participant index. A proposed versioned private protocol must update every producer and consumer together. Wider identifiers alone do not supply a third channel, validated admission, command ownership, or all-peer frame delivery.

No runtime native networking expansion is enabled in 0.0.1-dev. The first gameplay milestone is three real clients with independent native inputs in trainer; campaign progression and mission scenes come afterward.

## Evidence boundary

Completed observations include isolated single-player trainer loads, exact instruction checks, guarded storage/cache observation, execution of the own player-count probe, partial framework source loading, and normal process exit. Synthetic helpers exercise parser, identity, transaction, reset, and offline policy behavior. Private analysis artifacts and raw runtime logs are not part of this public repository.

**Cooperative gameplay QA remains none.** No completed result demonstrates two-player compatibility, three-player synchronization, remote input ownership, revive, checkpoint restart, vehicle behavior, or a full campaign playthrough for this x86 port.
