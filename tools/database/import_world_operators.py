#!/usr/bin/env python3
"""Offline exact TOPERATORTABLE projection after069; written for Phase B only.

Requires the full pinned historical manifest and an explicit injective character
map. No GM-name heuristic, operator credentials, new characters or source rows
are generated. Original archive/source migrations remain read-only.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from import_during_items import identity_map
from migrate import connect, digest, load_snapshot, read_target, table_hash


def import_world_operators(conn, manifest_path, world, characters):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    selected = [(t, model, rows) for t, model, rows in loaded
                if (t['database'], t['schema'], t['table']) == ('TGAME_RAGEZONE', 'dbo', 'TOPERATORTABLE')]
    if len(selected) != 1:
        raise RuntimeError('COMPLETE_OPERATOR_ARCHIVE_REQUIRED')
    table, model, rows = selected[0]
    # Public pinned backup profile proves this table is empty. A manifest that
    # merely repeats backup fingerprints cannot manufacture historical GM IDs.
    if rows:
        raise RuntimeError('PINNED_OPERATOR_ROW_COUNT_MISMATCH')
    required = {row['dwOperatorID'] for row in rows}
    if len(required) != len(rows):
        raise RuntimeError('AMBIGUOUS_SOURCE_OPERATOR_ID')
    mapped = identity_map(characters, required)
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    mapping_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    native_ids = sorted(mapped.values())
    native_hash = digest(','.join(str(i) for i in native_ids).encode('utf8'))
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040009)')
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,legacy_game."TOPERATORTABLE",'
                     'app_world."TCHARTABLE",app_world.worlds IN SHARE MODE')
        conn.execute('LOCK TABLE app_world."TOPERATORTABLE",reconstruction.operator_imports IN SHARE ROW EXCLUSIVE MODE')
        if not conn.execute('SELECT 1 FROM app_world.worlds WHERE group_id=%s', (world,)).fetchone():
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s',
                           (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        cp = conn.execute('SELECT row_count,source_sha256,target_sha256 FROM reconstruction.checkpoints '
                          "WHERE run_id=%s AND source_database='TGAME_RAGEZONE' AND source_schema='dbo' AND source_table='TOPERATORTABLE'",
                          (run[0],)).fetchone()
        actual = read_target(conn, model, run[0])
        if cp != (len(rows), table['value_sha256'], table['value_sha256']) or len(actual) != len(rows) or table_hash(actual, model['column_mapping']) != table['value_sha256']:
            raise RuntimeError('ARCHIVED_OPERATOR_VALUES_CHANGED')
        if conn.execute('SELECT 1 FROM app_world."TOPERATORTABLE" WHERE "bWorldID"=%s LIMIT 1', (world,)).fetchone() or conn.execute(
                'SELECT 1 FROM reconstruction.operator_imports WHERE world_id=%s', (world,)).fetchone():
            raise RuntimeError('OPERATOR_PROJECTION_ALREADY_EXISTS_RECONCILE_OFFLINE')
        for identity in native_ids:
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s AND "dwCharID"=%s AND "bDelete"=0',
                                (world, identity)).fetchone():
                raise RuntimeError('MAPPED_OPERATOR_CHARACTER_REQUIRED')
            conn.execute('INSERT INTO app_world."TOPERATORTABLE"("bWorldID","dwOperatorID") VALUES(%s,%s)', (world, identity))
        conn.execute('INSERT INTO reconstruction.operator_imports(world_id,run_id,character_mapping_sha256,value_sha256,native_sha256,source_rows,operator_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s)', (world, run[0], mapping_hash, table['value_sha256'], native_hash, len(rows), len(native_ids)))
    return {'world_id': world, 'run_id': run[0], 'operator_rows': len(native_ids), 'character_mapping_sha256': mapping_hash}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--characters', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = import_world_operators(conn, args.manifest, args.world, json.loads(args.characters.read_text()))
            print(json.dumps(result))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
