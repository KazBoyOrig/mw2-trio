# Experimental trainer body preview

From a source checkout, first build and run the native helpers with an existing
compatible x86 compiler. See the [repository body-preview guide](../../docs/BODY-PREVIEW.md)
for the complete source-build procedure and current sanitized results. Commands
below run from this experiment directory. The frozen `PRIVATE_EXPERIMENTAL`
profile name and manifest history are internal identifiers retained unchanged.

This is an opt-in developer candidate for two players in the Russian MW2 (2009)
x86 build184, campaign `trainer`, `start=pit` only. It is not a public release or a
whole-campaign fix. Credit: the original co-op research/playbook is
[cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything); the
framework retains the pinned MIT co-op scripts and their upstream attribution.

This directory is self-contained. The wrapper uses its pinned sibling
`play_pair.py`; the baseline launcher at the top of the repository is separate. It pins DLL SHA-256
`be51d30339edd669e67525ec0f0fada4f481d09ae56f8af509248fb181c37f7b`, the private
framework manifest and own V11 probe. A different build is refused. The exact game
executable SHA-256 is
`4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9`.

Bounded private same-PC tests passed native resource registration, normal V11
assignment and V13 idle visual setup with both full bodies visible. V13 includes
QA placement; this launcher serves normal V11 without placement. The latest
isolated movement run completed all seven phases within the controller's
three-second limit, but strict scoring rejected one old neutral command at the
start of the client-right phase. Six phase scores passed; overall movement is
still FAIL. A subsequent controller activation gate passed offline tests and
has not yet run in the game. Animation validation is still pending. Separate-PC body
rendering, mission progression and three-player support are unverified. A
`body=assigned` status alone does not prove a correct rendered pose.

The wrapper's exact bundle validation and host/client PLAN_ONLY commands passed
against existing prepared copies. Its own 13 synthetic wrapper tests passed.
Real body tests used the private paired harness; this wrapper has not yet had a
complete real-game host/client run. See [validation](docs/BODY-PREVIEW-VALIDATION.md)
for the boundaries of these results.

Use Python 3.11+ on Windows. No game assets or `steam_api.dll` are supplied. A
complete trusted installation with its own readable dependency and the exact
supported resource files is required. No antivirus/firewall/security settings are
changed. If Windows blocks a dependency, stop and inspect the detection or repair
the installation from a trusted source; the launcher does not restore or replace
that dependency. Do not point this tool at the original installation for play.

Run commands from the extracted package directory, where `tools` is present.
An existing independent copy prepared with the launcher can be reused if its
provenance and content checks pass. Otherwise prepare one copy per PC; this uses
roughly the full game's disk space on each PC. Without `--execute` preparation
only prints a plan. Destination must be new, absolute and outside the source directory:

```powershell
python tools/play_pair_body.py --prepare-copy "C:\MW2-Source" "C:\MW2-BodyTest"
python tools/play_pair_body.py --prepare-copy "C:\MW2-Source" "C:\MW2-BodyTest" --execute
```

On each PC, replace the example addresses with its actual eligible IPv4 endpoint
(LAN or already configured private tunnel). Both players use the same nonzero
128-bit session seed, which is an experiment identifier, not a password or account
credential. Reverse the local/peer addresses and ports for the friend:

```powershell
# Host: plan only; add --execute after reviewing the plan.
python tools/play_pair_body.py --role host --test-copy "C:\MW2-BodyTest" --local-ip 192.168.50.10 --peer-ip 192.168.50.11 --local-port 28960 --peer-port 28961 --session-seed 1234567890abcdef1234567890abcdef

# Friend: plan only; add --execute after reviewing the plan.
python tools/play_pair_body.py --role client --test-copy "C:\MW2-BodyTest" --local-ip 192.168.50.11 --peer-ip 192.168.50.10 --local-port 28961 --peer-port 28960 --session-seed 1234567890abcdef1234567890abcdef
```

Start the host, then the client. The client prints `READY` only after its native
campaign settings gate. The host presses Enter only after the friend reports that
message; no cross-peer settings agreement is implemented. Both copies must pass
the native invite/menu transition before the campaign settings are reasserted.

The body wrapper adds only `mw2-trio-body-assets-test.enable` to the CLI's existing
control/network/party/peer/runtime/scripts flags. It enables no QA alias,
storage/cache expansion, input bot or QA placement. Native source selection and
fresh resource proofs gate the genuine stock body/head and `%code` path; missing
proof must not be worked around. Existing `mw2trio` content must exactly match
this private manifest or launch is refused without overwriting it.

Exit normally, or press Ctrl+C in the launcher for bounded `quit` then termination
of its retained game handle if necessary. Proxy, flags, configurations and logs
are backed up and restored only after that game stops. If the launcher staged a
previously absent framework, it removes only the unchanged owned tree afterward.
Saves are excluded from copy preparation; newly created private saves are not
deleted. Private receipts and captured logs, including `bodyassets184.log`, are
under `local/play-pair/` in this private project. They may contain local paths or
network identifiers; do not publish them.
