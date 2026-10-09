#!/usr/bin/env python3
"""Generate a source traceability index, never a functional-coverage percentage.

Only legacy numeric opcode expressions are verified automatically. References in
production/test sources are navigation aids, not evidence that a feature works.
Run from any directory; no source files, backups or migrations are modified.
"""
import collections
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '_rewrite/docs/modernization/evidence/source-inventory.json'
TOKEN = re.compile(r'\b(?:CS|MW|DM|CT|SM|RW)_[A-Z0-9_]+\b')
CPP = {'.cpp', '.c', '.h', '.hpp'}


def source(path):
    return path.read_text(encoding='utf-8-sig', errors='replace')


def build():
    paths = sorted(Path(p) for p in subprocess.check_output(
        ['rg', '--files', 'Server', 'Lib/Own'], cwd=ROOT, text=True).splitlines())
    code = {str(p): source(ROOT / p) for p in paths if p.suffix in CPP}
    protocol = ROOT / 'Lib/Own/TProtocol/include'
    bases = dict((name, int(value, 16)) for name, value in re.findall(
        r'#define\s+(\w+)\s+\((0x[\da-fA-F]+)\)', source(protocol / 'ProtocolBase.h')))
    definitions = []
    unresolved = []
    for name in ('CSProtocol.h', 'MWProtocol.h', 'DMProtocol.h', 'CTProtocol.h', 'SSProtocol.h'):
        for line, text in enumerate(source(protocol / name).splitlines(), 1):
            match = re.match(r'\s*#define\s+((?:CS|MW|DM|CT|SM|RW)_[A-Z0-9_]+)\s+(.+)', text)
            if not match:
                continue
            packet, expression = match.groups()
            numeric = re.match(r'\((\w+)\s*\+\s*(0[xX][\da-fA-F]+)\)', expression)
            if not numeric or numeric[1] not in bases:
                unresolved.append({'name': packet, 'expression': expression, 'file': name, 'line': line})
                continue
            definitions.append({'name': packet, 'value': bases[numeric[1]] + int(numeric[2], 16),
                                'legacy_definition': f'Lib/Own/TProtocol/include/{name}:{line}'})
    modern = {n: int(v, 16) for n, v in re.findall(r'^\s+(\w+) = (0x[\da-fA-F]+),',
                                                 source(protocol / 'MessageId.h'), re.M)}
    refs = collections.defaultdict(lambda: collections.defaultdict(list))
    marker_files = []
    modern_sources = []
    for path, text in code.items():
        if 'Asio/' not in path or '/legacy_src/' in path:
            continue
        category = 'test_references' if '/tests/' in path else 'production_references'
        if category == 'production_references' and Path(path).suffix in {'.cpp', '.c'}:
            modern_sources.append(path)
        seen = set()
        markers = []
        for line, value in enumerate(text.splitlines(), 1):
            for name in set(TOKEN.findall(value)) - seen:
                refs[name][category].append(f'{path}:{line}')
                seen.add(name)
            if re.search(r'\b(TODO|FIXME|stub|placeholder|not ported|not implemented)\b', value, re.I):
                markers.append(line)
        if markers:
            marker_files.append({'path': path, 'lines': markers,
                                 'classification': 'UNREVIEWED_TEXT_MARKER_NOT_A_CONFIRMED_GAP'})
    packets = []
    for entry in definitions:
        entry.update(modern_value=modern.get(entry['name']),
                     opcode_status='VERIFIED' if modern.get(entry['name']) == entry['value'] else 'MISMATCH',
                     behavior_status='NOT_ANALYZED', real_client_status='PENDING', **refs[entry['name']])
        packets.append(entry)
    legacy = []
    for path, text in code.items():
        p = Path(path)
        if not path.startswith('Server/') or 'Asio/' in path or p.suffix not in {'.cpp', '.c'}:
            continue
        role = p.parts[1]
        candidates = sorted({r.split(':')[0] for n in set(TOKEN.findall(text))
                             for r in refs[n]['production_references']})
        legacy.append({'path': path, 'role': role, 'status': 'NOT_ANALYZED',
                       'candidate_modern_files_from_shared_packet_names': candidates,
                       'packet_names': sorted(set(TOKEN.findall(text))),
                       'postgresql_status': 'NOT_ANALYZED', 'behavior_tests': 'NOT_ANALYZED'})
    projects = []
    for p in sorted((ROOT / 'Server').rglob('*.vcxproj')):
        if 'Asio' in str(p) or '/.vs/' in str(p):
            continue
        compile_refs = re.findall(r'<ClCompile Include="([^"]+)"', source(p))
        missing = [x for x in compile_refs if not (p.parent / x.replace('\\', '/')).exists()]
        projects.append({'path': str(p.relative_to(ROOT)), 'translation_units': len(compile_refs),
                         'missing_translation_units': missing, 'status': 'NOT_ANALYZED'})
    values = collections.defaultdict(list)
    for p in packets:
        values[p['value']].append(p['name'])
    collisions = {f'0x{v:04X}': names for v, names in values.items() if len(names) > 1}
    modern_only = sorted(set(modern) - {p['name'] for p in definitions})
    mismatches = [p['name'] for p in packets if p['opcode_status'] != 'VERIFIED']
    return {'method': 'Static source index; lexical references include comments and do not prove implementation, dispatch or test coverage.',
            'client_build': 'UNKNOWN; source TVERSION=0x2918 is not a binary client version',
            'summary': {'legacy_translation_units': len(legacy), 'modern_translation_units': len(modern_sources),
                        'legacy_packet_definitions': len(packets), 'modern_enum_entries': len(modern),
                        'opcode_mismatches': mismatches, 'unparsed_packet_definitions': unresolved,
                        'modern_only_opcodes': modern_only, 'collision_values': len(collisions)},
            'legacy_projects': projects, 'legacy_components': legacy, 'modern_sources': modern_sources,
            'packets': packets, 'opcode_collisions': collisions, 'unreviewed_markers': marker_files,
            'source_sha256': {str((protocol / p).relative_to(ROOT)): hashlib.sha256((protocol / p).read_bytes()).hexdigest()
                              for p in ('ProtocolBase.h', 'CSProtocol.h', 'MWProtocol.h', 'DMProtocol.h',
                                        'CTProtocol.h', 'SSProtocol.h', 'MessageId.h')}}


if __name__ == '__main__':
    result = build()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result['summary'], indent=2))
    raise SystemExit(bool(result['summary']['opcode_mismatches'] or result['summary']['unparsed_packet_definitions']
                          or result['summary']['modern_only_opcodes']))
