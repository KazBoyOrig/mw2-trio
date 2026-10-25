"""Parent-owned private mailbox dispatcher candidate. Own code, MIT.

Import is inert: no CLI, process creation, game memory access or network calls.
Only run_input_plan can write through its explicitly provided mailbox callback.
Caller must own the retained Popen handles, independent test roots and gameslot.
"""
from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import time

import score_independent_inputs as policy
import score_shared_map as shared

ROLES = ("host", "client")
TASK = Path(__file__).resolve().parents[2]


def read_tail(path: Path) -> str:
    info = path.stat(follow_symlinks=False)
    if not path.is_file() or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError("live control log is not an ordinary file")
    with path.open("rb") as source:
        source.seek(0, os.SEEK_END)
        size = source.tell()
        source.seek(max(0, size - 512 * 1024))
        data = source.read(512 * 1024)
    if size > 512 * 1024:
        data = data.partition(b"\n")[2]
    if data and not data.endswith(b"\n"):
        data = data.rpartition(b"\n")[0] + b"\n"
    return data.decode("utf-8", errors="strict")


def latest_telemetry(text: str, role: str):
    """Validate the ENTIRE supplied tail once; retain only needed rows and ACKs.

    Uses the scorer's exact header/integer/field/clock rules. Older malformed
    rows still refuse the snapshot, even when every latest row is healthy.
    This replaces parse_log plus control_tick_records, not the 512KB window.
    Queue/safety-release raw parsing remains separate and unchanged.
    """
    latest, acknowledgments = {}, []
    previous, anchor = None, 0
    for line in text.splitlines():
        match = shared.HEADER.fullmatch(line)
        if not match:
            if line.startswith("tick="):
                raise ValueError(f"{role}: ambiguous completed native telemetry")
            continue
        try:
            tick, _ = shared.integer(match[1]), shared.integer(match[2])
            if previous is None:
                anchor = tick
            else:
                delta = (tick - previous) & shared.U32
                if delta >= shared.HALF:
                    raise ValueError("native tick regressed or ambiguous wrap")
                anchor += delta
            previous = tick
            body = match[3]
            kind = shared.kind_of(body)
            if kind:
                fields = shared.fields(body)  # Validate older rows, too.
                key = (kind, fields.get("slot")) if kind in ("input", "roster") else kind
                latest[key] = {"kind": kind, "tick": anchor, "fields": fields}
            elif body.startswith("mailbox enqueued "):
                ack = shared.MAILBOX.fullmatch(body)
                if ack:
                    acknowledgments.append((tick, ack[1], ack[2]))
        except ValueError as exc:
            raise ValueError(f"{role}: ambiguous completed native telemetry") from exc
    return latest, anchor, [{"tick": tick, "count": int(count), "bytes": int(size)}
                            for tick, count, size in acknowledgments]


def snapshot(text: str, role: str, map_name: str, specialops: int) -> dict:
    latest, anchor, acknowledgments = latest_telemetry(text, role)
    m, party = latest.get("map"), latest.get("party")
    if not (m and party and anchor - m["tick"] <= 1500 and anchor - party["tick"] <= 1500
            and shared.expected_map(m, map_name, specialops) and shared.check_bindings(m)
            and m["fields"].get("cl_paused") == "0" and shared.check_party(party, role)):
        raise ValueError(f"{role}: map/state6/paused0/binding gate failed")
    inputs = {}
    roster = {}
    for slot in range(2):
        r = latest.get(("input", str(slot)))
        member = latest.get(("roster", str(slot)))
        if not r or anchor - r["tick"] > 300 or not member or anchor - member["tick"] > 1500:
            raise ValueError(f"{role}: stale/missing dense input or roster")
        f = r["fields"]
        position = tuple(float(v) for v in f["origin"].split(","))
        if len(position) != 3 or not all(policy.math.isfinite(v) and abs(v) <= 10_000_000 for v in position):
            raise ValueError(f"{role}: invalid origin")
        inputs[slot] = {"level": shared.integer(f["level_time"]),
                        "server": shared.integer(f["server_cmd_time"]),
                        "applied": shared.integer(f["applied_cmd_time"]),
                        "position": position, "axes": f["applied_axes"],
                        "server_axes": f["server_axes"]}
        roster[slot] = member["fields"]
    count = max([shared.integer(m["fields"]["enqueued_mailboxes"])]
                + [a["count"] for a in acknowledgments])
    return {"tick": anchor & shared.U32, "level": min(r["level"] for r in inputs.values()),
            "inputs": inputs, "roster": roster, "port": shared.integer(party["fields"]["net_port"]),
            "count": count, "acks": acknowledgments}


def activation_mismatches(values, phase, pattern):
    """ACK means enqueued; BOTH worlds/slots must show actual producer+apply axes."""
    mismatches = []
    for role in ROLES:
        for slot in range(2):
            expected = policy.expected_axis(phase, slot, pattern)
            sample = values[role]["inputs"][slot]
            if not (policy.matches_axis(sample["server_axes"], expected)
                    and policy.matches_axis(sample["axes"], expected)):
                mismatches.append({"role": role, "slot": slot, "expected": expected,
                    "server_axes": sample["server_axes"], "applied_axes": sample["axes"]})
    return mismatches


def run_input_plan(roots, processes, mailbox, output_dir, map_name="trainer", specialops=1,
                   pattern="original-right",
                   *, _clock=time.monotonic, _sleep=time.sleep, _read=read_tail,
                   _pending=None, _persist=None):
    """Bounded callback-only runner; returns ledger with error on any failure.

    Root must propagate ledger.error to harness failure and append ledger.mailboxes
    to steps_sent in their recorded order before cleanup/cutoff scoring.
    Private underscore callbacks are pure-fixture injection points.
    """
    if (set(roots) != set(ROLES) or set(processes) != set(ROLES)
            or map_name not in policy.MAPS or type(specialops) is not int or specialops not in (0, 1)
            or pattern not in policy.PATTERNS
            or (map_name == policy.MAP and specialops != 1)):
        raise ValueError("unsupported private input-plan arguments")
    roots = {r: Path(p).resolve() for r, p in roots.items()}
    output = Path(output_dir).resolve()
    for p in (*roots.values(), output):
        p.relative_to(TASK)
    if len(set(roots.values())) != 2:
        raise ValueError("two independent roots are required")
    pending = _pending or (lambda role: (roots[role] / "control-command.txt").exists())
    ledger = {"version": 1, "map": map_name, "specialops": specialops, "pattern": pattern,
              "owner_slots": {"host": 0, "client": 1}, "input_method": "native-console-kbutton",
              "error": None, "all_releases_consumed": False, "phases": [], "mailboxes": [],
              "forced_stop": [], "emergency_release": {}}
    started = _clock()
    target, temporary = output / "input-events.json", output / "input-events.next"
    if _persist is None and (target.exists() or temporary.exists()):
        raise ValueError("existing input ledger preserved; choose a fresh owned output directory")
    def persist():
        if _persist:
            _persist(copy.deepcopy(ledger))
        else:
            with temporary.open("x", encoding="utf-8", newline="\n") as stream:
                json.dump(ledger, stream, indent=2, allow_nan=False)
                stream.write("\n")
            os.replace(temporary, target)
    def observe():
        values = {}
        for role in ROLES:
            if processes[role].poll() is not None:
                raise RuntimeError(f"{role}: own process exited")
            values[role] = snapshot(_read(roots[role] / "control184.log"), role, map_name, specialops)
        for role, value in values.items():
            for slot, f in value["roster"].items():
                local = slot == (0 if role == "host" else 1)
                expected_type = 2 if role == "host" and slot == 0 else 4
                port = 0 if role == "host" and slot == 0 else values["client" if role == "host" else "host"]["port"]
                if (f.get("occupied") != "1" or f.get("local_uid_match") != str(int(local))
                        or f.get("address_type") != str(expected_type) or f.get("port") != str(port)):
                    raise ValueError(f"{role}: canonical roster/UID projection changed")
        return values
    def acknowledged(role, event):
        for tick, body, _ in shared.control_tick_records(_read(roots[role] / "control184.log")):
            m = shared.MAILBOX.fullmatch(body)
            if not m:
                continue
            count, size = int(m[1]), int(m[2])
            if count > event["expected_count"]:
                raise RuntimeError(f"{role}: concurrent or skipped mailbox ACK")
            if count == event["expected_count"]:
                if size != event["bytes"]:
                    raise RuntimeError(f"{role}: own mailbox ACK bytes differ")
                if not pending(role):
                    event.update(consumed=True, native_tick=tick, after_count=count,
                                 mailbox_absent=True)
                    return True
        return False
    def validate_progress(name, current, previous, progress_times):
        # Same time/move checks apply while native command activation is pending.
        for role in ROLES:
            for slot in range(2):
                for key in ("level", "server", "applied"):
                    value = current[role]["inputs"][slot][key]
                    old = previous[role]["inputs"][slot][key]
                    if value < old:
                        raise ValueError(f"{name}: native {key} time regressed")
                    if value > old:
                        progress_times[role, slot, key] = _clock()
                    if _clock() - progress_times[role, slot, key] > .5:
                        raise TimeoutError(f"{name}: native {key} time stalled")
                if policy.distance(current[role]["inputs"][slot]["position"],
                                   previous[role]["inputs"][slot]["position"]) > 80:
                    raise ValueError(f"{name}: abrupt script/physics move")
    def queue(role, commands, deadline, *, safety=False, wait_for_ack=True):
        if _clock() >= deadline and not safety:
            raise TimeoutError(f"{role}: dispatch deadline already expired")
        if processes[role].poll() is not None:
            raise RuntimeError(f"{role}: own process exited before dispatch")
        while pending(role):
            if _clock() >= deadline:
                raise TimeoutError(f"{role}: own mailbox remained pending")
            if processes[role].poll() is not None:
                raise RuntimeError(f"{role}: own process exited before dispatch")
            _sleep(.02)
        # Release does not depend on game readiness; count/ACK parsing is raw.
        text = _read(roots[role] / "control184.log")
        rows, _, _ = shared.parse_log(text)
        counts = [int(r["fields"]["enqueued_mailboxes"]) for r in rows if r["kind"] == "map"]
        acks = [shared.MAILBOX.fullmatch(body) for _, body, _ in shared.control_tick_records(text)]
        counts += [int(m[1]) for m in acks if m]
        if not counts:
            raise ValueError(f"{role}: missing native mailbox count baseline")
        event = {"instance": role, "commands": commands, "at": _clock() - started,
                 "before_count": max(counts), "expected_count": max(counts) + 1,
                 "bytes": len(commands.encode("ascii")), "consumed": False}
        mailbox(roots[role], commands)
        ledger["mailboxes"].append(event)
        if not wait_for_ack:
            return event
        while True:
            if acknowledged(role, event):
                return event
            if _clock() >= deadline:
                raise TimeoutError(f"{role}: native mailbox ACK deadline")
            if processes[role].poll() is not None:
                raise RuntimeError(f"{role}: own process exited before ACK")
            _sleep(.02)
    def release_both(deadline):
        released = {r: {"commands": policy.RELEASE, "consumed": False} for r in ROLES}
        # Enqueue every currently available role before waiting for either ACK.
        # A stuck first process must not delay the other process's release.
        while True:
            for role, event in released.items():
                if event["consumed"]:
                    continue
                try:
                    if "expected_count" not in event:
                        if pending(role):
                            event["error"] = "previous own mailbox still pending; never overwritten"
                            continue
                        event = queue(role, policy.RELEASE, deadline, safety=True, wait_for_ack=False)
                        released[role] = event
                    acknowledged(role, event)
                except Exception as exc:
                    event["error"] = str(exc)
            if all(e["consumed"] for e in released.values()) or _clock() >= deadline:
                break
            _sleep(.02)
        return released
    persist()
    try:
        initial = observe()
        if min(v["level"] for v in initial.values()) <= 10000:
            raise ValueError("input gate requires post-regroup game time >10000")
        deadline = _clock() + 3
        while True:
            current = observe()
            progress = min(v["level"] for v in current.values()) - min(v["level"] for v in initial.values())
            advanced = all(current[r]["inputs"][i][k] - initial[r]["inputs"][i][k] >= 1000
                           for r in ROLES for i in range(2) for k in ("server", "applied"))
            if any(v["axes"] != "0,0" for state in current.values() for v in state["inputs"].values()):
                raise ValueError("initial neutral native axes required")
            if any(policy.distance(current[r]["inputs"][i]["position"], initial[r]["inputs"][i]["position"]) > 4
                   for r in ROLES for i in range(2)):
                raise ValueError("initial regroup/positions are not stable")
            if progress >= 1000 and advanced:
                break
            if _clock() >= deadline:
                raise TimeoutError("initial native simulation did not advance within wall bound")
            _sleep(.02)
        for name in policy.PHASES:
            beginning = _clock()
            deadline = beginning + 3
            phase = {"name": name, "dispatch": {}, "release": {},
                     "timing": {"phase_begin": beginning, "deadline": deadline}}
            ledger["phases"].append(phase)
            for role in ROLES:
                phase["dispatch"][role] = queue(role, policy.command_for(name, role, pattern), deadline)
            phase["timing"]["dispatch_finished"] = _clock()
            activation = {"begin": _clock(), "deadline": deadline, "polls": 0,
                          "pending_polls": 0, "activated": False}
            phase["activation"] = activation
            previous = None
            progress_times = {(r, i, k): _clock() for r in ROLES for i in range(2)
                              for k in ("level", "server", "applied")}
            while True:
                activation["last_deadline_check"] = _clock()
                if activation["last_deadline_check"] >= deadline:
                    raise TimeoutError(f"{name}: native axes activation exceeded same 3s wall watchdog")
                poll_begin = _clock()
                current = observe()
                activation["polls"] += 1
                activation["last_poll"] = {"begin": poll_begin, "end": _clock(),
                    "native_ticks": {r: current[r]["tick"] for r in ROLES},
                    "game_ms": min(v["level"] for v in current.values())}
                if previous is not None:
                    validate_progress(name, current, previous, progress_times)
                activation["mismatches"] = activation_mismatches(current, name, pattern)
                # Polling cannot consume the deadline and then activate a phase.
                activation["last_deadline_check"] = _clock()
                if activation["last_deadline_check"] >= deadline:
                    raise TimeoutError(f"{name}: native axes activation exceeded same 3s wall watchdog")
                if not activation["mismatches"]:
                    activation.update(activated=True, at=_clock())
                    break
                activation["pending_polls"] += 1
                activation["last_pending"] = {**activation["last_poll"],
                                               "mismatches": activation["mismatches"]}
                previous = current
                _sleep(.02)
            phase["start_game_ms"] = min(v["level"] for v in current.values())
            wanted = 2000 if name in ("neutral_before", "neutral_after") else 1000
            phase["timing"]["target_game_ms"] = phase["start_game_ms"] + wanted
            previous = current
            while True:
                phase["timing"]["last_deadline_check"] = _clock()
                if phase["timing"]["last_deadline_check"] >= deadline:
                    raise TimeoutError(f"{name}: independent 3s wall watchdog")
                poll_begin = _clock()
                phase["timing"]["poll_started"] = poll_begin
                current = observe()
                phase["timing"]["last_poll"] = {"begin": poll_begin, "end": _clock(),
                    "native_ticks": {r: current[r]["tick"] for r in ROLES},
                    "game_ms": min(v["level"] for v in current.values())}
                validate_progress(name, current, previous, progress_times)
                now = min(v["level"] for v in current.values())
                if now - phase["start_game_ms"] >= wanted:
                    phase["end_game_ms"] = now
                    break
                previous = current
                _sleep(.02)
            phase["release"] = release_both(deadline)
            if not all(e["consumed"] for e in phase["release"].values()):
                raise RuntimeError(f"{name}: release not confirmed")
            phase["wall_ms"] = round((_clock() - beginning) * 1000)
            if phase["end_game_ms"] - phase["start_game_ms"] > wanted + 200:
                raise ValueError(f"{name}: game-clock dispatch overshoot")
            persist()
        ledger["all_releases_consumed"] = True
    except Exception as exc:
        ledger["error"] = f"{type(exc).__name__}: {exc}"
        failed_at = _clock()
        ledger["failure_wall"] = {"at": failed_at, "elapsed": failed_at - started,
            "phase": ledger["phases"][-1]["name"] if ledger["phases"] else "initial"}
    finally:
        ledger["emergency_release"] = release_both(_clock() + 1)
        if not all(e["consumed"] for e in ledger["emergency_release"].values()):
            ledger["all_releases_consumed"] = False
            ledger["error"] = ledger["error"] or "final safety release not confirmed"
            for role in ROLES:
                process = processes[role]
                if process.poll() is None:
                    try:
                        process.terminate()  # only a retained own handle supplied by parent
                        process.wait(timeout=2)
                        ledger["forced_stop"].append(role)
                    except Exception as exc:
                        ledger.setdefault("stop_errors", {})[role] = str(exc)
        persist()
    return ledger
