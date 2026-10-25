# Two-player developer launcher — 0.0.1-dev

This page describes the baseline launcher at the repository root. The separate
[body preview](BODY-PREVIEW.md) has its own source, framework and pinned launcher
under `experiments/trainer-body`; follow that guide for the opt-in body candidate.

This experimental launcher targets **campaign `trainer`, start `pit`, only** on the
exact x86 build184 listed in [README](../README.md#exact-target). It is not a playable
release, full campaign port or automatic mission progression system. Bounded
native movement QA passed through loopback and exact-peer transport, using two
independent copies on one PC. A separate end-to-end run of this public launcher
also passed: both roles became ready, loaded campaign trainer, advanced both
slots' native clocks by over ten seconds and exited normally with clean file
restoration. The owner later reported a successful trainer session on two PCs
over Radmin VPN, with invisible teammate bodies. That manual result does not
establish automated cross-PC gameplay agreement or campaign progression.
Integration results and the body investigation are recorded in [TESTING](TESTING.md).

The workflow follows **[cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything)**.
The staged MIT campaign framework is credited to [stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop).
The launcher ships project code only. Supply your own complete, working game and
dependencies; it does not download, replace missing game dependencies, change
security/firewall settings, configure a VPN, or access an account/provider API.

If `steam_api.dll` is missing or blocked, preparation stops at the dependency
preflight. See [dependency troubleshooting](TROUBLESHOOTING.md). Before launch,
the dependency's full hash must also match the prepared-copy inventory.

## Build and check first

Build the DLL and run the public helper suite using the [README instructions](../README.md#build-from-source).
The launcher accepts only this checkout's `build/winmm.dll`, with a SHA-256 matching
a passed `build/tests/*/results.json` receipt. It verifies the x86 PE32 DLL header
and every staged framework file against `content/framework/manifest.json`.
Rebuild and rerun helpers together when code changes. These checks establish build
and component provenance, not multiplayer compatibility.

An explicitly labelled developer bundle may already contain the tested own DLL,
its original helper receipt and `CANDIDATE-MANIFEST.json`. Preserve the included
licenses and verify the recorded file hashes. You can use that unchanged tested
artifact without rebuilding; source changes require a new build and helper run.

Run launcher-specific tests separately; they use synthetic files, telemetry and
fake process handles, without a game or network traffic:

```powershell
python -B -m unittest discover -s tests -p 'test_*.py'
```

## Prepare independent copies

Every command defaults to **PLAN_ONLY**: it reads and validates, then prints a plan.
Only adding `--execute` permits copying or game execution. Use absolute paths.

```powershell
python tools/play_pair.py --prepare-copy 'D:\Games\MW2-Source' 'D:\MW2-Dev-Host'
python tools/play_pair.py --prepare-copy 'D:\Games\MW2-Source' 'D:\MW2-Dev-Host' --execute
```

Each player prepares their own copy on their own computer. The destination must
not exist and must be separate from the source, with neither path nested inside
the other. Preparation streams a full independent data copy, verifies each copied
file's hash, and refuses reparse points. It excludes profiles, saves, logs, existing
prototype content/proxies and opt-ins. It creates no hard links and writes nothing
to the source. A private provenance marker is written last; an interrupted partial
copy is left unmarked and cannot be launched by this tool. Do not delete the source.

**Disk space:** each tested independent game copy occupies about **12.7 GB**.
Two copies for same-PC regression tests therefore need about **25.5 GB**, plus
build tools, temporary files and diagnostic logs. Each additional test copy costs
another full game's worth of storage. On separate PCs, each participant needs
one independent copy. The mod's developer archive itself is under 1 MB; game
data accounts for almost all of the copy size. Check free space before preparing
another copy, and retain any existing test material until its owned game has
stopped and its recovery state has been inspected.

Play accepts only a root carrying that marker, with the exact executable hash,
matching inventory file sizes and independent files. Existing process markers or pending mailboxes cause
refusal. The marker is a local safety boundary, not an authenticity signature or
permission to use an incomplete/nonworking installation.

## Agree the exact endpoints

Supply your own local IPv4 address and the friend's IPv4 address as visible on an
already configured LAN/VPN. Use a local port owned by each copy and cross-match the
peer port with the friend's local port. No DNS names or automatic discovery are used.
The conservative parser refuses leading zeros, loopback, link-local, multicast/
reserved addresses and addresses ending `.0` or `.255`. Native code additionally
verifies actual adapter ownership, subnet host eligibility and the socket's exact
bind; passing the parser alone is not connectivity proof. The tool changes no
adapter, router, VPN or firewall configuration.

Choose the same **nonzero 32-hex-digit session seed** on both sides. It derives
role-distinct cached participant IDs; it is **not a password, authentication key or
peer-authentication protocol**. The native high identity DWORD is retained and the
provider/account interface is untouched. The cached value affects shared native
local/remote classification and other consumers; this is a development identity
flow, with saves/progression unverified. Do not publish private configuration/logs.

The two configurations must mirror each other. Example addresses below are
placeholders to replace with your actual eligible LAN/VPN addresses:

| Argument | Host | Client |
|---|---|---|
| `--role` | `host` | `client` |
| `--local-ip` | `192.168.50.10` | `192.168.50.11` |
| `--peer-ip` | `192.168.50.11` | `192.168.50.10` |
| `--local-port` | `28960` | `28961` |
| `--peer-port` | `28961` | `28960` |
| `--session-seed` | same agreed 32-hex value | same agreed 32-hex value |

## Plan, then explicitly execute

Host, from this repository's root:

```powershell
python tools/play_pair.py --role host --test-copy 'D:\MW2-Dev-Host' --local-ip 192.168.50.10 --peer-ip 192.168.50.11 --local-port 28960 --peer-port 28961 --session-seed 1234567890abcdef1234567890abcdef
```

Client, using its own prepared copy and mirrored values:

```powershell
python tools/play_pair.py --role client --test-copy 'D:\MW2-Dev-Client' --local-ip 192.168.50.11 --peer-ip 192.168.50.10 --local-port 28961 --peer-port 28960 --session-seed 1234567890abcdef1234567890abcdef
```

Review each plan, then repeat with **`--execute`**. The example seed is public test
data, not a secret. Use matching source builds/content on both sides; no automatic
cross-peer version, seed or settings agreement is implemented.

The launcher backs up its managed proxy, flags, peer configuration, logs and private
copy's configuration files before changing them. Foreign proxies and differing
existing `mw2trio` content are refused. Exact existing framework content is left
in place; otherwise verified own content is staged temporarily. Control, network,
party, peer, runtime and source-loader opt-ins are enabled. Loopback-QA, storage,
cache and dense-input experiments stay off. Sound volume is left unchanged.

The owned game receives ordinary `+set net_ip`/`+set net_port` startup arguments,
then bounded main-thread commands after readiness. Ordinary settings and command
acceptance are **not** socket-bind proof: a native `lan_open` refusal is an error,
never something to work around with raw memory writes, flag changes or assumptions
about restart behavior. The launcher stops at a contradictory native diagnostic.

The order is identity setup before socket/party use, exact endpoint setup, native
socket open, host creation or client join/connect, then stable admitted membership.
Both sides reassert campaign mode, `specialops=0`, `coop_mapName=trainer`, `start=pit`,
god mode off and empty QA commands after admission. Exact mailbox byte/count ACKs
are required. Fresh native telemetry confirms `sv_running=0`, `coop=1`,
`specialops=0`, `game_mode=2` and `coop_map=trainer` before local readiness. The
start/god/QA commands are acknowledged but do not have separate telemetry readback;
this is not full settings agreement or proof of a reached script start point.

Admission alone is too early for that reassertion. Each role first waits for its
own fresh native invite-UI transition to `game_mode=2`, alongside stable admitted
membership. That asynchronous transition can reset `specialops` to 1; settings
queued before it can be refused or overwritten. Only afterward does the launcher
send the campaign override and require two seconds of fresh matching settings.
If the UI transition or the campaign settings never settle, setup fails without
requesting a launch.

**Manual readiness is required.** The client prints `READY` only after its own
campaign settings gate. The host waits for its own gate, then asks you to confirm
that the friend printed `READY` before pressing Enter. This human confirmation is
a developer launch step, not a tested cross-peer settings handshake or a gameplay
test. While waiting for Enter, the launcher continues observing its own process
and diagnostic failures; EOF refuses confirmation. The host rechecks its own fresh
state before requesting native launch.

The launcher does not drive movement, revive, checkpoints, mission changes or a
full playthrough. It cannot promise that different machines will connect. A loaded
world, two entities or advancing command times alone do not establish independent
control. Unsupported behavior belongs in a private development report.

## Exit and recovery

Exit the owned game normally. Ctrl+C in the launcher requests normal quit and waits
up to eight seconds; if necessary it terminates **only its retained own process
handle**, then waits boundedly. It never searches processes by name/PID or stops a
different game. Managed files and modes are restored only after that process has
stopped. Newly staged, unchanged own framework content is removed; unexpected
content changes are preserved for inspection instead of recursively deleted.

Private logs, backups and a receipt remain under ignored `local/play-pair/`. A crash,
forced stop or restoration problem must not be reported as a gameplay PASS. If the
owned process cannot be confirmed stopped, the launcher leaves the active marker
and backups and does not restore a live proxy. Restoration/verification failures
also retain the marker for inspection. Inspect that private receipt and
resolve the owned process before recovery; do not delete a marker simply to bypass
the refusal. Configuration is scoped to the independent copy, and the original
installation and original saves remain untouched.
