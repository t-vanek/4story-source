#!/usr/bin/env python3
"""Offline TITEMUSEDTABLE projection; Phase B only, after migration053.

Requires the complete verified historical archive and an explicit character ID
map. Preserves every source row, including zero/long durations that LOADCHAR
handles differently. Never creates characters or replaces live recovery data.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from import_during_items import identity_map
from migrate import connect, digest, load_snapshot, read_target, table_hash


def import_cooldowns(conn, manifest_path, world, characters):
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
    sources = [(t, model, rows) for t, model, rows in loaded
               if (t['database'], t['schema'], t['table']) == ('TGAME_RAGEZONE', 'dbo', 'TITEMUSEDTABLE')]
    if len(sources) != 1:
        raise RuntimeError('SOURCE_ITEM_COOLDOWNS_REQUIRED')
    table, model, rows = sources[0]
    mapping = identity_map(characters, {r['dwCharID'] for r in rows})
    values = [(mapping[r['dwCharID']], r['wDelayGroupID'] & 65535, r['dwTick'] & 4294967295) for r in rows]
    if len({(c, g) for c, g, _ in values}) != len(values):
        raise RuntimeError('AMBIGUOUS_SOURCE_COOLDOWN_KEY_RETAINED_ONLY_IN_ARCHIVE')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    map_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        conn.execute('LOCK TABLE app_world.worlds,app_global."TCURRENTUSER" IN SHARE MODE')
        if not conn.execute('SELECT 1 FROM app_world.worlds WHERE group_id=%s', (world,)).fetchone():
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                            (0x34574C44, world)).fetchone()[0]:
            raise RuntimeError('ACTIVE_WORLD_OWNER')
        for server in range(256):
            if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                                (0x344D4150, (world << 8) | server)).fetchone()[0]:
                raise RuntimeError('ACTIVE_MAP_OWNER')
        if conn.execute('SELECT 1 FROM app_world.map_sessions WHERE world_id=%s LIMIT 1', (world,)).fetchone() or conn.execute(
                'SELECT 1 FROM app_global."TCURRENTUSER" WHERE "bGroupID"=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('WORLD_SESSIONS_MUST_BE_SETTLED')
        conn.execute('LOCK TABLE app_world."TCHARTABLE" IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        archived = read_target(conn, model, run[0])
        if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
            raise RuntimeError('ARCHIVED_ITEM_COOLDOWNS_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.item_cooldown_imports WHERE run_id=%s AND world_id=%s',
                        (run[0], world)).fetchone():
            raise RuntimeError('ITEM_COOLDOWN_IMPORT_ALREADY_RECORDED')
        for target in mapping.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s '
                                'AND "dwCharID"=%s AND "bDelete"=0', (world, target)).fetchone():
                raise RuntimeError('MAPPED_CHARACTER_MISSING')
            if conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_CHARACTER_RECOVERY_CANNOT_BE_OVERWRITTEN')
            if conn.execute('SELECT 1 FROM app_world.item_cooldowns WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_ITEM_COOLDOWNS_CANNOT_BE_OVERWRITTEN')
        for row in values:
            conn.execute('INSERT INTO app_world.item_cooldowns(world_id,char_id,delay_group,remaining) '
                         'VALUES(%s,%s,%s,%s)', (world, *row))
        conn.execute('INSERT INTO reconstruction.item_cooldown_imports(run_id,world_id,character_mapping_sha256,imported_rows) '
                     'VALUES(%s,%s,%s,%s)', (run[0], world, map_hash, len(values)))
    return dict(run_id=run[0], world_id=world, imported_rows=len(values), character_mapping_sha256=map_hash)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = import_cooldowns(conn, args.manifest, args.world, json.loads(args.character_map.read_text()))
    except Exception as error:
        # Retain the explicit blocker, without connection credentials or rows.
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        return 1
    print(json.dumps({'status': 'imported', **result}, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
