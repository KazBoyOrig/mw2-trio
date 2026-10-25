"""Opt-in developer launcher for two-player trainer/pit only. Own code, MIT.

Default is read-only PLAN_ONLY. No download, process search, firewall/security
change, account API, automatic progression, or native memory access is provided.
"""
from __future__ import annotations

import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import queue
import re
import shutil
import stat
import struct
import subprocess
import sys
import threading
import time
import uuid

REPO = Path(__file__).resolve().parents[1]
EXE_SHA = "4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9"
MARKER = ".mw2-trio-copy.json"
ACTIVE = ".mw2-trio-active.json"
FLAGS = ("mw2-trio-control.enable", "mw2-trio-network-test.enable",
         "mw2-trio-party-test.enable", "mw2-trio-peer-test.enable",
         "mw2-trio-two-player-test.enable", "mw2-trio-scripts-test.enable")
OFF_FLAGS = ("mw2-trio-loopback-qa.enable", "mw2-trio-storage-test.enable",
             "mw2-trio-cache-test.enable", "mw2-trio-input-test.enable")
LOGS = ("control184.log", "startup184.log", "bootstrap-diagnostics.log",
        "network184.log", "scripts184.log", "runtime184.log", "party184.log",
        "cache184-fatal.log", "main/console.log")
BOOT = "set developer 1\nset logfile 2\nset com_maxfps 60\n"
CAMPAIGN = ('set gameMode so\nset specialops 0\nset coop_mapName trainer\nset start pit\nset mw2coop_god 0\n'
            'set mw2coop_qa ""\nset mw2coop_qa_phase ""\n')
TICK = re.compile(r"^tick=(\d+) tid=(\d+) (.*)$")
ACK = re.compile(r"mailbox enqueued count=(\d+) bytes=(\d+); execution requires native state evidence")
U32 = 0xffffffff


class Refused(RuntimeError):
    pass


def regular(path: Path, directory=False, single=False):
    s = path.lstat()
    if stat.S_ISLNK(s.st_mode) or getattr(s, "st_file_attributes", 0) & 0x400:
        raise Refused(f"Reparse/symlink refused: {path}")
    if not (stat.S_ISDIR(s.st_mode) if directory else stat.S_ISREG(s.st_mode)):
        raise Refused(f"Unexpected file type: {path}")
    if single and s.st_nlink != 1:
        raise Refused(f"Hardlinked writable file refused: {path}")
    return s


def ancestors(path: Path):
    for parent in reversed(path.parents):
        regular(parent, directory=True)


def absolute(text):
    p = Path(text)
    if not p.is_absolute():
        raise Refused("Use explicit absolute paths")
    ancestors(p)
    canonical = p.resolve(strict=False)
    ancestors(canonical)
    return canonical


def relative(text):
    p = PurePosixPath(text)
    if not text or "\\" in text or ":" in text or p.is_absolute() or any(x in ("", ".", "..") for x in text.split("/")):
        raise Refused("Noncanonical manifest path")
    return Path(*p.parts)


def read(path, limit=1024 * 1024):
    regular(path)
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise Refused(f"File exceeds bounded read: {path.name}")
    return data


def digest(path):
    regular(path)
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def steam_dependency(root, expected=None):
    """Read the required dependency before copying or staging; never load it."""
    try:
        observed = digest(root / "steam_api.dll")
    except FileNotFoundError as exc:
        raise Refused("Required steam_api.dll is missing. Review protection history and repair the game "
                      "from a trusted source; see docs/TROUBLESHOOTING.md.") from exc
    except OSError as exc:
        code = getattr(exc, "winerror", None)
        if code in (225, 226):
            raise Refused(f"Windows security blocked steam_api.dll (error {code}). Stop preparation or launch "
                          "and review the detection; see docs/TROUBLESHOOTING.md.") from exc
        raise Refused("Cannot read required steam_api.dll. Check access to the game files; "
                      "this error alone does not establish an antivirus detection.") from exc
    if expected is not None and (not re.fullmatch(r"[0-9a-f]{64}", expected) or observed != expected):
        raise Refused("steam_api.dll differs from the prepared-copy inventory. Repair or prepare "
                      "a trusted installation; no file was replaced.")
    return observed


def ipv4(text):
    if not re.fullmatch(r"(?:0|[1-9][0-9]{0,2})(?:\.(?:0|[1-9][0-9]{0,2})){3}", text):
        raise argparse.ArgumentTypeError("Use canonical numeric IPv4, without leading zeros")
    a = tuple(int(x) for x in text.split("."))
    if (max(a) > 255 or a[0] in (0, 127) or a[0] >= 224
            or a[:2] == (169, 254) or a[3] in (0, 255)):
        raise argparse.ArgumentTypeError("Address is outside the exact-peer endpoint policy")
    return text


def port(text):
    if not re.fullmatch(r"[1-9][0-9]{0,4}", text) or int(text) > 65535:
        raise argparse.ArgumentTypeError("Use a canonical port from 1 to 65535")
    return int(text)


def seed(text):
    if not re.fullmatch(r"[0-9a-fA-F]{32}", text) or int(text, 16) == 0:
        raise argparse.ArgumentTypeError("Use a nonzero 128-bit seed (32 hexadecimal digits)")
    return text.lower()


def peer_config(args):
    if args.role not in ("host", "client"):
        raise Refused("Role must be host or client")
    local, peer = ipv4(args.local_ip), ipv4(args.peer_ip)
    lp, pp = port(str(args.local_port)), port(str(args.peer_port))
    if (local, lp) == (peer, pp):
        raise Refused("Local and peer endpoint tuples must differ")
    text = (f"version=1\nrole={args.role}\nsession_nonce={seed(args.session_seed)}\n"
            f"local_ip={local}\nlocal_port={lp}\npeer_ip={peer}\npeer_port={pp}\n")
    if len(text.encode("ascii")) > 256:
        raise Refused("Peer configuration is too long")
    return text


def tree(root):
    regular(root, directory=True)
    for base, dirs, files in os.walk(root, followlinks=False):
        for name in sorted(dirs):
            regular(Path(base) / name, directory=True)
        for name in sorted(files):
            p = Path(base) / name
            regular(p)
            yield p.relative_to(root), p


def selected(rel):
    parts = tuple(x.lower() for x in rel.parts)
    return not (any(x in {"players", "players2", "save", "saves", "savegames", "logs", "mw2trio", "__pycache__"} for x in parts)
                or parts[-1].endswith((".sav", ".svg", ".log", ".enable", ".dmp"))
                or parts[-1].startswith(("mw2-trio-", ".mw2-trio-", "control-command", "owned-"))
                or (len(parts) == 1 and parts[0] in {"winmm.dll", "__iw4sp", "__iw4mp"}))


def copy_new(source, target):
    before = regular(source)
    ancestors(target)
    h = hashlib.sha256()
    with source.open("rb") as src, target.open("xb") as dst:
        for chunk in iter(lambda: src.read(4 * 1024 * 1024), b""):
            dst.write(chunk)
            h.update(chunk)
    after = regular(source)
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise Refused("Source changed during independent copy")
    regular(target, single=True)
    if os.path.samefile(source, target) or digest(target) != h.hexdigest():
        raise Refused("Independent copy verification failed")
    return {"bytes": before.st_size, "sha256": h.hexdigest()}


def prepare(source, target, execute=False):
    source, target = absolute(source), absolute(target)
    regular(source, directory=True)
    if target.exists() or source == target or source.is_relative_to(target) or target.is_relative_to(source):
        raise Refused("Destination must be new and separate, with neither path nested in the other")
    regular(target.parent, directory=True)
    if digest(source / "iw4sp.exe") != EXE_SHA:
        raise Refused("Unsupported source executable")
    dependency_sha = steam_dependency(source)
    regular(source / "main", directory=True)
    regular(source / "zone", directory=True)
    files = [(rel, p) for rel, p in tree(source) if selected(rel)]
    result = {"mode": "EXECUTE" if execute else "PLAN_ONLY", "operation": "prepare-copy",
              "files": len(files), "bytes": sum(p.stat().st_size for _, p in files),
              "destination": str(target), "source_untouched": True}
    if not execute:
        return result
    target.mkdir()
    inventory = {}
    # On failure leave the partial copy unmarked and ineligible; never delete it recursively.
    for rel, p in files:
        dest = target / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        inventory[rel.as_posix()] = copy_new(p, dest)
    if inventory.get("steam_api.dll", {}).get("sha256") != dependency_sha:
        raise Refused("steam_api.dll changed after preflight; partial copy is not eligible for launch")
    marker = {"schema": "mw2-trio-independent-copy-v1", "root": str(target),
              "source": str(source), "exe_sha256": EXE_SHA, "files": inventory}
    with (target / MARKER).open("x", encoding="utf-8") as stream:
        json.dump(marker, stream, indent=2)
    return result


def verify_copy(root):
    regular(root, directory=True)
    marker = json.loads(read(root / MARKER, 16 * 1024 * 1024))
    if (marker.get("schema") != "mw2-trio-independent-copy-v1" or marker.get("root") != str(root)
            or marker.get("exe_sha256") != EXE_SHA or not isinstance(marker.get("files"), dict)):
        raise Refused("Missing or incompatible independent-copy provenance")
    source = absolute(marker["source"])
    if source == root or source.is_relative_to(root) or root.is_relative_to(source):
        raise Refused("Copy provenance aliases the source")
    dependency = marker["files"].get("steam_api.dll")
    if not isinstance(dependency, dict) or not isinstance(dependency.get("sha256"), str):
        raise Refused("Incomplete dependency inventory")
    steam_dependency(root, dependency["sha256"])
    for name, item in marker["files"].items():
        p = root / relative(name)
        if not selected(relative(name)) or regular(p, single=True).st_size != item["bytes"]:
            raise Refused("Incomplete or linked prepared copy")
    for name in ("iw4sp.exe", "steam_api.dll"):
        if name not in marker["files"]:
            raise Refused("Incomplete copy inventory")
    if digest(root / "iw4sp.exe") != EXE_SHA:
        raise Refused("Unsupported target executable")
    for _, p in tree(root):
        regular(p, single=True)
    if any(p.name not in FLAGS + OFF_FLAGS for p in root.glob("mw2-trio-*.enable")):
        raise Refused("Unknown experiment opt-in preserved; use a clean prepared copy")
    if any((root / name).exists() for name in (ACTIVE, "control-command.txt", "control-command.next", "__iw4sp")):
        raise Refused("Existing process marker or mailbox: stop and inspect; nothing was overwritten")


def verify_framework(folder, expected=None):
    manifest = read(folder / "manifest.json")
    data = json.loads(manifest)
    wanted = {"manifest.json": {"bytes": len(manifest), "sha256": hashlib.sha256(manifest).hexdigest()}}
    for item in data["files"]:
        name = relative(item["file"]).as_posix()
        if name in wanted or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
            raise Refused("Duplicate/invalid framework manifest entry")
        wanted[name] = {"bytes": item["bytes"], "sha256": item["sha256"]}
    actual = {rel.as_posix(): p for rel, p in tree(folder)}
    if set(actual) != set(wanted) or expected is not None and wanted != expected:
        raise Refused("Framework tree differs; existing content is preserved")
    for name, info in wanted.items():
        if regular(actual[name], single=True).st_size != info["bytes"] or digest(actual[name]) != info["sha256"]:
            raise Refused("Framework content hash differs")
    return wanted


def bundle(repo):
    dll = repo / "build/winmm.dll"
    data = read(dll, 16 * 1024 * 1024)
    offset = struct.unpack_from("<I", data, 0x3c)[0] if len(data) >= 64 else len(data)
    if (data[:2] != b"MZ" or offset + 26 > len(data) or data[offset:offset+4] != b"PE\0\0"
            or struct.unpack_from("<H", data, offset+4)[0] != 0x14c
            or not struct.unpack_from("<H", data, offset+22)[0] & 0x2000
            or struct.unpack_from("<H", data, offset+24)[0] != 0x10b):
        raise Refused("Own built proxy must be an x86 PE32 DLL")
    sha = digest(dll)
    known = set()
    for receipt in sorted((repo / "build/tests").glob("*/results.json")):
        ancestors(receipt)
        record = json.loads(read(receipt, 4 * 1024 * 1024))
        if record.get("passed") is True:
            known.add(record.get("dll_sha256"))
    if sha not in known:
        raise Refused("Run the public helper suite for this exact built DLL first")
    return {"dll": dll, "dll_sha256": sha, "known_dlls": known,
            "framework": verify_framework(repo / "content/framework")}


def mkdir_safe(path):
    if path.exists():
        regular(path, directory=True)
    else:
        mkdir_safe(path.parent)
        path.mkdir()


class Files:
    """Back up all managed files before mutation; restore only after own exit."""
    def __init__(self, root, output):
        self.root, self.output, self.saved = root, output, {}
        self.backup_complete = False

    def backup(self, names):
        if self.backup_complete:
            raise Refused("Managed file backup is single-use")
        for name in sorted(set(names)):
            rel = relative(name)
            p = self.root / rel
            ancestors(p)
            if p.exists():
                regular(p, single=True)
                b = self.output / "before" / rel
                mkdir_safe(b.parent)
                copy_new(p, b)
                shutil.copystat(p, b)
                self.saved[name] = b
            else:
                self.saved[name] = None
        self.backup_complete = True

    def remove(self, name):
        if not self.backup_complete:
            raise Refused("No mutation before the complete managed-file backup")
        p = self.root / relative(name)
        ancestors(p)
        if p.exists():
            regular(p, single=True)
            p.unlink()

    def put(self, name, data):
        if not self.backup_complete:
            raise Refused("No staging before the complete managed-file backup")
        p = self.root / relative(name)
        mkdir_safe(p.parent)
        self.remove(name)
        with p.open("xb") as stream:
            stream.write(data)

    def restore(self):
        errors = []
        if not self.backup_complete:
            # Backup itself only reads the copy. No staging or engine start can
            # have occurred; unbacked pre-existing cfg files must stay untouched.
            return errors
        # Preserve nonconfiguration player files/saves created in the private copy.
        players = self.root / "players"
        if players.exists():
            for rel, _ in tree(players):
                if rel.suffix.lower() == ".cfg":
                    self.saved.setdefault("players/" + rel.as_posix(), None)
        for name, backup in self.saved.items():
            try:
                self.remove(name)
                if backup is not None:
                    p = self.root / relative(name)
                    mkdir_safe(p.parent)
                    copy_new(backup, p)
                    shutil.copystat(backup, p)
            except OSError as exc:
                errors.append(f"{name}: {exc}")
            except Refused as exc:
                errors.append(str(exc))
        return errors


def fields(body):
    return dict(re.findall(r"(?:^| )([A-Za-z0-9_]+)=([^ ]*)", body))


def tick_now():
    return ctypes.windll.kernel32.GetTickCount() & U32


class Control:
    def __init__(self, root, process, *, clock=time.monotonic, sleep=time.sleep, tick=tick_now):
        self.root, self.process = root, process
        self.clock, self.sleep, self.tick = clock, sleep, tick
        self.events = []
        self.count = 0

    def rows(self):
        path = self.root / "control184.log"
        if not path.exists():
            return []
        regular(path, single=True)
        with path.open("rb") as stream:
            stream.seek(max(0, path.stat().st_size - 2 * 1024 * 1024))
            text = stream.read(2 * 1024 * 1024).decode("utf-8", errors="strict")
        rows = []
        for line in text.splitlines():
            m = TICK.fullmatch(line)
            if m:
                rows.append((int(m[1]) & U32, m[3], fields(m[3])))
        return rows

    def latest(self, prefix):
        return next((r for r in reversed(self.rows()) if r[1].startswith(prefix)), None)

    def fresh(self, row):
        return row is not None and (self.tick() - row[0]) & U32 <= 2500

    def check(self):
        if self.process.poll() is not None:
            raise Refused("Owned game exited during setup")
        for _, body, f in self.rows():
            if "fatal" in body.lower() or (body.startswith("party_commands ") and int(f.get("result", 0)) >= 11):
                raise Refused("Native diagnostic refused the setup; inspect private logs")
        path = self.root / "party184.log"
        if path.exists():
            text = read(path, 1024 * 1024).decode("ascii")
            if re.search(r"(?:^| )open_result=-[1-9]\d*|party184 result=1[1-7](?: |$)", text):
                raise Refused("Native party/socket guard refused; no bind success is assumed")

    def wait(self, predicate, seconds=90, stable=0):
        deadline, since, seen = self.clock() + seconds, None, set()
        while self.clock() < deadline:
            self.check()
            row = predicate()
            if row and self.fresh(row):
                since = self.clock() if since is None else since
                seen.add(row[0])
                if self.clock() - since >= stable and (not stable or len(seen) >= 2):
                    return row
            else:
                since, seen = None, set()
            self.sleep(.1)
        raise Refused("Bounded native readiness/ACK gate timed out")

    def write(self, commands):
        data = commands.encode("ascii")
        if not 1 <= len(data) <= 4096:
            raise Refused("Invalid bounded mailbox")
        target, temp = self.root / "control-command.txt", self.root / "control-command.next"
        if target.exists() or temp.exists():
            raise Refused("Pending mailbox preserved")
        with temp.open("xb") as stream:
            stream.write(data)
        # Windows rename refuses an existing destination; never replace a mailbox.
        os.rename(temp, target)
        self.count += 1
        event = {"count": self.count, "bytes": len(data), "commands": commands, "consumed": False}
        self.events.append(event)
        return event

    def consume(self, event, seconds=30):
        def accepted():
            for row in self.rows():
                m = ACK.fullmatch(row[1])
                if not m:
                    continue
                count, size = int(m[1]), int(m[2])
                if count > event["count"]:
                    raise Refused("Unexpected concurrent mailbox ACK")
                if count == event["count"]:
                    if size != event["bytes"]:
                        raise Refused("Mailbox ACK byte count differs")
                    if not (self.root / "control-command.txt").exists():
                        event.update(consumed=True, native_tick=row[0])
                        return row
            return None
        self.wait(accepted, seconds)

    def send(self, commands, seconds=30):
        event = self.write(commands)
        self.consume(event, seconds)
        return event

    def party_gate(self, role):
        row = self.latest("party host=")
        if not row:
            return None
        f = row[2]
        wanted = {"host": str(int(role == "host")), "active": "1", "launch": "0",
                  "session_active": "1", "member0": "1", "member1": "1", "native_online": "0"}
        wanted.update({"pending_accept_ack": "0", "launch_blocked": "0"} if role == "host" else {"client_state": "0"})
        return row if all(f.get(k) == v for k, v in wanted.items()) else None

    def campaign_frame(self, event):
        row = self.latest("frame=")
        if not row or not event.get("consumed"):
            return None
        f = row[2]
        wanted = {"sv_running": "0", "coop": "1", "specialops": "0", "game_mode": "2"}
        return row if (all(f.get(k) == v for k, v in wanted.items())
                       and int(f.get("enqueued_mailboxes", -1)) == event["count"]
                       and (row[0] - event["native_tick"]) & U32 < 0x80000000) else None

    def invite_gate(self, role):
        # Occupied members can precede the asynchronous invite UI transition.
        # That transition registers gameMode=so and resets specialops to1. Wait
        # for its native mode2 frame before issuing the campaign override.
        party = self.party_gate(role)
        row = self.latest("frame=")
        wanted = {"sv_running": "0", "coop": "1", "game_mode": "2"}
        return row if (self.fresh(party) and self.fresh(row)
                       and all(row[2].get(k) == v for k, v in wanted.items())) else None

    def campaign_gate(self, event, role):
        party = self.party_gate(role)
        return self.campaign_frame(event) if self.fresh(party) and party[2].get("coop_map") == "trainer" else None

    def configure_campaign(self, role):
        self.wait(lambda: self.invite_gate(role), 90, stable=2)
        event = self.send(CAMPAIGN)
        self.wait(lambda: self.campaign_gate(event, role), 30, stable=2)
        return event


def wait_for_operator(control, message, prompt=input):
    """Keep observing only the retained process while console input waits."""
    control.check()
    result = queue.Queue(maxsize=1)
    def ask():
        try:
            prompt(message)
            result.put((True, None))
        except BaseException as exc:
            result.put((False, exc))
    threading.Thread(target=ask, name="own-launch-confirmation", daemon=True).start()
    while True:
        control.check()
        try:
            accepted, error = result.get_nowait()
        except queue.Empty:
            control.sleep(.1)
            continue
        if not accepted:
            raise Refused(f"Operator confirmation unavailable: {type(error).__name__}")
        return


def stop_owned(control, seconds=8):
    """Only the retained Popen is eligible for termination; no PID lookup."""
    process = control.process
    if process.poll() is not None:
        return False
    deadline = control.clock() + seconds
    while control.clock() < deadline and process.poll() is None:
        if not (control.root / "control-command.txt").exists() and not (control.root / "control-command.next").exists():
            try:
                control.write("quit\n")
                break
            except (OSError, Refused):
                pass
        control.sleep(.1)
    while control.clock() < deadline and process.poll() is None:
        control.sleep(.1)
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=5)
        return True
    return False


def play(args, repo=REPO, *, popen=subprocess.Popen, prompt=input):
    root = absolute(args.test_copy)
    config = peer_config(args)
    verify_copy(root)
    own = bundle(repo)
    content = root / "mw2trio"
    had_content = content.exists()
    if had_content:
        verify_framework(content, own["framework"])
    if (root / "winmm.dll").exists() and digest(root / "winmm.dll") not in own["known_dlls"]:
        raise Refused("Foreign winmm.dll preserved; use a clean prepared copy")
    plan = {"mode": "EXECUTE" if args.execute else "PLAN_ONLY", "role": args.role,
            "map": "trainer", "start": "pit", "dll_sha256": own["dll_sha256"],
            "manual_friend_ready_required": True, "network_or_game_started": False}
    if not args.execute:
        return plan
    if os.name != "nt":
        raise Refused("Game execution is Windows-only")
    output = repo / "local/play-pair" / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:12])
    mkdir_safe(output)
    token = uuid.uuid4().hex
    lock = root / ACTIVE
    with lock.open("x", encoding="ascii") as stream:
        json.dump({"token": token, "launcher_pid": os.getpid()}, stream)
    files = Files(root, output)
    process = control = None
    staged = False
    receipt = {**plan, "output": str(output), "error": None, "forced_termination": False, "restore_errors": []}
    try:
        names = ["winmm.dll", "mw2-trio-peer.cfg", "control-command.txt", "control-command.next", *FLAGS, *OFF_FLAGS, *LOGS]
        players = root / "players"
        if players.exists():
            names += ["players/" + rel.as_posix() for rel, _ in tree(players) if rel.suffix.lower() == ".cfg"]
        files.backup(names)
        for name in (*FLAGS, *OFF_FLAGS, *LOGS):
            files.remove(name)
        files.put("winmm.dll", read(own["dll"], 16 * 1024 * 1024))
        files.put("mw2-trio-peer.cfg", config.encode("ascii"))
        for name in FLAGS:
            files.put(name, b"explicit developer two-player trainer test\n")
        if not had_content:
            content.mkdir()
            staged = True
            for name in own["framework"]:
                dest = content / relative(name)
                mkdir_safe(dest.parent)
                copy_new(repo / "content/framework" / relative(name), dest)
        if digest(root / "winmm.dll") != own["dll_sha256"]:
            raise Refused("Staged DLL hash differs from the tested build snapshot")
        verify_framework(content, own["framework"])
        argv = [str(root / "iw4sp.exe"), "+set", "net_ip", args.local_ip,
                "+set", "net_port", str(args.local_port)]
        # Bootstrap mailbox must already exist at the first native frame.
        control = Control(root, None)
        boot = control.write(BOOT)
        process = popen(argv, cwd=str(root), close_fds=True)
        control.process = process
        with lock.open("w", encoding="ascii") as stream:
            json.dump({"token": token, "launcher_pid": os.getpid(), "game_pid": process.pid}, stream)
        receipt["network_or_game_started"] = True
        setup_deadline = control.clock() + 90
        control.wait(lambda: control.latest("party_commands registered=1"), setup_deadline - control.clock())
        control.consume(boot, setup_deadline - control.clock())
        control.wait(lambda: control.latest("frame="), setup_deadline - control.clock())
        # No raw dvar writes. Startup args and ordinary sets are not bind proof.
        settings = ('mw2trio_peer_identity\n' + f'set net_ip {args.local_ip}\nset net_port {args.local_port}\n'
                    'mw2trio_lan_open\nset coop_autoLaunch 0\nset coop_start ""\n' + CAMPAIGN
                    + 'set coop_mapName trainer\n'
                    + ('mw2trio_lan_host\n' if args.role == "host" else 'mw2trio_party_join\n'))
        control.send(settings)
        if args.role == "client":
            control.send(f"mw2trio_lan_connect {args.peer_ip} {args.peer_port}\n")
        campaign = control.configure_campaign(args.role)
        if args.role == "host":
            wait_for_operator(control, "Local campaign gate passed. Confirm your friend printed READY, then press Enter to launch trainer/pit: ", prompt)
            # Human confirmation is not an implemented cross-peer agreement protocol.
            control.wait(lambda: control.campaign_gate(campaign, "host"), 10)
            control.send("mw2trio_launch\n")
        else:
            print("READY: local campaign settings acknowledged. Tell the host to launch. No remote settings agreement is proven.")
        print("Developer session running. Exit the game normally, or press Ctrl+C here for bounded owned-process cleanup.")
        while process.poll() is None:
            time.sleep(.5)
    except KeyboardInterrupt:
        receipt["error"] = "Operator interrupted developer session"
    except Exception as exc:
        receipt["error"] = f"{type(exc).__name__}: {exc}"
    finally:
        try:
            if process is not None and process.poll() is None:
                receipt["forced_termination"] = stop_owned(control)
        except (Exception, KeyboardInterrupt) as exc:
            receipt["restore_errors"].append("Owned process cleanup incomplete: " + str(exc))
        stopped = process is None or process.poll() is not None
        receipt["owned_process_stopped"] = stopped
        receipt["exit_code"] = process.poll() if process is not None else None
        if stopped and process is not None and receipt["exit_code"] != 0:
            receipt["error"] = receipt["error"] or f"Owned game exited with code {receipt['exit_code']}"
        receipt["mailboxes"] = control.events if control else []
        if stopped:
            for name in LOGS:
                try:
                    p = root / relative(name)
                    if p.exists():
                        dest = output / "after" / relative(name)
                        mkdir_safe(dest.parent)
                        copy_new(p, dest)
                except Exception as exc:
                    receipt["restore_errors"].append(str(exc))
            if staged:
                try:
                    verify_framework(content, own["framework"])
                    for rel, p in list(tree(content)):
                        p.unlink()
                    for directory in sorted((p for p in content.rglob("*") if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
                        directory.rmdir()
                    content.rmdir()
                except Exception as exc:
                    receipt["restore_errors"].append("Staged content preserved: " + str(exc))
            try:
                receipt["restore_errors"] += files.restore()
            except Exception as exc:
                receipt["restore_errors"].append("Restore incomplete: " + str(exc))
            try:
                receipt["executable_unchanged"] = digest(root / "iw4sp.exe") == EXE_SHA
                if not receipt["executable_unchanged"]:
                    receipt["restore_errors"].append("Executable changed; provenance must be inspected")
                if not receipt["restore_errors"] and json.loads(read(lock)).get("token") == token:
                    lock.unlink()
            except Exception as exc:
                receipt["restore_errors"].append("Post-exit verification: " + str(exc))
        with (output / "receipt.json").open("x", encoding="utf-8") as stream:
            json.dump(receipt, stream, indent=2)
            stream.write("\n")
    return receipt


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true", help="explicitly permit preparation or game execution")
    parser.add_argument("--prepare-copy", nargs=2, metavar=("SOURCE", "DEST"))
    parser.add_argument("--role", choices=("host", "client"))
    parser.add_argument("--test-copy")
    parser.add_argument("--local-ip", type=ipv4)
    parser.add_argument("--peer-ip", type=ipv4)
    parser.add_argument("--local-port", type=port)
    parser.add_argument("--peer-port", type=port)
    parser.add_argument("--session-seed", type=seed)
    args = parser.parse_args(argv)
    try:
        play_keys = (args.role, args.test_copy, args.local_ip, args.peer_ip, args.local_port, args.peer_port, args.session_seed)
        if args.prepare_copy:
            if any(x is not None for x in play_keys):
                raise Refused("Prepare-copy and play arguments must be separate")
            result = prepare(*args.prepare_copy, execute=args.execute)
        else:
            if any(x is None for x in play_keys):
                raise Refused("Play requires role, test copy, both endpoint tuples and session seed")
            result = play(args)
        print(json.dumps(result, indent=2))
        return 1 if result.get("error") or result.get("restore_errors") else 0
    except (OSError, ValueError, KeyError, TypeError, Refused) as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
