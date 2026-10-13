#!/usr/bin/env python3
"""Offline native protected-list projection after migration061 (Phase B only).

Complete pinned archive, exact reviewed character identity map and explicit
original VARCHAR/wire encodings required. Keeps remembered names, not today's
character names; rejects unrepresentable rows without changing their archive.
"""
import argparse
import base64
import codecs
from collections import Counter
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from import_during_items import identity_map
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash


def import_protected(conn, manifest_path, world, characters, varchar_encoding, wire_encoding):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    for encoding in (varchar_encoding, wire_encoding):
        if 'Mail'.encode(codecs.lookup(encoding).name) != b'Mail':
            raise RuntimeError('EXPLICIT_ANSI_COMPATIBLE_ENCODING_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    matching = [(t, model, rows) for t, model, rows in loaded
                if (t['database'], t['schema'], t['table']) == ('TGAME_RAGEZONE', 'dbo', 'TPROTECTEDTABLE')]
    if len(matching) != 1:
        raise RuntimeError('EXACT_SOURCE_PROTECTED_TABLE_REQUIRED')
    table, model, rows = matching[0]
    required = {r['dwCharID'] for r in rows} | {r['dwProtected'] for r in rows}
    mapping = identity_map(characters, required)
    if len({(r['dwCharID'], r['dwProtected']) for r in rows}) != len(rows):
        raise RuntimeError('DUPLICATE_SOURCE_PROTECTED_ROW_RETAINED_IN_ARCHIVE')
    if any(count > 255 for count in Counter(r['dwCharID'] for r in rows).values()):
        raise RuntimeError('SOURCE_PROTECTED_LIST_NOT_REPRESENTABLE_AS_BYTE')
    name_column = next(c for c in model['column_mapping'] if c['source'] == 'szNAME')
    source_encoding = 'utf-16-le' if name_column['mssql_type'].lower().startswith(('nvarchar', 'nchar')) else varchar_encoding
    byte_map = {(r['row_number'], r['column']): r['bytes'] for r in table.get('_byte_records', [])}
    wire_names = []
    for n, row in enumerate(rows, 1):
        raw = byte_map.get((n, 'szNAME'))
        if raw is None:
            raise RuntimeError('ORIGINAL_PROTECTED_NAME_BYTES_REQUIRED')
        raw = base64.b64decode(raw, validate=True) if isinstance(raw, str) else bytes(raw)
        if raw.decode(source_encoding, errors='strict') != row['szNAME']:
            raise RuntimeError('SOURCE_PROTECTED_NAME_ENCODING_MISMATCH')
        wire = row['szNAME'].encode(wire_encoding, errors='strict')
        if not wire or len(wire) > 50 or b'\0' in wire:
            raise RuntimeError('SOURCE_PROTECTED_NAME_NOT_REPRESENTABLE_IN_WIRE')
        wire_names.append(wire)
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    mapping_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        conn.execute('LOCK TABLE app_world.worlds,app_global."TCURRENTUSER" IN SHARE MODE')
        worlds = [r[0] for r in conn.execute('SELECT group_id FROM app_world.worlds ORDER BY group_id').fetchall()]
        if world not in worlds:
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        for active_world in worlds:
            for server in range(256):
                if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                                    (0x344D4150, (active_world << 8) | server)).fetchone()[0]:
                    raise RuntimeError('ACTIVE_MAP_OWNER')
        if conn.execute('SELECT 1 FROM app_world.map_sessions LIMIT 1').fetchone() or conn.execute(
                'SELECT 1 FROM app_global."TCURRENTUSER" LIMIT 1').fetchone():
            raise RuntimeError('ALL_SESSIONS_MUST_BE_SETTLED')
        conn.execute('LOCK TABLE app_world."TCHARTABLE",app_world."TPROTECTEDTABLE" IN SHARE ROW EXCLUSIVE MODE')
        conn.execute('LOCK TABLE legacy_game."TPROTECTEDTABLE",reconstruction.source_text_bytes IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        archived = read_target(conn, model, run[0])
        if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
            raise RuntimeError('ARCHIVED_PROTECTED_VALUES_CHANGED')
        if rows and target_bytes_hash(conn, (run[0], 'TGAME_RAGEZONE', 'dbo', 'TPROTECTEDTABLE')) != table['text_bytes_value_sha256']:
            raise RuntimeError('ARCHIVED_PROTECTED_NAME_BYTES_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.protected_imports WHERE run_id=%s AND world_id=%s', (run[0], world)).fetchone():
            raise RuntimeError('PROTECTED_IMPORT_ALREADY_RECORDED')
        if conn.execute('SELECT 1 FROM app_world."TPROTECTEDTABLE" WHERE "bWorldID"=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('EXISTING_PROTECTED_ROWS_CANNOT_BE_OVERWRITTEN')
        owners = {mapping[r['dwCharID']] for r in rows}
        for target in mapping.values():
            existing = conn.execute('SELECT "bDelete" FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s AND "dwCharID"=%s',
                                    (world, target)).fetchone()
            # Soft-deleted targets are valid in source TProtectedInsert. A
            # missing target identity can be recycled later: require its exact
            # reviewed projection instead of guessing a new dangling ID.
            if not existing or (target in owners and existing[0]):
                raise RuntimeError('MAPPED_PROTECTED_CHARACTER_MISSING_OR_DELETED_OWNER')
            if target in owners and conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE world_id=%s AND char_id=%s',
                                                (world, target)).fetchone():
                raise RuntimeError('EXISTING_CHARACTER_RECOVERY_CANNOT_BE_OVERWRITTEN')
        for row, wire in zip(rows, wire_names):
            conn.execute('INSERT INTO app_world."TPROTECTEDTABLE"("bWorldID","dwCharID","dwProtected","szNAME",name_bytes,"bOption") '
                         'VALUES(%s,%s,%s,%s,%s,%s)',
                         (world, mapping[row['dwCharID']], mapping[row['dwProtected']], row['szNAME'], wire, row['bOption']))
        conn.execute('INSERT INTO reconstruction.protected_imports(run_id,world_id,character_mapping_sha256,'
                     'source_varchar_encoding,wire_encoding,source_rows) VALUES(%s,%s,%s,%s,%s,%s)',
                     (run[0], world, mapping_hash, varchar_encoding, wire_encoding, len(rows)))
    return {'run_id': run[0], 'world': world, 'source_rows': len(rows), 'character_mapping_sha256': mapping_hash,
            'source_varchar_encoding': varchar_encoding, 'wire_encoding': wire_encoding}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    parser.add_argument('--source-varchar-encoding', required=True)
    parser.add_argument('--wire-encoding', required=True)
    args = parser.parse_args()
    # No automatic retry, including an unknown COMMIT result.
    with connect() as conn:
        print(json.dumps(import_protected(conn, args.manifest, args.world, json.loads(args.character_map.read_text()),
                                         args.source_varchar_encoding, args.wire_encoding), indent=2))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(f'PROTECTED_IMPORT_FAILED: {error}', file=sys.stderr)
        sys.exit(1)
