# Architecture — 0.0.1-dev

**MW2 Trio is an experimental x86 development project.** Two independent copies have connected over native UDP, loaded campaign trainer, and passed seven phases of independent native movement with matching measured world positions. These are bounded same-PC tests, including exact-peer transport on an existing nonloopback interface. Separate-PC connectivity, aim/fire/use, campaign progression and three/four-player sessions remain unverified; a complete two-player port is not claimed.

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

The compiled runtime contains optional diagnostic and two-player integration experiments. They require explicit opt-ins, the exact identity and the verified installation/engine phase. Building the DLL creates no flags and starts no session.

| Layer | Source | Current role and boundary |
|---|---|---|
| System proxy and identity | [`bootstrap.c`](../src/bootstrap.c), `winmm32_*`, `pure_sha256.*` | Forwards the system WinMM interface and exposes read-only executable identity checks. SHA-256 uses the project's portable implementation. |
| Startup barrier | [`startup184.c`](../src/startup184.c) | Verifies the first static-load startup call and installs experiments before the original WinMain, outside DllMain. Provides the shared pre-engine quiescence callbacks. |
| Main-thread control | [`control184.c`](../src/control184.c) | Runs diagnostic observation and a bounded local command mailbox on the initial game thread. Queued commands are not proof that an engine action completed. |
| Player storage | [`storage184.c`](../src/storage184.c) | Guards 20 sites and reserves three gclient/metadata records with canaries and guard pages. It does not expand native server admission or establish three active players. |
| Member cache | [`cache184.c`](../src/cache184.c) | Relocates 31 classified cache operands and one constructor reset call. Allocation and reset cover three records; native traversal remains two. |
| GSC source loader | [`scripts184.c`](../src/scripts184.c) | Loads bounded own script files and applies strict text patches to sources obtained from the player's game. It does not enable campaign co-op. |
| Script runtime support | [`runtime184.c`](../src/runtime184.c) | Installs guarded pre-engine script/dvar visibility, localization and error-diagnostic support. It does not synchronize settings or implement progression. |
| Socket observation and QA boundary | [`net184.c`](../src/net184.c), [`reliable184.c`](../src/reliable184.c) | Wraps the two stock socket imports with bounded token diagnostics. Explicit loopback QA latches transport to `127.0.0.2`; it does not implement a replacement transport or wider participant protocol. |
| Native party commands | [`party184.c`](../src/party184.c) | Registers own commands on the verified main-thread callback after engine initialization. Calls guarded native two-member party and launch paths. |
| Native socket/connect adapters | [`netopen184.c`](../src/netopen184.c), [`connect184.c`](../src/connect184.c) | Opens the native UDP socket and queues the native reliable Connect payload from guarded fresh-menu commands. In QA mode, verifies the actual loopback bind and confines the destination. Native code owns retry, ACK and launch delivery. |
| Same-PC identity fixture | [`qa_identity184.c`](../src/qa_identity184.c) | Explicit client-only, one-time expected-value update to the cached native identity before party/socket use. Keeps provider/account and identity-validation code unchanged; watches the accepted value and scope every callback. |
| Denied-service diagnostics | [`service184.c`](../src/service184.c), [`backend184.c`](../src/backend184.c) | Pure classifiers for narrowly recognized attempts already blocked by the QA transport boundary. They never authorize external I/O; unknown outside traffic remains fatal. |
| Exact-peer scope | [`peer_scope184.c`](../src/peer_scope184.c) | Pure strict seven-key configuration parser, endpoint/subnet policy and session-seed-derived role IDs. Sticky latch/seal/observe checks; no engine, provider or socket API. |
| Exact-peer adapter | [`peer_transport184.c`](../src/peer_transport184.c) | Separately selected by peer-test opt-in. Confirms an existing up/Preferred owned IPv4 interface, unchanged configuration and actual native socket/endpoint tuple. Supplies an immutable scope to packet checks; does not open sockets or configure adapters. |

In exact-peer mode, `qa_identity184` also exposes a separately guarded role-based cached-ID operation for **both** host and client. The low DWORD is derived from the shared seed with different role parity; the original nonzero high DWORD is retained. The own-module read/execute-only scope callback must return exactly 1 before commit and on every observer frame. This changes cached native classification, not provider/account APIs, and is not authentication or a shipping identity solution.

The socket wrappers give known backend-service denial precedence over the exact-peer allowance. Eligible traffic must use the latched native game socket and match the configured complete endpoint tuple, verified by actual `getsockname`; unknown traffic is fatal. Recognized denied services remain denied even when their abort policy is nonfatal. This is a boundary on the wrapped native imports, not proof of whole-process/provider egress isolation. NAT rewriting, unexpected UDP source addresses and adapter/configuration drift are not supported.

Native socket opening validates pending/latched `net_ip` and `net_port` values because the original engine applies them during registration/open. Ordinary `+set` arguments and command ACKs alone are not bind proof. After stock opening, the adapter requires the actual exact bind and rejects native port fallback before party creation. No raw dvar writes or permission-bit changes are used.

The pre-engine installation order is storage, cache, scripts, then script runtime support, followed by the original WinMain; absent module flags leave those experiments disabled. Installation uses exact-byte checks, verified pre-engine callbacks, protection restoration, and checked failure handling. Party commands and socket observation initialize later through the verified main-thread callback. Installed code and relocated storage must outlive any engine or UI pointer that can reference them. Rollback is restricted to the phase before those pointers can escape; runtime unloading is not supported.

An active control callback observes storage/cache guards, script runtime support, the latched QA transport boundary and an active identity fixture every owner-thread frame, before ordinary mailbox opt-out or telemetry throttling. Fatal installation/restore failures stop the owned process before WinMain; runtime contract failures stop that isolated process. Component flags alone do not supply this observer: the control callback must also be active.

### Explicit opt-ins

These are developer experiments, not a multiplayer installation recipe. Flags are ordinary files beside the test DLL/executable; each module also enforces its own build, thread and phase requirements.

| File | Scope |
|---|---|
| `mw2-trio-control.enable` | Main-thread observer, bounded command mailbox and normal diagnostics. |
| `mw2-trio-storage-test.enable` | Pre-engine three-record player storage experiment. |
| `mw2-trio-cache-test.enable` | Pre-engine three-record cache allocation/reset with two-record traversal. |
| `mw2-trio-scripts-test.enable` | Pre-engine own-source/strict-patch loader. |
| `mw2-trio-two-player-test.enable` | Pre-engine script runtime support; requires control and does not create peers. |
| `mw2-trio-network-test.enable` | Socket import diagnostics, initialized from control. |
| `mw2-trio-party-test.enable` | Guarded main-thread native party/socket/connect commands. |
| `mw2-trio-loopback-qa.enable` | Latched same-PC transport restriction and eligibility for the separate explicit client identity command; requires network/party/control scope. The flag alone does not change identity. |
| `mw2-trio-peer-test.enable` | Separate exact-peer development scope, mutually exclusive with loopback QA. Requires unchanged `mw2-trio-peer.cfg`, owned local interface, network/party/control scope and explicit role identity command before native socket/party use. |
| `mw2-trio-input-test.enable` | Read-only dense native input telemetry after world state 6, bounded to 120 seconds or 6,000 frames. It does not generate input or prove independence. |

Removing a flag does not uninstall live hooks. In particular, lost loopback/identity scope after activation is fatal rather than permission to resume unrestricted traffic. The QA alias affects shared native local/remote classification, headset, progress and heartbeat consumers; it is not merely a packet label. Saves and progression under this alias are unverified. It is not a shipping identity flow.

## Storage capacity is not participant membership

These values describe different things:

- **Allocated records:** memory reserved for possible participant state.
- **Native accepted participants:** connections admitted and bound to distinct identities and server/channel records.
- **Script players:** actual entities in `level.players`.
- **Local players:** controllers and views owned by one process.

The intended three-player topology is one host and two remote clients, with one local player on each PC. The completed single-player capacity experiments have one local player, `coop=0`, and one actual script player even when three records are allocated. The later two-copy integration uses the original two-player capacities and reaches two actual script players. A presence mask for three distinct record pointers does not show three connections or independent inputs.

The cache distinction is deliberate: each cache record is `0x38` bytes; allocation/reset reserve `3 * 0x38 = 0xA8` bytes. All four relocated traversal endpoints still use **two** records. Expanding those loops alone would call original two-member session accessors with index 2 and reach unrelated session state. The original constructor's `0x7C` clear remains intact; a separate guarded clear resets the relocated three-record cache. Neighboring lobby and dvar fields retain their layout.

## Script loading and staged content

[`content/probe`](../content/probe/) contains a small own diagnostic script and its startup patch. The probe reports the actual script array size: `players=1` in the isolated component checks and `players=2` in the later shared campaign-trainer check.

[`content/framework`](../content/framework/) stages 14 upstream own mod scripts, the additional probe, and 26 patch files. Its provenance records the pinned MIT source. The patches retain small context fragments needed to locate changes in the player's own scripts; no complete extracted game script is included.

The x86 loader uses the target engine's source-buffer interface and native rawfile allocation/read functions. Relative paths and file sizes are bounded, traversal paths are rejected, and patch operations fail closed on malformed, missing, or ambiguous matches. Patch acceptance does not establish that every referenced GSC builtin has the required x86 behavior.

The completed trainer single-player load requested **four own sources and five patched scripts, applying 19 blocks**:

| Requested own sources | Requested patched scripts |
|---|---|
| `_mw2coop`, `_mw2coop_qa`, `_mw2coop_roadkill`, `_mw2trio_probe` | `_utility`, `_load`, `_coop`, `_intelligence`, `trainer` |

This historical single-player receipt is partial loading/compilation coverage: **4 of 15 staged own sources and 5 of 26 patch files**. The separate parser comparison covered all 26 patch files and 343 operations, including the probe, but does not extend runtime coverage. With `coop=0`, that run kept campaign co-op framework threads gated off. The later `coop=1` trainer `pit` test exercised a two-player start and advancing shared simulation; it does not establish coverage of the other mission modules or upstream gameplay scenarios.

The staged framework still contains pair assumptions: `level.player2`, a singular partner helper, one passenger seat or witness camera, shared second-climb-lane caches, and QA checks for exactly two participants. Some loops already handle an array of players; that does not resolve shared resources, revive relationships, or mission geometry. See the [roadmap](ROADMAP.md) for the dependency order.

## Offline roster policy

[`tests/trio_policy.c`](../tests/trio_policy.c) and its tests are an **offline prototype**, not a runtime networking module. They model a bounded three-member roster, stable slots, session generation, revision, per-peer acknowledgements, reconnect invalidation, and fourth-member rejection. ACKs include SHA-256 of the complete canonical recipient snapshot.

The model treats native identity and address data as opaque. A future adapter must bind the actual packet sender to its connection, slot, identity, and incarnation before accepting an ACK. It must also authenticate the expected host, reject stale snapshot application, and separate host mutation/readiness from client snapshot/ACK behavior. A content hash does not provide sender authentication.

The prototype does not modify native session objects, open sockets, admit peers, change packet framing, or submit user commands to the game. Its tests establish properties of that model only.

## Unfinished native network layer

Static work has classified **257 candidate server/channel relocation operands** and explicitly preserved 337 neighboring aliases. That inventory remains an inactive implementation aid. Fixed-count and unrolled helpers, embedded pointers initialized before engine startup, native session lifetime, and wire-format consumers require coordinated work beyond operand relocation.

The authoritative-frame participant path includes a one-bit selector, and existing heartbeat framing uses a value that conflicts with a third participant index. A proposed versioned private protocol must update every producer and consumer together. Wider identifiers alone do not supply a third channel, validated admission, command ownership, or all-peer frame delivery.

The guarded native two-player path is available only as a developer experiment. Native capacity beyond two and the proposed wider protocol are not implemented. Two independent native movement streams have passed bounded same-PC trainer checks. Separate-PC tests, native aim/fire/use and two-player progression are the next gates. Three-client input, mission resources and cinematics remain later coordinated expansion work. Four participants require another coherent capacity/protocol/script review; they are not enabled by reserving three records.

## Evidence boundary

Completed observations include single-player component checks, native two-copy admission/shared loads, and seven-phase native movement checks in `trainer`/`pit`. Both fixed-loopback QA and exact-peer mode on one existing interface passed. Host-only and client-only movement affected the assigned actor, distinct simultaneous inputs passed, every compared equal-time position matched, and both processes exited normally with unchanged executables. Each run observed 100 seconds after readiness. Both processes were on one PC.

Synthetic helpers exercise parser, identity, transaction, reset, transport and offline policy behavior. The native suite passed 72 helper/build cases. Public Python fixtures separately exercise the launcher and read-only QA scorers. Private raw logs, configuration and analysis artifacts are excluded. See [testing](TESTING.md) and its sanitized evidence for exact acceptance boundaries.

These results establish bounded native movement ownership and measured position agreement. Separate-PC compatibility, general input devices/aim/fire/use, three-player synchronization, revive, checkpoint restart, vehicles and full campaign progression remain open. The [developer launcher](TWO-PLAYER.md) is a separate reproducible path with explicit manual readiness; it does not implement cross-peer settings/version agreement or automatic progression.
