"""Read-only exact-build184 shared-map receipt scorer. Own code, MIT.

Reads result.json and the two after/control184.log files; never writes them.
Success means late native command application advances on both copies. It does
not establish independent input, identical worlds, campaign progression or QA.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import stat
import sys

EXE_SHA = "4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9"
U32 = 0xFFFFFFFF
HALF = 0x80000000
WINDOW_MS = 20_000
FRESH_MS = 3_000
MIN_SAMPLES = 3
MIN_ADVANCE_MS = 1_000
HEADER = re.compile(r"^tick=(\d+) tid=(\d+) (.*)$")
FIELD = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)=([^\s]*)")
KINDS = ("map", "party", "input", "roster")
MAILBOX = re.compile(r"mailbox enqueued count=(\d+) bytes=(\d+); execution requires native state evidence")
BOOT_COMMAND = "set developer 1\nset logfile 2\nset snd_volume 0\nset com_maxfps 20\n"
BODY_V11_INPUT_PROFILE = "body-v11-isolated-kbutton"
BODY_V11_BOOT_EXTRA = "set in_mouse 0\nunbindall\n-forward\n-back\n-moveleft\n-moveright\nset com_maxfps 60\n"
BODY_V11_DLL_SHA = "be51d30339edd669e67525ec0f0fada4f481d09ae56f8af509248fb181c37f7b"
BODY_V11_PROBE_SHA = "7fab32a1a26b16d083036766949a249c0bfdb1dc24a62fe0795e1b0c4aee9722"
BODY_V11_PROBE_PATH = "mw2trio/scripts/maps/_mw2trio_probe.gsc"
PEER_FLAG = "mw2-trio-peer-test.enable"
QA_FLAG = "mw2-trio-loopback-qa.enable"
PEER_MARKER = "Private exact-peer UDP boundary latched; provider APIs unchanged"
READINESS = "active hook + completed native frame + party telemetry + consumed setup mailbox"


def integer(value: str, *, hexadecimal: bool = False) -> int:
    pattern = r"0x[0-9a-fA-F]+" if hexadecimal else r"[0-9]+"
    if not re.fullmatch(pattern, value):
        raise ValueError("invalid integer")
    answer = int(value, 16 if hexadecimal else 10)
    if not 0 <= answer <= U32:
        raise ValueError("integer outside uint32")
    return answer


def fields(body: str) -> dict[str, str]:
    pairs = FIELD.findall(body)
    result = dict(pairs)
    if len(result) != len(pairs):
        raise ValueError("duplicate diagnostic field")
    return result


def kind_of(body: str) -> str | None:
    if body.startswith("frame=") and " enqueued_mailboxes=" in body and " map=" in body:
        return "map"
    for kind, prefix in (("party", "party host="), ("input", "input slot="),
                         ("roster", "roster slot=")):
        if body.startswith(prefix):
            return kind
    return None


def parse_log(text: str) -> tuple[list[dict], int, list[str]]:
    rows: list[dict] = []
    errors: list[str] = []
    previous: int | None = None
    clock = 0
    for line_no, line in enumerate(text.splitlines(), 1):
        match = HEADER.fullmatch(line)
        if not match:
            if line.startswith("tick="):
                errors.append(f"line{line_no}: malformed tick header")
            continue
        try:
            tick = integer(match[1])
            tid = integer(match[2])
            if previous is None:
                clock = tick
            else:
                delta = (tick - previous) & U32
                if delta >= HALF:
                    raise ValueError("native tick regressed or ambiguous wrap")
                clock += delta
            previous = tick
            body = match[3]
            kind = kind_of(body)
            if kind:
                rows.append({"kind": kind, "tick": clock, "tid": tid,
                             "line": line_no, "fields": fields(body)})
        except ValueError as exc:
            errors.append(f"line{line_no}: {exc}")
    return rows, clock, errors


def expected_map(row: dict, map_name: str, specialops: int) -> bool:
    f = row["fields"]
    return (f.get("map") == map_name and f.get("sv_running") == "1"
            and f.get("coop") == "1" and f.get("specialops") == str(specialops))


def check_bindings(row: dict) -> bool:
    f = row["fields"]
    try:
        return (integer(f["raw_level_maxclients"]) == 2
                and integer(f["raw_gclient_base"], hexadecimal=True) == 0x108DC68
                and integer(f["raw_entity0_client"], hexadecimal=True) == 0x108DC68
                and integer(f["raw_entity1_client"], hexadecimal=True) == 0x1098C3C
                and integer(f["raw_client0_index"]) == 0
                and integer(f["raw_client1_index"]) == 1
                and integer(f["raw_valid"], hexadecimal=True) & 0xFF == 0xFF
                and integer(f["raw_coop_enabled"]) == 1)
    except (KeyError, ValueError):
        return False


def check_party(row: dict, role: str) -> bool:
    f = row["fields"]
    local = 0 if role == "host" else 1
    try:
        return (integer(f["host"]) == (role == "host")
                and integer(f["launch"]) == 0
                and integer(f["session"], hexadecimal=True) == 0x1BDE8B8
                and integer(f["session_active"]) == 1
                and integer(f["member0"]) == integer(f["member1"]) == 1
                and integer(f["client_state"]) == 6
                and integer(f["raw_launch_count"]) == 2
                and [integer(f[f"channel{i}_type"]) for i in range(2)]
                == [2 if i == local else 4 for i in range(2)])
    except (KeyError, ValueError):
        return False


def progress(samples: list[dict], field: str, anchor: int) -> dict:
    result = {"samples": len(samples), "distinct_values": 0, "advance_ms": 0,
              "latest_age_ms": None, "last_progress_age_ms": None, "pass": False}
    try:
        values = [integer(row["fields"][field]) for row in samples]
        if not samples:
            return result
        result["latest_age_ms"] = anchor - samples[-1]["tick"]
        distinct = 1
        advance = 0
        last_change = samples[0]["tick"]
        for a, b, row in zip(values, values[1:], samples[1:]):
            delta = (b - a) & U32
            if delta >= HALF:
                result["error"] = "counter regression/reset or ambiguous wrap"
                return result
            advance += delta
            if delta:
                distinct += 1
                last_change = row["tick"]
        result.update(distinct_values=distinct, advance_ms=advance,
                      last_progress_age_ms=anchor - last_change)
        result["pass"] = (len(samples) >= MIN_SAMPLES and distinct >= MIN_SAMPLES
                          and advance >= MIN_ADVANCE_MS
                          and result["latest_age_ms"] <= FRESH_MS
                          and result["last_progress_age_ms"] <= FRESH_MS)
    except (KeyError, ValueError):
        result["error"] = "missing or malformed counter"
    return result


def score_log(text: str, role: str, map_name: str, specialops: int, *,
              host_port: int = 28960, client_port: int = 28961,
              observation_tick: int | None = None) -> dict:
    rows, anchor, errors = parse_log(text)
    if observation_tick is not None:
        delta = (observation_tick - (anchor & U32)) & U32
        if delta >= HALF:
            errors.append("observation boundary predates included native records")
        else:
            anchor += delta
    grouped = {kind: [r for r in rows if r["kind"] == kind] for kind in KINDS}
    late = {kind: [r for r in grouped[kind] if anchor - r["tick"] <= WINDOW_MS]
            for kind in KINDS}
    latest_map = grouped["map"][-1] if grouped["map"] else None
    latest_party = grouped["party"][-1] if grouped["party"] else None
    checks: dict[str, bool] = {}
    checks["parse_unambiguous"] = not errors
    checks["latest_map_expected_and_fresh"] = bool(
        latest_map and expected_map(latest_map, map_name, specialops)
        and anchor - latest_map["tick"] <= FRESH_MS)
    checks["latest_party_in_game_and_fresh"] = bool(
        latest_party and check_party(latest_party, role)
        and anchor - latest_party["tick"] <= FRESH_MS)
    checks["local_reported_net_port_matches_both_log_projection"] = bool(
        latest_party and latest_party["fields"].get("net_port")
        == str(host_port if role == "host" else client_port))
    checks["late_map_and_indices_stable"] = (
        len(late["map"]) >= MIN_SAMPLES
        and all(expected_map(r, map_name, specialops) and check_bindings(r) for r in late["map"]))
    checks["late_party_channels_stable"] = (
        len(late["party"]) >= MIN_SAMPLES and all(check_party(r, role) for r in late["party"]))
    threads = {r["tid"] for group in late.values() for r in group}
    checks["one_late_native_owner_thread"] = len(threads) == 1
    slots: dict[str, dict] = {}
    for slot in range(2):
        samples = [r for r in late["input"] if r["fields"].get("slot") == str(slot)]
        sample_ticks = [r["tick"] for r in samples]
        finite = True
        for row in samples:
            try:
                vector = [float(x) for x in row["fields"]["origin"].split(",")]
                if len(vector) != 3 or not all(math.isfinite(x) for x in vector):
                    finite = False
            except (KeyError, ValueError):
                finite = False
        counters = {name: progress(samples, name, anchor) for name in
                    ("level_time", "server_cmd_time", "applied_cmd_time")}
        good = (len(samples) >= MIN_SAMPLES and len(set(sample_ticks)) >= MIN_SAMPLES
                and finite and all(item["pass"] for item in counters.values()))
        slots[str(slot)] = {"pass": good, "finite_origins": finite,
                            "distinct_native_ticks": len(set(sample_ticks)), "counters": counters}
        checks[f"slot{slot}_late_native_application_advances"] = good
        roster = [r for r in late["roster"] if r["fields"].get("slot") == str(slot)]
        signatures: set[tuple[int, int, int, int]] = set()
        roster_good = len(roster) >= MIN_SAMPLES
        for row in roster:
            try:
                f = row["fields"]
                signature = tuple(integer(f[name]) for name in
                                  ("occupied", "local_uid_match", "address_type", "port"))
                signatures.add(signature)
                local = slot == (0 if role == "host" else 1)
                # Canonical session addresses are not the native input channels.
                # Exact184 coopAccept upserts BOTH client roster entries using
                # the host source address (0x65A7B6/0x65A82E). Thus client own
                # member1 is canonical type4/hostport, while native channel1 is
                # type2. UID-match still identifies the local member correctly.
                expected_type = 2 if role == "host" and slot == 0 else 4
                expected_port = (0 if role == "host" and slot == 0 else
                                 client_port if role == "host" else host_port)
                roster_good &= (signature[0] == 1 and signature[1] == local
                                and signature[2] == expected_type
                                and signature[3] == expected_port)
            except (KeyError, ValueError):
                roster_good = False
        checks[f"slot{slot}_late_canonical_roster_projection_stable"] = bool(
            roster_good and len(signatures) == 1 and roster
            and anchor - roster[-1]["tick"] <= FRESH_MS)
    checks["only_two_native_input_slots"] = all(r["fields"].get("slot") in ("0", "1") for r in late["input"])
    return {
        "pass": all(checks.values()),
        "map_loaded_at_least_once": any(expected_map(r, map_name, specialops) for r in grouped["map"]),
        "native_tick_anchor": anchor, "window_ms": WINDOW_MS,
        "late_samples": {kind: len(group) for kind, group in late.items()},
        "latest_map_line": latest_map["line"] if latest_map else None,
        "latest_party_line": latest_party["line"] if latest_party else None,
        "checks": checks, "failures": [name for name, ok in checks.items() if not ok],
        "parse_errors": errors[:20], "slots": slots,
    }


def harness_checks(receipt: dict) -> dict[str, bool]:
    exits = receipt.get("exit_codes")
    return {
        "harness_error_none": "error" in receipt and receipt["error"] is None,
        "no_forced_cleanup": receipt.get("forced_cleanup") == [],
        "both_owned_exit_codes_zero": (isinstance(exits, dict)
                                       and set(exits) == {"host", "client"}
                                       and all(type(exits[r]) is int and exits[r] == 0
                                               for r in ("host", "client"))),
        "all_owned_processes_stopped": receipt.get("all_owned_processes_stopped") is True,
        "executable_hashes_unchanged": receipt.get("executable_hashes_unchanged") is True,
        "scenario_evidence_pass": receipt.get("scenario_evidence_pass") is True,
        "no_cleanup_errors": not receipt.get("cleanup_errors"),
    }


def control_tick_records(text: str) -> list[tuple[int, str, str]]:
    records = []
    for line in text.splitlines():
        match = HEADER.fullmatch(line)
        if match:
            records.append((integer(match[1]), match[3], line))
    return records


def peer_config(text: str) -> dict:
    """Mirror the bounded native config grammar; this is not authentication."""
    if not isinstance(text, str):
        raise ValueError("peer config must be text")
    data = text.encode("ascii")
    if not 1 <= len(data) <= 256 or not data.endswith(b"\n"):
        raise ValueError("peer config is not bounded newline-terminated ASCII")
    ending = "\r\n" if "\r" in text else "\n"
    lines = text.split(ending)
    result = {}
    for line in lines[:-1]:
        if any(not 33 <= ord(c) <= 126 for c in line) or line.count("=") != 1:
            raise ValueError("peer config has whitespace or inconsistent line endings")
        key, value = line.split("=")
        if not value or key in result:
            raise ValueError("peer config has empty or duplicate fields")
        result[key] = value
    if lines[-1] or set(result) != {"version", "role", "session_nonce", "local_ip",
                                    "local_port", "peer_ip", "peer_port"}:
        raise ValueError("peer config fields differ")
    if result["version"] != "1" or result["role"] not in ("host", "client"):
        raise ValueError("peer config version/role differs")
    if not re.fullmatch(r"[0-9a-fA-F]{32}", result["session_nonce"]) or int(result["session_nonce"], 16) == 0:
        raise ValueError("peer config nonce is malformed")
    result["session_nonce"] = result["session_nonce"].lower()
    for key in ("local_port", "peer_port"):
        value = result[key]
        if not re.fullmatch(r"[1-9][0-9]{0,4}", value) or int(value) > 65535:
            raise ValueError("peer port is not canonical1..65535")
        result[key] = int(value)
    for key in ("local_ip", "peer_ip"):
        value = result[key]
        octets = value.split(".")
        if (len(octets) != 4 or any(not re.fullmatch(r"0|[1-9][0-9]{0,2}", x) for x in octets)
                or any(int(x) > 255 for x in octets)):
            raise ValueError("peer IPv4 is not canonical numeric IPv4")
        a, b, _, d = map(int, octets)
        if a in (0, 127) or a >= 224 or (a == 169 and b == 254) or d in (0, 255):
            raise ValueError("peer IPv4 is ineligible")
    if (result["local_ip"], result["local_port"]) == (result["peer_ip"], result["peer_port"]):
        raise ValueError("peer tuple aliases its own socket")
    return result


def same_pc_peer_provenance(receipt: dict, texts: dict[str, str], artifacts: dict | None) -> None:
    """Only two processes of the saved own runner on one IP share this clock.

    A separate-PC receipt is deliberately refused even if it uses valid peer
    mode. The nonce and saved diagnostics are receipt evidence, not proof of
    peer agreement, provider identity or whole-process network confinement.
    """
    scenario = receipt["scenario"]
    roles = {"host", "client"}
    if (type(scenario.get("version")) is not int or scenario["version"] != 1
            or not isinstance(scenario.get("instances"), list) or len(scenario["instances"]) != 2
            or set(scenario["instances"]) != roles
            or receipt.get("scope") != "bounded diagnostic scenario; no gameplay QA claim"
            or receipt.get("observation_end") is None):
        raise ValueError("exact-peer cutoff needs the explicit own-runner precleanup marker")
    seconds, ready_seconds = scenario.get("seconds"), scenario.get("readiness_seconds")
    if (type(seconds) is not int or not 15 <= seconds <= 120
            or type(ready_seconds) is not int or not 15 <= ready_seconds <= 90):
        raise ValueError("exact-peer own-runner time bounds differ")
    flags = scenario.get("flags")
    if (not isinstance(flags, list) or any(not isinstance(x, str) for x in flags)
            or len(set(flags)) != len(flags)
            or not {PEER_FLAG, "mw2-trio-control.enable", "mw2-trio-network-test.enable",
                    "mw2-trio-party-test.enable", "mw2-trio-two-player-test.enable"} <= set(flags)
            or QA_FLAG in flags):
        raise ValueError("exact-peer cutoff requires exclusive explicit peer mode")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", receipt.get("dll_sha256", "")):
        raise ValueError("own-runner DLL identity missing")
    pids = receipt["pids"]
    registration = receipt.get("gameslot_registration")
    readiness = receipt.get("readiness")
    if (not isinstance(registration, dict) or set(registration) != roles
            or not isinstance(readiness, dict) or set(readiness) != roles):
        raise ValueError("both own process registrations and readiness barriers required")
    for role in roles:
        ready = readiness[role]
        if (not isinstance(ready, dict) or ready.get("evidence") != READINESS
                or type(ready.get("elapsed")) not in (int, float)
                or not math.isfinite(ready["elapsed"]) or not 0 <= ready["elapsed"] <= ready_seconds
                or registration[role] != f"REGISTERED {pids[role]} on 2 lease(s)"):
            raise ValueError("own PID registration/readiness evidence differs")
    configs = scenario.get("peer_configs")
    if not isinstance(configs, dict) or set(configs) != roles:
        raise ValueError("both explicit role peer configs required")
    parsed = {role: peer_config(configs[role]) for role in roles}
    host, client = parsed["host"], parsed["client"]
    if (host["role"] != "host" or client["role"] != "client"
            or host["session_nonce"] != client["session_nonce"]
            or host["local_ip"] != client["local_ip"]
            or host["local_ip"] != client["peer_ip"] or client["local_ip"] != host["peer_ip"]
            or host["local_port"] != client["peer_port"] or client["local_port"] != host["peer_port"]):
        raise ValueError("cutoff only supports opposite-role, same-PC exact tuple pairs with one seed")
    if not isinstance(artifacts, dict) or set(artifacts) != roles:
        raise ValueError("exact-peer saved artifacts missing")
    identity_hashes = set()
    for role in roles:
        saved, config = artifacts[role], parsed[role]
        if (not isinstance(saved, dict) or saved.get("error")
                or saved.get("config") != configs[role].encode("ascii")
                or saved.get("flag") not in (b"owned bounded harness\n", b"owned bounded harness\r\n")
                or saved.get("qa_flag_present") is not False):
            raise ValueError("exact-peer saved config/flag differs or QA mode is present")
        party = [fields(line) for line in saved["party"].splitlines() if line.startswith("party184 ")]
        active = [i for i, row in enumerate(party) if row.get("result") == "7"
                  and row.get("registered") == "1" and row.get("qa_result") == "1"
                  and row.get("qa_active") == "1"]
        opened = [i for i, row in enumerate(party) if row.get("result") == "6"
                  and row.get("open_result") == "1" and row.get("socket_present") == "1"
                  and row.get("bound_port") == str(config["local_port"])
                  and row.get("startup_result") == "0"]
        if not active or not opened or min(active) >= min(opened):
            raise ValueError("cached private ID activation then exact native opener evidence required")
        for i, row in enumerate(party):
            if (row.get("registered") != "1" or row.get("rejected") != "0"
                    or row.get("qa_failed_precondition") != "0" or row.get("qa_failed_site") != "0"
                    or (i >= min(active) and (row.get("qa_result"), row.get("qa_active")) != ("1", "1"))
                    or (i >= min(opened) and (row.get("open_result"), row.get("socket_present"),
                                               row.get("bound_port"), row.get("startup_result"))
                        != ("1", "1", str(config["local_port"]), "0"))):
                raise ValueError("cached ID/opener state changed or recorded a failure")
        net = saved["network"].splitlines()
        if net.count(PEER_MARKER) != 1 or any("QA boundary latched:" in line for line in net):
            raise ValueError("exclusive native exact-peer/provider-unchanged marker required")
        if parse_log(saved["network"])[2]:
            raise ValueError("native exact-peer packet chronology is malformed")
        handshake = set()
        for _, body, _ in control_tick_records(saved["network"]):
            if not body.startswith(("send command=", "recv command=")):
                continue
            row = fields(body)
            if row.get("peer") != f"{config['peer_ip']}:{config['peer_port']}":
                raise ValueError("saved native packet tuple differs from the exact configured peer")
            if row.get("command") == "R" and row.get("inner") in ("coopConnect", "coopAccept"):
                if integer(row["bytes"]) == 0 or row.get("bytes") != row.get("result") or row.get("reason") != "none":
                    raise ValueError("native admission token did not successfully transfer")
                handshake.add((body.split(" ", 1)[0], row["inner"]))
        expected = ({("recv", "coopConnect"), ("send", "coopAccept")} if role == "host" else
                    {("send", "coopConnect"), ("recv", "coopAccept")})
        if not expected <= handshake:
            raise ValueError("both exact-peer native admission directions required")
        rows, _, errors = parse_log(texts[role])
        parties = [r for r in rows if r["kind"] == "party"]
        latest = parties[-1]["fields"] if parties else {}
        if (errors or latest.get("native_online") != "0" or latest.get("identity_present") != "1"
                or latest.get("net_port") != str(config["local_port"])
                or not re.fullmatch(r"[0-9a-fA-F]{8}", latest.get("identity_hash", ""))):
            raise ValueError("latest native private identity/offline/bind telemetry differs")
        hooks = [fields(body) for _, body, _ in control_tick_records(texts[role])
                 if body.startswith("hook state=active ")]
        if not any(all(h.get(k) == value for k, value in
                       (("installed", "1"), ("active", "1"), ("reason", "0"), ("error", "0"),
                        ("rollback_error", "0"), ("protection_error", "0"),
                        ("initial_tid", str(parties[-1]["tid"])))) for h in hooks):
            raise ValueError("own active native hook does not identify the diagnostic owner thread")
        identity_hashes.add(latest["identity_hash"].lower())
    if len(identity_hashes) != 2:
        raise ValueError("both role-local cached identities must be distinct")


def own_quit_evidence(receipt: dict, texts: dict[str, str],
                      peer_artifacts: dict | None = None, *, verified_boot: str = BOOT_COMMAND) -> dict[str, dict]:
    """Narrow same-PC harness evidence: boot1 + actual steps + final quit ACK."""
    scenario = receipt.get("scenario", {})
    pids = receipt.get("pids", {})
    if (not isinstance(scenario, dict) or set(scenario.get("instances", [])) != {"host", "client"}
            or not isinstance(pids, dict) or set(pids) != {"host", "client"}
            or any(type(v) is not int or not 0 < v <= U32 for v in pids.values())
            or len(set(pids.values())) != 2
            or receipt.get("scenario_clock_started_after_all_ready") is not True):
        raise ValueError("own-quit cutoff requires both owned same-PC role processes and readiness clock")
    flags = scenario.get("flags", [])
    if PEER_FLAG in flags or "peer_configs" in scenario:
        same_pc_peer_provenance(receipt, texts, peer_artifacts)
    elif QA_FLAG not in flags:
        raise ValueError("own-quit cutoff requires known same-PC loopback or exact-peer harness provenance")
    steps = receipt.get("steps_sent")
    if not isinstance(steps, list):
        raise ValueError("own-quit cutoff requires recorded steps_sent")
    expected = {role: [len(verified_boot.encode("ascii"))] for role in ("host", "client")}
    for step in steps:
        if not isinstance(step, dict) or step.get("instance") not in expected:
            raise ValueError("invalid recorded step ownership")
        commands = step.get("commands")
        if not isinstance(commands, str) or not 1 <= len(commands) <= 4096:
            raise ValueError("invalid recorded mailbox block")
        if any(re.match(r"^\s*quit(?:\s|$)", line, re.I) for line in re.split(r"[;\r\n]+", commands)):
            raise ValueError("scenario step contains quit; cleanup cannot be inferred")
        data = commands.encode("ascii")
        expected[step["instance"]].append(len(data) + (not data.endswith(b"\n")))
    result = {}
    for role, text in texts.items():
        acknowledgments = []
        for tick, body, _ in control_tick_records(text):
            match = MAILBOX.fullmatch(body)
            if match:
                acknowledgments.append((tick, integer(match[1]), integer(match[2])))
        sizes = expected[role] + [5]  # exact final own "quit\n"
        if (len(acknowledgments) != len(sizes)
                or [x[1] for x in acknowledgments] != list(range(1, len(sizes) + 1))
                or [x[2] for x in acknowledgments] != sizes):
            raise ValueError(f"{role} mailbox ACK count/bytes do not identify boot+steps+one final own quit")
        tick, count, size = acknowledgments[-1]
        result[role] = {"tick": tick, "count": count, "bytes": size}
    return result


def observation_boundary(receipt: dict, texts: dict[str, str], *, derive_own_quit: bool,
                         peer_artifacts: dict | None = None, verified_boot: str = BOOT_COMMAND) -> dict:
    marker = receipt.get("observation_end")
    required = marker is not None or derive_own_quit
    result = {"source": "full_saved_logs", "required": required, "valid": True,
              "applied": False, "cutoff_tick": None, "exclusive": True}
    if not required:
        return result
    try:
        if not all(harness_checks(receipt).values()):
            raise ValueError("cutoff refused because harness receipt is not clean")
        quits = own_quit_evidence(receipt, texts, peer_artifacts, verified_boot=verified_boot)
        if peer_artifacts is not None:
            result["provenance"] = {"mode": "own_same_pc_exact_peer",
                "saved_artifact_sha256": {role: {key: hashlib.sha256(
                    value.encode("utf-8") if isinstance(value, str) else value).hexdigest()
                    for key, value in peer_artifacts[role].items() if key in ("config", "flag", "party", "network")}
                    for role in ("host", "client")}}
        a, b = quits["host"]["tick"], quits["client"]["tick"]
        if (b - a) & U32 >= HALF:
            earliest = b
        else:
            earliest = a
        if min((a - b) & U32, (b - a) & U32) > 15_000:
            raise ValueError("own quit acknowledgments exceed the bounded cleanup separation")
        if marker is None:
            cutoff = earliest
            source = "explicit_opt_in_derived_earliest_consumed_own_quit"
        else:
            if (not isinstance(marker, dict) or type(marker.get("version")) is not int
                    or marker["version"] != 1 or marker.get("source") != "runner_precleanup"
                    or marker.get("before_any_quit") is not True
                    or not isinstance(marker.get("mailboxes_empty"), dict)
                    or set(marker["mailboxes_empty"]) != {"host", "client"}
                    or any(v is not True for v in marker["mailboxes_empty"].values())):
                raise ValueError("invalid runner precleanup marker contract")
            cutoff = marker.get("global_tick")
            if type(cutoff) is not int or not 0 <= cutoff <= U32:
                raise ValueError("invalid global native observation tick")
            if not 0 <= ((earliest - cutoff) & U32) <= FRESH_MS:
                raise ValueError("runner marker is after first quit or too far before cleanup")
            ticks = marker.get("last_control_ticks")
            if not isinstance(ticks, dict) or set(ticks) != {"host", "client"}:
                raise ValueError("runner marker lacks both saved native control ticks")
            for role in ("host", "client"):
                tick = ticks[role]
                if (type(tick) is not int or not 0 <= tick <= U32
                        or not 0 <= ((cutoff - tick) & U32) <= FRESH_MS
                        or tick not in {x[0] for x in control_tick_records(texts[role])}):
                    raise ValueError(f"{role} saved control tick does not validate the shared observation marker")
            source = "validated_runner_precleanup_marker"
        result.update(source=source, applied=True, cutoff_tick=cutoff, own_quit_ack=quits,
                      scope="common same-PC GetTickCount32 boundary; all native records at/after it excluded")
    except (ValueError, TypeError, KeyError, UnicodeError) as exc:
        result.update(valid=False, reason=str(exc))
    return result


def before_observation_end(text: str, cutoff: int | None) -> tuple[str, int]:
    if cutoff is None:
        return text, 0
    kept = []
    removed = 0
    for line in text.splitlines():
        match = HEADER.fullmatch(line)
        if match and not 0 < ((cutoff - integer(match[1])) & U32) < HALF:
            removed += 1
        else:
            kept.append(line)
    return "\n".join(kept) + "\n", removed


def bounded_read(path: Path, limit: int, root: Path | None = None) -> bytes:
    resolved = path.resolve(strict=True)
    if root is not None:
        resolved.relative_to(root)
    info = path.stat(follow_symlinks=False)
    if not stat.S_ISREG(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError("receipt/log must be a regular file without reparse redirection")
    if info.st_size > limit:
        raise ValueError("receipt/log exceeds bounded reader size")
    with path.open("rb") as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise ValueError("receipt/log grew beyond reader bound")
    return data


def saved_peer_artifacts(root: Path) -> dict:
    result = {}
    for role in ("host", "client"):
        directory = root / role / "after"
        try:
            try:
                bounded_read(directory / QA_FLAG, 256, root)
                qa_present = True
            except FileNotFoundError:
                qa_present = False
            result[role] = {
                "config": bounded_read(directory / "mw2-trio-peer.cfg", 256, root),
                "flag": bounded_read(directory / PEER_FLAG, 256, root),
                "qa_flag_present": qa_present,
                "party": bounded_read(directory / "party184.log", 8 * 1024 * 1024, root).decode("utf-8"),
                "network": bounded_read(directory / "network184.log", 8 * 1024 * 1024, root).decode("utf-8"),
            }
        except (OSError, ValueError) as exc:
            result[role] = {"error": type(exc).__name__}
    return result


def body_v11_verified_boot(receipt: dict, root: Path, profile: str | None,
                          map_name: str, specialops: int) -> str:
    """One pinned private profile; no caller-supplied arbitrary boot override."""
    if profile is None:
        return BOOT_COMMAND
    if profile != BODY_V11_INPUT_PROFILE:
        raise ValueError("unknown explicit input score profile")
    s = receipt.get("scenario", {})
    flags = s.get("flags") if isinstance(s, dict) else None
    required = {"mw2-trio-control.enable", "mw2-trio-network-test.enable", "mw2-trio-party-test.enable",
                QA_FLAG, "mw2-trio-two-player-test.enable", "mw2-trio-scripts-test.enable",
                "mw2-trio-input-test.enable", "mw2-trio-body-assets-test.enable"}
    if (map_name != "trainer" or type(specialops) is not int or specialops != 0
            or not isinstance(s, dict) or not isinstance(receipt.get("observation_end"), dict)
            or s.get("boot_extra") != BODY_V11_BOOT_EXTRA
            or s.get("show_owned_windows") is not False
            or not isinstance(flags, list) or not all(isinstance(x, str) for x in flags)
            or len(flags) != len(set(flags)) or set(flags) != required
            or s.get("dll_profile") != "body-assets" or receipt.get("dll_profile") != "body-assets"
            or receipt.get("dll_sha256") != BODY_V11_DLL_SHA
            or s.get("probe_candidate") != "stock-body-v11"
            or s.get("probe_override_sha256") != BODY_V11_PROBE_SHA):
        raise ValueError("explicit body V11 input profile scope/boot/DLL/source differs")
    plan = s.get("input_plan")
    if (not isinstance(plan, dict) or plan.get("version") != 1 or plan.get("map") != "trainer"
            or plan.get("specialops") != 0 or plan.get("pattern") != "both-return"):
        raise ValueError("body V11 input profile requires the known campaign movement plan")
    for role in ("host", "client"):
        override = receipt.get("content_override", {}).get(role, {})
        row = override.get("files", {}).get(BODY_V11_PROBE_PATH, {})
        if (override.get("source_sha256") != BODY_V11_PROBE_SHA or row.get("staged") != BODY_V11_PROBE_SHA
                or receipt.get("content_override_restored", {}).get(role) is not True):
            raise ValueError("body V11 source path/hash/restoration receipt differs")
        for name, expected, bound in (("winmm.dll", BODY_V11_DLL_SHA, 4 * 1024 * 1024),
                                      (BODY_V11_PROBE_PATH, BODY_V11_PROBE_SHA, 64 * 1024)):
            raw = bounded_read(root / role / "after" / name, bound, root)
            if hashlib.sha256(raw).hexdigest() != expected:
                raise ValueError("saved body V11 DLL/probe bytes differ")
    return BOOT_COMMAND + BODY_V11_BOOT_EXTRA


def score_receipt(path: Path, map_name: str, specialops: int, *, derive_own_quit: bool = False,
                  profile: str | None = None) -> dict:
    receipt_path = path / "result.json" if path.is_dir() else path
    receipt_root = receipt_path.resolve(strict=True).parent
    raw = bounded_read(receipt_path, 8 * 1024 * 1024)
    receipt = json.loads(raw.decode("utf-8-sig"))
    if (not isinstance(receipt, dict) or not isinstance(receipt.get("exe_sha256"), str)
            or receipt["exe_sha256"].lower() != EXE_SHA):
        raise ValueError("receipt does not identify the exact supported184 EXE")
    verified_boot = body_v11_verified_boot(receipt, receipt_root, profile, map_name, specialops)
    result = {
        "schema": "shared-map-score-v1", "pass": False, "expected_map": map_name,
        "expected_specialops": specialops, "receipt_sha256": hashlib.sha256(raw).hexdigest(),
        "policy": {"late_window_ms": WINDOW_MS, "freshness_ms": FRESH_MS,
                   "max_tail_stall_ms": FRESH_MS, "min_samples": MIN_SAMPLES,
                   "min_distinct_counter_values": MIN_SAMPLES, "min_advance_ms": MIN_ADVANCE_MS},
        "scope": "late two-copy native applied-command advancement; not input independence, synchronization or campaign completion",
        "input_independence_proven": False,
        "roles": {},
    }
    logs = {}
    ports = {}
    for role in ("host", "client"):
        log = bounded_read(receipt_root / role / "after/control184.log", 64 * 1024 * 1024, receipt_root)
        text = log.decode("utf-8", errors="strict")
        logs[role] = (log, text)
    scenario = receipt.get("scenario", {})
    peer_artifacts = (saved_peer_artifacts(receipt_root) if isinstance(scenario, dict)
                      and ("peer_configs" in scenario or (isinstance(scenario.get("flags"), list)
                          and PEER_FLAG in scenario["flags"])) else None)
    boundary = observation_boundary(receipt, {r: x[1] for r, x in logs.items()},
                                    derive_own_quit=derive_own_quit, peer_artifacts=peer_artifacts,
                                    verified_boot=verified_boot)
    result["observation_boundary"] = boundary
    for role in ("host", "client"):
        log, text = logs[role]
        text, removed = before_observation_end(text, boundary["cutoff_tick"])
        logs[role] = (log, text)
        boundary.setdefault("excluded_native_lines", {})[role] = removed
        rows, _, _ = parse_log(text)
        party = [r for r in rows if r["kind"] == "party"]
        try:
            port = integer(party[-1]["fields"]["net_port"])
            if not 1 <= port <= 65535:
                raise ValueError("native port outside range")
        except (IndexError, KeyError, ValueError) as exc:
            raise ValueError(f"{role} latest party diagnostic lacks a valid reported net_port") from exc
        ports[role] = port
    result["reported_net_ports"] = ports
    for role in ("host", "client"):
        log, text = logs[role]
        role_result = score_log(text, role, map_name, specialops,
                                host_port=ports["host"], client_port=ports["client"],
                                observation_tick=boundary["cutoff_tick"])
        role_result["control_log_sha256"] = hashlib.sha256(log).hexdigest()
        result["roles"][role] = role_result
    result["shared_load_at_least_once_both"] = all(r["map_loaded_at_least_once"] for r in result["roles"].values())
    result["advancing_native_application_both"] = all(r["pass"] for r in result["roles"].values())
    result["harness_checks"] = harness_checks(receipt)
    result["harness_clean"] = all(result["harness_checks"].values())
    result["harness_failures"] = [name for name, ok in result["harness_checks"].items() if not ok]
    result["pass"] = result["advancing_native_application_both"] and result["harness_clean"] and boundary["valid"]
    if profile is not None:
        result["input_score_profile"] = profile
        result["profile_boot_bytes"] = len(verified_boot.encode("ascii"))
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True, help="saved receipt directory or result.json")
    parser.add_argument("--map", dest="map_name", required=True)
    parser.add_argument("--derive-own-quit-cutoff", action="store_true",
                        help="explicit historical same-PC clean-harness own-quit boundary inference")
    parser.add_argument("--profile", choices=(BODY_V11_INPUT_PROFILE,),
                        help="explicit pinned private V11 isolated-keyboard boot contract")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--specialops", type=int, choices=(0, 1))
    mode.add_argument("--specialops0", dest="specialops", action="store_const", const=0)
    mode.add_argument("--specialops1", dest="specialops", action="store_const", const=1)
    args = parser.parse_args(argv)
    try:
        if not re.fullmatch(r"[A-Za-z0-9_]{1,95}", args.map_name):
            raise ValueError("expected map must be a bounded native map identifier")
        result = score_receipt(args.receipt, args.map_name, args.specialops,
                               derive_own_quit=args.derive_own_quit_cutoff, profile=args.profile)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        result = {"schema": "shared-map-score-v1", "pass": False,
                  "error": f"{type(exc).__name__}: {exc}", "input_independence_proven": False}
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
