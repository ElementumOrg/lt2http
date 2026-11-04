#!/usr/bin/env python3
"""Checks the translations in web/public/i18n: every file has the keys of en_gb.json with the same {0} placeholders,
and every key used by the Web UI and the server is in en_gb.json. Run by make lint."""
import glob
import json
import os
import re
import sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
i18n = os.path.join(root, 'web', 'public', 'i18n')
errors = []


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


english = load(os.path.join(i18n, 'en_gb.json'))
placeholders = {key: sorted(re.findall(r'\{\d+\}', text)) for key, text in english.items()}

for path in sorted(glob.glob(os.path.join(i18n, '*.json'))):
    name = os.path.basename(path)
    try:
        texts = load(path)
    except ValueError as e:
        errors.append(f'{name}: {e}')
        continue
    for key in english.keys() - texts.keys():
        errors.append(f'{name}: missing {key}')
    for key in texts.keys() - english.keys():
        errors.append(f'{name}: unknown {key}')
    for key, text in texts.items():
        if not isinstance(text, str) or not text.strip():
            errors.append(f'{name}: empty {key}')
        elif key in placeholders and sorted(re.findall(r'\{\d+\}', text)) != placeholders[key]:
            errors.append(f'{name}: {key} has placeholders {re.findall(r"{[0-9]+}", text)}, not {placeholders[key]}')

# Keys in the code: 'group.key' strings in the Web UI (also the ones picked like t(x ? 'a.b' : 'a.c')), "group.key" in the server
groups = 'nav|ui|state|storage|action|confirm|label|section|tracker|pieces|info|torrents|details|files|add|remove|settings|network'
used = set()
for path in glob.glob(os.path.join(root, 'web', 'src', '**', '*.ts*'), recursive=True):
    with open(path, encoding='utf-8') as f:
        used |= set(re.findall(rf"'((?:{groups})\.[a-z_]+)'", f.read()))
for path in glob.glob(os.path.join(root, 'src', '**', '*.*'), recursive=True):
    if os.sep + 'external' + os.sep in path:
        continue
    with open(path, encoding='utf-8') as f:
        used |= set(re.findall(r'"((?:label|section|action|storage|confirm|info|pieces)\.[a-z_]+)"', f.read()))
# Built from values: state.<state_string>, tracker.<status>
used |= {f'state.{s}' for s in ['Queued', 'Checking', 'Finding', 'Downloading', 'Finished', 'Seeding', 'Allocating', 'Stalled', 'Paused',
                                'Buffering', 'Playing', 'Error']}
used |= {f'tracker.{s}' for s in ['working', 'updating', 'failed', 'not_contacted', 'off', 'next_announce']}
for key in sorted(used - english.keys()):
    errors.append(f'en_gb.json: missing {key}, used in the code')

for error in errors:
    print(error)
sys.exit(1 if errors else 0)
