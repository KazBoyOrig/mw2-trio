"""Private stock-map native input reader candidate; own code, MIT.

No control, process or network API. Requires a parent-generated dispatch ledger
and sufficiently dense exact-game-time samples; refuses sparse/ambiguous proof.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
import sys

import score_shared_map as shared

MAP = "so_killspree_trainer"
MAPS = (MAP, "trainer")
PATTERNS = ("original-right", "client-return-left", "both-return")
RELEASE = "-forward\n-back\n-moveleft\n-moveright\n"
PHASES = ("neutral_before", "host_forward", "neutral_between1", "client_right",
          "neutral_between2", "distinct_both", "neutral_after")
EDGE_MS = 200
MIN_PAIRS = 3
MAX_PAIR_GAP_MS = 200


def command_for(phase: str, role: str, pattern: str = "original-right") -> str:
    if pattern not in PATTERNS:
        raise ValueError("unsupported private movement pattern")
    positive = "+forward\n" if role == "host" and phase in ("host_forward", "distinct_both") else ""
    if role == "host" and phase == "distinct_both" and pattern == "both-return":
        positive = "+back\n"
    if role == "client" and phase in ("client_right", "distinct_both"):
        positive = "+moveleft\n" if phase == "distinct_both" and pattern in ("client-return-left", "both-return") else "+moveright\n"
    return RELEASE + positive


def expected_axis(phase: str, slot: int, pattern: str = "original-right") -> tuple[int, int]:
    if pattern not in PATTERNS:
        raise ValueError("unsupported private movement pattern")
    return ((-1 if phase == "distinct_both" and pattern == "both-return" else 1)
            if slot == 0 and phase in ("host_forward", "distinct_both") else 0,
            (-1 if phase == "distinct_both" and pattern in ("client-return-left", "both-return") else 1)
            if slot == 1 and phase in ("client_right", "distinct_both") else 0)


def matches_axis(text: str, expected: tuple[int, int]) -> bool:
    if not re.fullmatch(r"-?\d+,-?\d+", text):
        return False
    values = tuple(int(x) for x in text.split(","))
    return all(96 <= value <= 127 if active == 1 else
               -127 <= value <= -96 if active == -1 else
               value == 0 if active == 0 else False
               for value, active in zip(values, expected))


def distance(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    return math.dist(a, b)


def command_progress(values: list[int]) -> bool:
    deltas = [(b - a) & shared.U32 for a, b in zip(values, values[1:])]
    return (len(set(values)) >= 3 and all(delta < shared.HALF for delta in deltas)
            and sum(deltas) >= 300)


def phase_score(rows: dict[str, list[dict]], phase: dict, *,
                host_port: int = 28960, client_port: int = 28961,
                map_name: str = MAP, specialops: int = 1,
                pattern: str = "original-right") -> dict:
    start, end, name = phase["start_game_ms"], phase["end_game_ms"], phase["name"]
    result = {"name": name, "pass": False, "slots": {}, "failures": []}
    for slot in range(2):
        expected = expected_axis(name, slot, pattern)
        captures: dict[str, dict[tuple[int, int], dict]] = {}
        errors = []
        for role in ("host", "client"):
            indexed = {}
            for row in rows[role]:
                if row["kind"] != "input" or row["fields"].get("slot") != str(slot):
                    continue
                f = row["fields"]
                try:
                    level = shared.integer(f["level_time"])
                    if not start + EDGE_MS <= level <= end - EDGE_MS:
                        continue
                    command_time = shared.integer(f["applied_cmd_time"])
                    key = level, command_time
                    origin = tuple(float(x) for x in f["origin"].split(","))
                    if len(origin) != 3 or not all(math.isfinite(x) and abs(x) <= 10_000_000 for x in origin):
                        raise ValueError("nonfinite or invalid origin")
                    axes_ok = (matches_axis(f["server_axes"], expected)
                               and matches_axis(f["applied_axes"], expected))
                    sample = {"origin": origin, "axes_ok": axes_ok,
                              "server_time": shared.integer(f["server_cmd_time"]),
                              "axes": (f["server_axes"], f["applied_axes"])}
                    if key in indexed and indexed[key] != sample:
                        errors.append(f"{role}: conflicting same-frame sample")
                    indexed[key] = sample
                except (KeyError, ValueError):
                    errors.append(f"{role}: malformed interior input sample")
            captures[role] = indexed
        keys = sorted(set(captures["host"]) & set(captures["client"]))
        checks = {"dense_exact_world_pairs": len(keys) >= MIN_PAIRS,
                  "all_interior_axes_owner_pattern": bool(captures["host"] and captures["client"])
                  and all(sample["axes_ok"] for index in captures.values() for sample in index.values()),
                  "unambiguous_samples": not errors}
        motion = None
        max_world_error = None
        max_step = None
        max_excursion = None
        if keys:
            levels = [key[0] for key in keys]
            gap = max((b - a for a, b in zip(levels, levels[1:])), default=0)
            checks["dense_exact_world_pairs"] &= (levels[-1] - levels[0] >= 300
                                                  and gap <= MAX_PAIR_GAP_MS)
            checks["command_times_advance_in_both_worlds"] = (
                command_progress([key[1] for key in keys])
                and all(command_progress([captures[role][key]["server_time"] for key in keys])
                        for role in ("host", "client")))
            max_world_error = max(distance(captures["host"][key]["origin"],
                                           captures["client"][key]["origin"]) for key in keys)
            checks["same_world_positions_at_exact_time"] = max_world_error <= 0.5
            motion = distance(captures["host"][keys[0]]["origin"],
                              captures["host"][keys[-1]]["origin"])
            steps = [distance(captures["host"][a]["origin"], captures["host"][b]["origin"])
                     for a, b in zip(keys, keys[1:])]
            max_step = max(steps, default=0)
            # Neutral ownership needs the whole captured interior to remain
            # near its first position, including samples with no cross-copy
            # pair. Endpoint displacement alone misses an out-and-back move.
            first_positions = {role: index[min(index)]["origin"] for role, index in captures.items()}
            max_excursion = max(
                distance(sample["origin"], first_positions[role])
                for role in ("host", "client") for sample in captures[role].values())
            checks["bounded_motion_without_large_step"] = max_step <= 80 and motion <= 256
            checks["movement_matches_active_or_neutral_slot"] = motion >= 2 if any(expected) else max_excursion <= 4
        else:
            checks.update(same_world_positions_at_exact_time=False,
                          command_times_advance_in_both_worlds=False,
                          bounded_motion_without_large_step=False,
                          movement_matches_active_or_neutral_slot=False)
        item = {"pass": all(checks.values()), "checks": checks, "exact_pairs": len(keys),
                "captured_host": len(captures["host"]), "captured_client": len(captures["client"]),
                "motion_units": motion, "max_step_units": max_step,
                "max_excursion_units": max_excursion,
                "max_world_error_units": max_world_error, "errors": errors[:8]}
        result["slots"][str(slot)] = item
        if not item["pass"]:
            result["failures"].append(f"slot{slot}")
    for role in ("host", "client"):
        maps = [r for r in rows[role] if r["kind"] == "map"]
        parties = [r for r in rows[role] if r["kind"] == "party"]
        rosters = {str(i): [r for r in rows[role] if r["kind"] == "roster"
                            and r["fields"].get("slot") == str(i)] for i in range(2)}
        input_ticks = []
        for r in rows[role]:
            if r["kind"] == "input":
                try:
                    if start <= shared.integer(r["fields"]["level_time"]) <= end:
                        input_ticks.append(r["tick"])
                except (KeyError, ValueError):
                    pass
        paused_ok = bool(input_ticks)
        for tick in input_ticks:
            context = [r for r in maps if r["tick"] <= tick]
            latest = context[-1] if context else None
            paused_ok &= bool(latest and tick - latest["tick"] <= 1500
                              and shared.expected_map(latest, map_name, specialops)
                              and shared.check_bindings(latest)
                              and latest["fields"].get("cl_paused") == "0")
            party = [r for r in parties if r["tick"] <= tick]
            paused_ok &= bool(party and tick - party[-1]["tick"] <= 1500
                              and shared.check_party(party[-1], role))
            for slot in range(2):
                roster = [r for r in rosters[str(slot)] if r["tick"] <= tick]
                local = slot == (0 if role == "host" else 1)
                expected_type = 2 if role == "host" and slot == 0 else 4
                port = (0 if role == "host" and slot == 0 else
                        client_port if role == "host" else host_port)
                try:
                    f = roster[-1]["fields"]
                    paused_ok &= (tick - roster[-1]["tick"] <= 1500
                                  and f.get("occupied") == "1"
                                  and f.get("local_uid_match") == str(int(local))
                                  and f.get("address_type") == str(expected_type)
                                  and f.get("port") == str(port))
                except IndexError:
                    paused_ok = False
        if not paused_ok:
            result["failures"].append(f"{role}: missing fresh map/owner/binding/cl_paused0 context")
    result["pass"] = not result["failures"]
    return result


def ledger_checks(ledger: dict) -> tuple[list[dict], list[str]]:
    if not isinstance(ledger, dict):
        return [], ["ledger must be an object"]
    errors = []
    pattern = ledger.get("pattern", "original-right")
    if pattern not in PATTERNS:
        return [], ["ledger movement pattern is unsupported"]
    phases = ledger.get("phases", [])
    owners = ledger.get("owner_slots")
    if (type(ledger.get("version")) is not int or ledger.get("version") != 1 or ledger.get("map") not in MAPS
            or type(ledger.get("specialops", 1)) is not int or ledger.get("specialops", 1) not in (0, 1)
            or (ledger.get("map") == MAP and ledger.get("specialops", 1) != 1)
            or ledger.get("owner_slots") != {"host": 0, "client": 1}
            or not isinstance(owners, dict) or any(type(v) is not int for v in owners.values())
            or ledger.get("input_method") != "native-console-kbutton"
            or "error" not in ledger or ledger["error"] is not None
            or ledger.get("all_releases_consumed") is not True):
        errors.append("ledger identity/method/owner/release contract differs")
    if (not isinstance(phases, list) or not all(isinstance(p, dict) for p in phases)
            or [p.get("name") for p in phases] != list(PHASES)):
        return [], errors + ["ledger requires the exact seven ordered phases"]
    previous_end = 0
    for p in phases:
        start, end, wall = p.get("start_game_ms"), p.get("end_game_ms"), p.get("wall_ms")
        if (type(start) is not int or type(end) is not int or type(wall) is not int
                or not 0 <= start < end <= shared.U32 or start < previous_end
                or not 1000 <= end - start <= 2200 or not 0 < wall <= 3000):
            errors.append(f"{p['name']}: invalid/nonbounded game or wall interval")
            continue
        previous_end = end
        for role in ("host", "client"):
            for label, expected in (("dispatch", command_for(p["name"], role, pattern)), ("release", RELEASE)):
                container = p.get(label, {})
                item = container.get(role, {}) if isinstance(container, dict) else {}
                if not isinstance(item, dict):
                    item = {}
                if item.get("commands") != expected or item.get("consumed") is not True:
                    errors.append(f"{p['name']}: {role} {label} not exact/consumed")
    return phases, errors


def score(receipt_path: Path, ledger_path: Path) -> dict:
    receipt_file = receipt_path / "result.json" if receipt_path.is_dir() else receipt_path
    root = receipt_file.resolve(strict=True).parent
    receipt = json.loads(shared.bounded_read(receipt_file, 8 * 1024 * 1024).decode("utf-8-sig"))
    ledger = json.loads(shared.bounded_read(ledger_path, 1024 * 1024).decode("utf-8-sig"))
    phases, errors = ledger_checks(ledger)
    map_name = ledger.get("map", MAP) if isinstance(ledger, dict) else MAP
    if map_name not in MAPS:
        raise ValueError("input candidate supports only trainer or so_killspree_trainer")
    specialops = ledger.get("specialops", 1) if isinstance(ledger, dict) else 1
    pattern = ledger.get("pattern", "original-right") if isinstance(ledger, dict) else "original-right"
    base = shared.score_receipt(receipt_path, map_name, specialops)
    excluded = {"mw2-trio-storage-test.enable", "mw2-trio-cache-test.enable"}
    if map_name == MAP:
        excluded |= {"mw2-trio-scripts-test.enable", "mw2-trio-two-player-test.enable"}
    if excluded.intersection(receipt.get("scenario", {}).get("flags", [])):
        errors.append("input candidate refuses unrelated experiment flags for this map")
    rows = {}
    for role in ("host", "client"):
        log = shared.bounded_read(root / role / "after/control184.log", 64 * 1024 * 1024, root)
        rows[role], _, parse_errors = shared.parse_log(log.decode("utf-8"))
        errors.extend(f"{role}: {e}" for e in parse_errors)
    results = [phase_score(rows, p, host_port=base["reported_net_ports"]["host"],
                           client_port=base["reported_net_ports"]["client"],
                           map_name=map_name, specialops=specialops, pattern=pattern)
               for p in phases] if not errors else []
    return {"schema": "stock-native-input-score-candidate-v1",
            "pass": base["pass"] and not errors and len(results) == 7 and all(p["pass"] for p in results),
            "shared_map_pass": base["pass"], "harness_clean": base["harness_clean"],
            "map": map_name, "specialops": specialops, "pattern": pattern,
            "ledger_errors": errors, "phases": results,
            "scope": "candidate proof of planned native axes and bounded matching world movement only; no campaign progression/separate-PC/full-controller QA"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--events", type=Path, required=True, help="parent-generated dispatch/release ledger")
    args = parser.parse_args(argv)
    try:
        result = score(args.receipt, args.events)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        result = {"schema": "stock-native-input-score-candidate-v1", "pass": False,
                  "error": f"{type(exc).__name__}: {exc}"}
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
