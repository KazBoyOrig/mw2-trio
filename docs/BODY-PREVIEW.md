# Trainer body preview — October 25, 2026

The [standalone experiment](../experiments/trainer-body/README.md) adds genuine
third-person teammate bodies to one exact Russian MW2 (2009) x86 build184
campaign `trainer`/`pit` configuration. The root source and launcher retain their
earlier two-player baseline. Both paths remain developer prototypes.

## Observed results

| Gate | Actual result | Limit |
|---|---|---|
| Genuine body/head resource registration | PASS on both instances | Stock engine registration, same PC |
| Ordinary V11 player assignment | PASS | Genuine 90-bone body, 24-bone head and 17-bone M4; matching ownership |
| Controlled V13 idle visual check | PASS for recorded frames | Full remote body/head/weapon; one-time QA placement; some occlusion |
| Latest candidate native movement | Overall FAIL | All seven phases completed; six passed strict scoring, one retained an old neutral command at its first interior boundary |
| Later input-controller activation gate | Offline PASS; game untested | Waits for native server/applied axes on both copies/all slots before measurement, within the original three-second deadline |
| Locomotion/firing animation and separate-PC body candidate | Unverified | Earlier baseline/Radmin connectivity does not validate the body candidate |
| Revive, checkpoints, transitions and full campaign | Unverified | No progression claim |
| Three/four-player gameplay | Not implemented | Still future work |

The body candidate does not replace players with NPCs or static follower props.
It uses stock co-op player assets through the existing map loader, preserving
campaign map/script selection. Exact executable/resource/code/import checks,
owner-thread/load-generation checks and fresh two-phase readiness proofs guard
the path. The initial worker wait is bounded to one ten-second attempt; pending
completion is never accepted. All resource/assignment/visual runs used isolated
copies and restored managed files after the owned processes stopped.

The latest native movement failure remains a failure. One old neutral command
was captured at the inclusive first 200ms boundary of client-right movement.
Shared-map state, measured cross-copy positions and the other six phases passed.
The later controller gate distinguishes enqueue acknowledgment from actual
native command application. It keeps the original measurement durations,
three-second deadline, 200ms edges and all scoring limits. Its offline tests
do not establish a passing game run. See [sanitized evidence](evidence/trainer-body-preview-2026-10-25.json).

## Build and use the experiment

The GitHub source snapshot contains no compiled game/runtime dependencies or
game assets. Use an already installed compatible x86 LLVM-MinGW compiler:

```powershell
# From the repository root:
Set-Location experiments/trainer-body
$cc = 'D:\Tools\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-clang.exe'
.\tests\run-tests.ps1 -Compiler $cc -Python python
python -B -m unittest discover -s tests -p 'test_*.py'
python -B tools/play_pair_body.py --help
```

The helper runner builds only own programs and runs no game. The body launcher
pins the actually tested DLL, its sibling launcher, the own V11 script and the
complete content manifest. A different build is refused. Do not change a pin
merely to bypass a refusal; compiler/system-export differences require review
and a new validation record.

The experiment build sets `SOURCE_DATE_EPOCH=1792922961` only for its own build
and restores the caller's environment afterward. This reproduces the tested
DLL's two link-time timestamps with identical compiler/export/source inputs.
The publication build reproduced the exact tested DLL hash, rather than changing
the launcher's pin to accept a newly timestamped file.

Publication checks passed 80 native helper/build cases and 172 Python fixtures
in this experiment, plus the root baseline's separate 140-test suite. These
overlap earlier component fixtures and are not additional gameplay tests.

Follow [the host/client guide](../experiments/trainer-body/README.md) from the
experiment directory. Commands default to PLAN_ONLY. Reuse an eligible existing
independent copy when checks pass; otherwise allow roughly the full game's disk
space per new copy. Existing different `mw2trio` content is preserved and refused.
Do not mix baseline and experimental frameworks, DLLs or launchers. A full real
paired session through the body wrapper itself remains unverified; the real
body results above came from the private paired harness.

The frozen content manifest's historical `UNRUN` labels and the launcher's
`PRIVATE_EXPERIMENTAL` identifier predate this publication. They remain unchanged
to preserve exact tested bytes; this report supplies current results. Private
receipts, paths, network identifiers, game files and full extracted scripts are
not published. The fourteen private fixture-dependent contract tests are not
included in the portable source snapshot.

Credits to [cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything)
for the playbook and [stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop) for the
pinned MIT framework remain in [CREDITS.md](../CREDITS.md) and bundled notices.
