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


def activate_quest_catalog(conn, manifest_path, *, global_database_alias, special_boxes=False, item_gambling=False, item_upgrades=False):
    if global_database_alias != 'TGLOBAL_GSP=TGLOBAL_RAGEZONE':
        raise RuntimeError('EXPLICIT_GLOBAL_DATABASE_ALIAS_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    names = {'TQCONDITIONCHART', 'TQUESTCHART', 'TQUESTTERMCHART', 'TQREWARDCHART',
             'TQUESTITEMCHART', 'TTITLECHART', 'TITEMCHART', 'TSKILLCHART', 'TSKILLDATA',
             'TLEVELCHART', 'TITEMMAGICCHART', 'TPORTALCHART', 'TDESTINATIONCHART'}
    if special_boxes:
        names.add('TSPECIALBOXCHART')
    if item_gambling:
        names.add('TGAMBLECHART')
    if item_upgrades:
        names.update(('TITEMGRADECHART', 'TGEMGRADECHART', 'ACCESSORYMAGICTABLE'))
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
                     'legacy_game."TDESTINATIONCHART",legacy_game."TSPECIALBOXCHART",legacy_game."TGAMBLECHART",'
                     'legacy_game."TITEMGRADECHART",legacy_game."TGEMGRADECHART",legacy_game."ACCESSORYMAGICTABLE",legacy_global."TLIMITEDLEVELCHART",'
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
        if item_upgrades:
            shared['TITEMGRADECHART'] = actor_run
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
        # The original un-ordered SELECT binds varchar(1) bClass to BYTE. Keep
        # archival text unchanged and admit only unambiguous decimal conversion.
        # NULL bindings and groups >=256 enter undefined/stale-value or BYTE-loop
        # behavior in the source; reject those records instead of inventing data.
        boxes = next((rows for t, _, rows in selected if t['table'] == 'TSPECIALBOXCHART'), [])
        groups = {}
        for row_number, row in enumerate(boxes, 1):
            if any(value is None for value in row.values()):
                raise RuntimeError(f'SPECIAL_BOX_NULL_BINDING_ROW_{row_number}')
            if not isinstance(row['bClass'], str) or row['bClass'] not in tuple('0123456789'):
                raise RuntimeError(f'SPECIAL_BOX_CLASS_ENCODING_ROW_{row_number}')
            # Do not reject a source class >TCLASS_COUNT: its branch simply
            # never matches an ordinary player. Likewise missing item templates
            # are skipped by source CreateItem, not replaced or deleted here.
            groups[row['wGroup']] = groups.get(row['wGroup'], 0) + 1
        if any(count > 255 for count in groups.values()):
            raise RuntimeError('SPECIAL_BOX_GROUP_EXCEEDS_SOURCE_BYTE_LOOP')
        gambles = next((rows for t, _, rows in selected if t['table'] == 'TGAMBLECHART'), [])
        gamble_groups = {}
        for row_number, row in enumerate(gambles, 1):
            if any(value is None for value in row.values()):
                raise RuntimeError(f'GAMBLE_NULL_BINDING_ROW_{row_number}')
            if not row['bCountMax']:
                raise RuntimeError(f'GAMBLE_ZERO_COUNT_DIVISOR_ROW_{row_number}')
            key = (row['bType'], row['bKind'])
            gamble_groups[key] = (gamble_groups.get(key, 0) + (row['wProb'] & 65535)) & 65535
        if any(total == 0 for total in gamble_groups.values()):
            raise RuntimeError('GAMBLE_ZERO_PROBABILITY_DIVISOR')
        # Do not filter bMinLevel/bMaxLevel or remove duplicate TGAMBLE rows:
        # the source uses every row in query order, including those duplicates.
        # TITEMMAGICCHART identity/shape is already covered by the active actor
        # receipt and verified_actor_import. Keep original bMinLevel archived;
        # Map LoadData leaves its runtime member value-initialized at zero.
        if item_upgrades:
            gems = next(rows for t, _, rows in selected if t['table'] == 'TGEMGRADECHART')
            # Unlike m_itemgrade, the module does not explicitly initialize the
            # gem array. Require the five addressable entries; never invent a
            # missing probability or choose between duplicates. Keep extra
            # historical rows archived (the backup has six). The native reader
            # does not reproduce the source loader's out-of-bounds array write.
            if any(sum(row['bGem'] == grade for row in gems) != 1 for grade in range(5)):
                raise RuntimeError('COMPLETE_UNAMBIGUOUS_GEM_GRADE_ARRAY_REQUIRED')
            accessory = next(rows for t, _, rows in selected if t['table'] == 'ACCESSORYMAGICTABLE')
            for number, row in enumerate(accessory, 1):
                if any(value is None for value in row.values()):
                    raise RuntimeError(f'ACCESSORY_NULL_BINDING_ROW_{number}')
                if not (0 <= row['ID'] <= 255 and 0 < row['MagicID'] <= 255
                        and 0 <= row['MinValue'] <= 65535 and 0 < row['MaxValue'] <= 65535):
                    raise RuntimeError(f'ACCESSORY_BINDING_OR_DIVISOR_RANGE_ROW_{number}')
            magic_ids = {row['bMagic'] for t, _, rows in selected if t['table'] == 'TITEMMAGICCHART' for row in rows}
            if any(row['MagicID'] not in magic_ids for row in accessory):
                raise RuntimeError('ACCESSORY_MISSING_MAGIC_REFERENCE')
        previous = conn.execute('SELECT run_id,map_manifest,character_manifest,actor_manifest,special_boxes,item_gambling,item_upgrades '
                                'FROM runtime_control.quest_condition_catalog WHERE singleton FOR UPDATE').fetchone()
        # A retry of the older content profile must not silently remove the
        # extended publication. Require explicit full proof again for updates.
        if previous and previous[4] and not special_boxes:
            raise RuntimeError('SPECIAL_BOX_PUBLICATION_REQUIRES_EXTENDED_PROFILE')
        if previous and previous[5] and not item_gambling:
            raise RuntimeError('ITEM_GAMBLING_PUBLICATION_REQUIRES_EXTENDED_PROFILE')
        if previous and previous[6] and not item_upgrades:
            raise RuntimeError('ITEM_UPGRADE_PUBLICATION_REQUIRES_EXTENDED_PROFILE')
        changed = previous != (run, map_manifest, character_manifest, actor_manifest, special_boxes, item_gambling, item_upgrades)
        retired = 0
        if changed:
            offline_owner_barrier(conn)
            conn.execute('INSERT INTO runtime_control.quest_condition_catalog(singleton,run_id,map_manifest,character_manifest,actor_manifest,special_boxes,item_gambling,item_upgrades) VALUES(true,%s,%s,%s,%s,%s,%s,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,map_manifest=EXCLUDED.map_manifest,'
                         'character_manifest=EXCLUDED.character_manifest,actor_manifest=EXCLUDED.actor_manifest,special_boxes=EXCLUDED.special_boxes,item_gambling=EXCLUDED.item_gambling,item_upgrades=EXCLUDED.item_upgrades,activated_at=clock_timestamp()',
                         (run, map_manifest, character_manifest, actor_manifest, special_boxes, item_gambling, item_upgrades))
            activation = conn.execute('INSERT INTO runtime_control.quest_condition_activations(previous_run_id,run_id,map_manifest,character_manifest,actor_manifest,special_boxes,item_gambling,item_upgrades) '
                                      'VALUES(%s,%s,%s,%s,%s,%s,%s,%s) RETURNING activation_id',
                                      (previous[0] if previous else None, run, map_manifest, character_manifest, actor_manifest, special_boxes, item_gambling, item_upgrades)).fetchone()[0]
            for world, server, token in conn.execute('SELECT world_id,server_id,owner_token FROM app_world.map_runtime_owner ORDER BY world_id,server_id').fetchall():
                replacement = secrets.token_hex(32)
                conn.execute('INSERT INTO runtime_control.quest_condition_owner_retirements VALUES(%s,%s,%s,%s,%s)',
                             (activation, world, server, token, replacement))
                conn.execute('UPDATE app_world.map_runtime_owner SET owner_token=%s,backend_pid=0,acquired_at=clock_timestamp() '
                             'WHERE world_id=%s AND server_id=%s', (replacement, world, server))
                retired += 1
    return dict(run_id=run, map_manifest=map_manifest, changed=changed, retired_map_owners=retired,
                special_box_rows=len(boxes), special_boxes=special_boxes,
                gamble_rows=len(gambles), item_gambling=item_gambling, item_upgrades=item_upgrades,
                condition_rows=next(len(rows) for t, _, rows in selected if t['table'] == 'TQCONDITIONCHART'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--global-database-alias', required=True, choices=['TGLOBAL_GSP=TGLOBAL_RAGEZONE'],
                        help='Explicit source database mapping for TGetLimitedLevel')
    parser.add_argument('--special-boxes', action='store_true',
                        help='Also publish the complete original TSPECIALBOXCHART through migration054')
    parser.add_argument('--item-gambling', action='store_true',
                        help='Also publish complete TGAMBLECHART and ordered item magic through migration056')
    parser.add_argument('--item-upgrades', action='store_true',
                        help='Also publish complete gem/accessory and matching grade/magic charts through migration057')
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = activate_quest_catalog(conn, args.manifest, global_database_alias=args.global_database_alias,
                                            special_boxes=args.special_boxes, item_gambling=args.item_gambling, item_upgrades=args.item_upgrades)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Quest catalog publication failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
