"""Check the Git index for accidental private/runtime material before publishing."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
tracked = subprocess.run(['git', '-C', str(ROOT), 'ls-files', '-z'], check=True,
                         capture_output=True).stdout.decode('utf-8').split('\0')
patterns = {
    'personal Windows home path': r'[A-Za-z]:[\\/]+Users[\\/]+(?!Public[\\/])[^\s"\'<>]+',
    'private key': r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
    'GitHub credential': r'\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,})\b',
}
forbidden = {'.exe', '.dll', '.o', '.obj', '.pdb', '.dmp', '.ff', '.iwd', '.bin', '.log', '.enable'}
errors = []
files = [p for p in tracked if p]
for name in files:
    path = ROOT / name
    if path.is_symlink() or path.suffix.lower() in forbidden or Path(name).parts[0] in {'build', 'local', 'work', 'toolchains'}:
        errors.append(f'{name}: forbidden runtime/private file')
        continue
    data = subprocess.run(['git', '-C', str(ROOT), 'show', ':' + name],
                          check=True, capture_output=True).stdout
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        errors.append(f'{name}: non-UTF-8/binary file')
        continue
    if b'\0' in data:
        errors.append(f'{name}: embedded binary NUL')
    for label, pattern in patterns.items():
        if re.search(pattern, text):
            errors.append(f'{name}: {label}')
if errors:
    raise SystemExit('\n'.join(errors))
manifest = json.loads((ROOT / 'content/framework/manifest.json').read_text(encoding='utf-8'))
for row in manifest['files']:
    name = 'content/framework/' + row['file']
    data = subprocess.run(['git', '-C', str(ROOT), 'show', ':' + name],
                          check=True, capture_output=True).stdout
    if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
        raise SystemExit('Indexed pinned content differs from manifest: ' + name)
print(f'PASS publication scan: {len(files)} indexed UTF-8 source/documentation files; no matched private/runtime patterns')
print(f'PASS content manifest: {len(manifest["files"])} indexed file hashes and sizes match')
print('This pattern scan supplements manual provenance/license review; it is not a complete secret detector.')
