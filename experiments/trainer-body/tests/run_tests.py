"""Build/run own x86 helpers. Never launches, attaches to or patches a game."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGET = '4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--game-exe', type=Path)
    args = parser.parse_args()
    if os.name != 'nt':
        raise SystemExit('The complete helper suite requires 64-bit Windows and x86 LLVM-MinGW.')
    cc = args.compiler.resolve(strict=True)
    if args.game_exe:
        args.game_exe = args.game_exe.resolve(strict=True)
        if hashlib.sha256(args.game_exe.read_bytes()).hexdigest() != TARGET:
            raise SystemExit('Optional game file has a different fingerprint; no file was executed.')
    out = ROOT / 'build/tests' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + f'-{os.getpid()}')
    out.mkdir(parents=True, exist_ok=False)
    results = []

    def run(command, label, expected_passes=None, cwd=None):
        p = subprocess.run([str(a) for a in command], cwd=cwd or out, capture_output=True,
                           text=True, encoding='utf-8', errors='replace', timeout=30)
        text = p.stdout + p.stderr
        (out / (label + '.log')).write_text(text, encoding='utf-8')
        count = len(re.findall(r'^PASS ', text, re.M))
        if p.returncode or (expected_passes is not None and count != expected_passes):
            raise RuntimeError(f'{label} failed (exit {p.returncode}, PASS lines {count}):\n{text[:4000]}')
        results.append({'case': label, 'passed': True, 'pass_lines': count})
        print(f'PASS {label}')
        return text

    def compile_helper(name, sources, unicode=False, extra=()):
        exe = out / (name + '.exe')
        command = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', '-ffreestanding',
                   '-fno-builtin', '-static-libgcc', '-I', ROOT / 'src', '-I', ROOT / 'tests']
        if unicode:
            command.append('-municode')
        command += list(sources) + list(extra) + ['-o', exe]
        run(command, 'compile-' + name)
        return exe

    dll = ROOT / 'build/winmm.dll'
    if not dll.is_file():
        raise RuntimeError('Run build.ps1 first')
    # Helpers load the proxy only in their own unsupported process. Keep the
    # default load outside all opt-in folders, so it cannot schedule native work.
    for name in ('mw2-trio-control.enable', 'mw2-trio-storage-test.enable',
                 'mw2-trio-cache-test.enable', 'mw2-trio-scripts-test.enable',
                 'mw2-trio-two-player-test.enable','mw2-trio-party-test.enable','mw2-trio-network-test.enable',
                 'mw2-trio-loopback-qa.enable','mw2-trio-input-test.enable','mw2-trio-peer-test.enable',
                 'mw2-trio-body-assets-test.enable'):
        if (dll.parent / name).exists():
            raise RuntimeError('Remove experimental opt-in flags from build/ before helper tests')
    fixture = out / 'synthetic-wrong-hash.bin'
    run([sys.executable, '-I', ROOT / 'tools/make_guard_fixture.py', fixture], 'synthetic-fixture')
    default_command = [ROOT / 'build/bootstrap-helper.exe', dll]
    if args.game_exe:
        default_command += [args.game_exe, fixture]
    run(default_command, 'bootstrap-default', 12 if args.game_exe else 10)
    if (dll.parent / 'startup184.log').exists() or (dll.parent / 'control184.log').exists():
        raise RuntimeError('Inactive default unexpectedly wrote a native startup/control log')

    sha = compile_helper('sha256-helper', [ROOT / 'tests/sha256-helper.c', ROOT / 'src/pure_sha256.c'], True)
    for length in (0, 1, 55, 56, 63, 64, 65, 119, 120, 127, 128, 129, 4097, 65591):
        data = bytes((i * 131 + 17) & 255 for i in range(length))
        path = out / f'own-vector-{length}.bin'
        path.write_bytes(data)
        text = run([sha, path, 'irregular'], f'sha-{length}', 6)
        if f'SHA256_FILE={hashlib.sha256(data).hexdigest()}' not in text:
            raise RuntimeError('SHA helper differs from Python hashlib')
    scripts = compile_helper('scripts184-helper', [ROOT / 'tests/scripts184-helper.c'], True)
    run([scripts], 'script-parser-paths', 46)
    body_policy = compile_helper('bodyassets184-policy-helper', [ROOT / 'tests/bodyassets184-policy-helper.c'])
    text = run([body_policy], 'body-assets-exact-name-policy')
    if 'bodyassets184 pure policy: 42 checks, 0 failures; no native integration' not in text:
        raise RuntimeError('Body asset policy fixture coverage differs')
    body_core = compile_helper('bodyassets184-helper', [ROOT / 'tests/bodyassets184-helper.c',
        ROOT / 'src/bodyassets184_core.c', ROOT / 'src/bodyassets184_tx.c'])
    text = run([body_core], 'body-assets-generation-and-own-page-transaction')
    if 'bodyassets184 offline: 199 checks, 0 failures; no game/native engine' not in text:
        raise RuntimeError('Body asset generation/transaction fixture coverage differs')
    body_probe = compile_helper('bodyassets184-probe-helper', [ROOT / 'tests/bodyassets184-probe-helper.c',
        ROOT / 'src/bodyassets184_probe.c', ROOT / 'src/pure_sha256.c'])
    text = run([body_probe], 'body-assets-typed-synthetic-resource-proof')
    if 'bodyassets184 synthetic DB: 75 checks, 0 failures; no game/native engine' not in text:
        raise RuntimeError('Body asset typed-resource fixture coverage differs')
    body_adapter = compile_helper('bodyassets184-adapter-helper', [ROOT / 'tests/bodyassets184-adapter-helper.c',
        ROOT / 'src/bodyassets184_core.c', ROOT / 'src/bodyassets184_tx.c',
        ROOT / 'src/bodyassets184_probe.c', ROOT / 'src/pure_sha256.c'])
    text = run([body_adapter], 'body-assets-actual-adapter-own-pages-and-files')
    if 'bodyassets184 actual adapter: 128 checks, 0 failures; own files/pages only' not in text:
        raise RuntimeError('Body asset native-adapter fixture coverage differs')
    reliable = compile_helper('reliable184-helper', [ROOT / 'tests/reliable184-helper.c', ROOT / 'src/reliable184.c'])
    reliable_text = run([reliable], 'reliable-first-token-and-rejection')
    if '160204 checks passed; 40000 bounded guard-page mutations' not in reliable_text:
        raise RuntimeError('Reliable decoder fixture coverage differs')
    service = compile_helper('service184-helper', [ROOT / 'tests/service184-helper.c', ROOT / 'src/service184.c'])
    service_text = run([service], 'blocked-external-ip-positive-classifier')
    if '90076 checks passed; exhaustive39-byte field mutations;40000 guard-page inputs' not in service_text:
        raise RuntimeError('Blocked service classifier fixture coverage differs')
    backend = compile_helper('backend184-helper', [ROOT / 'tests/backend184-helper.c', ROOT / 'src/backend184.c'])
    backend_text = run([backend], 'blocked-native-backend-context-classifier')
    if 'pure context checks passed; no payload/game/network calls' not in backend_text:
        raise RuntimeError('Native backend classifier fixture coverage differs')
    net = compile_helper('net184-helper', [ROOT / 'tests/net184-helper.c', ROOT / 'src/reliable184.c', ROOT / 'src/service184.c', ROOT / 'src/backend184.c', ROOT / 'src/pure_sha256.c'], extra=['-lws2_32'])
    run([net], 'socket-forwarding-and-private-boundaries-with-fake-callees', 23)
    qa_identity = compile_helper('qa_identity184-helper', [ROOT / 'tests/qa_identity184-helper.c'])
    run([qa_identity], 'private-identity-own-fixtures', 130)
    peer_scope = compile_helper('peer_scope184-helper', [ROOT / 'tests/peer_scope184-helper.c',
        ROOT / 'src/peer_scope184.c', ROOT / 'src/pure_sha256.c'])
    scope_text = run([peer_scope], 'exact-peer-pure-config-tuples-identity')
    if '473325 pure checks passed; no game/provider/network calls' not in scope_text:
        raise RuntimeError('Pure peer fixture coverage differs')
    peer_transport = compile_helper('peer_transport184-helper', [ROOT / 'tests/peer_transport184-helper.c',
        ROOT / 'src/peer_scope184.c', ROOT / 'src/pure_sha256.c'], extra=['-lws2_32'])
    transport_text = run([peer_transport], 'exact-peer-mock-file-interface-bind')
    if '983 mock checks passed; no real files/interfaces/sockets/game/provider calls' not in transport_text:
        raise RuntimeError('Mock peer adapter fixture coverage differs')
    runtime = compile_helper('runtime184-helper', [ROOT / 'tests/runtime184-helper.c'])
    run([runtime], 'runtime-flags-and-error-wrapper', 65)
    cow = compile_helper('runtime184-cow-helper', [ROOT / 'tests/runtime184-cow-helper.c'],
                         extra=['-Wl,--image-base,0x10000000'])
    run([cow], 'runtime-exact-table-cow-contract', 35)
    image_cow = compile_helper('runtime184-image-cow-helper', [ROOT / 'tests/runtime184-image-cow-helper.c'])
    run([image_cow], 'runtime-own-page-transaction', 16)
    run([image_cow, '--own-image'], 'runtime-own-image-cow-reproduction', 6)

    source = (ROOT / 'src/bootstrap.c').read_text(encoding='utf-8')
    marker = 'static int proxy_init(void)'
    if source.count(marker) != 1:
        raise RuntimeError('Bootstrap guard boundary changed; review snapshot extraction')
    snapshot = out / 'guard-readonly-snapshot.c'
    snapshot.write_text(source[:source.index(marker)], encoding='utf-8')
    cache = compile_helper('cache184-helper', [ROOT / 'tests/cache184-helper.c', snapshot, ROOT / 'src/pure_sha256.c'])
    run([cache], 'cache-transaction-reset', 37)

    dynamic = compile_helper('control-negative-helper', [ROOT / 'tests/control-negative-helper.c'], True)
    static = compile_helper('startup-negative-helper', [ROOT / 'tests/startup-negative-helper.c'], True, ['-lwinmm'])
    for component in ('storage', 'cache', 'scripts','two-player'):
        for kind in ('dynamic', 'static', 'missing-control'):
            label = f'{component}-{kind}'
            folder = out / label
            folder.mkdir()
            shutil.copy2(dll, folder / 'winmm.dll')
            (folder / f'mw2-trio-{component}-test.enable').write_text('own negative helper only\n', encoding='ascii')
            if kind != 'missing-control':
                (folder / 'mw2-trio-control.enable').write_text('own negative helper only\n', encoding='ascii')
            mailbox = folder / 'control-command.txt'
            mailbox.write_text('// must remain untouched\n', encoding='ascii')
            if kind == 'dynamic':
                helper = folder / dynamic.name
                shutil.copy2(dynamic, helper)
                command = [helper, folder / 'winmm.dll', folder / 'control184.log', mailbox]
                passes = 4
            else:
                helper = folder / static.name
                shutil.copy2(static, helper)
                command = [helper, folder / 'startup184.log', mailbox]
                if kind == 'missing-control':
                    command.append('missing-control')
                passes = 5
            run(command, label, passes, folder)
            startup = (folder / 'startup184.log').read_text(encoding='ascii')
            if 'phase=attach-refused' not in startup or 'phase=attach-installed' in startup or not mailbox.exists():
                raise RuntimeError('Unsupported helper reached a native startup path or consumed its mailbox')

    policy = (ROOT / 'tests/trio_policy.c').read_text(encoding='utf-8')
    variants = [
        ('baseline', '', '', 0),
        ('one-peer-ready', 'r->acknowledged == 7', 'r->acknowledged != 1', 1),
        ('ignore-revision-and-digest', '|| get32(wire + 20) != r->revision', '', 1),
        ('ignore-generation-and-digest', '|| get64(wire + 12) != r->generation', '', 1),
        ('fourth-clobbers', 'if (slot == TRIO_MEMBERS) return -1;', 'if (slot == TRIO_MEMBERS) slot = 1;', 2),
        ('ignore-recipient', '|| wire[11] != known_sender', '', 1),
        ('stock-namespace', 'memcmp(wire, magic, 8)', '0', 2),
        ('ignore-snapshot-digest', 'if (memcmp(digest, wire + 32, 32)) return 0;', '(void)digest;', 1),
    ]
    for name, old, new, matches in variants:
        if matches and policy.count(old) != matches:
            raise RuntimeError('Policy mutation no longer matches the reviewed source: ' + name)
        changed = policy.replace(old, new) if matches else policy
        if name in ('ignore-revision-and-digest', 'ignore-generation-and-digest'):
            changed = changed.replace('if (memcmp(digest, wire + 32, 32)) return 0;', '(void)digest;')
        candidate = out / (name + '.c')
        candidate.write_text(changed, encoding='utf-8')
        exe = compile_helper('policy-' + name, [candidate, ROOT / 'tests/trio_policy_test.c', ROOT / 'src/pure_sha256.c'])
        p = subprocess.run([str(exe)], cwd=out, capture_output=True, text=True, timeout=30)
        (out / ('policy-' + name + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8')
        expected = 0 if name == 'baseline' else 1
        if p.returncode != expected:
            raise RuntimeError(f'Policy test did not detect expected outcome: {name}, exit {p.returncode}')
        results.append({'case': 'policy-' + name, 'passed': True, 'exit_code': p.returncode,
                        'summary': p.stdout.splitlines()[-1] if p.stdout else ''})
        print(f'PASS policy-{name}' + (' (broken variant detected)' if expected else ''))

    report = {'scope': 'own helpers only; no game launch, native installation or network I/O',
              'game_file_opened_read_only': bool(args.game_exe), 'cases': results,
              'policy_mutation_cases': 200000, 'broken_policy_variants_caught': 7,
              'passed': True, 'dll_sha256': hashlib.sha256(dll.read_bytes()).hexdigest()}
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'RESULT: PASS ({len(results)} helper/build cases; no cooperative gameplay test)')
    print('Receipt: ' + (out / 'results.json').relative_to(ROOT).as_posix())


if __name__ == '__main__':
    main()
