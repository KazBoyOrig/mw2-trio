# Experimental body preview: validation boundaries

This is an experimental source snapshot dated October 25, 2026. It targets only
the exact Russian x86 build184 campaign `trainer`, starting at `pit`, with two
players. It does not establish complete campaign, other-language, three-player
or four-player compatibility.

| Check actually run | Result | Boundary |
|---|---|---|
| Strict build and native helper runner | 80 cases PASS | Own helper processes and synthetic memory; no gameplay assertion |
| Independent native phase/guard review and worker-wait fixtures | PASS | Overlapping component checks, not additional gameplay cases |
| Paired resource-only game test | PASS | Both genuine body/head headers registered through the stock engine |
| Normal V11 assignment in two game instances | PASS | Body90/head24/M4(17), total131 bones, matching server/client ownership |
| Controlled V13 idle visual check on both instances | PASS for recorded idle frames | Complete remote body/head/weapon; one-time QA placement used; some details occluded |
| Latest isolated movement test | Overall FAIL | All seven phases complete within three seconds; six pass strict scoring, one retains an old neutral command at its first interior boundary |
| Subsequent controller activation gate | 121 offline tests PASS | Waits for both copies/all slots to show expected native producer/applied axes before measurement; game execution is unverified |
| Body launcher bundle and host/client commands | PLAN_ONLY PASS | Existing independent prepared copies; no game/network started by these commands |
| Body wrapper fixtures | 13 tests PASS | Synthetic files and process doubles |
| Publication source rebuild | 80 helper/build cases PASS | Reproduced the exact frozen DLL using a fixed build epoch |
| Published experiment Python suite | 172 tests PASS | Includes portable controller and explicit V11 profile fixtures; no game launch |
| Separate-PC body preview, firing, restart, checkpoints and progression | Unverified | Earlier baseline two-PC success does not establish these candidate results |

The latest movement test's releases were acknowledged by both game instances.
Both games exited normally with code0; the harness restored managed files and
the original executable hashes were unchanged. The strict shared-map check and
all measured cross-copy position checks passed. One client-right sample still
contained the old neutral command at the inclusive first 200ms boundary; the
overall movement score remains FAIL. Earlier watchdog failures are retained.

The private controller now parses the full diagnostic window in one pass, about
27% faster on the saved windows. It also waits for the expected native server
and applied axes on both copies and both slots before recording the measurement
start. Activation, the unchanged one/two-second measurement and releases share
the original three-second deadline. No duration, sample edge, scoring threshold
or failed receipt was relaxed. The 121 offline controller/scorer tests include
delayed application, never-applied commands and late activation exhausting the
original deadline. This subsequent controller change has not had a game test;
it is not evidence of successful native movement or rendered animation.

The native worker wait is bounded to one ten-second attempt during the initial
owned resource query. It does not accept pending worker completion. All exact
executable, resource, source, phase, thread and generation checks remain required.
The measured successful wait is not a universal engine-progress guarantee.

The bundled helper receipt belongs to DLL SHA-256
`be51d30339edd669e67525ec0f0fada4f481d09ae56f8af509248fb181c37f7b`.
The launcher also pins its own sibling launcher, content manifest and V11 probe.
Rebuilding can produce a different DLL; that output must be reviewed and tested
before changing the pins. Never replace hashes simply to bypass a refusal.

Own source, native helper fixtures and portable Python tests are included. The
private workspace's 155-test Python run also included source/executable contract
checks requiring private research fixtures; that fixture-dependent test module
is deliberately excluded from this portable package. Its count is not the
portable package's test count. The native helper runner can be invoked with an
already installed compatible x86 LLVM-MinGW compiler; no toolchain is bundled:

The published source adds the controller and explicit body-input profile
fixtures. Its 172 tests include 66 shared-map, 32 input-reader, 30 launcher,
13 body-wrapper, 23 dispatcher and 8 profile fixtures. The root baseline's
140-test suite is separate and overlaps these fixtures. Do not add these counts
as distinct gameplay tests. The publication rebuild uses a fixed
`SOURCE_DATE_EPOCH` and restores the caller's environment, reproducing the exact
tested DLL when compiler, system exports and sources match. See the repository's
`docs/BODY-PREVIEW.md` for the distinction between baseline and experiment.

```powershell
./tests/run-tests.ps1 -Compiler "C:\Toolchains\llvm-mingw\bin\i686-w64-mingw32-clang.exe" -Python python
python -B -m unittest discover -s tests -p "test_*.py"
```

Next gates are reliable bounded movement measurements, rendered locomotion and
firing, repeated load/restart and cleanup, then campaign interactions, revive,
checkpoints and transitions. Wider player counts follow those two-player gates.
No game assets, Steam API library, private logs or account credentials belong
in this package. Credits and upstream MIT notices are retained in `CREDITS.md`
and `third_party/`.
