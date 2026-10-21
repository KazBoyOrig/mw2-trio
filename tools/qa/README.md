# Saved-receipt QA readers

These standard-library Python 3.10+ tools read previously saved diagnostic receipts. They do not launch or control a game, send packets, change a receipt, or generate input. `-B` also prevents Python bytecode files.

Run from the repository root:

```text
python -B tools/qa/score_shared_map.py --receipt "<private-receipt-directory>" --map trainer --specialops0
python -B tools/qa/score_independent_inputs.py --receipt "<private-receipt-directory>" --events "<private-receipt-directory>/input-events.json"
```

Both commands print JSON to standard output and return 0 for a passing result or 1 for a failure. The shared-map reader checks the exact supported executable identity, clean owned-process exit, late native party/channel state and advancing command application on both copies. The input reader additionally requires the saved seven-phase dispatch/release ledger, sufficiently dense matching game-time samples, expected owner input axes and bounded matching world movement. It refuses sparse or ambiguous evidence. A capacity value or successful map load alone is insufficient.

The receipt directory must contain `result.json` and each role's saved `host/after/control184.log` and `client/after/control184.log`. Exact-peer cleanup-boundary validation additionally reads each role's saved `party184.log`, `network184.log`, `mw2-trio-peer.cfg` and explicit peer flag; a saved QA flag conflicts with that mode. The input command also reads the explicit events ledger. These are private inputs: do not commit raw receipts, network logs, configurations, participant identities or installation paths. No real receipt or private fixed-directory test runner is included here.

Cleanup can create expected disconnect records after observation ends. The readers accept the narrow `runner_precleanup` contract only when the saved own runner evidence supports one common same-PC native clock: both owned roles/PIDs, readiness, exact mailbox acknowledgments and clean cleanup. Exact-peer mode additionally requires opposite role configurations with one seed, reversed exact endpoints on one numeric local IP, native cached-participant/opener evidence and the exact-peer boundary marker. This is receipt provenance, not authentication. It does not infer synchronized clocks on separate computers. The optional historical `--derive-own-quit-cutoff` switch is restricted to the known legacy same-PC loopback evidence; exact-peer mode requires its explicit marker.

Run only synthetic fixture tests, including the launcher's process doubles:

```text
python -B -m unittest discover -s tests -p "test_*.py"
```

The two reader suites contain 66 and 32 tests; the launcher suite contains 30, for 128 public Python fixtures. Fixture addresses, participant labels, clocks and input samples are invented data. Tests launch no game and send no packets. A passing receipt score covers only its saved scenario. It does not establish complete campaign progression, stability across all missions, separate-PC play, or whole-process network confinement.
