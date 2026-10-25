# MW2 Trio

**Version 0.0.1-dev · pre-alpha · developer prototype**

Experimental work toward a three-player campaign in **Call of Duty: Modern Warfare 2 (2009)** on separate PCs, targeting one exact x86 build184 executable. Development currently prioritizes working two-player sessions before expanding the player count.

**Three-player campaign co-op is not implemented yet.** Two independent game processes have joined a native UDP lobby, loaded campaign `trainer` at `pit`, and passed automated independent movement and matching-world checks. Automated tests used **one PC**, through both loopback QA and an exact pair of endpoints on an existing nonloopback interface. On October 24, 2026, the project owner also reported a successful trainer session on **two physical PCs over Radmin VPN**. This manual report does not establish automated cross-PC state agreement, shooting/interactions or campaign progression. A complete two-player campaign port is not claimed.

**October 25 body preview:** genuine teammate bodies, heads and held weapons are now visible in a controlled idle scene on two isolated instances on one PC. The opt-in implementation is published separately under [experiments/trainer-body](experiments/trainer-body/README.md). Its latest movement run completed all seven phases, but strict scoring rejected one old neutral command at the start of client movement. A subsequent command-activation gate has passed offline tests and has not run in the game. This is an experimental trainer result; locomotion/firing animation, separate-PC body rendering and campaign progression remain unverified. See [scope, instructions and evidence](docs/BODY-PREVIEW.md).

**Special thanks to [cranebeaker](https://github.com/cranebeaker), creator of [coop-anything](https://github.com/cranebeaker/coop-anything).** Their excellent guides provided the research, isolation, reverse-engineering and automated-testing approach behind this project. This prototype also builds on MIT-licensed work from [stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop). See [credits and provenance](CREDITS.md).

## What works today

The table describes the baseline at the repository root. The [standalone body preview](experiments/trainer-body/README.md) has its own source, build, pinned launcher and validation boundaries; its native changes are not enabled by the baseline launcher.

| Component | Verified scope |
|---|---|
| x86 `winmm.dll` proxy | System-export forwarding and exact executable fingerprint checks |
| Main-thread diagnostic control | A bounded local command mailbox starts trainer and exits the owned test process |
| Native player storage | Three separate client/animation records, 20 guarded code sites, guard pages and canaries |
| Member name/statistics cache | Three reserved records, 32 guarded sites and a verified reset; native traversal intentionally remains at two |
| GSC source loader | Core upstream scripts and an own player-count probe load on the target x86 engine |
| Roster/readiness candidate | Offline-only versioned snapshots, content-bound ACKs, stale/reconnect checks and fourth-member rejection |
| Native two-player admission | Real Connect/Accept, heartbeat exchange, two occupied members and distinct local ownership; two independent copies on one PC |
| Shared two-player map loading | Native UDP launch and advancing command application in stock `so_killspree_trainer` and campaign `trainer`; bounded same-PC tests |
| Script runtime support | Exact-build dvar visibility/localization/error diagnostics; campaign trainer's `pit` start reached two script-visible players |
| Independent native movement | Seven automated neutral, host-only, client-only and simultaneous phases passed in campaign trainer; matching positions across both worlds |
| Exact-peer development transport | Explicit mirrored endpoint configuration, actual adapter/socket-bind checks and role-distinct cached IDs; same-PC integration passed |
| Developer host/join launcher | Independent copy preparation, guarded readiness/launch and post-exit restoration passed end to end on one PC |

An earlier combined component test ran trainer for **45 seconds** with **one active local player and co-op disabled**, then exited normally. The executable on disk was unchanged. Three allocated records are storage capacity, not three connected players.

That run requested four own GSC files and five patched scripts, applying 19 patch blocks. It did not load every mission module. The original broader text comparison covered 26 scripts and 343 blocks, including the probe; text applicability does not prove campaign behavior. See [testing evidence and limits](docs/TESTING.md).

The latest campaign input test observed both processes for **100 seconds after readiness**. Both reached `trainer`/`pit`, `coop=1`, `specialops=0`, `game_mode=2`, two script-visible players and native world state 6. Deliberate native movement affected the assigned player while the other stayed still; simultaneous distinct inputs also passed. Positions matched at equal native game/command times, with zero measured error in the compared samples. All seven phases passed, all 30 input-plan mailboxes were acknowledged, and both processes exited normally with unchanged executables. See [sanitized exact-peer evidence](docs/evidence/native-input-exact-peer-2026-10-21.json) and [test requirements and limits](docs/TESTING.md).

These runs retain the original two-player capacities. The fixed-loopback QA identity fixture and the separately guarded exact-peer identity flow are opt-in development paths. The latter has passed on one existing interface with two ports on **one machine**; this does not establish connectivity between friends' PCs. The three-record storage/cache experiments are separate and remain disabled for these two-player tests.

The [two-player developer guide](docs/TWO-PLAYER.md) covers building, preparing independent game copies, explicit host/join configuration and recovery. The launcher defaults to a read-only plan and requires `--execute` for changes or game launch.

For the experimental body candidate, use the [body-preview guide](docs/BODY-PREVIEW.md) and work from `experiments/trainer-body`. Do not mix the two framework directories or launchers. Eligible independently prepared game copies can be reused when their provenance/content checks pass.

Allow about **12.7 GB per independent game copy**; a two-copy test on one PC needs about **25.5 GB**, plus tools and logs. The mod archive is under 1 MB. See the guide's [copy preparation and disk-space notes](docs/TWO-PLAYER.md#prepare-independent-copies) before creating more copies.

## Who this is for

At this stage the repository is useful to developers and reverse engineers who want to continue the port. If you want to play the campaign with two friends today, this version cannot provide that experience. The [roadmap](docs/ROADMAP.md) explains the remaining native networking, lobby, script and mission work.

## Exact target

| Field | Value |
|---|---|
| Game | Modern Warfare 2, original 2009 PC release |
| Architecture | PE32 / i386, x86 |
| Build marker | `184` |
| PE timestamp | `0x4B7215E1` |
| Image base | `0x00400000` |
| `iw4sp.exe` SHA-256 | `4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9` |
| Tested host OS | 64-bit Windows 11 |

Check your own file with PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'D:\MW2-Test\iw4sp.exe'
```

A build label alone is insufficient. Other hashes, the modern x64 release and the upstream mod's native DLL are incompatible with these addresses. Keep the fingerprint guards intact; another executable needs its own reverse-engineering work. Compatibility with other Windows versions is unverified.

You supply a complete, already-working game installation. This repository includes no game executable, `steam_api.dll`, installer, fastfile, extracted full game script, save or other game asset.

Missing or unreadable `steam_api.dll` stops the launcher's preparation preflight. See [dependency troubleshooting](docs/TROUBLESHOOTING.md); a file hash match is a consistency check, not an antivirus clearance.

## Build from source

Requirements:

- 64-bit Windows with its x86 system `winmm.dll`.
- PowerShell and Python 3.11 or newer.
- LLVM-MinGW with `i686-w64-mingw32-clang.exe`.

The development toolchain was [LLVM-MinGW 20261006 / LLVM 23.1.3](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006). The official `llvm-mingw-20261006-ucrt-x86_64.zip` has SHA-256:

```text
317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a
```

Download/extract the compiler separately, then run from the repository root:

```powershell
$cc = 'D:\Tools\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-clang.exe'
.\build.ps1 -Compiler $cc
```

Alternatively set `MW2TRIO_CC` or put that compiler on `PATH`. The scripts do not install or download tools. `build.ps1` reads the local Windows export table, generates x86 forwarding stubs and builds `build/winmm.dll` plus `build/bootstrap-helper.exe`. It does not install anything into a game.

Generated output and local test material are ignored by Git. No opt-in flag is created by the build.

## Run the helper tests

```powershell
.\tests\run-tests.ps1 -Compiler $cc
```

Optional: also verify an exact target executable as a read-only file:

```powershell
.\tests\run-tests.ps1 -Compiler $cc -GameExe 'D:\MW2-Test\iw4sp.exe'
```

These commands compile and run only the repository's own helper programs. They do not launch, attach to or patch a game, and they send no network packets. The optional game argument is opened for hashing and PE inspection only. Receipts go under `build/tests/<run>/`.

The current native development suite passed **72 helper/build cases**, extending the historical 48-case publication-stage suite. Coverage includes export/identity checks, parsers, transaction/protection checks, unsupported-process cases, exact-peer configuration/transport fixtures and the offline policy's seven broken variants. These are component tests; the recorded real-game movement tests are separate evidence.

Run the public Python launcher and read-only QA reader fixtures separately:

```powershell
python -B -m unittest discover -s tests -p 'test_*.py'
```

The current Python suite passed **140 fixtures**, including twelve dependency-preflight cases. These use synthetic data and fake process handles. They do not launch a game or establish cross-PC compatibility.

## Optional single-player component experiment

This procedure reproduces a developer experiment; it is not a multiplayer installation guide. Use a full, independent test copy and keep your original installation and saves separate. Do not use hard links for writable files. Do not overwrite an existing mod proxy without preserving it first.

1. Make a full test copy of the exact working game. Omit the original `players`/save folders so the test copy starts with its own profile.
2. Verify the copied `iw4sp.exe` hash above. Close the test game.
3. Back up any existing `winmm.dll` in that test copy. Copy the newly built `build/winmm.dll` beside the copied executable.
4. To check only forwarding/menu startup, launch with no experiment flags.
5. To reproduce the bounded trainer experiment, follow the opt-in example below, launch the copied executable, collect its own logs and close it through its normal quit path. Do not leave the flags enabled after testing.

For the opt-in example, replace the path with your independent test folder. Start with no existing `mw2trio` folder, or preserve it before copying content:

```powershell
$testCopy = 'D:\MW2-Trio-Test'
$expected = '4DD53FABF9677980AAF081009584ADE99D6BF440330046E5C8950E87EF0547D9'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $testCopy 'iw4sp.exe')).Hash -ne $expected) {
    throw 'Unsupported executable; stop here.'
}
if (Test-Path -LiteralPath (Join-Path $testCopy 'mw2trio')) {
    throw 'Preserve existing test content before continuing.'
}
Copy-Item -LiteralPath '.\content\framework' -Destination (Join-Path $testCopy 'mw2trio') -Recurse
foreach ($name in @('mw2-trio-control.enable', 'mw2-trio-storage-test.enable',
                    'mw2-trio-cache-test.enable', 'mw2-trio-scripts-test.enable')) {
    Set-Content -LiteralPath (Join-Path $testCopy $name) -Value 'offline component test only' -Encoding Ascii
}
@'
set developer 1
set logfile 2
set snd_volume 0
set com_maxfps 20
set coop 0
set specialops 0
set mw2coop_qa ""
map trainer
'@ | Set-Content -LiteralPath (Join-Path $testCopy 'control-command.txt') -Encoding Ascii
```

Launch only the copied executable. Expected evidence is `trainer`, `sv_running=1`, `coop=0`, one actual script player, intact storage/cache guards, and cache `allocation_count=3` / `traversal_count=2`. Relevant files beside the test executable are `startup184.log`, `control184.log` and `scripts184.log`. See [architecture](docs/ARCHITECTURE.md) for the flags and failure behavior.

The mailbox is plain bounded ASCII consumed by this prototype on the main game thread. After observing the test, queue normal exit:

```powershell
Set-Content -LiteralPath (Join-Path $testCopy 'control-command.txt') -Value 'quit' -Encoding Ascii
```

Once the owned test game has exited, remove the four `.enable` files. To remove the prototype, restore the saved proxy or remove only the prototype's `winmm.dll`, its `mw2trio` content and its own logs from that independent copy. Keep those backups until you have checked the result.

## Known limitations

- Expansion beyond two requires coordinated changes to native channels, server records, fixed-count helpers, session membership and private wire framing. Lobby count edits alone are unsafe.
- The native cache reserves three records while traversing two. The third slot is deliberately unavailable to the original member API.
- The upstream framework still assumes pairs in revive, cameras, vehicles and mission resources. Merely replacing `2` with `3` would break unrelated NPC and engine logic.
- The two-player campaign result covers the trainer's `pit` start, independent native movement and matching bounded position samples. Aim/fire/use, death/revive, checkpoints, other missions and full-campaign behavior remain unverified.
- The exact-peer flow has only been exercised between processes on one PC. Cross-PC compatibility, NAT traversal, packet-loss recovery and cross-peer version/settings agreement remain open.
- The offline roster model is not wired into the game. Its hashes check snapshot agreement; they are not authentication.
- All runtime modifications are experimental and remain opt-in, with strict build/phase/thread/signature guards and process-lifetime storage.

## Contribute

Start with [architecture](docs/ARCHITECTURE.md), [roadmap](docs/ROADMAP.md), [test requirements](docs/TESTING.md) and [contribution guidelines](CONTRIBUTING.md). The immediate targets are separate-PC automated tests, native aim/fire/use and reproducible two-player progression. Useful contributions also include exact native reference audits, bounded state-machine tests and the coordinated work needed for three-client sessions.

Please report the version, executable hash, test scope and minimal reproduction. Keep game files, full extracted/decompiled sources, saves, credentials and personal filesystem paths out of issues and pull requests.

## License

Own source and adapted MIT components are distributed under the [MIT license](LICENSE), with upstream notices retained. Small game-script excerpts in patch contexts remain the property of their respective owners. See [CREDITS.md](CREDITS.md) and [third_party](third_party/) for provenance and license scope.
