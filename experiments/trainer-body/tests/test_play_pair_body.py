"""Private body-wrapper tests: own fixture bytes/Popen doubles, no game/DLL/network."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock


PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))
import play_pair_body as body

tool = body.launcher


def put(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


class ProcessDouble:
    # Synthetic label only: never passed to a process/operating-system API.
    pid = 321

    def __init__(self):
        self.code = None
        self.terminated = False

    def poll(self):
        return self.code

    def terminate(self):
        self.terminated = True
        self.code = -15

    def wait(self, timeout=None):
        return self.code


class BodyWrapperFixtures(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="own-body-cli-fixture-", dir=PROJECT / "tests")
        self.root = Path(self.temp.name).resolve(strict=True)
        if self.root.parent != (PROJECT / "tests").resolve(strict=True):
            raise RuntimeError("Own fixture containment differs")
        self.addCleanup(self.temp.cleanup)
        self.repo = self.root / "repo"
        self.framework = self.repo / "content/framework"
        data = bytearray(256)  # Own PE-shaped data only; not executable/loaded.
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3c, 64)
        data[64:68] = b"PE\0\0"
        struct.pack_into("<H", data, 68, 0x14c)
        struct.pack_into("<H", data, 86, 0x2000)
        struct.pack_into("<H", data, 88, 0x10b)
        self.dll = put(self.repo / "build/winmm.dll", data)
        self.probe = put(self.framework / "scripts/maps/_mw2trio_probe.gsc", b"// OWN nonexecuted GSC fixture\n")
        self.license = put(self.framework / "LICENSE", b"Own synthetic license fixture\n")
        rows = [{"file": str(path.relative_to(self.framework).as_posix()),
                 "bytes": path.stat().st_size, "sha256": tool.digest(path)}
                for path in (self.license, self.probe)]
        self.manifest = put(self.framework / "manifest.json", json.dumps({"files": rows}).encode("ascii"))
        self.receipt = put(self.repo / "build/tests/own-fixture/results.json", json.dumps({
            "passed": True, "dll_sha256": tool.digest(self.dll)}).encode("ascii"))
        for name, path in (("DLL_SHA", self.dll), ("MANIFEST_SHA", self.manifest), ("PROBE_SHA", self.probe)):
            change = mock.patch.object(body, name, tool.digest(path))
            change.start()
            self.addCleanup(change.stop)
        source = self.root / "source"
        exe = put(source / "iw4sp.exe", b"OWN SYNTHETIC EXE DATA, NEVER EXECUTED")
        put(source / "steam_api.dll", b"OWN SYNTHETIC DEPENDENCY DATA, NEVER LOADED")
        put(source / "main/own-data", b"own fixture")
        put(source / "zone/own-data", b"own fixture")
        change = mock.patch.object(tool, "EXE_SHA", tool.digest(exe))
        change.start()
        self.addCleanup(change.stop)
        self.copy = self.root / "copy"
        tool.prepare(str(source), str(self.copy), True)

    def args(self, execute=False):
        return argparse.Namespace(role="host", test_copy=str(self.copy), local_ip="192.168.50.10",
            peer_ip="192.168.50.11", local_port=28960, peer_port=28961,
            session_seed="1234567890abcdef1234567890abcdef", execute=execute)

    def tree_bytes(self):
        return {p.relative_to(self.copy).as_posix(): p.read_bytes() for p in self.copy.rglob("*") if p.is_file()}

    def test_exact_profile_still_requires_pe_receipt_and_entire_framework(self):
        result = body.body_bundle(self.repo)
        self.assertEqual(result["dll_sha256"], body.DLL_SHA)
        self.assertEqual(result["framework"]["manifest.json"]["sha256"], body.MANIFEST_SHA)
        self.assertEqual(result["framework"]["scripts/maps/_mw2trio_probe.gsc"]["sha256"], body.PROBE_SHA)
        self.assertEqual(result["known_dlls"], {body.DLL_SHA})

    def test_each_profile_pin_refuses_different_bytes(self):
        for path in (self.dll, self.manifest, self.probe):
            before = path.read_bytes()
            path.write_bytes(before + b"changed own fixture")
            with self.subTest(path=path.name), self.assertRaisesRegex(tool.Refused, "exact tested"):
                body.body_bundle(self.repo)
            path.write_bytes(before)

    def test_unrelated_framework_hash_change_is_not_hidden_by_three_pins(self):
        self.license.write_bytes(b"own changed data")
        with self.assertRaisesRegex(tool.Refused, "Framework content hash differs"):
            body.body_bundle(self.repo)

    def test_exact_dll_without_passed_helper_receipt_refused(self):
        self.receipt.write_text(json.dumps({"passed": False, "dll_sha256": body.DLL_SHA}), encoding="ascii")
        with self.assertRaisesRegex(tool.Refused, "helper suite"):
            body.body_bundle(self.repo)

    def test_bundle_result_races_cannot_override_frozen_pin(self):
        result = body.body_bundle(self.repo)
        for change in (dict(result, dll_sha256="0" * 64),
                       dict(result, framework={**result["framework"], "manifest.json": {"sha256": "0" * 64}})):
            with mock.patch.object(body, "_BASE_BUNDLE", return_value=change), self.assertRaises(tool.Refused):
                body.body_bundle(self.repo)

    def test_plan_only_has_no_popen_or_output_or_copy_changes(self):
        before = self.tree_bytes()
        start = mock.Mock(side_effect=AssertionError("No process may be started by a plan"))
        result = body.body_play(self.args(), self.repo, popen=start)
        self.assertEqual(result["mode"], "PLAN_ONLY")
        self.assertEqual(result["private_body_profile"], body.PROFILE)
        self.assertFalse(result["network_or_game_started"])
        self.assertEqual(before, self.tree_bytes())
        self.assertFalse((self.repo / "local").exists())
        start.assert_not_called()

    def test_changed_dependency_refused_without_staging_or_replacement(self):
        dependency = self.copy / "steam_api.dll"
        dependency.write_bytes(b"OWN changed dependency")
        before = self.tree_bytes()
        start = mock.Mock(side_effect=AssertionError("Dependency mismatch cannot launch"))
        with self.assertRaisesRegex(tool.Refused, "differs from the prepared-copy inventory"):
            body.body_play(self.args(True), self.repo, popen=start)
        self.assertEqual(before, self.tree_bytes())
        start.assert_not_called()

    def test_pending_mailbox_is_never_overwritten(self):
        put(self.copy / "control-command.txt", b"own pending command\n")
        before = self.tree_bytes()
        with self.assertRaisesRegex(tool.Refused, "Existing process marker or mailbox"):
            body.body_play(self.args(True), self.repo, popen=mock.Mock())
        self.assertEqual(before, self.tree_bytes())

    def test_foreign_framework_is_preserved_without_start(self):
        put(self.copy / "mw2trio/own-foreign-file", b"own foreign fixture")
        before = self.tree_bytes()
        start = mock.Mock(side_effect=AssertionError("Foreign content cannot launch"))
        with self.assertRaises((OSError, tool.Refused)):
            body.body_play(self.args(True), self.repo, popen=start)
        self.assertEqual(before, self.tree_bytes())
        start.assert_not_called()

    def test_start_failure_restores_extra_body_flag_log_and_absent_framework(self):
        flag = put(self.copy / body.BODY_FLAG, b"own prior body opt-in")
        log = put(self.copy / body.BODY_LOG, b"own prior body log")
        result = body.body_play(self.args(True), self.repo,
            popen=mock.Mock(side_effect=RuntimeError("own simulated Popen failure")))
        self.assertIn("simulated Popen failure", result["error"])
        self.assertTrue(result["owned_process_stopped"])
        self.assertEqual(result["restore_errors"], [])
        self.assertEqual(flag.read_bytes(), b"own prior body opt-in")
        self.assertEqual(log.read_bytes(), b"own prior body log")
        for name in ("mw2trio", "winmm.dll", tool.ACTIVE, "control-command.txt", "control-command.next"):
            self.assertFalse((self.copy / name).exists())

    def test_mock_success_stages_only_body_optin_and_restores_after_owned_exit(self):
        original_flag = put(self.copy / body.BODY_FLAG, b"own prior body flag")
        original_log = put(self.copy / body.BODY_LOG, b"own prior body log")
        put(self.copy / "players/config.cfg", b"own prior config")
        process = ProcessDouble()
        prompt = mock.Mock(return_value="")

        def start(argv, **kwargs):
            self.assertTrue(all((self.copy / flag).exists() for flag in tool.FLAGS))
            self.assertFalse(any((self.copy / flag).exists() for flag in tool.OFF_FLAGS))
            self.assertEqual(tool.digest(self.copy / "mw2trio/scripts/maps/_mw2trio_probe.gsc"), body.PROBE_SHA)
            (self.copy / body.BODY_LOG).write_bytes(b"OWN SYNTHETIC body proof, no engine\n")
            (self.copy / "players/config.cfg").write_bytes(b"own simulated config mutation")
            return process

        class FlowDouble(tool.Control):
            # Only waiting is doubled; these native predicates retain inherited tests.
            def __init__(self, root, proc):
                super().__init__(root, proc, tick=lambda: 1000)

            def rows(self):
                party = dict(host="1", active="1", launch="0", session_active="1", member0="1", member1="1",
                    native_online="0", pending_accept_ack="0", launch_blocked="0", coop_map="trainer")
                frame = dict(sv_running="0", coop="1", specialops="0", game_mode="2", enqueued_mailboxes=str(self.count))
                return [(1000, "party host=1", party), (1000, "frame=20", frame),
                        (1000, "party_commands registered=1", {"registered": "1"})]

            def wait(self, predicate, seconds=90, stable=0):
                row = predicate()
                if not row:
                    raise AssertionError("Own mock lifecycle unexpectedly failed")
                return row

            def consume(self, event, seconds=30):
                event.update(consumed=True, native_tick=1000)
                (self.root / "control-command.txt").unlink()
                if event["commands"] == "mw2trio_launch\n":
                    self.process.code = 0

        with mock.patch.object(tool, "Control", FlowDouble):
            result = body.body_play(self.args(True), self.repo, popen=start, prompt=prompt)
        self.assertIsNone(result["error"])
        self.assertEqual(result["restore_errors"], [])
        self.assertEqual(result["exit_code"], 0)
        self.assertFalse(process.terminated)
        self.assertEqual(original_flag.read_bytes(), b"own prior body flag")
        self.assertEqual(original_log.read_bytes(), b"own prior body log")
        self.assertEqual((self.copy / "players/config.cfg").read_bytes(), b"own prior config")
        self.assertFalse((self.copy / "mw2trio").exists())
        self.assertFalse((self.copy / tool.ACTIVE).exists())
        self.assertEqual((Path(result["output"]) / "after" / body.BODY_LOG).read_bytes(),
                         b"OWN SYNTHETIC body proof, no engine\n")
        prompt.assert_called_once()


class BodyWrapperFrozenInputs(unittest.TestCase):
    def test_private_snapshot_profile_and_managed_lists(self):
        self.assertEqual(hashlib.sha256((PROJECT / "tools/play_pair.py").read_bytes()).hexdigest(), body.BASE_TOOL_SHA)
        self.assertEqual(tool.FLAGS.count(body.BODY_FLAG), 1)
        self.assertEqual(tool.LOGS.count(body.BODY_LOG), 1)
        self.assertNotIn(body.BODY_FLAG, tool.OFF_FLAGS)
        self.assertTrue(all(name not in tool.FLAGS for name in tool.OFF_FLAGS))
        self.assertEqual(len(tool.FLAGS), 7)

    def test_actual_frozen_private_bundle_is_read_only_and_has_passed_receipt(self):
        result = body.body_bundle(PROJECT)
        self.assertEqual(result["dll_sha256"], body.DLL_SHA)
        self.assertEqual(result["framework"]["manifest.json"]["sha256"], body.MANIFEST_SHA)
        self.assertEqual(result["framework"]["scripts/maps/_mw2trio_probe.gsc"]["sha256"], body.PROBE_SHA)
        self.assertIn(body.DLL_SHA, result["known_dlls"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
