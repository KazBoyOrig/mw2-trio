"""Pure synthetic-log regression tests. No game/process/network/file fixture."""
import json
from pathlib import Path
import unittest
from unittest import mock
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "qa"))
import score_shared_map as scorer


def make_log(role="host", count=21, transform=None, tick_base=100_000,
             counter_base=10_000):
    local = 0 if role == "host" else 1
    lines = []
    for i in range(count):
        tick = (tick_base + i * 1000) & scorer.U32
        value = (counter_base + i * 1000) & scorer.U32
        records = [
            "frame=%d enqueued_mailboxes=3 map=trainer sv_running=1 coop=1 specialops=0 "
            "raw_valid=0xff raw_level_maxclients=2 raw_gclient_base=0x0108dc68 "
            "raw_coop_enabled=1 raw_entity0_client=0x0108dc68 raw_entity1_client=0x01098c3c "
            "raw_client0_index=0 raw_client1_index=1" % (i + 1),
            "party host=%d active=0 launch=0 session=0x01bde8b8 session_active=1 "
            "member0=1 member1=1 channel0_type=%d channel1_type=%d raw_launch_count=2 client_state=6 net_port=%d"
            % (role == "host", 2 if local == 0 else 4, 2 if local == 1 else 4,
               28960 if role == "host" else 28961),
        ]
        for slot in range(2):
            records.append("roster slot=%d occupied=1 local_uid_match=%d address_type=%d port=%d"
                           % (slot, slot == local,
                              2 if role == "host" and slot == 0 else 4,
                              0 if role == "host" and slot == 0 else
                              28961 if role == "host" else 28960))
            records.append("input slot=%d level_time=%d server_cmd_time=%d applied_cmd_time=%d "
                           "server_cmd_hash=12345678 applied_cmd_hash=12345678 "
                           "server_axes=0,0 applied_axes=0,0 origin=1.0,2.0,3.0"
                           % (slot, value, value, value))
        for body in records:
            body = transform(i, body) if transform else body
            if body is not None:
                lines.append("tick=%d tid=1234 %s" % (tick, body))
    return "\n".join(lines) + "\n"


def make_peer_fixture():
    roles = ("host", "client")
    configs = {r: ("version=1\nrole=%s\nsession_nonce=00112233445566778899aabbccddeeff\n"
                   "local_ip=10.23.45.1\nlocal_port=%d\npeer_ip=10.23.45.1\npeer_port=%d\n"
                   % (r, 28960 if r == "host" else 28961, 28961 if r == "host" else 28960)) for r in roles}
    receipt = {"scope": "bounded diagnostic scenario; no gameplay QA claim",
               "exe_sha256": scorer.EXE_SHA, "dll_sha256": "a" * 64,
               "error": None, "forced_cleanup": [], "exit_codes": {r: 0 for r in roles},
               "all_owned_processes_stopped": True, "executable_hashes_unchanged": True,
               "scenario_evidence_pass": True, "pids": {"host": 100, "client": 101},
               "scenario_clock_started_after_all_ready": True,
               "gameslot_registration": {"host": "REGISTERED 100 on 2 lease(s)",
                                         "client": "REGISTERED 101 on 2 lease(s)"},
               "readiness": {r: {"elapsed": 6.0, "evidence": scorer.READINESS} for r in roles},
               "scenario": {"version": 1, "instances": list(roles), "readiness_seconds": 90,
                            "seconds": 100, "flags": [scorer.PEER_FLAG, "mw2-trio-control.enable",
                                "mw2-trio-network-test.enable", "mw2-trio-party-test.enable",
                                "mw2-trio-two-player-test.enable"], "peer_configs": configs},
               "observation_end": {"version": 1, "source": "runner_precleanup", "before_any_quit": True,
                                    "global_tick": 120080, "last_control_ticks": {r: 120000 for r in roles},
                                    "mailboxes_empty": {r: True for r in roles}}, "steps_sent": []}
    texts, artifacts = {}, {}
    for role in roles:
        port, peer_port = (28960, 28961) if role == "host" else (28961, 28960)
        text = "tick=97000 tid=4000 hook state=active installed=1 active=1 reason=0 error=0 rollback_error=0 protection_error=0 initial_tid=1234\n"
        text += "tick=98000 tid=1234 mailbox enqueued count=1 bytes=%d; execution requires native state evidence\n" % len(scorer.BOOT_COMMAND)
        for i in range(18):
            commands = "set fixture%d %s\n" % (i, role)
            receipt["steps_sent"].append({"instance": role, "commands": commands})
            text += "tick=%d tid=1234 mailbox enqueued count=%d bytes=%d; execution requires native state evidence\n" % (99000+i, i+2, len(commands))
        text += make_log(role, transform=lambda i, b:
                         b + (" native_online=0 identity_present=1 identity_hash=" +
                              ("12345678" if role == "host" else "87654321")) if b.startswith("party ") else b)
        if role == "host":
            text += "tick=121000 tid=1234 frame=100 enqueued_mailboxes=19 map=trainer sv_running=0 coop=1 specialops=0\n"
        text += "tick=%d tid=1234 mailbox enqueued count=20 bytes=5; execution requires native state evidence\n" % (121200 if role == "host" else 120100)
        texts[role] = text
        def party(result, active, opened):
            return ("party184 result=%d registered=1 host=0 join=0 launch=0 rejected=0 connect_result=0 "
                    "open_result=%d socket_present=%d bound_port=%d startup_result=0 qa_result=%d "
                    "qa_active=%d qa_failed_precondition=0 qa_failed_site=0\n"
                    % (result, opened, opened, port if opened else 0, active, active))
        net = scorer.PEER_MARKER + "\n"
        for direction, inner in (("recv", "coopConnect"), ("send", "coopAccept")) if role == "host" else (("send", "coopConnect"), ("recv", "coopAccept")):
            net += "tick=100000 tid=1234 %s command=R bytes=40 result=40 peer=10.23.45.1:%d inner=%s reason=none\n" % (direction, peer_port, inner)
        artifacts[role] = {"config": configs[role].encode(), "flag": b"owned bounded harness\n",
                           "qa_flag_present": False, "party": party(1, 0, 0)+party(7, 1, 0)+party(6, 1, 1),
                           "network": net}
    return receipt, texts, artifacts


def score_peer_fixture(receipt, texts, artifacts):
    own_path = Path(__file__)
    def reader(path, _limit, _root=None):
        if path == own_path:
            return json.dumps(receipt).encode()
        role = "host" if "host" in path.parts else "client"
        saved = artifacts.get(role, {})
        key = {"control184.log": "control", "mw2-trio-peer.cfg": "config", scorer.PEER_FLAG: "flag",
               "party184.log": "party", "network184.log": "network"}.get(path.name)
        if path.name == scorer.QA_FLAG:
            if saved.get("qa_flag_present"):
                return b"owned bounded harness\n"
            raise FileNotFoundError()
        if key == "control":
            return texts[role].encode()
        if key not in saved:
            raise FileNotFoundError()
        return saved[key].encode() if isinstance(saved[key], str) else saved[key]
    with mock.patch.object(scorer, "bounded_read", side_effect=reader):
        return scorer.score_receipt(own_path, "trainer", 0)


class SharedMapTests(unittest.TestCase):
    def score(self, text, role="host"):
        return scorer.score_log(text, role, "trainer", 0)

    def bad(self, transform):
        result = self.score(make_log(transform=transform))
        self.assertFalse(result["pass"])
        return result

    def test_valid_host(self):
        self.assertTrue(self.score(make_log())["pass"])

    def test_valid_client(self):
        self.assertTrue(self.score(make_log("client"), "client")["pass"])

    def test_wrong_client_canonical_type2_refused(self):
        text = make_log("client", transform=lambda i, b:
                        b.replace("address_type=4", "address_type=2")
                        if b.startswith("roster slot=1 ") else b)
        self.assertFalse(self.score(text, "client")["pass"])

    def test_client_canonical_own_member_still_uses_host_port(self):
        text = make_log("client", transform=lambda i, b:
                        b.replace("port=28960", "port=28961")
                        if b.startswith("roster slot=1 ") else b)
        self.assertFalse(self.score(text, "client")["pass"])

    def test_latest_map_overrides_historical(self):
        result = self.bad(lambda i, b: b.replace("map=trainer", "map=") if i == 20 else b)
        self.assertTrue(result["map_loaded_at_least_once"])
        self.assertFalse(result["checks"]["latest_map_expected_and_fresh"])

    def test_latest_receiver_state(self):
        self.bad(lambda i, b: b.replace("client_state=6", "client_state=1") if i == 20 else b)

    def test_latest_member_removed(self):
        self.bad(lambda i, b: b.replace("member1=1", "member1=0") if i == 20 else b)

    def test_capacity_alone_does_not_pass(self):
        self.bad(lambda i, b: None if b.startswith("input ") else b)

    def test_missing_slot1(self):
        self.bad(lambda i, b: None if b.startswith("input slot=1 ") else b)

    def test_frozen_level_even_when_commands_advance(self):
        self.bad(lambda i, b: b.replace("level_time=%d" % (10000 + i * 1000), "level_time=10000"))

    def test_frozen_server_command(self):
        self.bad(lambda i, b: b.replace("server_cmd_time=%d" % (10000 + i * 1000), "server_cmd_time=10000"))

    def test_frozen_applied_command(self):
        self.bad(lambda i, b: b.replace("applied_cmd_time=%d" % (10000 + i * 1000), "applied_cmd_time=10000"))

    def test_latest_five_seconds_stalled(self):
        self.bad(lambda i, b: b.replace(str(10000 + i * 1000), "25000")
                 if i > 15 and b.startswith("input ") else b)

    def test_nonfinite_origin(self):
        self.bad(lambda i, b: b.replace("origin=1.0,2.0,3.0", "origin=nan,2.0,3.0") if i == 20 else b)

    def test_binding_pointer_changed(self):
        self.bad(lambda i, b: b.replace("raw_entity1_client=0x01098c3c", "raw_entity1_client=0x0108dc68") if i == 20 else b)

    def test_index_alias(self):
        self.bad(lambda i, b: b.replace("raw_client1_index=1", "raw_client1_index=0"))

    def test_roster_port_changed(self):
        self.bad(lambda i, b: b.replace("port=28961", "port=28962") if i == 20 else b)

    def test_wrong_local_uid_match(self):
        self.bad(lambda i, b: b.replace("local_uid_match=1", "local_uid_match=0"))

    def test_timestamp_wrap(self):
        self.assertTrue(self.score(make_log(tick_base=scorer.U32 - 10000))["pass"])

    def test_counter_wrap(self):
        self.assertTrue(self.score(make_log(counter_base=scorer.U32 - 10000))["pass"])

    def test_counter_reset(self):
        self.bad(lambda i, b: b.replace("applied_cmd_time=30000", "applied_cmd_time=0") if i == 20 else b)

    def test_duplicate_fields_refused(self):
        self.bad(lambda i, b: b + " member1=1" if i == 20 and b.startswith("party ") else b)

    def test_too_few_samples(self):
        self.assertFalse(self.score(make_log(count=2))["pass"])

    def test_stale_records_despite_later_other_native_log(self):
        text = make_log() + "tick=126000 tid=1234 mailbox enqueued count=4 bytes=5\n"
        self.assertFalse(self.score(text)["pass"])

    def test_unknown_input_slot_refused(self):
        text = make_log() + "tick=120000 tid=1234 input slot=2 level_time=1\n"
        self.assertFalse(self.score(text)["pass"])

    def test_early_other_map_outside_late_window(self):
        text = "tick=1 tid=1234 frame=0 enqueued_mailboxes=0 map= sv_running=0\n" + make_log()
        self.assertTrue(self.score(text)["pass"])

    def test_malformed_native_tick(self):
        self.assertFalse(self.score(make_log() + "tick=broken tid=1234 input slot=0\n")["pass"])

    def receipt_score(self, **changes):
        receipt = {"exe_sha256": scorer.EXE_SHA, "error": None, "forced_cleanup": [],
                   "exit_codes": {"host": 0, "client": 0},
                   "all_owned_processes_stopped": True, "executable_hashes_unchanged": True,
                   "scenario_evidence_pass": True}
        receipt.update(changes)
        own_path = Path(__file__)
        def reader(path, _limit, _root=None):
            if path == own_path:
                return json.dumps(receipt).encode()
            role = "host" if "host" in path.parts else "client"
            return make_log(role).encode()
        with mock.patch.object(scorer, "bounded_read", side_effect=reader):
            return scorer.score_receipt(own_path, "trainer", 0)

    def test_clean_receipt_and_positive_logs(self):
        result = self.receipt_score()
        self.assertTrue(result["pass"])
        self.assertTrue(result["advancing_native_application_both"])
        self.assertFalse(result["input_independence_proven"])

    def test_crashed_client_after_positive_logs_fails_overall(self):
        result = self.receipt_score(exit_codes={"host": 0, "client": 0xC0000005})
        self.assertTrue(result["advancing_native_application_both"])
        self.assertFalse(result["harness_clean"])
        self.assertFalse(result["pass"])
        self.assertIn("both_owned_exit_codes_zero", result["harness_failures"])

    def test_other_harness_failures_with_positive_logs(self):
        for changes in ({"error": "bounded failure"}, {"forced_cleanup": ["client"]},
                        {"all_owned_processes_stopped": False},
                        {"executable_hashes_unchanged": False},
                        {"scenario_evidence_pass": False}, {"cleanup_errors": {"client": "failure"}}):
            with self.subTest(changes=changes):
                result = self.receipt_score(**changes)
                self.assertTrue(result["advancing_native_application_both"])
                self.assertFalse(result["pass"])

    def teardown_fixture(self, *, marker=None, bad_before=False, derive=True,
                         crash=False, wrong_quit=False, wrap=False):
        receipt = {"exe_sha256": scorer.EXE_SHA, "error": None, "forced_cleanup": [],
                   "exit_codes": {"host": 0, "client": 0xC0000005 if crash else 0},
                   "all_owned_processes_stopped": True, "executable_hashes_unchanged": True,
                   "scenario_evidence_pass": True, "steps_sent": [],
                   "pids": {"host": 100, "client": 101},
                   "scenario_clock_started_after_all_ready": True,
                   "scenario": {"instances": ["host", "client"],
                                "flags": ["mw2-trio-loopback-qa.enable"]}}
        if marker is not None:
            receipt["observation_end"] = marker
        base = scorer.U32 - 10000 if wrap else 100000
        logs = {}
        for role in ("host", "client"):
            def tick(offset):
                return (base + offset) & scorer.U32
            text = "tick=%d tid=1234 mailbox enqueued count=1 bytes=%d; execution requires native state evidence\n" % (tick(-1000), len(scorer.BOOT_COMMAND))
            text += make_log(role, tick_base=base)
            if bad_before and role == "host":
                text += "tick=%d tid=1234 frame=99 enqueued_mailboxes=1 map=trainer sv_running=0 coop=1 specialops=0\n" % tick(20050)
            if role == "host":
                text += "tick=%d tid=1234 frame=100 enqueued_mailboxes=1 map=trainer sv_running=0 coop=1 specialops=0\n" % tick(21000)
            text += "tick=%d tid=1234 mailbox enqueued count=2 bytes=%d; execution requires native state evidence\n" % (tick(21200 if role == "host" else 20100), 6 if wrong_quit else 5)
            logs[role] = text
        own_path = Path(__file__)
        def reader(path, _limit, _root=None):
            if path == own_path:
                return json.dumps(receipt).encode()
            return logs["host" if "host" in path.parts else "client"].encode()
        with mock.patch.object(scorer, "bounded_read", side_effect=reader):
            return scorer.score_receipt(own_path, "trainer", 0, derive_own_quit=derive)

    def marker(self, cutoff=120080, host=120000, client=120000):
        return {"version": 1, "source": "runner_precleanup", "before_any_quit": True,
                "global_tick": cutoff, "last_control_ticks": {"host": host, "client": client},
                "mailboxes_empty": {"host": True, "client": True}}

    def test_clean_teardown_derived_boundary_passes(self):
        result = self.teardown_fixture()
        self.assertTrue(result["pass"])
        self.assertEqual(result["observation_boundary"]["cutoff_tick"], 120100)
        self.assertEqual(result["observation_boundary"]["excluded_native_lines"], {"host": 2, "client": 1})

    def test_no_silent_derived_boundary(self):
        result = self.teardown_fixture(derive=False)
        self.assertFalse(result["pass"])
        self.assertFalse(result["observation_boundary"]["applied"])

    def test_failure_before_quit_still_fails(self):
        result = self.teardown_fixture(bad_before=True)
        self.assertFalse(result["pass"])
        self.assertTrue(result["observation_boundary"]["applied"])
        self.assertFalse(result["roles"]["host"]["checks"]["latest_map_expected_and_fresh"])

    def test_crash_metadata_cannot_request_cutoff(self):
        result = self.teardown_fixture(crash=True)
        self.assertFalse(result["pass"])
        self.assertFalse(result["observation_boundary"]["valid"])
        self.assertFalse(result["observation_boundary"]["applied"])

    def test_wrong_quit_bytes_cannot_request_cutoff(self):
        result = self.teardown_fixture(wrong_quit=True)
        self.assertFalse(result["pass"])
        self.assertFalse(result["observation_boundary"]["valid"])

    def test_explicit_marker_passes(self):
        result = self.teardown_fixture(marker=self.marker(), derive=False)
        self.assertTrue(result["pass"])
        self.assertEqual(result["observation_boundary"]["source"], "validated_runner_precleanup_marker")

    def test_same_tick_marker_is_allowed_but_excludes_equal_records(self):
        result = self.teardown_fixture(marker=self.marker(120000), derive=False)
        self.assertTrue(result["pass"])
        self.assertGreater(result["observation_boundary"]["excluded_native_lines"]["host"], 2)

    def test_future_saved_tick_refused(self):
        result = self.teardown_fixture(marker=self.marker(host=120100), derive=False)
        self.assertFalse(result["pass"])
        self.assertFalse(result["observation_boundary"]["valid"])

    def test_marker_after_quit_refused(self):
        result = self.teardown_fixture(marker=self.marker(120101), derive=False)
        self.assertFalse(result["pass"])
        self.assertFalse(result["observation_boundary"]["valid"])

    def test_failure_before_explicit_cutoff_still_fails(self):
        result = self.teardown_fixture(marker=self.marker(), bad_before=True, derive=False)
        self.assertFalse(result["pass"])
        self.assertTrue(result["observation_boundary"]["applied"])

    def test_cutoff_tick_wrap(self):
        result = self.teardown_fixture(wrap=True)
        self.assertTrue(result["pass"])
        self.assertEqual(result["observation_boundary"]["cutoff_tick"], 10099)


class ExactPeerCutoffTests(unittest.TestCase):
    def bad(self, change):
        receipt, texts, artifacts = make_peer_fixture()
        change(receipt, texts, artifacts)
        result = score_peer_fixture(receipt, texts, artifacts)
        self.assertFalse(result["pass"])
        self.assertFalse(result["observation_boundary"]["applied"])
        return result

    def test_valid_exact_peer_same_pc_marker(self):
        receipt, texts, artifacts = make_peer_fixture()
        self.assertEqual(len(receipt["steps_sent"]), 36)
        result = score_peer_fixture(receipt, texts, artifacts)
        self.assertTrue(result["pass"])
        self.assertEqual(result["observation_boundary"]["source"], "validated_runner_precleanup_marker")
        self.assertEqual(result["observation_boundary"]["excluded_native_lines"], {"host": 2, "client": 1})
        self.assertFalse(result["input_independence_proven"])

    def test_missing_own_runner_fields(self):
        for key in ("scope", "dll_sha256", "gameslot_registration", "readiness", "pids",
                    "scenario_clock_started_after_all_ready"):
            with self.subTest(key=key):
                self.bad(lambda r, t, a: r.pop(key))

    def test_both_owned_roles_and_pids_required(self):
        for value in ({"host": 100}, {"host": 100, "client": 100},
                      {"host": True, "client": 101}, {"host": 0, "client": 101},
                      {"host": 100, "client": scorer.U32+1}):
            with self.subTest(value=value):
                self.bad(lambda r, t, a: r.update(pids=value))
        for instances in (["host"], ["host", "client", "client"], ["host", "other"]):
            with self.subTest(instances=instances):
                self.bad(lambda r, t, a: r["scenario"].update(instances=instances))

    def test_both_lease_registrations_must_match_exact_pids(self):
        for role in ("host", "client"):
            for value in (None, "REGISTERED 999 on 2 lease(s)", "REGISTERED 100 on 1 lease(s)", True):
                with self.subTest(role=role, value=value):
                    self.bad(lambda r, t, a: r["gameslot_registration"].update({role: value}))

    def test_readiness_both_fields_and_bounds(self):
        for role in ("host", "client"):
            for field, value in (("elapsed", -1), ("elapsed", 91), ("elapsed", float("nan")),
                                 ("elapsed", True), ("evidence", "remote ready"), ("evidence", None)):
                with self.subTest(role=role, field=field, value=value):
                    self.bad(lambda r, t, a: r["readiness"][role].update({field: value}))
            self.bad(lambda r, t, a: r["readiness"].pop(role))

    def test_runner_time_bounds_types(self):
        for field, value in (("version", True), ("version", 2), ("seconds", 121), ("seconds", 14),
                             ("readiness_seconds", 91), ("readiness_seconds", 14)):
            with self.subTest(field=field, value=value):
                self.bad(lambda r, t, a: r["scenario"].update({field: value}))

    def test_peer_flag_exclusive_and_required_dependencies(self):
        for flag in (scorer.PEER_FLAG, "mw2-trio-control.enable", "mw2-trio-network-test.enable",
                     "mw2-trio-party-test.enable", "mw2-trio-two-player-test.enable"):
            with self.subTest(flag=flag):
                self.bad(lambda r, t, a: r["scenario"]["flags"].remove(flag))
        for extra in (scorer.QA_FLAG, scorer.PEER_FLAG, {}):
            with self.subTest(extra=extra):
                self.bad(lambda r, t, a: r["scenario"]["flags"].append(extra))
        self.bad(lambda r, t, a: r["scenario"].update(flags=None))

    def test_missing_or_wrong_role_config(self):
        for role in ("host", "client"):
            self.bad(lambda r, t, a: r["scenario"]["peer_configs"].pop(role))
            self.bad(lambda r, t, a: r["scenario"]["peer_configs"].update({role: None}))
        self.bad(lambda r, t, a: r["scenario"].pop("peer_configs"))

    def test_strict_config_grammar(self):
        text = make_peer_fixture()[0]["scenario"]["peer_configs"]["host"]
        cases = ["", text[:-1], "\ufeff"+text, text+"unknown=1\n", text+"role=host\n",
                 text.replace("version=1", "version=2"), text.replace("role=host", "role=client"),
                 text.replace("local_port=28960", "local_port=028960"),
                 text.replace("local_port=28960", "local_port=0"),
                 text.replace("local_port=28960", "local_port=65536"),
                 text.replace("local_port=28960", "local_port=-1"),
                 text.replace("local_ip=10.23.45.1", "local_ip=10.23.045.1"),
                 text.replace("local_ip=10.23.45.1", "local_ip=localhost"),
                 text.replace("local_ip=10.23.45.1", "local_ip=127.0.0.2"),
                 text.replace("local_ip=10.23.45.1", "local_ip=0.1.2.3"),
                 text.replace("local_ip=10.23.45.1", "local_ip=224.1.2.3"),
                 text.replace("local_ip=10.23.45.1", "local_ip=10.23.45.255"),
                 text.replace("local_ip=10.23.45.1", "local_ip=169.254.1.2"),
                 text.replace("local_ip=10.23.45.1", "local_ip=10.23.45.256"),
                 text.replace("00112233445566778899aabbccddeeff", "0"*32),
                 text.replace("00112233445566778899aabbccddeeff", "z"*32),
                 text.replace("role=host", "role=host "), text.replace("role=host", "role==host"),
                 text.replace("version=1\n", "version=1\r\n"), text+"x"*257]
        for i, malformed in enumerate(cases):
            def change(r, t, a):
                r["scenario"]["peer_configs"]["host"] = malformed
                a["host"]["config"] = malformed.encode("utf-8")
            with self.subTest(case=i):
                self.bad(change)
        for key in scorer.peer_config(text):
            malformed = "\n".join(x for x in text.split("\n") if not x.startswith(key+"="))
            with self.subTest(missing=key):
                with self.assertRaises(ValueError):
                    scorer.peer_config(malformed)

    def test_opposite_tuple_and_nonce_required(self):
        for before, after in (("role=client", "role=host"), ("peer_port=28960", "peer_port=28962"),
                              ("local_port=28961", "local_port=28960"),
                              ("00112233445566778899aabbccddeeff", "10112233445566778899aabbccddeeff")):
            def change(r, t, a):
                config = r["scenario"]["peer_configs"]["client"].replace(before, after)
                r["scenario"]["peer_configs"]["client"] = config
                a["client"]["config"] = config.encode()
            with self.subTest(before=before):
                self.bad(change)

    def test_valid_remote_pair_cannot_share_local_cutoff(self):
        def change(r, t, a):
            h = r["scenario"]["peer_configs"]["host"].replace("peer_ip=10.23.45.1", "peer_ip=10.23.45.2")
            c = r["scenario"]["peer_configs"]["client"].replace("local_ip=10.23.45.1", "local_ip=10.23.45.2")
            for role, config in (("host", h), ("client", c)):
                r["scenario"]["peer_configs"][role] = config
                a[role]["config"] = config.encode()
        result = self.bad(change)
        self.assertIn("same-PC", result["observation_boundary"]["reason"])

    def test_saved_artifacts_must_exist_and_match(self):
        for role in ("host", "client"):
            for key in ("config", "flag", "party", "network"):
                with self.subTest(role=role, key=key):
                    self.bad(lambda r, t, a: a[role].pop(key))
            self.bad(lambda r, t, a: a[role].update(config=a[role]["config"]+b"\n"))
            self.bad(lambda r, t, a: a[role].update(flag=b"manual"))
            self.bad(lambda r, t, a: a[role].update(qa_flag_present=True))

    def test_cached_id_must_precede_native_open(self):
        def change(r, t, a):
            lines = a["host"]["party"].splitlines()
            lines[1], lines[2] = lines[2], lines[1]
            a["host"]["party"] = "\n".join(lines)+"\n"
        self.bad(change)

    def test_cached_id_opener_failure_or_lost_state(self):
        for role in ("host", "client"):
            for before, after in (("qa_result=1", "qa_result=0"), ("qa_active=1", "qa_active=0"),
                                  ("qa_failed_site=0", "qa_failed_site=1"),
                                  ("qa_failed_precondition=0", "qa_failed_precondition=1"),
                                  ("socket_present=1", "socket_present=0"),
                                  ("open_result=1", "open_result=0"), ("startup_result=0", "startup_result=1"),
                                  ("registered=1", "registered=0"), ("rejected=0", "rejected=1")):
                with self.subTest(role=role, before=before):
                    self.bad(lambda r, t, a: a[role].update(party=a[role]["party"].replace(before, after)))
            self.bad(lambda r, t, a: a[role].update(party=a[role]["party"].replace("bound_port="+str(28960 if role == "host" else 28961), "bound_port=29999")))

    def test_network_latch_and_provider_condition_required(self):
        for role in ("host", "client"):
            for replacement in ("", scorer.PEER_MARKER.replace("unchanged", "changed"),
                                scorer.PEER_MARKER+"\n"+scorer.PEER_MARKER,
                                scorer.PEER_MARKER+"\nQA boundary latched: IPv4 127.0.0.2 only until process exit"):
                with self.subTest(role=role, replacement=replacement):
                    self.bad(lambda r, t, a: a[role].update(network=a[role]["network"].replace(scorer.PEER_MARKER, replacement)))

    def test_network_both_successful_exact_admission_directions_required(self):
        for role in ("host", "client"):
            for before, after in (("peer=10.23.45.1:", "peer=10.23.45.2:"),
                                  ("result=40", "result=0"), ("reason=none", "reason=unknown"),
                                  ("inner=coopAccept", "inner=unknown"), ("inner=coopConnect", "inner=unknown"),
                                  ("tick=100000", "tick=broken")):
                with self.subTest(role=role, before=before):
                    self.bad(lambda r, t, a: a[role].update(network=a[role]["network"].replace(before, after)))

    def test_offline_current_identity_and_port_required(self):
        for role in ("host", "client"):
            for before, after in (("native_online=0", "native_online=1"),
                                  ("identity_present=1", "identity_present=0"),
                                  ("identity_hash=", "identity_wrong=")):
                with self.subTest(role=role, before=before):
                    self.bad(lambda r, t, a: t.update({role: t[role].replace(before, after)}))
        self.bad(lambda r, t, a: t.update(client=t["client"].replace("identity_hash=87654321", "identity_hash=12345678")))

    def test_native_owner_hook_must_match(self):
        for before, after in (("hook state=active", "hook state=failed"), ("initial_tid=1234", "initial_tid=9999"),
                              ("rollback_error=0", "rollback_error=1"), ("protection_error=0", "protection_error=1")):
            with self.subTest(before=before):
                self.bad(lambda r, t, a: t.update(host=t["host"].replace(before, after)))

    def test_marker_stays_strict_for_peer(self):
        for field, value in (("before_any_quit", False), ("source", "remote_utc"), ("global_tick", 120101),
                             ("last_control_ticks", {"host": 120000}), ("mailboxes_empty", {"host": True, "client": False})):
            with self.subTest(field=field):
                self.bad(lambda r, t, a: r["observation_end"].update({field: value}))

    def test_no_inferred_peer_boundary_without_explicit_runner_marker(self):
        r, t, a = make_peer_fixture()
        r.pop("observation_end")
        result = scorer.observation_boundary(r, t, derive_own_quit=True, peer_artifacts=a)
        self.assertFalse(result["valid"])
        self.assertFalse(result["applied"])

    def test_all36_mailboxes_and_exact_final_quit_still_required(self):
        for before, after in (("count=20 bytes=5", "count=20 bytes=6"),
                              ("count=20 bytes=5", "count=21 bytes=5")):
            with self.subTest(before=before):
                self.bad(lambda r, t, a: t.update(host=t["host"].replace(before, after)))
        def wrong_step_bytes(r, t, a):
            expected = len(r["steps_sent"][0]["commands"])
            before = "count=2 bytes=%d" % expected
            self.assertIn(before, t["host"])
            t["host"] = t["host"].replace(before, "count=2 bytes=%d" % (expected+1))
        self.bad(wrong_step_bytes)
        self.bad(lambda r, t, a: r["steps_sent"].pop(0))
        self.bad(lambda r, t, a: r["steps_sent"][0].update(commands="quit\n"))

    def test_precleanup_failure_and_stall_are_still_failures(self):
        for before, after in (("applied_cmd_time=30000", "applied_cmd_time=25000"),
                              ("raw_entity1_client=0x01098c3c", "raw_entity1_client=0x0108dc68")):
            r, t, a = make_peer_fixture()
            t["host"] = t["host"].replace(before, after)
            result = score_peer_fixture(r, t, a)
            self.assertTrue(result["observation_boundary"]["applied"])
            self.assertFalse(result["pass"])

    def test_numeric_vpn_address_and_other_exact_ports_are_not_hardcoded(self):
        r, t, a = make_peer_fixture()
        for role in ("host", "client"):
            config = r["scenario"]["peer_configs"][role].replace("10.23.45.1", "26.1.2.3").replace("28960", "29990").replace("28961", "29991")
            r["scenario"]["peer_configs"][role] = config
            a[role]["config"] = config.encode()
            for key in ("party", "network"):
                a[role][key] = a[role][key].replace("10.23.45.1", "26.1.2.3").replace("28960", "29990").replace("28961", "29991")
            t[role] = t[role].replace("28960", "29990").replace("28961", "29991")
        self.assertTrue(score_peer_fixture(r, t, a)["pass"])

    def test_consistent_crlf_config_is_valid(self):
        r, t, a = make_peer_fixture()
        for role in ("host", "client"):
            config = r["scenario"]["peer_configs"][role].replace("\n", "\r\n")
            r["scenario"]["peer_configs"][role] = config
            a[role]["config"] = config.encode()
        self.assertTrue(score_peer_fixture(r, t, a)["pass"])

    def test_windows_runner_flag_newline_is_valid(self):
        r, t, a = make_peer_fixture()
        for role in ("host", "client"):
            a[role]["flag"] = b"owned bounded harness\r\n"
        self.assertTrue(score_peer_fixture(r, t, a)["pass"])


if __name__ == "__main__":
    unittest.main()
