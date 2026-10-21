"""Own synthetic files, telemetry and Popen doubles only; no game or packets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import play_pair as tool


def put(path, data=b"own fixture"):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def framework(root):
    data = b"own MIT fixture only"
    put(root / "LICENSE", data)
    put(root / "manifest.json", json.dumps({"files": [{"file": "LICENSE", "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest()}]}).encode())
    return tool.verify_framework(root)


class ProcessDouble:
    pid = 123  # Synthetic label, never passed to an operating-system API.
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


class PairToolTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="own-pair-helper-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def args(self, **changes):
        args = dict(role="host", test_copy=str(self.root / "copy"), local_ip="192.168.50.10",
                    peer_ip="192.168.50.11", local_port=28960, peer_port=28961,
                    session_seed="1234567890abcdef1234567890abcdef", execute=False)
        return argparse.Namespace(**(args | changes))

    def source(self):
        root = self.root / "source"
        put(root / "iw4sp.exe", b"OWN SYNTHETIC EXE DATA; NOT EXECUTABLE")
        put(root / "steam_api.dll", b"OWN SYNTHETIC DEPENDENCY; NOT A DLL")
        put(root / "main/own-data", b"a")
        put(root / "zone/own-data", b"b")
        put(root / "players/config.cfg", b"private source profile")
        put(root / "saves/own.sav", b"private source save")
        put(root / "mw2trio/own-script", b"source mod is excluded")
        put(root / "winmm.dll", b"source proxy is excluded")
        self.patch = mock.patch.object(tool, "EXE_SHA", tool.digest(root / "iw4sp.exe"))
        self.patch.start()
        self.addCleanup(self.patch.stop)
        return root

    def test_strict_ipv4_policy_and_canonical_form(self):
        for value in ("192.168.1.1", "26.1.2.3", "100.64.1.2", "1.1.1.1"):
            self.assertEqual(tool.ipv4(value), value)
        for value in ("127.0.0.2", "0.1.2.3", "169.254.1.2", "224.1.2.3", "255.1.2.3",
                      "192.168.1.0", "192.168.1.255", "192.168.01.2", "1.2.3", "1.2.3.256", "host", "1.2.3.4\n"):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                tool.ipv4(value)

    def test_port_and_seed_bounds(self):
        self.assertEqual(tool.port("65535"), 65535)
        self.assertEqual(tool.port("1"), 1)
        for value in ("0", "65536", "01", "+1", "1.0", " 1"):
            with self.assertRaises(argparse.ArgumentTypeError):
                tool.port(value)
        for value in ("0" * 32, "a" * 31, "g" * 32, "a" * 33):
            with self.assertRaises(argparse.ArgumentTypeError):
                tool.seed(value)

    def test_seven_key_config_and_identical_tuple_refusal(self):
        text = tool.peer_config(self.args())
        self.assertEqual(len(text.splitlines()), 7)
        self.assertTrue(text.endswith("\n"))
        self.assertLessEqual(len(text), 256)
        self.assertIn("role=host\n", text)
        with self.assertRaises(tool.Refused):
            tool.peer_config(self.args(peer_ip="192.168.50.10", peer_port=28960))

    def test_prepare_default_is_read_only(self):
        source = self.source()
        target = self.root / "copy"
        before = {str(p): p.read_bytes() for p in source.rglob("*") if p.is_file()}
        self.assertEqual(tool.prepare(str(source), str(target))["mode"], "PLAN_ONLY")
        self.assertFalse(target.exists())
        self.assertEqual(before, {str(p): p.read_bytes() for p in source.rglob("*") if p.is_file()})

    def test_prepare_independent_copy_and_private_exclusions(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        tool.verify_copy(target)
        self.assertFalse((target / "players").exists())
        self.assertFalse((target / "saves").exists())
        self.assertFalse((target / "mw2trio").exists())
        self.assertFalse((target / "winmm.dll").exists())
        self.assertFalse(os.path.samefile(source / "iw4sp.exe", target / "iw4sp.exe"))
        self.assertEqual((target / "iw4sp.exe").stat().st_nlink, 1)

    def test_prepare_refuses_nested_existing_or_relative_paths(self):
        source = self.source()
        for destination in (source / "nested", source, source.parent):
            with self.assertRaises(tool.Refused):
                tool.prepare(str(source), str(destination), True)
        with self.assertRaises(tool.Refused):
            tool.prepare("relative", str(self.root / "copy"))
        (self.root / "other").mkdir()
        with self.assertRaises(tool.Refused):
            tool.prepare(str(source), str(self.root / "other/../source/nested"), True)
        self.assertFalse((source / "nested").exists())

    def test_hardlinked_copy_file_refuses(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        os.link(target / "main/own-data", self.root / "own-hardlink-negative")
        with self.assertRaises(tool.Refused):
            tool.verify_copy(target)

    def test_unmarked_incomplete_or_changed_copy_refuses(self):
        source = self.source()
        target = self.root / "copy"
        target.mkdir()
        with self.assertRaises(OSError):
            tool.verify_copy(target)
        target.rmdir()
        tool.prepare(str(source), str(target), True)
        (target / "main/own-data").unlink()
        with self.assertRaises(OSError):
            tool.verify_copy(target)

    def test_marker_and_pending_mailbox_refuse_without_overwrite(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        for name in (tool.ACTIVE, "control-command.txt", "control-command.next", "__iw4sp"):
            p = put(target / name)
            with self.assertRaises(tool.Refused):
                tool.verify_copy(target)
            self.assertEqual(p.read_bytes(), b"own fixture")
            p.unlink()

    def test_manifest_extra_changed_or_traversal_refuses(self):
        root = self.root / "framework"
        expected = framework(root)
        extra = put(root / "extra")
        with self.assertRaises(tool.Refused):
            tool.verify_framework(root, expected)
        extra.unlink()
        (root / "LICENSE").write_bytes(b"different")
        with self.assertRaises(tool.Refused):
            tool.verify_framework(root, expected)
        for value in ("../escape", "a/../b", "C:/escape", "a\\b", "/absolute", "a//b"):
            with self.assertRaises(tool.Refused):
                tool.relative(value)

    def test_built_dll_requires_matching_passed_own_receipt(self):
        repo = self.root / "repo"
        framework(repo / "content/framework")
        pe = bytearray(256)
        pe[:2] = b"MZ"
        struct.pack_into("<I", pe, 0x3c, 128)
        pe[128:132] = b"PE\0\0"
        struct.pack_into("<H", pe, 132, 0x14c)
        struct.pack_into("<H", pe, 150, 0x2000)
        struct.pack_into("<H", pe, 152, 0x10b)
        dll = put(repo / "build/winmm.dll", pe)
        with self.assertRaises(tool.Refused):
            tool.bundle(repo)
        put(repo / "build/tests/own/results.json", json.dumps({"passed": True, "dll_sha256": tool.digest(dll)}).encode())
        self.assertEqual(tool.bundle(repo)["dll_sha256"], tool.digest(dll))
        dll.write_bytes(pe + b"changed")
        with self.assertRaises(tool.Refused):
            tool.bundle(repo)

    def test_transaction_restores_only_managed_files_and_config(self):
        root, out = self.root / "copy", self.root / "output"
        original = put(root / "winmm.dll", b"old own proxy")
        put(root / "players/config.cfg", b"old mode")
        foreign = put(root / "other-mod.cfg", b"keep")
        files = tool.Files(root, out)
        files.backup(["winmm.dll", "players/config.cfg", "mw2-trio-peer.cfg"])
        files.put("winmm.dll", b"new own proxy")
        files.put("players/config.cfg", b"test mode")
        files.put("mw2-trio-peer.cfg", b"own config")
        put(root / "players/new.cfg", b"game-generated mode")
        put(root / "players/own.sav", b"private new save preserved")
        self.assertEqual(files.restore(), [])
        self.assertEqual(original.read_bytes(), b"old own proxy")
        self.assertEqual((root / "players/config.cfg").read_bytes(), b"old mode")
        self.assertFalse((root / "players/new.cfg").exists())
        self.assertTrue((root / "players/own.sav").exists())
        self.assertEqual(foreign.read_bytes(), b"keep")

    def test_partial_backup_never_deletes_unbacked_original_player_config(self):
        root, out = self.root / "copy", self.root / "output"
        cfg = put(root / "players/config.cfg", b"existing original config must survive")
        console = put(root / "main/console.log", b"existing log")
        files = tool.Files(root, out)
        with mock.patch.object(tool, "copy_new", side_effect=OSError("own early backup failure")):
            with self.assertRaises(OSError):
                files.backup(["players/config.cfg", "main/console.log"])
        self.assertFalse(files.backup_complete)
        with self.assertRaises(tool.Refused):
            files.put("mw2-trio-peer.cfg", b"must not stage")
        self.assertEqual(files.restore(), [])
        self.assertEqual(cfg.read_bytes(), b"existing original config must survive")
        self.assertEqual(console.read_bytes(), b"existing log")
        self.assertFalse((root / "mw2-trio-peer.cfg").exists())

    def control(self):
        self.now = 0.
        def sleep(n):
            self.now += n
        root = self.root / "control"
        root.mkdir()
        p = ProcessDouble()
        c = tool.Control(root, p, clock=lambda: self.now, sleep=sleep, tick=lambda: 2000)
        return c, p

    def test_pending_mailbox_is_not_replaced(self):
        c, _ = self.control()
        p = put(c.root / "control-command.txt", b"existing")
        with self.assertRaises(tool.Refused):
            c.write("quit\n")
        self.assertEqual(p.read_bytes(), b"existing")

    def test_exact_ack_and_wrong_bytes_refusal(self):
        c, _ = self.control()
        body = "mailbox enqueued count=1 bytes=5; execution requires native state evidence"
        c.rows = lambda: [(2000, body, {})]
        event = {"count": 1, "bytes": 5, "consumed": False}
        c.consume(event)
        self.assertTrue(event["consumed"])
        with self.assertRaises(tool.Refused):
            c.consume({"count": 1, "bytes": 4})

    def test_host_menu_state_one_client_requires_state_zero(self):
        c, _ = self.control()
        f = dict(host="1", active="1", launch="0", session_active="1", member0="1", member1="1",
                 native_online="0", pending_accept_ack="0", launch_blocked="0", client_state="1")
        c.rows = lambda: [(2000, "party host=1", f)]
        self.assertTrue(c.party_gate("host"))
        f["host"] = "0"
        self.assertIsNone(c.party_gate("client"))
        f["client_state"] = "0"
        self.assertTrue(c.party_gate("client"))

    def test_guard_refusal_fails_fast(self):
        c, _ = self.control()
        put(c.root / "party184.log", b"party184 result=17 open_result=-6\n")
        with self.assertRaises(tool.Refused):
            c.wait(lambda: None, 90)
        self.assertEqual(self.now, 0)

    def test_stability_needs_two_distinct_fresh_native_ticks(self):
        c, _ = self.control()
        c.tick = lambda: 1000 + round(self.now * 1000)
        row = c.wait(lambda: (c.tick(), "own fixture", {}), 3, stable=2)
        self.assertGreaterEqual(row[0], 3000)
        self.now = 0
        c.tick = lambda: 1000
        with self.assertRaises(tool.Refused):
            c.wait(lambda: (1000, "same native tick forever", {}), 3, stable=2)

    def test_matching_campaign_frame_needs_exact_count_and_mode(self):
        c, _ = self.control()
        f = dict(sv_running="0", coop="1", specialops="0", game_mode="2", enqueued_mailboxes="3")
        c.rows = lambda: [(2000, "frame=10", f)]
        event = dict(count=3, native_tick=1999, consumed=True)
        self.assertTrue(c.campaign_frame(event))
        f["specialops"] = "1"
        self.assertIsNone(c.campaign_frame(event))
        f["specialops"] = "0"
        event["count"] = 2
        self.assertIsNone(c.campaign_frame(event))

    def test_campaign_gate_refuses_stale_party_or_other_selected_map(self):
        c, _ = self.control()
        p = dict(host="1", active="1", launch="0", session_active="1", member0="1", member1="1",
                 native_online="0", pending_accept_ack="0", launch_blocked="0", coop_map="trainer")
        f = dict(sv_running="0", coop="1", specialops="0", game_mode="2", enqueued_mailboxes="3")
        c.rows = lambda: [(2000, "frame=10", f), (2000, "party host=1", p)]
        event = dict(count=3, native_tick=1999, consumed=True)
        self.assertTrue(c.campaign_gate(event, "host"))
        c.rows = lambda: [(2000, "frame=10", f), (tool.U32 - 5000, "party host=1", p)]
        self.assertIsNone(c.campaign_gate(event, "host"))
        c.rows = lambda: [(2000, "frame=10", f), (2000, "party host=1", p)]
        p["coop_map"] = "other_map"
        self.assertIsNone(c.campaign_gate(event, "host"))

    def test_host_waits_for_late_invite_mode_before_campaign_override(self):
        c, _ = self.control()
        c.tick = lambda: 100000 + round(self.now * 1000)
        armed = False
        send_times = []
        def rows():
            # Original failure: admission completes early, but host UI only
            # later registers mode2 and overwrites specialops from0 to1.
            mode = "2" if self.now >= 4 else "0"
            specialops = "0" if armed or self.now < 4 else "1"
            f = dict(sv_running="0", coop="1", game_mode=mode, specialops=specialops,
                     enqueued_mailboxes="3" if armed else "2")
            p = dict(host="1", active="1", launch="0", session_active="1", member0="1", member1="1",
                     native_online="0", pending_accept_ack="0", launch_blocked="0", client_state="1", coop_map="trainer")
            return [(c.tick(), "frame=20", f), (c.tick(), "party host=1", p)]
        c.rows = rows
        def consumed_native_override(commands, seconds=30):
            nonlocal armed
            self.assertEqual(commands, tool.CAMPAIGN)
            self.assertGreaterEqual(self.now, 6)
            self.assertEqual(c.latest("frame=")[2]["specialops"], "1")
            armed = True
            send_times.append(self.now)
            return dict(count=3, bytes=len(commands), consumed=True, native_tick=c.tick())
        c.send = consumed_native_override
        self.assertTrue(c.party_gate("host"))
        event = c.configure_campaign("host")
        self.assertEqual(len(send_times), 1)
        self.assertTrue(event["consumed"])
        self.assertEqual(c.campaign_gate(event, "host")[2]["specialops"], "0")
        self.assertGreaterEqual(self.now, send_times[0] + 2)

    def test_admission_without_invite_transition_never_dispatches_campaign(self):
        c, _ = self.control()
        c.tick = lambda: 100000 + round(self.now * 1000)
        p = dict(host="1", active="1", launch="0", session_active="1", member0="1", member1="1",
                 native_online="0", pending_accept_ack="0", launch_blocked="0", client_state="1", coop_map="trainer")
        c.rows = lambda: [(c.tick(), "frame=20", dict(sv_running="0", coop="1", game_mode="0", specialops="0")),
                          (c.tick(), "party host=1", p)]
        c.send = mock.Mock(side_effect=AssertionError("No settings may precede the verified invite transition"))
        with self.assertRaises(tool.Refused):
            c.configure_campaign("host")
        c.send.assert_not_called()
        self.assertLess(self.now, 90.2)

    def test_operator_eof_is_refused_instead_of_implicit_launch(self):
        c, p = self.control()
        with self.assertRaises(tool.Refused):
            tool.wait_for_operator(c, "own fixture", mock.Mock(side_effect=EOFError))
        self.assertFalse(p.terminated)
        self.assertEqual(c.events, [])

    def test_operator_wait_detects_own_exit_without_enter(self):
        c, p = self.control()
        release = threading.Event()
        entered = threading.Event()
        def pending_prompt(message):
            entered.set()
            release.wait(2)
        original_check = c.check
        checks = 0
        def check():
            nonlocal checks
            checks += 1
            if checks >= 2:
                entered.wait(1)
                p.code = 0
            original_check()
        c.check = check
        try:
            with self.assertRaises(tool.Refused):
                tool.wait_for_operator(c, "own fixture", pending_prompt)
        finally:
            release.set()
        self.assertTrue(entered.is_set())
        self.assertFalse(p.terminated)

    def test_only_retained_handle_terminated_after_bounded_quit(self):
        c, p = self.control()
        other = ProcessDouble()
        self.assertTrue(tool.stop_owned(c))
        self.assertLess(self.now, 8.2)
        self.assertTrue(p.terminated)
        self.assertFalse(other.terminated)
        self.assertEqual(c.events[0]["commands"], "quit\n")

    def test_normal_owned_exit_needs_no_termination(self):
        c, p = self.control()
        p.code = 0
        self.assertFalse(tool.stop_owned(c))
        self.assertFalse(p.terminated)
        self.assertEqual(c.events, [])

    def test_popen_failure_restores_proxy_flags_and_staged_content(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        repo = self.root / "repo"
        expected = framework(repo / "content/framework")
        dll = put(repo / "build/winmm.dll", b"OWN BUILT-DLL DOUBLE; NEVER LOADED")
        own = dict(dll=dll, dll_sha256=tool.digest(dll), known_dlls={tool.digest(dll)}, framework=expected)
        original = put(target / tool.OFF_FLAGS[0], b"own old flag")
        with mock.patch.object(tool, "bundle", return_value=own):
            value = tool.play(self.args(execute=True), repo, popen=mock.Mock(side_effect=RuntimeError("own simulated start failure")))
        self.assertIn("simulated start failure", value["error"])
        self.assertEqual(value["restore_errors"], [])
        self.assertFalse((target / "winmm.dll").exists())
        self.assertFalse((target / "mw2trio").exists())
        self.assertFalse((target / tool.ACTIVE).exists())
        self.assertFalse((target / "control-command.txt").exists())
        self.assertEqual(original.read_bytes(), b"own old flag")

    def test_plan_never_invokes_popen_or_creates_local_output(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        repo = self.root / "repo"
        expected = framework(repo / "content/framework")
        dll = put(repo / "build/winmm.dll", b"OWN DLL DOUBLE")
        own = dict(dll=dll, dll_sha256=tool.digest(dll), known_dlls={tool.digest(dll)}, framework=expected)
        create = mock.Mock(side_effect=AssertionError("Popen must not run"))
        with mock.patch.object(tool, "bundle", return_value=own):
            value = tool.play(self.args(), repo, popen=create)
        self.assertEqual(value["mode"], "PLAN_ONLY")
        create.assert_not_called()
        self.assertFalse((repo / "local").exists())

    def test_staged_dll_rebuild_race_refuses_before_popen(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        repo = self.root / "repo"
        expected = framework(repo / "content/framework")
        dll = put(repo / "build/winmm.dll", b"OWN TESTED SNAPSHOT DOUBLE")
        own = dict(dll=dll, dll_sha256=tool.digest(dll), known_dlls={tool.digest(dll)}, framework=expected)
        create = mock.Mock(side_effect=AssertionError("Untested staged bytes must not launch"))
        def changed_after_validation(repo):
            dll.write_bytes(b"OWN CHANGED BUILD DOUBLE")
            return own
        with mock.patch.object(tool, "bundle", side_effect=changed_after_validation):
            value = tool.play(self.args(execute=True), repo, popen=create)
        self.assertIn("Staged DLL hash differs", value["error"])
        self.assertFalse(value["network_or_game_started"])
        create.assert_not_called()
        self.assertFalse((target / "winmm.dll").exists())

    def test_successful_host_lifecycle_double_restores_after_own_exit(self):
        source = self.source()
        target = self.root / "copy"
        tool.prepare(str(source), str(target), True)
        put(target / "players/config.cfg", b"own prior mode")
        repo = self.root / "repo"
        expected = framework(repo / "content/framework")
        dll = put(repo / "build/winmm.dll", b"OWN DLL DOUBLE; NEVER LOADED")
        own = dict(dll=dll, dll_sha256=tool.digest(dll), known_dlls={tool.digest(dll)}, framework=expected)
        process = ProcessDouble()
        prompt = mock.Mock(return_value="")
        def start(argv, **kwargs):
            self.assertEqual(argv[1:], ["+set", "net_ip", "192.168.50.10", "+set", "net_port", "28960"])
            self.assertEqual(kwargs["cwd"], str(target))
            self.assertEqual(len((target / "mw2-trio-peer.cfg").read_text().splitlines()), 7)
            self.assertTrue(all((target / f).exists() for f in tool.FLAGS))
            self.assertFalse(any((target / f).exists() for f in tool.OFF_FLAGS))
            (target / "players/config.cfg").write_bytes(b"own simulated engine mode change")
            return process
        class FlowDouble(tool.Control):
            # Bypass native waiting only; exact predicates have separate tests.
            def __init__(self, root, proc):
                super().__init__(root, proc, tick=lambda: 1000)
            def rows(self):
                p = dict(host="1", active="1", launch="0", session_active="1", member0="1", member1="1",
                         native_online="0", pending_accept_ack="0", launch_blocked="0", client_state="1", coop_map="trainer")
                f = dict(sv_running="0", coop="1", specialops="0", game_mode="2", enqueued_mailboxes=str(self.count))
                return [(1000, "party host=1", p), (1000, "frame=20", f),
                        (1000, "party_commands registered=1", {"registered": "1"})]
            def wait(self, predicate, seconds=90, stable=0):
                row = predicate()
                if not row:
                    raise AssertionError("Own lifecycle gate double unexpectedly failed")
                return row
            def consume(self, event, seconds=30):
                event.update(consumed=True, native_tick=1000)
                (self.root / "control-command.txt").unlink()
                if event["commands"] == "mw2trio_launch\n":
                    self.process.code = 0
        with mock.patch.object(tool, "bundle", return_value=own), mock.patch.object(tool, "Control", FlowDouble):
            value = tool.play(self.args(execute=True), repo, popen=start, prompt=prompt)
        self.assertIsNone(value["error"])
        self.assertEqual(value["restore_errors"], [])
        self.assertEqual(value["exit_code"], 0)
        self.assertFalse(value["forced_termination"])
        self.assertFalse(process.terminated)
        self.assertEqual(len(value["mailboxes"]), 4)
        setup = value["mailboxes"][1]["commands"]
        self.assertLess(setup.index("mw2trio_peer_identity"), setup.index("mw2trio_lan_open"))
        prompt.assert_called_once()
        self.assertEqual((target / "players/config.cfg").read_bytes(), b"own prior mode")
        self.assertFalse((target / "mw2trio").exists())
        self.assertFalse((target / tool.ACTIVE).exists())


if __name__ == "__main__":
    unittest.main()
