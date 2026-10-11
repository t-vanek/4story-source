#!/usr/bin/env python3
"""Offline PvP monetary projection; Phase B only, after migration058.

Requires the complete verified archive and an exact character map for the union
of TPVPOINTTABLE/TMONTHPVPOINTTABLE owners. The source monthly getter omits country
from its predicate: multiple rows for one character are ambiguous and remain
archival until resolved explicitly. Class records, ranks and monthly rollover are
separate contracts; this tool does not declare them migrated into runtime.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from import_during_items import identity_map
from migrate import connect, digest, load_snapshot, read_target, table_hash


def import_balances(conn, manifest_path, world, characters):
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
    names = {'TPVPOINTTABLE', 'TMONTHPVPOINTTABLE'}
    tables = {t['table']: (t, model, rows) for t, model, rows in loaded
              if t['database'] == 'TGAME_RAGEZONE' and t['schema'] == 'dbo' and t['table'] in names}
    if set(tables) != names:
        raise RuntimeError('BOTH_SOURCE_PVP_TABLES_REQUIRED')
    for name, (_, _, rows) in tables.items():
        if len({r['dwCharID'] for r in rows}) != len(rows):
            raise RuntimeError('AMBIGUOUS_' + name + '_CHARACTER_RETAINED_ONLY_IN_ARCHIVE')
    mapping = identity_map(characters, {r['dwCharID'] for _, _, rows in tables.values() for r in rows})
    points = {r['dwCharID']: r for r in tables['TPVPOINTTABLE'][2]}
    months = {r['dwCharID']: r for r in tables['TMONTHPVPOINTTABLE'][2]}
    values = []
    for original, target in sorted(mapping.items()):
        # Both stored procedures explicitly initialize absent-row outputs to0.
        point = points.get(original)
        month = months.get(original)
        values.append((target, point['dwTotalPoint'] & 4294967295 if point else 0,
                       point['dwUseablePoint'] & 4294967295 if point else 0,
                       month['dwPoint'] & 4294967295 if month else 0))
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    map_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        conn.execute('LOCK TABLE app_world.worlds,app_global."TCURRENTUSER" IN SHARE MODE')
        if not conn.execute('SELECT 1 FROM app_world.worlds WHERE group_id=%s', (world,)).fetchone():
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        for server in range(256):
            if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                                (0x344D4150, (world << 8) | server)).fetchone()[0]:
                raise RuntimeError('ACTIVE_MAP_OWNER')
        if conn.execute('SELECT 1 FROM app_world.map_sessions WHERE world_id=%s LIMIT 1', (world,)).fetchone() or conn.execute(
                'SELECT 1 FROM app_global."TCURRENTUSER" WHERE "bGroupID"=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('WORLD_SESSIONS_MUST_BE_SETTLED')
        conn.execute('LOCK TABLE app_world."TCHARTABLE",legacy_game."TPVPOINTTABLE",'
                     'legacy_game."TMONTHPVPOINTTABLE" IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        for table, model, rows in tables.values():
            archived = read_target(conn, model, run[0])
            if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
                raise RuntimeError('ARCHIVED_PVP_VALUES_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.pvp_balance_imports WHERE run_id=%s AND world_id=%s',
                        (run[0], world)).fetchone():
            raise RuntimeError('PVP_IMPORT_ALREADY_RECORDED')
        for target in mapping.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s '
                                'AND "dwCharID"=%s AND "bDelete"=0', (world, target)).fetchone():
                raise RuntimeError('MAPPED_CHARACTER_MISSING')
            if conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_CHARACTER_RECOVERY_CANNOT_BE_OVERWRITTEN')
            if conn.execute('SELECT 1 FROM app_world.pvp_balances WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_PVP_BALANCE_CANNOT_BE_OVERWRITTEN')
        for row in values:
            conn.execute('INSERT INTO app_world.pvp_balances(world_id,char_id,total,available,month) '
                         'VALUES(%s,%s,%s,%s,%s)', (world, *row))
        conn.execute('INSERT INTO reconstruction.pvp_balance_imports(run_id,world_id,character_mapping_sha256,'
                     'source_point_rows,source_month_rows,imported_characters) VALUES(%s,%s,%s,%s,%s,%s)',
                     (run[0], world, map_hash, len(points), len(months), len(values)))
    return dict(run_id=run[0], world_id=world, imported_characters=len(values),
                source_point_rows=len(points), source_month_rows=len(months), character_mapping_sha256=map_hash)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = import_balances(conn, args.manifest, args.world, json.loads(args.character_map.read_text()))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        return 1
    print(json.dumps({'status': 'imported', **result}, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
