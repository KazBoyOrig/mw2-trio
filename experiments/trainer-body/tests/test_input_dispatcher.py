"""Callback/Popen doubles only; never invokes any real process or mailbox."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/qa"))
import copy
import re
import tempfile
import unittest

import input_dispatcher as bot
import score_independent_inputs as reader


class ProcessDouble:
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


class World:
    def __init__(self, mode=0):
        self.wall = 0.
        self.game = 12000
        self.mode = mode
        self.count = {r: 3 for r in bot.ROLES}
        self.acks = {r: [] for r in bot.ROLES}
        self.axes = [[0, 0], [0, 0]]
        self.positions = [[0., 0., 0.], [0., 30., 0.]]
        self.processes = {r: ProcessDouble() for r in bot.ROLES}
        self.writes = []
        self.write_times = []
        self.stuck = None
        self.freeze = False
        self.bad_bytes = False
        self.client_mode = mode
    def clock(self):
        return self.wall
    def sleep(self, seconds):
        self.wall += seconds
        if not self.freeze:
            self.game += round(seconds * 1000)
            for i in range(2):
                self.positions[i][0] += self.axes[i][0] * seconds * 190
                self.positions[i][1] += self.axes[i][1] * seconds * 190
    def role(self, path):
        return "host" if "fixture-host" in str(path) else "client"
    def pending(self, role):
        return role == self.stuck
    def mailbox(self, root, commands):
        role = self.role(root)
        assert not self.pending(role)
        self.writes.append((role, commands))
        self.write_times.append((role, self.wall))
        i = 0 if role == "host" else 1
        self.axes[i] = [int("+forward\n" in commands) - int("+back\n" in commands),
                        int("+moveright\n" in commands) - int("+moveleft\n" in commands)]
        self.count[role] += 1
        size = len(commands) + int(self.bad_bytes)
        self.bad_bytes = False
        self.acks[role].append("tick=%d tid=1234 mailbox enqueued count=%d bytes=%d; execution requires native state evidence\n"
                               % (100000 + round(self.wall * 1000), self.count[role], size))
    def read(self, path):
        role = self.role(path)
        tick = 100000 + round(self.wall * 1000)
        prefix = "tick=%d tid=1234 " % tick
        local = 0 if role == "host" else 1
        lines = ["frame=1 enqueued_mailboxes=%d map=trainer sv_running=1 coop=1 specialops=%d cl_paused=0 "
                 "raw_valid=0xff raw_level_maxclients=2 raw_gclient_base=0x0108dc68 raw_coop_enabled=1 "
                 "raw_entity0_client=0x0108dc68 raw_entity1_client=0x01098c3c raw_client0_index=0 raw_client1_index=1"
                 % (self.count[role], self.mode if role == "host" else self.client_mode),
                 "party host=%d active=0 launch=0 session=0x01bde8b8 session_active=1 member0=1 member1=1 "
                 "client_state=6 raw_launch_count=2 channel0_type=%d channel1_type=%d net_port=%d"
                 % (role == "host", 2 if role == "host" else 4, 4 if role == "host" else 2,
                    28960 if role == "host" else 28961)]
        for slot in range(2):
            port = 0 if role == "host" and slot == 0 else 28961 if role == "host" else 28960
            lines.append("roster slot=%d occupied=1 local_uid_match=%d address_type=%d port=%d"
                         % (slot, local == slot, 2 if role == "host" and slot == 0 else 4, port))
            lines.append("input slot=%d level_time=%d server_cmd_time=%d applied_cmd_time=%d "
                         "server_axes=%d,%d applied_axes=%d,%d origin=%s"
                         % (slot, self.game, self.game, self.game,
                            self.axes[slot][0] * 127, self.axes[slot][1] * 127,
                            self.axes[slot][0] * 127, self.axes[slot][1] * 127,
                            ",".join(str(v) for v in self.positions[slot])))
        return "".join(self.acks[role]) + "".join(prefix + line + "\n" for line in lines)
    def run(self, pattern="original-right"):
        roots = {r: bot.TASK / "tests" / ("fixture-" + r) for r in bot.ROLES}
        return bot.run_input_plan(roots, self.processes, self.mailbox,
                                  bot.TASK / "tests", specialops=self.mode, pattern=pattern,
                                  _clock=self.clock, _sleep=self.sleep, _read=self.read,
                                  _pending=self.pending, _persist=lambda value: None)


class ClientApplicationLagWorld(World):
    """Enqueue ACK is immediate; one world's native axis observation is delayed."""
    def __init__(self, delay, field):
        super().__init__(0)
        self.delay, self.field = delay, field
        self.apply_after = None
        self.first_applied_level = None

    def mailbox(self, root, commands):
        super().mailbox(root, commands)
        if self.role(root) == "client" and self.count["client"] == 10:
            self.apply_after = self.wall + self.delay

    def read(self, path):
        text = super().read(path)
        if self.role(path) == "client" and self.count["client"] == 10:
            if self.wall < self.apply_after:
                text = re.sub(r"(input slot=1 [^\n]*?\b" + self.field + r"=)[^ ]+",
                              r"\g<1>0,0", text)
            elif self.first_applied_level is None:
                self.first_applied_level = self.game
        return text


class DispatcherTests(unittest.TestCase):
    def test_activation_requires_server_and_applied_axes_in_both_worlds_both_slots(self):
        values = {}
        world = World(0)
        world.axes = [[1, 0], [0, -1]]
        for role in bot.ROLES:
            values[role] = bot.snapshot(world.read(Path("fixture-" + role)), role, "trainer", 0)
        self.assertEqual(bot.activation_mismatches(values, "distinct_both", "client-return-left"), [])
        for role in bot.ROLES:
            for slot in range(2):
                for field in ("server_axes", "axes"):
                    changed = copy.deepcopy(values)
                    changed[role]["inputs"][slot][field] = "0,0"
                    with self.subTest(role=role, slot=slot, field=field):
                        mismatches = bot.activation_mismatches(changed, "distinct_both", "client-return-left")
                        self.assertEqual(len(mismatches), 1)
                        self.assertEqual((mismatches[0]["role"], mismatches[0]["slot"]), (role, slot))

    def test_delayed_application_waits_beyond_enqueue_ack_without_discarding_phase_duration(self):
        world = ClientApplicationLagWorld(delay=.4, field="server_axes")
        value = world.run("both-return")
        self.assertIsNone(value["error"])
        self.assertEqual(reader.ledger_checks(value)[1], [])
        phase = value["phases"][3]
        activation = phase["activation"]
        self.assertTrue(activation["activated"])
        self.assertGreater(activation["pending_polls"], 0)
        self.assertGreaterEqual(activation["at"], world.apply_after)
        self.assertEqual(phase["start_game_ms"], world.first_applied_level)
        self.assertEqual(phase["end_game_ms"] - phase["start_game_ms"], 1000)
        self.assertTrue(all(event["consumed"] for event in phase["dispatch"].values()))
        self.assertLess(phase["dispatch"]["client"]["native_tick"], activation["last_poll"]["native_ticks"]["client"])
        self.assertEqual(activation["last_pending"]["mismatches"][0]["role"], "client")
        self.assertAlmostEqual(activation["deadline"] - phase["timing"]["phase_begin"], 3)

    def test_never_applied_native_axes_fail_activation_inside_original_deadline(self):
        world = ClientApplicationLagWorld(delay=10, field="applied_axes")
        value = world.run()
        self.assertIn("native axes activation exceeded same 3s wall watchdog", value["error"])
        phase = value["phases"][-1]
        self.assertEqual(phase["name"], "client_right")
        self.assertNotIn("start_game_ms", phase)
        self.assertNotIn("end_game_ms", phase)
        self.assertFalse(phase["activation"]["activated"])
        self.assertEqual(phase["activation"]["mismatches"][0]["role"], "client")
        self.assertAlmostEqual(phase["activation"]["deadline"] - phase["timing"]["phase_begin"], 3)
        self.assertLessEqual(value["failure_wall"]["at"], phase["activation"]["deadline"] + .021)
        self.assertTrue(all(event["consumed"] for event in value["emergency_release"].values()))
        self.assertEqual(world.axes, [[0, 0], [0, 0]])

    def test_late_activation_does_not_reset_deadline_for_one_second_measurement(self):
        world = ClientApplicationLagWorld(delay=2.2, field="server_axes")
        value = world.run()
        phase = value["phases"][-1]
        self.assertEqual(value["error"], "TimeoutError: client_right: independent 3s wall watchdog")
        self.assertEqual(phase["name"], "client_right")
        self.assertTrue(phase["activation"]["activated"])
        self.assertNotIn("end_game_ms", phase)
        self.assertEqual(phase["activation"]["deadline"], phase["timing"]["deadline"])
        self.assertAlmostEqual(phase["timing"]["deadline"] - phase["timing"]["phase_begin"], 3)
        self.assertEqual(phase["timing"]["target_game_ms"], phase["start_game_ms"] + 1000)
        self.assertLessEqual(value["failure_wall"]["at"], phase["timing"]["deadline"] + .021)
        self.assertTrue(all(event["consumed"] for event in value["emergency_release"].values()))

    def test_success_keeps_existing_phase_wall_and_game_bounds_with_diagnostics(self):
        value = World(0).run()
        self.assertIsNone(value["error"])
        for phase in value["phases"]:
            timing = phase["timing"]
            self.assertAlmostEqual(timing["deadline"] - timing["phase_begin"], 3)
            wanted = 2000 if phase["name"] in ("neutral_before", "neutral_after") else 1000
            self.assertEqual(timing["target_game_ms"], phase["start_game_ms"] + wanted)
            self.assertLess(phase["wall_ms"], 3000)
            self.assertLessEqual(phase["end_game_ms"] - phase["start_game_ms"], wanted + 200)
            self.assertLessEqual(timing["last_poll"]["begin"], timing["last_poll"]["end"])
            self.assertEqual(set(timing["last_poll"]["native_ticks"]), set(bot.ROLES))

    def test_slow_reads_still_fail_same_three_second_watchdog_and_record_last_poll(self):
        class SlowWorld(World):
            def read(self, path):
                if self.writes:
                    self.wall += .1  # Own synthetic CPU cost; no real sleep/process.
                return super().read(path)
        world = SlowWorld(0)
        value = world.run()
        self.assertIn("independent 3s wall watchdog", value["error"])
        phase = value["phases"][-1]
        timing = phase["timing"]
        self.assertAlmostEqual(timing["deadline"] - timing["phase_begin"], 3)
        self.assertGreaterEqual(timing["last_deadline_check"], timing["deadline"])
        self.assertLess(timing["last_poll"]["game_ms"], timing["target_game_ms"])
        self.assertGreaterEqual(value["failure_wall"]["at"], timing["deadline"])
        self.assertEqual(value["failure_wall"]["phase"], phase["name"])
        self.assertEqual(world.axes, [[0, 0], [0, 0]])

    def test_final_neutral_watchdog_keeps_six_completed_phases_and_exact_failure_metadata(self):
        class FinalSlowWorld(World):
            def read(self, path):
                if min(self.count.values()) >= 16:
                    self.wall += .1  # Cost begins only at final neutral dispatch.
                return super().read(path)
        world = FinalSlowWorld(0)
        value = world.run("both-return")
        self.assertEqual(value["error"], "TimeoutError: neutral_after: independent 3s wall watchdog")
        self.assertEqual(len(value["phases"]), 7)
        self.assertTrue(all("end_game_ms" in phase for phase in value["phases"][:6]))
        phase = value["phases"][-1]
        self.assertNotIn("end_game_ms", phase)
        self.assertEqual(value["failure_wall"]["phase"], "neutral_after")
        timing = phase["timing"]
        self.assertAlmostEqual(timing["deadline"] - timing["phase_begin"], 3)
        self.assertGreaterEqual(timing["last_deadline_check"], timing["deadline"])
        self.assertEqual(timing["target_game_ms"], phase["start_game_ms"] + 2000)
        self.assertLess(timing["last_poll"]["game_ms"], timing["target_game_ms"])
        self.assertEqual(set(timing["last_poll"]["native_ticks"]), set(bot.ROLES))
        self.assertEqual(timing["poll_started"], timing["last_poll"]["begin"])
        self.assertGreaterEqual(value["failure_wall"]["at"], timing["deadline"])
        self.assertTrue(all(event["consumed"] for event in value["emergency_release"].values()))
        self.assertFalse(value["forced_stop"])
        self.assertEqual(world.axes, [[0, 0], [0, 0]])

    def test_both_return_traverses_previous_paths_and_releases_both(self):
        world=World(0)
        value=world.run("both-return")
        self.assertIsNone(value["error"])
        self.assertEqual(reader.ledger_checks(value)[1],[])
        self.assertEqual(value["phases"][5]["dispatch"]["host"]["commands"],reader.RELEASE+"+back\n")
        self.assertEqual(value["phases"][5]["dispatch"]["client"]["commands"],reader.RELEASE+"+moveleft\n")
        self.assertAlmostEqual(world.positions[0][0],0.,places=6)
        self.assertAlmostEqual(world.positions[1][1],30.,places=6)
        self.assertEqual(world.axes,[[0,0],[0,0]])
        self.assertTrue(value["all_releases_consumed"])
    def test_complete_campaign_mode0_with_real_ack_contract(self):
        world = World(0)
        value = world.run()
        self.assertIsNone(value["error"])
        self.assertTrue(value["all_releases_consumed"])
        self.assertEqual(len(value["phases"]), 7)
        self.assertEqual(len(value["mailboxes"]), 30)
        self.assertEqual(reader.ledger_checks(value)[1], [])
        self.assertTrue(all(x["consumed"] for x in value["mailboxes"]))
        self.assertEqual(world.axes, [[0, 0], [0, 0]])
        self.assertFalse(any(p.terminated for p in world.processes.values()))
        self.assertEqual(value["pattern"], "original-right")
        self.assertFalse(any("+moveleft\n" in commands for _, commands in world.writes))

    def test_return_left_explicit_ledger_and_actual_negative_fake_native_axes(self):
        world = World(0)
        value = world.run("client-return-left")
        self.assertIsNone(value["error"])
        self.assertEqual(value["pattern"], "client-return-left")
        self.assertEqual(reader.ledger_checks(value)[1], [])
        self.assertEqual(len(value["mailboxes"]), 30)
        self.assertEqual(value["phases"][3]["dispatch"]["client"]["commands"], reader.RELEASE + "+moveright\n")
        self.assertEqual(value["phases"][5]["dispatch"]["client"]["commands"], reader.RELEASE + "+moveleft\n")
        self.assertAlmostEqual(world.positions[1][1], 30., places=6)
        self.assertEqual(world.axes, [[0, 0], [0, 0]])
        self.assertTrue(value["all_releases_consumed"])
        self.assertFalse(any(p.terminated for p in world.processes.values()))

    def test_unknown_pattern_refuses_before_any_dispatch(self):
        for pattern in (None, "", "return-left", [], 1):
            world = World()
            with self.subTest(pattern=pattern), self.assertRaises(ValueError):
                world.run(pattern)
            self.assertEqual(world.writes, [])
            self.assertFalse(any(p.terminated for p in world.processes.values()))

    def test_complete_campaign_mode1_is_explicitly_supported(self):
        self.assertIsNone(World(1).run()["error"])

    def test_live_mode_asymmetry_fails_without_positive_input(self):
        world = World(0)
        world.client_mode = 1
        value = world.run()
        self.assertIsNotNone(value["error"])
        self.assertFalse(any("+forward" in c or "+moveright" in c for _, c in world.writes))
        self.assertEqual(world.axes, [[0, 0], [0, 0]])

    def test_frozen_game_clock_times_out_and_releases(self):
        world = World()
        world.freeze = True
        value = world.run()
        self.assertIn("TimeoutError", value["error"])
        self.assertLessEqual(world.wall, 4.1)
        self.assertEqual(world.axes, [[0, 0], [0, 0]])

    def test_pending_mailbox_never_overwritten_and_only_retained_handles_stopped(self):
        world = World()
        world.stuck = "client"
        value = world.run()
        self.assertIsNotNone(value["error"])
        self.assertEqual(value["forced_stop"], ["host", "client"])
        self.assertFalse(any(r == "client" for r, _ in world.writes))
        self.assertTrue(all(p.terminated for p in world.processes.values()))
        self.assertLessEqual(world.wall, 5.2)

    def test_ack_bytes_mismatch_fails_and_release_is_still_sent(self):
        world = World()
        world.bad_bytes = True
        value = world.run()
        self.assertIn("bytes differ", value["error"])
        self.assertTrue(all(e["consumed"] for e in value["emergency_release"].values()))
        self.assertEqual(world.axes, [[0, 0], [0, 0]])

    def test_stuck_first_role_does_not_delay_second_safety_release(self):
        world = World()
        world.stuck = "host"
        value = world.run()
        self.assertIsNotNone(value["error"])
        client_writes = [t for role, t in world.write_times if role == "client"]
        self.assertTrue(client_writes)
        self.assertLessEqual(client_writes[0], 4.1)
        self.assertTrue(value["emergency_release"]["client"]["consumed"])

    def test_dead_client_never_gets_positive_dispatch(self):
        world = World()
        world.processes["client"].code = 1
        value = world.run()
        self.assertIsNotNone(value["error"])
        self.assertFalse(any("+" in commands for _, commands in world.writes))
        self.assertEqual(value["forced_stop"], ["host"])
        self.assertFalse(world.processes["client"].terminated)


class SinglePassTelemetryTests(unittest.TestCase):
    def reference(self, text, role="host"):
        # Previous full-window validation semantics, retained as an own oracle.
        rows, anchor, errors = bot.shared.parse_log(text)
        if errors:
            raise ValueError(f"{role}: ambiguous completed native telemetry")
        latest = {}
        for row in rows:
            kind = row["kind"]
            key = (kind, row["fields"].get("slot")) if kind in ("input", "roster") else kind
            latest[key] = {key: row[key] for key in ("kind", "tick", "fields")}
        acks = []
        for tick, text, _ in bot.shared.control_tick_records(text):
            match = bot.shared.MAILBOX.fullmatch(text)
            if match:
                acks.append({"tick": tick, "count": int(match[1]), "bytes": int(match[2])})
        return latest, anchor, acks

    def test_valid_window_equivalence_with_wrap_unknown_rows_and_acks(self):
        world = World(0)
        variants = [world.read(Path("fixture-host")),
            "tick=4294967290 tid=1 own unparsed body key=1 key=2\n"
            "tick=4294967290 tid=1 mailbox enqueued count=0008 bytes=36; execution requires native state evidence\n"
            "tick=5 tid=1 input slot=0 level_time=22\n",
            "ignored line\ntick=2 tid=0 roster slot=9 occupied=1\n"
            "tick=2 tid=0 frame=1 enqueued_mailboxes=5 map=trainer\n"]
        for text in variants:
            with self.subTest(text=text[:60]):
                self.assertEqual(bot.latest_telemetry(text, "host"), self.reference(text))

    def test_full_window_rejects_old_malformed_rows_even_when_latest_are_healthy(self):
        healthy = World(0).read(Path("fixture-host"))
        prefixes = ["tick=oops tid=1 old\n", "tick=99999 tid=4294967296 old\n",
            "tick=99999 tid=1 input slot=0 slot=1 level_time=1\n",
            "tick=100001 tid=1 old\n", "tick=99999 tid=1 roster slot=0 slot=0\n",
            "tick=99999 tid=1 frame=0 enqueued_mailboxes=1 map=a map=b\n"]
        for prefix in prefixes:
            text = prefix + healthy
            with self.subTest(prefix=prefix):
                with self.assertRaisesRegex(ValueError, "ambiguous completed native telemetry"):
                    self.reference(text)
                with self.assertRaisesRegex(ValueError, "ambiguous completed native telemetry"):
                    bot.snapshot(text, "host", "trainer", 0)

    def test_duplicate_validation_is_kept_for_all_older_relevant_rows(self):
        healthy = World(0).read(Path("fixture-host"))
        padding = "tick=99999 tid=1 own padding data=ok\n" * 11000
        text = "tick=99998 tid=1 input slot=0 slot=0\n" + padding + healthy
        self.assertGreater(len(text), 300000)
        with self.assertRaisesRegex(ValueError, "ambiguous completed native telemetry"):
            bot.snapshot(text, "host", "trainer", 0)

    def test_single_pass_does_not_call_either_old_complete_parser(self):
        from unittest.mock import patch
        text = World(0).read(Path("fixture-host"))
        with patch.object(bot.shared, "parse_log", side_effect=AssertionError("duplicate full parse")), \
                patch.object(bot.shared, "control_tick_records", side_effect=AssertionError("duplicate ACK pass")):
            self.assertEqual(bot.snapshot(text, "host", "trainer", 0)["level"], 12000)

    def test_read_tail_retains_exact_512kb_and_completed_line_rules(self):
        here = Path(__file__).resolve().parent
        with tempfile.TemporaryDirectory(prefix="own-poll-tail-fixture-", dir=here) as name:
            path = Path(name) / "own-control.log"
            data = b"own padding line\n" * 40000 + b"completed own line\npartial tick=bad"
            path.write_bytes(data)
            expected = data[-512 * 1024:].partition(b"\n")[2]
            expected = expected.rpartition(b"\n")[0] + b"\n"
            self.assertEqual(bot.read_tail(path), expected.decode("utf-8"))
            path.write_bytes(b"old own row\n\xff\n")
            with self.assertRaises(UnicodeDecodeError):
                bot.read_tail(path)


if __name__ == "__main__":
    unittest.main()
