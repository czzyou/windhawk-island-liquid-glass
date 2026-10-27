"""Keep successful local release builds reusable until their inputs change."""
import hashlib
import json


def fingerprint(root, version):
    paths = {root / 'dynamic-island-liquid-glass.wh.cpp',
             root / f'local@dynamic-island-liquid-glass_{version}.dll',
             root / '32' / f'local@dynamic-island-liquid-glass_{version}.dll',
             root / 'requirements.txt'}
    for pattern in ('*.py', '*.h', '*.inc', 'updater/*.ps1', 'updater/*.py'):
        paths.update(root.glob(pattern))
    return {'version': version, 'files': {
        path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(paths)}}


def record(root, version):
    (root / '.local-build.json').write_text(
        json.dumps(fingerprint(root, version), indent=2) + '\n', encoding='utf-8')


def valid(root, version):
    try:
        saved = json.loads((root / '.local-build.json').read_text(encoding='utf-8'))
        return saved == fingerprint(root, version)
    except (OSError, ValueError):
        return False
