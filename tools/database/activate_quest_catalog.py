#!/usr/bin/env python3
"""Publish original quest conditions from a verified archive while Maps are offline.

The three shared definition/term/reward tables must match the active Map catalog
exactly. This only publishes content; it never imports character progress.
"""
import argparse
import json
from pathlib import Path
import secrets
import sys

from activate_actor_catalog import offline_owner_barrier, verified_actor_import
from activate_catalog import MAPPING, PINNED
from migrate import connect, digest, load_snapshot


def activate_quest_catalog(conn, manifest_path):
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    names = {'TQCONDITIONCHART', 'TQUESTCHART', 'TQUESTTERMCHART', 'TQREWARDCHART'}
    selected = [(t, m, r) for t, m, r in loaded
                if t['database'] == 'TGAME_RAGEZONE' and t['schema'] == 'dbo' and t['table'] in names]
    if {t['table'] for t, _, _ in selected} != names:
        raise RuntimeError('COMPLETE_QUEST_DEFINITION_TABLES_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='1s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040007)')
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'legacy_game."TQCONDITIONCHART",legacy_game."TQUESTCHART",legacy_game."TQUESTTERMCHART",'
                     'legacy_game."TQREWARDCHART",runtime_control.active_catalog IN SHARE MODE')
        run = verified_actor_import(conn, (manifest, selected, fingerprint))
        release = conn.execute("SELECT run_id,manifest_sha256 FROM game_compat.catalog_release WHERE status='verified'").fetchall()
        if len(release) != 1:
            raise RuntimeError('VERIFIED_MAP_CATALOG_REQUIRED')
        map_run, map_manifest = release[0]
        for t, _, rows in selected:
            if t['table'] == 'TQCONDITIONCHART':
                continue
            cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                              'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                              'AND source_schema=%s AND source_table=%s',
                              (map_run, t['database'], t['schema'], t['table'])).fetchone()
            if cp != (len(rows), t['value_sha256'], t['value_sha256'], t['text_bytes_value_sha256'], t['text_bytes_value_sha256']):
                raise RuntimeError('QUEST_DEFINITIONS_DIFFER_FROM_ACTIVE_MAP')
        previous = conn.execute('SELECT run_id,map_manifest FROM runtime_control.quest_condition_catalog WHERE singleton FOR UPDATE').fetchone()
        changed = previous != (run, map_manifest)
        retired = 0
        if changed:
            offline_owner_barrier(conn)
            conn.execute('INSERT INTO runtime_control.quest_condition_catalog(singleton,run_id,map_manifest) VALUES(true,%s,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,map_manifest=EXCLUDED.map_manifest,activated_at=clock_timestamp()',
                         (run, map_manifest))
            activation = conn.execute('INSERT INTO runtime_control.quest_condition_activations(previous_run_id,run_id,map_manifest) '
                                      'VALUES(%s,%s,%s) RETURNING activation_id', (previous[0] if previous else None, run, map_manifest)).fetchone()[0]
            for world, server, token in conn.execute('SELECT world_id,server_id,owner_token FROM app_world.map_runtime_owner ORDER BY world_id,server_id').fetchall():
                replacement = secrets.token_hex(32)
                conn.execute('INSERT INTO runtime_control.quest_condition_owner_retirements VALUES(%s,%s,%s,%s,%s)',
                             (activation, world, server, token, replacement))
                conn.execute('UPDATE app_world.map_runtime_owner SET owner_token=%s,backend_pid=0,acquired_at=clock_timestamp() '
                             'WHERE world_id=%s AND server_id=%s', (replacement, world, server))
                retired += 1
    return dict(run_id=run, map_manifest=map_manifest, changed=changed, retired_map_owners=retired,
                condition_rows=next(len(rows) for t, _, rows in selected if t['table'] == 'TQCONDITIONCHART'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = activate_quest_catalog(conn, args.manifest)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Quest catalog publication failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
