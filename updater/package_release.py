"""Extract settings defaults using the same YAML parser as the source build."""
import json
import re
import sys
from pathlib import Path

import yaml


def settings_defaults(source):
    block = source.split('// ==WindhawkModSettings==', 1)[1].split('/*', 1)[1].split('*/', 1)[0]
    result = []

    def walk(items, prefix=''):
        for entry in items:
            values = [(k, v) for k, v in entry.items() if not k.startswith('$')]
            if len(values) != 1:
                raise ValueError('Each setting must contain one name')
            key, value = values[0]
            name = prefix + key
            if isinstance(value, list):
                if not all(isinstance(item, dict) for item in value):
                    raise ValueError('Array defaults require explicit implementation')
                walk(value, name + '.')
            elif isinstance(value, (bool, int)):
                result.append(dict(name=name, kind='DWord', value=int(value)))
            elif isinstance(value, str):
                result.append(dict(name=name, kind='String', value=value))
            else:
                raise ValueError(f'Unsupported setting: {name}')

    walk(yaml.safe_load(block))
    assert len({s['name'].lower() for s in result}) == len(result)
    assert all(re.fullmatch(r'[A-Za-z0-9_.-]{1,128}', s['name']) for s in result)
    return result


if __name__ == '__main__':
    source = Path(sys.argv[1]).read_text(encoding='utf-8-sig')
    Path(sys.argv[2]).write_text(json.dumps(settings_defaults(source), ensure_ascii=False), encoding='utf-8')
