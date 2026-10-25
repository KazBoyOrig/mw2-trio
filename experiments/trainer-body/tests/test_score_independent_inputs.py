"""Pure own telemetry/ledger fixtures; no game/process/network operation."""
import copy
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "qa"))
import score_independent_inputs as reader
import score_shared_map as shared
from test_score_shared_map import make_log


def records(phase="host_forward", interval=100, pattern="original-right"):
    result = {}
    for role in ("host", "client"):
        template, _, errors = shared.parse_log(make_log(role, count=1))
        assert not errors
        rows = []
        for time in range(0, 1001, interval):
            for original in template:
                row = copy.deepcopy(original)
                row["tick"] = 100000 + time
                f = row["fields"]
                if row["kind"] == "map":
                    f.update(map=reader.MAP, specialops="1", cl_paused="0")
                if row["kind"] == "input":
                    slot = int(f["slot"])
                    axes = reader.expected_axis(phase, slot, pattern)
                    f.update(level_time=str(time), server_cmd_time=str(time), applied_cmd_time=str(time),
                             server_axes=",".join(str(x * 127) for x in axes),
                             applied_axes=",".join(str(x * 127) for x in axes),
                             origin=f"{axes[0] * time * .19},{slot * 30 + axes[1] * time * .19},0")
                rows.append(row)
        result[role] = rows
    return result


def phase(name="host_forward"):
    return {"name": name, "start_game_ms": 0, "end_game_ms": 1000}


def ledger(pattern=None):
    phases = []
    for i, name in enumerate(reader.PHASES):
        phases.append({"name": name, "start_game_ms": i * 1000, "end_game_ms": (i + 1) * 1000,
                       "wall_ms": 1500,
                       "dispatch": {r: {"commands": reader.command_for(name, r, pattern or "original-right"), "consumed": True}
                                    for r in ("host", "client")},
                       "release": {r: {"commands": reader.RELEASE, "consumed": True}
                                   for r in ("host", "client")}})
    value = {"version": 1, "map": reader.MAP, "owner_slots": {"host": 0, "client": 1},
            "input_method": "native-console-kbutton", "error": None,
            "all_releases_consumed": True, "phases": phases}
    if pattern is not None:
        value["pattern"] = pattern
    return value


class IndependentInputTests(unittest.TestCase):
    def test_both_return_preserves_solo_inputs_and_requires_reverse_owned_axes(self):
        p = "both-return"
        self.assertEqual(reader.command_for("host_forward","host",p),reader.RELEASE+"+forward\n")
        self.assertEqual(reader.command_for("client_right","client",p),reader.RELEASE+"+moveright\n")
        self.assertEqual(reader.command_for("distinct_both","host",p),reader.RELEASE+"+back\n")
        self.assertEqual(reader.expected_axis("distinct_both",0,p),(-1,0))
        self.assertEqual(reader.expected_axis("distinct_both",1,p),(0,-1))
        self.assertEqual(reader.ledger_checks(ledger(p))[1],[])
        self.assertTrue(reader.phase_score(records("distinct_both",pattern=p),phase("distinct_both"),pattern=p)["pass"])
        self.assertFalse(reader.phase_score(records("distinct_both",pattern="client-return-left"),phase("distinct_both"),pattern=p)["pass"])
    def test_four_positive_patterns(self):
        for name in ("neutral_before", "host_forward", "client_right", "distinct_both"):
            with self.subTest(name=name):
                self.assertTrue(reader.phase_score(records(name), phase(name))["pass"])

    def test_return_left_preserves_first_right_and_reverses_only_distinct_phase(self):
        pattern = "client-return-left"
        self.assertEqual(reader.command_for("client_right", "client", pattern), reader.RELEASE + "+moveright\n")
        self.assertEqual(reader.command_for("distinct_both", "client", pattern), reader.RELEASE + "+moveleft\n")
        self.assertEqual(reader.expected_axis("distinct_both", 1, pattern), (0, -1))
        for name in reader.PHASES:
            with self.subTest(name=name):
                self.assertTrue(reader.phase_score(records(name, pattern=pattern), phase(name), pattern=pattern)["pass"])

    def test_signed_axis_bounds_preserve_positive_and_neutral_contracts(self):
        for text in ("0,-127", "0,-96"):
            self.assertTrue(reader.matches_axis(text, (0, -1)))
        for text in ("0,-128", "0,-95", "0,0", "0,96", "0,127", "-127,0", "0,-127,0"):
            self.assertFalse(reader.matches_axis(text, (0, -1)))
        self.assertFalse(reader.matches_axis("0,-127", (0, 1)))
        self.assertFalse(reader.matches_axis("-127,0", (1, 0)))
        self.assertFalse(reader.matches_axis("0,-127", (0, 0)))

    def test_return_left_refuses_positive_or_wrong_negative_native_axes(self):
        pattern = "client-return-left"
        for axes in ("0,127", "0,-95", "0,-128", "-127,0"):
            rows = records("distinct_both", pattern=pattern)
            for group in rows.values():
                for row in group:
                    if row["kind"] == "input" and row["fields"]["slot"] == "1":
                        row["fields"].update(server_axes=axes, applied_axes=axes)
            with self.subTest(axes=axes):
                self.assertFalse(reader.phase_score(rows, phase("distinct_both"), pattern=pattern)["pass"])

    def test_return_left_still_refuses_collision_despite_correct_negative_axes(self):
        pattern = "client-return-left"
        rows = records("distinct_both", pattern=pattern)
        for group in rows.values():
            for row in group:
                if row["kind"] == "input" and row["fields"]["slot"] == "1":
                    row["fields"]["origin"] = "0,30,0"
        result = reader.phase_score(rows, phase("distinct_both"), pattern=pattern)
        self.assertTrue(result["slots"]["1"]["checks"]["all_interior_axes_owner_pattern"])
        self.assertFalse(result["slots"]["1"]["checks"]["movement_matches_active_or_neutral_slot"])
        self.assertFalse(result["pass"])

    def test_historical_default_refuses_return_left_trace(self):
        rows = records("distinct_both", pattern="client-return-left")
        self.assertFalse(reader.phase_score(rows, phase("distinct_both"))["pass"])

    def test_return_left_ledger_and_command_pattern_are_bound(self):
        value = ledger("client-return-left")
        self.assertEqual(reader.ledger_checks(value)[1], [])
        value["pattern"] = "original-right"
        self.assertTrue(reader.ledger_checks(value)[1])
        value = ledger("client-return-left")
        value["phases"][5]["dispatch"]["client"]["commands"] = reader.command_for("distinct_both", "client")
        self.assertTrue(reader.ledger_checks(value)[1])

    def test_unknown_patterns_refuse_without_reinterpreting_historical_ledgers(self):
        historical = ledger()
        self.assertNotIn("pattern", historical)
        self.assertEqual(reader.ledger_checks(historical)[1], [])
        for pattern in (None, "", "return-left", "ORIGINAL-RIGHT", [], 1):
            value = ledger()
            value["pattern"] = pattern
            with self.subTest(pattern=pattern):
                self.assertTrue(reader.ledger_checks(value)[1])
        with self.assertRaises(ValueError):
            reader.command_for("distinct_both", "client", "unknown")
        with self.assertRaises(ValueError):
            reader.expected_axis("distinct_both", 1, "unknown")

    def test_mirrored_stream_fails(self):
        rows = records()
        for group in rows.values():
            for r in group:
                if r["kind"] == "input" and r["fields"]["slot"] == "1":
                    r["fields"].update(server_axes="127,0", applied_axes="127,0")
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_swapped_world_slots_fail(self):
        rows = records()
        for r in rows["client"]:
            if r["kind"] == "input":
                r["fields"]["slot"] = str(1 - int(r["fields"]["slot"]))
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_changed_hash_only_does_not_prove_input(self):
        rows = records()
        for group in rows.values():
            for r in group:
                if r["kind"] == "input":
                    r["fields"].update(server_axes="0,0", applied_axes="0,0", applied_cmd_hash="different")
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_cross_world_position_difference(self):
        rows = records()
        for r in rows["client"]:
            if r["kind"] == "input":
                r["fields"]["origin"] = "500,500,0"
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_command_time_offset_needs_capture_not_inference(self):
        rows = records()
        for r in rows["client"]:
            if r["kind"] == "input":
                r["fields"]["applied_cmd_time"] = str(int(r["fields"]["applied_cmd_time"]) + 50)
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_active_axes_without_motion_is_partial_only(self):
        rows = records()
        for group in rows.values():
            for r in group:
                if r["kind"] == "input":
                    r["fields"]["origin"] = "0,0,0"
        result = reader.phase_score(rows, phase())
        self.assertTrue(result["slots"]["0"]["checks"]["all_interior_axes_owner_pattern"])
        self.assertFalse(result["pass"])

    def test_neutral_out_and_back_motion_is_not_idle(self):
        rows = records()
        for group in rows.values():
            for row in group:
                if row["kind"] == "input" and row["fields"]["slot"] == "1":
                    level = int(row["fields"]["level_time"])
                    row["fields"]["origin"] = "0,60,0" if level == 500 else "0,30,0"
        result = reader.phase_score(rows, phase())
        slot = result["slots"]["1"]
        self.assertEqual(slot["motion_units"], 0)
        self.assertEqual(slot["max_excursion_units"], 30)
        self.assertTrue(slot["checks"]["all_interior_axes_owner_pattern"])
        self.assertTrue(slot["checks"]["bounded_motion_without_large_step"])
        self.assertFalse(slot["checks"]["movement_matches_active_or_neutral_slot"])
        self.assertFalse(result["pass"])

    def test_unpaired_neutral_excursion_is_not_ignored(self):
        rows = records()
        extra = copy.deepcopy(next(row for row in rows["host"]
                                   if row["kind"] == "input" and row["fields"]["slot"] == "1"))
        extra["tick"] = 100450
        extra["fields"].update(level_time="450", server_cmd_time="450", applied_cmd_time="450", origin="0,60,0")
        rows["host"].append(extra)
        result = reader.phase_score(rows, phase())
        slot = result["slots"]["1"]
        self.assertTrue(slot["checks"]["same_world_positions_at_exact_time"])
        self.assertEqual(slot["max_excursion_units"], 30)
        self.assertFalse(slot["checks"]["movement_matches_active_or_neutral_slot"])
        self.assertFalse(result["pass"])

    def test_neutral_excursion_at_tolerance_still_passes(self):
        rows = records()
        for group in rows.values():
            for row in group:
                if row["kind"] == "input" and row["fields"]["slot"] == "1":
                    level = int(row["fields"]["level_time"])
                    row["fields"]["origin"] = "0,34,0" if level == 500 else "0,30,0"
        result = reader.phase_score(rows, phase())
        self.assertEqual(result["slots"]["1"]["max_excursion_units"], 4)
        self.assertTrue(result["pass"])

    def test_replayed_frozen_command_times_are_not_stream_proof(self):
        rows = records()
        for group in rows.values():
            for r in group:
                if r["kind"] == "input":
                    r["fields"].update(server_cmd_time="0", applied_cmd_time="0")
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_huge_float_is_bounded_not_json_crash(self):
        rows = records()
        for r in rows["client"]:
            if r["kind"] == "input":
                r["fields"]["origin"] = "1e308,0,0"
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_sparse_capture_refused(self):
        for interval in (250, 1000):
            self.assertFalse(reader.phase_score(records(interval=interval), phase())["pass"])

    def test_paused_context_refused(self):
        rows = records()
        for r in rows["host"]:
            if r["kind"] == "map":
                r["fields"]["cl_paused"] = "1"
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_party_not_ready_during_phase_refused(self):
        rows = records()
        for r in rows["client"]:
            if r["kind"] == "party":
                r["fields"]["client_state"] = "1"
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_bad_indices_refused(self):
        rows = records()
        for r in rows["host"]:
            if r["kind"] == "map":
                r["fields"]["raw_client1_index"] = "0"
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_missing_uid_binding_refused(self):
        rows = records()
        rows["client"] = [r for r in rows["client"] if r["kind"] != "roster"]
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_conflicting_same_frame(self):
        rows = records()
        r = copy.deepcopy(next(r for r in rows["host"] if r["kind"] == "input" and r["tick"] == 100500))
        r["fields"]["origin"] = "99,99,99"
        rows["host"].append(r)
        self.assertFalse(reader.phase_score(rows, phase())["pass"])

    def test_ledger_positive(self):
        phases, errors = reader.ledger_checks(ledger())
        self.assertEqual(len(phases), 7)
        self.assertEqual(errors, [])

    def test_bad_ledger_and_lost_release(self):
        for value in ([], {}, {"version": True}, {"phases": [None]}):
            self.assertTrue(reader.ledger_checks(value)[1])
        value = ledger()
        value["phases"][1]["release"]["host"]["consumed"] = False
        self.assertTrue(reader.ledger_checks(value)[1])

    def test_hold_wall_bound(self):
        value = ledger()
        value["phases"][1]["wall_ms"] = 3001
        self.assertTrue(reader.ledger_checks(value)[1])

    def test_trainer_campaign_mode0_parameterization(self):
        value = ledger()
        value.update(map="trainer", specialops=0)
        self.assertEqual(reader.ledger_checks(value)[1], [])
        rows = records()
        for group in rows.values():
            for r in group:
                if r["kind"] == "map":
                    r["fields"].update(map="trainer", specialops="0")
        self.assertTrue(reader.phase_score(rows, phase(), map_name="trainer", specialops=0)["pass"])

    def test_campaign_live_mode_asymmetry_refused(self):
        rows = records()
        for role, group in rows.items():
            for r in group:
                if r["kind"] == "map":
                    r["fields"].update(map="trainer", specialops="0" if role == "host" else "1")
        self.assertFalse(reader.phase_score(rows, phase(), map_name="trainer", specialops=0)["pass"])

    def test_stock_mode0_is_not_the_supported_baseline(self):
        value = ledger()
        value["specialops"] = 0
        self.assertTrue(reader.ledger_checks(value)[1])


if __name__ == "__main__":
    unittest.main()
