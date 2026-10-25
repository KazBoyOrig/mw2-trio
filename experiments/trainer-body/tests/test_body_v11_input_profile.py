"""Own fake-artifact/profile checks; no game, process, network or input actions."""
import copy
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/qa"))
import unittest
from unittest import mock

import score_shared_map as shared
import score_independent_inputs as inputs
from test_score_shared_map import make_log


def fixture():
    source = Path(__file__).resolve().parents[1] / "content/framework/scripts/maps/_mw2trio_probe.gsc"
    probe = source.read_bytes()
    assert hashlib.sha256(probe).hexdigest() == shared.BODY_V11_PROBE_SHA
    dll = b"own synthetic DLL fixture; never loaded or executed"
    dll_sha = hashlib.sha256(dll).hexdigest()
    roles = ("host", "client")
    flags = ["mw2-trio-control.enable", "mw2-trio-network-test.enable", "mw2-trio-party-test.enable",
             shared.QA_FLAG, "mw2-trio-two-player-test.enable", "mw2-trio-scripts-test.enable",
             "mw2-trio-input-test.enable", "mw2-trio-body-assets-test.enable"]
    receipt = {"exe_sha256": shared.EXE_SHA, "dll_sha256": dll_sha, "dll_profile": "body-assets",
               "error": None, "forced_cleanup": [], "exit_codes": {r: 0 for r in roles},
               "all_owned_processes_stopped": True, "executable_hashes_unchanged": True,
               "scenario_evidence_pass": True, "steps_sent": [], "pids": {"host": 100, "client": 101},
               "scenario_clock_started_after_all_ready": True,
               "scenario": {"instances": list(roles), "flags": flags, "dll_profile": "body-assets",
                            "boot_extra": shared.BODY_V11_BOOT_EXTRA, "show_owned_windows": False,
                            "probe_candidate": "stock-body-v11", "probe_override_sha256": shared.BODY_V11_PROBE_SHA,
                            "input_plan": {"version": 1, "map": "trainer", "specialops": 0, "pattern": "both-return"}},
               "content_override": {r: {"source_sha256": shared.BODY_V11_PROBE_SHA,
                   "files": {shared.BODY_V11_PROBE_PATH: {"staged": shared.BODY_V11_PROBE_SHA}}} for r in roles},
               "content_override_restored": {r: True for r in roles},
               "observation_end": {"version": 1, "source": "runner_precleanup", "before_any_quit": True,
                                   "global_tick": 120080, "last_control_ticks": {r: 120000 for r in roles},
                                   "mailboxes_empty": {r: True for r in roles}}}
    logs = {}
    for role in roles:
        logs[role] = ("tick=99000 tid=1234 mailbox enqueued count=1 bytes=%d; execution requires native state evidence\n"
                      % len(shared.BOOT_COMMAND + shared.BODY_V11_BOOT_EXTRA)) + make_log(role)
        if role == "host":
            logs[role] += "tick=121000 tid=1234 frame=100 enqueued_mailboxes=1 map=trainer sv_running=0 coop=1 specialops=0\n"
        logs[role] += ("tick=%d tid=1234 mailbox enqueued count=2 bytes=5; execution requires native state evidence\n"
                       % (121200 if role == "host" else 120100))
    return receipt, logs, {r: {"winmm.dll": dll, shared.BODY_V11_PROBE_PATH: probe} for r in roles}, dll_sha


def evaluate(value, profile=shared.BODY_V11_INPUT_PROFILE, map_name="trainer", specialops=0):
    receipt, logs, artifacts, expected_dll = value
    own_path = Path(__file__)

    def read(path, _limit, _root=None):
        if path == own_path:
            return json.dumps(receipt).encode()
        role = "host" if "host" in path.parts else "client"
        if path.name == "control184.log":
            return logs[role].encode()
        name = shared.BODY_V11_PROBE_PATH if path.name == "_mw2trio_probe.gsc" else path.name
        return artifacts[role][name]

    with mock.patch.object(shared, "BODY_V11_DLL_SHA", expected_dll), mock.patch.object(shared, "bounded_read", side_effect=read):
        return shared.score_receipt(own_path, map_name, specialops, profile=profile)


class BodyV11InputProfileTests(unittest.TestCase):
    def test_pins_and_positive_exact_boot_cutoff(self):
        self.assertEqual(shared.BODY_V11_DLL_SHA, "be51d30339edd669e67525ec0f0fada4f481d09ae56f8af509248fb181c37f7b")
        self.assertEqual(shared.BODY_V11_PROBE_SHA, "7fab32a1a26b16d083036766949a249c0bfdb1dc24a62fe0795e1b0c4aee9722")
        f = fixture(); before = copy.deepcopy(f); result = evaluate(f)
        self.assertTrue(result["pass"], result)
        self.assertEqual(f, before)
        self.assertEqual(result["input_score_profile"], shared.BODY_V11_INPUT_PROFILE)
        self.assertEqual(result["profile_boot_bytes"], len(shared.BOOT_COMMAND + shared.BODY_V11_BOOT_EXTRA))
        self.assertTrue(result["observation_boundary"]["applied"])
        self.assertEqual(result["observation_boundary"]["cutoff_tick"], 120080)
        self.assertFalse(result["input_independence_proven"])

    def test_missing_or_unknown_profile_cannot_accept_extra_boot(self):
        self.assertFalse(evaluate(fixture(), profile=None)["pass"])
        with self.assertRaises(ValueError):
            evaluate(fixture(), profile="generic-override")

    def test_exact_boot_settings_and_window_scope_required(self):
        for change in (lambda r: r["scenario"].update(boot_extra="set com_maxfps 60\n"),
                       lambda r: r["scenario"].update(boot_extra=shared.BODY_V11_BOOT_EXTRA + "set ownfixture 1\n"),
                       lambda r: r["scenario"].update(show_owned_windows=True),
                       lambda r: r["scenario"]["flags"].remove("mw2-trio-body-assets-test.enable"),
                       lambda r: r["scenario"]["flags"].append("mw2-trio-peer-test.enable")):
            f = fixture(); change(f[0])
            with self.assertRaises(ValueError): evaluate(f)
        with self.assertRaises(ValueError): evaluate(fixture(), specialops=1)
        with self.assertRaises(ValueError): evaluate(fixture(), map_name="so_killspree_trainer")

    def test_dll_and_probe_metadata_pins_and_restoration_required(self):
        for change in (lambda r: r.update(dll_sha256="a" * 64),
                       lambda r: r["scenario"].update(probe_override_sha256="a" * 64),
                       lambda r: r["scenario"].update(probe_candidate="stock-body-v13-qa"),
                       lambda r: r["content_override"]["client"].update(source_sha256="a" * 64),
                       lambda r: r["content_override"]["host"]["files"].clear(),
                       lambda r: r["content_override_restored"].update(client=False)):
            f = fixture(); change(f[0])
            with self.assertRaises(ValueError): evaluate(f)

    def test_actual_saved_bytes_not_just_reported_hashes(self):
        for role in ("host", "client"):
            for name in ("winmm.dll", shared.BODY_V11_PROBE_PATH):
                f = fixture(); f[2][role][name] += b"wrong"
                with self.assertRaises(ValueError): evaluate(f)

    def test_ack_byte_count_and_final_quit_still_exact(self):
        first = "count=1 bytes=" + str(len(shared.BOOT_COMMAND + shared.BODY_V11_BOOT_EXTRA))
        for old, new in ((first, "count=1 bytes=" + str(len(shared.BOOT_COMMAND))),
                         (first, first + "0"), ("count=2 bytes=5", "count=2 bytes=6"),
                         ("count=2 bytes=5", "count=3 bytes=5")):
            f = fixture(); f[1]["host"] = f[1]["host"].replace(old, new)
            self.assertFalse(evaluate(f)["pass"])

    def test_precleanup_native_failure_still_fails(self):
        f = fixture(); f[1]["host"] = f[1]["host"].replace("raw_client1_index=1", "raw_client1_index=0")
        self.assertFalse(evaluate(f)["pass"])

    def test_input_cli_forwards_explicit_profile_without_changing_thresholds(self):
        with mock.patch.object(inputs, "score", return_value={"pass": False}) as call, mock.patch("builtins.print"):
            self.assertEqual(inputs.main(["--receipt", "own", "--events", "own-events",
                                          "--profile", shared.BODY_V11_INPUT_PROFILE]), 1)
        call.assert_called_once_with(Path("own"), Path("own-events"), profile=shared.BODY_V11_INPUT_PROFILE)
        self.assertEqual((inputs.EDGE_MS, inputs.MIN_PAIRS, inputs.MAX_PAIR_GAP_MS), (200, 3, 200))


if __name__ == "__main__":
    unittest.main()
