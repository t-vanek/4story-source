#!/usr/bin/env python3
"""Offline publication of the complete pinned TGUILDCHART after068; Phase B only.

Requires a full verified historical manifest, preserves archival data and never
generates guild caps or player state. Lost COMMIT responses require reconciliation,
not an automatic retry. Private snapshots and receipts must remain outside Git.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash


def activate_world_guild_levels(conn, manifest_path):
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
                if (t['database'], t['schema'], t['table']) == ('TGAME_RAGEZONE', 'dbo', 'TGUILDCHART')]
    if len(selected) != 1:
        raise RuntimeError('COMPLETE_SOURCE_GUILD_CHART_REQUIRED')
    table, model, rows = selected[0]
    if len(rows) != 11 or {row['bLevel'] for row in rows} != set(range(11)):
        raise RuntimeError('GUILD_LEVEL_KEYS_REQUIRE_SOURCE_RECONCILIATION')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='1s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040006)')
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes, '
                     'legacy_game."TGUILDCHART" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.world_guild_level_catalog, '
                     'runtime_control.world_guild_level_activations IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        key = (run[0], 'TGAME_RAGEZONE', 'dbo', 'TGUILDCHART')
        checkpoint = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                                  'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                                  'AND source_schema=%s AND source_table=%s', key).fetchone()
        actual = read_target(conn, model, run[0])
        byte_hash = target_bytes_hash(conn, key) if table.get('text_bytes_file') else None
        if (checkpoint != (11, table['value_sha256'], table['value_sha256'], table.get('text_bytes_value_sha256'), byte_hash)
                or len(actual) != 11 or table_hash(actual, model['column_mapping']) != table['value_sha256']
                or byte_hash != table.get('text_bytes_value_sha256')):
            raise RuntimeError('ARCHIVED_GUILD_CHART_VALUES_CHANGED')
        previous = conn.execute('SELECT run_id,manifest_sha256,value_sha256,row_count '
                                'FROM runtime_control.world_guild_level_catalog WHERE singleton FOR UPDATE').fetchone()
        release = (run[0], fingerprint, table['value_sha256'], 11)
        changed = previous != release
        if changed:
            conn.execute('INSERT INTO runtime_control.world_guild_level_catalog(singleton,run_id,manifest_sha256,value_sha256,row_count) '
                         'VALUES(true,%s,%s,%s,%s) ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id, '
                         'manifest_sha256=EXCLUDED.manifest_sha256,value_sha256=EXCLUDED.value_sha256,row_count=EXCLUDED.row_count, '
                         'activated_at=clock_timestamp()', release)
            conn.execute('INSERT INTO runtime_control.world_guild_level_activations(previous_run_id,run_id,manifest_sha256,value_sha256) '
                         'VALUES(%s,%s,%s,%s)', (previous[0] if previous else None, run[0], fingerprint, table['value_sha256']))
    return {'run_id': run[0], 'manifest_sha256': fingerprint, 'row_count': 11, 'changed': changed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(activate_world_guild_levels(conn, args.manifest)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
