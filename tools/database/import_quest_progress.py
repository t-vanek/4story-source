#!/usr/bin/env python3
"""Explicit offline import of archived quest progress into an existing native world.

This does not create accounts/characters or apply migrations. A reviewed JSON
mapping of source character IDs to existing native IDs is mandatory, including
{} for the supplied empty historical progress. No existing runtime row is
replaced. Both tables must come from one value-verified archival manifest.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from migrate import connect, digest, load_snapshot, read_target, table_hash


def import_progress(conn, manifest_path, world, character_mapping):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    tables = {t['table']: (t, model, rows) for t, model, rows in loaded
              if (t['database'], t['schema'], t['table']) in {
                  ('TGAME_RAGEZONE', 'dbo', 'TQUESTTABLE'),
                  ('TGAME_RAGEZONE', 'dbo', 'TQUESTTERMTABLE')}}
    if set(tables) != {'TQUESTTABLE', 'TQUESTTERMTABLE'}:
        raise RuntimeError('BOTH_SOURCE_QUEST_TABLES_REQUIRED')
    if not isinstance(character_mapping, dict):
        raise RuntimeError('EXPLICIT_CHARACTER_MAPPING_REQUIRED')
    mapping = {}
    for source, target in character_mapping.items():
        if not isinstance(source, str) or not isinstance(target, int) or isinstance(target, bool):
            raise RuntimeError('INVALID_CHARACTER_MAPPING')
        try:
            sid = int(source)
        except ValueError as ex:
            raise RuntimeError('INVALID_CHARACTER_MAPPING') from ex
        if str(sid) != source or not -2147483648 <= sid <= 2147483647 or not 0 < target <= 2147483647:
            raise RuntimeError('INVALID_CHARACTER_MAPPING')
        mapping[sid] = target
    source_chars = {r['dwCharID'] for _, _, rows in tables.values() for r in rows}
    if set(mapping) != source_chars or len(set(mapping.values())) != len(mapping):
        raise RuntimeError('EXACT_INJECTIVE_CHARACTER_MAPPING_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    mapping_hash = digest(json.dumps(character_mapping, sort_keys=True, separators=(',', ':')).encode())
    counts = {name: len(values[2]) for name, values in tables.items()}
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        # Same namespace as PostgreSQLMapOwner. Lock every server number so a
        # new owner cannot start between the offline check and commit.
        if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                            (0x34574C44, world)).fetchone()[0]:
            raise RuntimeError('ACTIVE_WORLD_OWNER')
        for server in range(256):
            if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                                (0x344D4150, (world << 8) | server)).fetchone()[0]:
                raise RuntimeError('WORLD_HAS_ACTIVE_MAP_OWNER')
        conn.execute('LOCK TABLE app_world."TCHARTABLE" IN SHARE MODE')
        if not conn.execute('SELECT 1 FROM app_world.worlds WHERE group_id=%s', (world,)).fetchone():
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        if conn.execute('SELECT 1 FROM app_world.map_sessions WHERE world_id=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('WORLD_SESSIONS_MUST_BE_SETTLED')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        for table, model, rows in tables.values():
            archived = read_target(conn, model, run[0])
            if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
                raise RuntimeError('ARCHIVED_PROGRESS_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.quest_progress_imports WHERE run_id=%s AND world_id=%s',
                        (run[0], world)).fetchone():
            # Deliberately do not turn a repeated command into a successful
            # data reconciliation; runtime may have moved on since import.
            raise RuntimeError('QUEST_IMPORT_ALREADY_RECORDED')
        for target in mapping.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s '
                                'AND "dwCharID"=%s AND "bDelete"=0', (world, target)).fetchone():
                raise RuntimeError('MAPPED_CHARACTER_MISSING')
            if conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_CHARACTER_RECOVERY_CANNOT_BE_OVERWRITTEN')
            if conn.execute('SELECT 1 FROM app_world."TQUESTTABLE" WHERE "bWorldID"=%s AND "dwCharID"=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_QUEST_PROGRESS_CANNOT_BE_OVERWRITTEN')
        parents = {(r['dwCharID'], r['dwQuestID']) for r in tables['TQUESTTABLE'][2]}
        if any((r['dwCharID'], r['dwQuestID']) not in parents for r in tables['TQUESTTERMTABLE'][2]):
            raise RuntimeError('ORPHAN_SOURCE_TERM_RETAINED_ONLY_IN_ARCHIVE')
        for r in tables['TQUESTTABLE'][2]:
            conn.execute('INSERT INTO app_world."TQUESTTABLE"("bWorldID","dwCharID","dwQuestID",'
                         '"dwTick","bCompleteCount","bTriggerCount") VALUES(%s,%s,%s,%s,%s,%s)',
                         (world, mapping[r['dwCharID']], r['dwQuestID'], r['dwTick'], r['bCompleteCount'], r['bTriggerCount']))
        for r in tables['TQUESTTERMTABLE'][2]:
            conn.execute('INSERT INTO app_world."TQUESTTERMTABLE"("bWorldID","dwCharID","dwQuestID",'
                         '"dwTermID","bTermType","bCount") VALUES(%s,%s,%s,%s,%s,%s)',
                         (world, mapping[r['dwCharID']], r['dwQuestID'], r['dwTermID'], r['bTermType'], r['bCount']))
        conn.execute('INSERT INTO reconstruction.quest_progress_imports(run_id,world_id,character_mapping_sha256,quests,terms) '
                     'VALUES(%s,%s,%s,%s,%s)', (run[0], world, mapping_hash, counts['TQUESTTABLE'], counts['TQUESTTERMTABLE']))
    return dict(run_id=run[0], world_id=world, rows=counts, character_mapping_sha256=mapping_hash)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    args = parser.parse_args()
    try:
        mapping = json.loads(args.character_map.read_text())
        with connect() as conn:
            result = import_progress(conn, args.manifest, args.world, mapping)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Quest import failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
