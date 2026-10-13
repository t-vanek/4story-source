#!/usr/bin/env python3
"""Offline full friend/group/directed-soulmate projection after066; Phase B only.

Does not apply migrations or create identities. Requires the complete pinned
archival snapshot, its verified value/byte receipts and an exact injective
character map. Existing social state, clocks/audit or live owners are refused.
No retry is performed after an uncertain commit.
"""
import argparse
import base64
import codecs
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from generate_migrations import qi
from import_during_items import identity_map
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash

TABLES = ('TFRIENDGROUPTABLE', 'TFRIENDTABLE', 'TSOULMATETABLE')


def import_social(conn, manifest_path, world, characters, varchar_encoding, wire_encoding):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    for encoding in (varchar_encoding, wire_encoding):
        if 'Mail'.encode(codecs.lookup(encoding).name) != b'Mail':
            raise RuntimeError('LOSSLESS_ANSI_ENCODING_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    tables = {t['table']: (t, model, rows) for t, model, rows in loaded
              if t['database'] == 'TGAME_RAGEZONE' and t['schema'] == 'dbo' and t['table'] in TABLES}
    if set(tables) != set(TABLES):
        raise RuntimeError('COMPLETE_SOURCE_SOCIAL_TABLES_REQUIRED')
    groups, friends, soulmates = [tables[t][2] for t in TABLES]
    required = {r['dwCharID'] for rows in (groups, friends, soulmates) for r in rows}
    required |= {r['dwFriendID'] for r in friends} | {r['dwTarget'] for r in soulmates}
    cmap = identity_map(characters, required)
    for rows, key in ((groups, ('dwCharID', 'bGroup')), (friends, ('dwCharID', 'dwFriendID')), (soulmates, ('dwCharID',))):
        if len({tuple(r[c] for c in key) for r in rows}) != len(rows):
            raise RuntimeError('AMBIGUOUS_SOCIAL_SOURCE_KEY_RETAINED_IN_ARCHIVE')
    # Group IDs and orphan group references are source data, not normalized by
    # the importer. Duplicate names and nonmutual relationships also survive.
    table, _, _ = tables['TFRIENDGROUPTABLE']
    byte_map = {(r['row_number'], r['column']): r['bytes'] for r in table.get('_byte_records', [])}
    names = []
    for n, row in enumerate(groups, 1):
        raw = byte_map.get((n, 'szName'))
        if raw is None:
            raise RuntimeError('ORIGINAL_GROUP_NAME_BYTES_REQUIRED')
        raw = base64.b64decode(raw, validate=True) if isinstance(raw, str) else bytes(raw)
        if raw.decode(varchar_encoding, errors='strict') != row['szName']:
            raise RuntimeError('SOURCE_GROUP_NAME_ENCODING_MISMATCH')
        wire = row['szName'].encode(wire_encoding, errors='strict')
        if len(wire) > 20 or b'\0' in wire or wire.decode(wire_encoding, errors='strict') != row['szName']:
            raise RuntimeError('GROUP_NAME_REQUIRES_EXPLICIT_RECONCILIATION')
        names.append(wire)
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    map_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        conn.execute('LOCK TABLE app_world.worlds,app_global."TCURRENTUSER" IN SHARE MODE')
        worlds = [r[0] for r in conn.execute('SELECT group_id FROM app_world.worlds ORDER BY group_id')]
        if world not in worlds:
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        # Hold all process locks throughout the projection, so owner startup
        # cannot race the empty-state check or import. No live cluster is used.
        for active in worlds:
            if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)', (0x34574C44, active)).fetchone()[0]:
                raise RuntimeError('ACTIVE_WORLD_OWNER')
            for server in range(256):
                if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)', (0x344D4150, (active << 8) | server)).fetchone()[0]:
                    raise RuntimeError('ACTIVE_MAP_OWNER')
        if conn.execute('SELECT 1 FROM app_global."TCURRENTUSER" LIMIT 1').fetchone() or conn.execute(
                'SELECT 1 FROM app_world.map_sessions LIMIT 1').fetchone():
            raise RuntimeError('ALL_SESSIONS_MUST_BE_SETTLED')
        conn.execute('LOCK TABLE app_world."TCHARTABLE" IN SHARE MODE')
        conn.execute('LOCK TABLE ' + ','.join('app_world.' + qi(t) for t in TABLES) +
                     ',app_world.social_mutations,reconstruction.social_imports IN SHARE ROW EXCLUSIVE MODE')
        conn.execute('LOCK TABLE ' + ','.join('legacy_game.' + qi(t) for t in TABLES) +
                     ',reconstruction.source_text_bytes IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        for table, model, rows in tables.values():
            archived = read_target(conn, model, run[0])
            if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
                raise RuntimeError('ARCHIVED_SOCIAL_VALUES_CHANGED')
            if table['table'] == 'TFRIENDGROUPTABLE' and rows and target_bytes_hash(
                    conn, (run[0], 'TGAME_RAGEZONE', 'dbo', table['table'])) != table['text_bytes_value_sha256']:
                raise RuntimeError('ARCHIVED_GROUP_NAME_BYTES_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.social_imports WHERE world_id=%s', (world,)).fetchone():
            raise RuntimeError('SOCIAL_IMPORT_ALREADY_RECORDED')
        if conn.execute('SELECT 1 FROM app_world.social_mutations WHERE world_id=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('PREVIOUS_SOCIAL_ACTIVITY_CANNOT_BE_OVERWRITTEN')
        for table in TABLES:
            if conn.execute('SELECT 1 FROM app_world.' + qi(table) + ' WHERE "bWorldID"=%s LIMIT 1', (world,)).fetchone():
                raise RuntimeError('EXISTING_SOCIAL_STATE_CANNOT_BE_OVERWRITTEN')
        for target in cmap.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s AND "dwCharID"=%s', (world, target)).fetchone():
                raise RuntimeError('MISSING_SOCIAL_CHARACTER_REQUIRES_RECONCILIATION')
        for row, wire in zip(groups, names):
            conn.execute('INSERT INTO app_world."TFRIENDGROUPTABLE"("bWorldID","dwCharID","bGroup","szName",name_bytes) '
                         'VALUES(%s,%s,%s,%s,%s)', (world, cmap[row['dwCharID']], row['bGroup'], row['szName'], wire))
        for row in friends:
            conn.execute('INSERT INTO app_world."TFRIENDTABLE"("bWorldID","dwCharID","dwFriendID","bGroup") '
                         'VALUES(%s,%s,%s,%s)', (world, cmap[row['dwCharID']], cmap[row['dwFriendID']], row['bGroup']))
        for row in soulmates:
            conn.execute('INSERT INTO app_world."TSOULMATETABLE"("bWorldID","dwCharID","dwTarget","dwTime") '
                         'VALUES(%s,%s,%s,%s)', (world, cmap[row['dwCharID']], cmap[row['dwTarget']], row['dwTime']))
        conn.execute('INSERT INTO reconstruction.social_imports(world_id,run_id,character_mapping_sha256,source_varchar_encoding,'
                     'wire_encoding,group_rows,friend_rows,soulmate_rows) VALUES(%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, run[0], map_hash, varchar_encoding, wire_encoding, len(groups), len(friends), len(soulmates)))
    return dict(world_id=world, run_id=run[0], character_mapping_sha256=map_hash,
                groups=len(groups), friends=len(friends), soulmates=len(soulmates))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    parser.add_argument('--source-varchar-encoding', required=True)
    parser.add_argument('--wire-encoding', required=True)
    args = parser.parse_args()
    with connect() as conn:
        print(json.dumps(import_social(conn, args.manifest, args.world, json.loads(args.character_map.read_text()),
                                      args.source_varchar_encoding, args.wire_encoding), indent=2))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)
