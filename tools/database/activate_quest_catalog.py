#!/usr/bin/env python3
"""Publish original quest conditions and rewards from a verified archive while Maps are offline.

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


def activate_quest_catalog(conn, manifest_path, *, global_database_alias):
    if global_database_alias != 'TGLOBAL_GSP=TGLOBAL_RAGEZONE':
        raise RuntimeError('EXPLICIT_GLOBAL_DATABASE_ALIAS_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    names = {'TQCONDITIONCHART', 'TQUESTCHART', 'TQUESTTERMCHART', 'TQREWARDCHART',
             'TQUESTITEMCHART', 'TTITLECHART', 'TITEMCHART', 'TSKILLCHART', 'TSKILLDATA',
             'TLEVELCHART', 'TITEMMAGICCHART', 'TPORTALCHART', 'TDESTINATIONCHART'}
    keys = {('TGAME_RAGEZONE', 'dbo', name) for name in names}
    keys.add(('TGLOBAL_RAGEZONE', 'dbo', 'TLIMITEDLEVELCHART'))
    selected = [(t, m, r) for t, m, r in loaded
                if (t['database'], t['schema'], t['table']) in keys]
    if {(t['database'], t['schema'], t['table']) for t, _, _ in selected} != keys:
        raise RuntimeError('COMPLETE_QUEST_DEFINITION_TABLES_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL READ COMMITTED')
        conn.execute("SET LOCAL lock_timeout='1s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040007)')
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'legacy_game."TQCONDITIONCHART",legacy_game."TQUESTCHART",legacy_game."TQUESTTERMCHART",'
                     'legacy_game."TQREWARDCHART",legacy_game."TQUESTITEMCHART",legacy_game."TTITLECHART",'
                     'legacy_game."TITEMCHART",legacy_game."TSKILLCHART",legacy_game."TSKILLDATA",'
                     'legacy_game."TLEVELCHART",legacy_game."TITEMMAGICCHART",legacy_game."TPORTALCHART",'
                     'legacy_game."TDESTINATIONCHART",legacy_global."TLIMITEDLEVELCHART",'
                     'runtime_control.active_catalog,runtime_control.character_catalog,runtime_control.actor_catalog IN SHARE MODE')
        run = verified_actor_import(conn, (manifest, selected, fingerprint))
        release = conn.execute("SELECT run_id,manifest_sha256 FROM game_compat.catalog_release WHERE status='verified'").fetchall()
        if len(release) != 1:
            raise RuntimeError('VERIFIED_MAP_CATALOG_REQUIRED')
        map_run, map_manifest = release[0]
        character = conn.execute("SELECT run_id,manifest_sha256 FROM character_compat.catalog_release WHERE status='verified'").fetchall()
        actor = conn.execute("SELECT run_id,manifest_sha256 FROM actor_compat.catalog_release WHERE status='verified'").fetchall()
        if len(character) != 1 or len(actor) != 1:
            raise RuntimeError('VERIFIED_CHARACTER_AND_ACTOR_CATALOGS_REQUIRED')
        character_run, character_manifest = character[0]
        actor_run, actor_manifest = actor[0]
        shared = {name: map_run for name in ('TQUESTCHART', 'TQUESTTERMCHART', 'TQREWARDCHART')}
        shared.update({name: character_run for name in ('TITEMCHART', 'TSKILLCHART', 'TSKILLDATA', 'TLEVELCHART')})
        shared['TITEMMAGICCHART'] = actor_run
        for t, _, rows in selected:
            if t['table'] not in shared:
                continue
            cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                              'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                              'AND source_schema=%s AND source_table=%s',
                              (shared[t['table']], t['database'], t['schema'], t['table'])).fetchone()
            if cp != (len(rows), t['value_sha256'], t['value_sha256'], t['text_bytes_value_sha256'], t['text_bytes_value_sha256']):
                raise RuntimeError('QUEST_REWARD_DEPENDENCIES_DIFFER_FROM_ACTIVE_CATALOGS')
        if conn.execute('SELECT count(*) FROM legacy_global."TLIMITEDLEVELCHART" WHERE "_import_run_id"=%s', (run,)).fetchone()[0] != 1:
            raise RuntimeError('UNAMBIGUOUS_SOURCE_LEVEL_CAP_REQUIRED')
        for table, columns in (('TQUESTITEMCHART', ('dwID',)), ('TTITLECHART', ('wTitleID',)),
                               ('TPORTALCHART', ('wPortalID',)), ('TDESTINATIONCHART', ('wPortalID', 'wDestID'))):
            key_columns = ','.join(f'"{column}"' for column in columns)
            if conn.execute(f'SELECT 1 FROM legacy_game."{table}" WHERE "_import_run_id"=%s '
                            f'GROUP BY {key_columns} HAVING count(*)>1 LIMIT 1', (run,)).fetchone():
                raise RuntimeError('AMBIGUOUS_SOURCE_REWARD_ID')
        previous = conn.execute('SELECT run_id,map_manifest,character_manifest,actor_manifest '
                                'FROM runtime_control.quest_condition_catalog WHERE singleton FOR UPDATE').fetchone()
        changed = previous != (run, map_manifest, character_manifest, actor_manifest)
        retired = 0
        if changed:
            offline_owner_barrier(conn)
            conn.execute('INSERT INTO runtime_control.quest_condition_catalog(singleton,run_id,map_manifest,character_manifest,actor_manifest) VALUES(true,%s,%s,%s,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,map_manifest=EXCLUDED.map_manifest,'
                         'character_manifest=EXCLUDED.character_manifest,actor_manifest=EXCLUDED.actor_manifest,activated_at=clock_timestamp()',
                         (run, map_manifest, character_manifest, actor_manifest))
            activation = conn.execute('INSERT INTO runtime_control.quest_condition_activations(previous_run_id,run_id,map_manifest,character_manifest,actor_manifest) '
                                      'VALUES(%s,%s,%s,%s,%s) RETURNING activation_id',
                                      (previous[0] if previous else None, run, map_manifest, character_manifest, actor_manifest)).fetchone()[0]
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
    parser.add_argument('--global-database-alias', required=True, choices=['TGLOBAL_GSP=TGLOBAL_RAGEZONE'],
                        help='Explicit source database mapping for TGetLimitedLevel')
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = activate_quest_catalog(conn, args.manifest, global_database_alias=args.global_database_alias)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Quest catalog publication failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
