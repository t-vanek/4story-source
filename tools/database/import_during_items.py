#!/usr/bin/env python3
"""Offline EXP/account entitlement projection from the complete verified archive.

Requires reviewed, exact character/account ID maps and an explicit IANA time
zone for source SMALLDATETIME. Does not create identities, apply migrations or
replace existing recovery/state. Run only during Phase B on an isolated copy.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import sys
from zoneinfo import ZoneInfo

from activate_catalog import MAPPING, PINNED
from migrate import connect, digest, load_snapshot, read_target, table_hash


def identity_map(raw, required):
    if not isinstance(raw, dict):
        raise RuntimeError('EXPLICIT_IDENTITY_MAP_REQUIRED')
    result = {}
    for source, target in raw.items():
        try:
            value = int(source)
        except (TypeError, ValueError) as ex:
            raise RuntimeError('INVALID_IDENTITY_MAP') from ex
        if (str(value) != source or not -2147483648 <= value <= 2147483647 or
                isinstance(target, bool) or not isinstance(target, int) or not 0 < target <= 2147483647):
            raise RuntimeError('INVALID_IDENTITY_MAP')
        result[value] = target
    if set(result) != required or len(set(result.values())) != len(result):
        raise RuntimeError('EXACT_INJECTIVE_IDENTITY_MAP_REQUIRED')
    return result


def source_epoch(value, zone):
    try:
        source = dt.datetime.fromisoformat(value)
    except (TypeError, ValueError) as ex:
        raise RuntimeError('INVALID_SOURCE_END_TIME') from ex
    if source.tzinfo is not None or source.second or source.microsecond:
        raise RuntimeError('SOURCE_SMALLDATETIME_REPRESENTATION_MISMATCH')
    first = source.replace(tzinfo=zone, fold=0)
    second = source.replace(tzinfo=zone, fold=1)
    if first.utcoffset() != second.utcoffset():
        raise RuntimeError('AMBIGUOUS_OR_NONEXISTENT_SOURCE_CALENDAR_TIME')
    utc = first.astimezone(dt.timezone.utc)
    if utc.astimezone(zone).replace(tzinfo=None) != source:
        raise RuntimeError('NONEXISTENT_SOURCE_CALENDAR_TIME')
    return int((utc - dt.datetime(1970, 1, 1, tzinfo=dt.timezone.utc)).total_seconds())


def import_items(conn, manifest_path, world, characters, accounts, source_timezone):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    zone = ZoneInfo(source_timezone)
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    keys = {('TGAME_RAGEZONE', 'dbo', 'TEXPITEMTABLE'), ('TGLOBAL_RAGEZONE', 'dbo', 'TDURINGITEMTABLE')}
    tables = {t['table']: (t, model, rows) for t, model, rows in loaded
              if (t['database'], t['schema'], t['table']) in keys}
    if set(tables) != {'TEXPITEMTABLE', 'TDURINGITEMTABLE'}:
        raise RuntimeError('BOTH_SOURCE_ENTITLEMENT_TABLES_REQUIRED')
    exp = tables['TEXPITEMTABLE'][2]
    premium = tables['TDURINGITEMTABLE'][2]
    cmap = identity_map(characters, {r['dwCharID'] for r in exp})
    amap = identity_map(accounts, {r['dwUserID'] for r in premium})
    if len(cmap) != len(exp):
        raise RuntimeError('AMBIGUOUS_EXP_SLOT_RETAINED_ONLY_IN_ARCHIVE')
    # SQL Server stores signed WORD/DWORD bit patterns; the operational schema
    # stores their unsigned value. It keeps exact instants, without guessing UTC
    # for the historical server's local clock. No expired rows are discarded.
    def values(row):
        return (row['wItemID'] & 65535, row['bType'], row['dwRemainTime'] & 4294967295,
                source_epoch(row['dEndTime'], zone))
    exp_values = [(cmap[r['dwCharID']], *values(r)) for r in exp]
    account_values = [(amap[r['dwUserID']], *values(r)) for r in premium]
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    char_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    account_hash = digest(json.dumps(accounts, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        # Account entitlement state can be shared by multiple worlds. All
        # native worlds must be offline, and creation/login cannot race import.
        conn.execute('LOCK TABLE app_world.worlds,app_global."TACCOUNT_PW",app_global."TCURRENTUSER" IN SHARE MODE')
        worlds = [r[0] for r in conn.execute('SELECT group_id FROM app_world.worlds ORDER BY group_id').fetchall()]
        if world not in worlds:
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        for active_world in worlds:
            # Native World may mutate social/mail state without a live Map.
            # Hold its process namespace through this whole offline projection.
            if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                                (0x34574C44, active_world)).fetchone()[0]:
                raise RuntimeError('ACTIVE_WORLD_OWNER')
            for server in range(256):
                if not conn.execute('SELECT pg_try_advisory_xact_lock(%s,%s)',
                                    (0x344D4150, (active_world << 8) | server)).fetchone()[0]:
                    raise RuntimeError('ACTIVE_MAP_OWNER')
        if conn.execute('SELECT 1 FROM app_world.map_sessions LIMIT 1').fetchone() or conn.execute(
                'SELECT 1 FROM app_global."TCURRENTUSER" LIMIT 1').fetchone():
            raise RuntimeError('ALL_SESSIONS_MUST_BE_SETTLED')
        conn.execute('LOCK TABLE app_world."TCHARTABLE" IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        for table, model, rows in tables.values():
            archived = read_target(conn, model, run[0])
            if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
                raise RuntimeError('ARCHIVED_ENTITLEMENTS_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.during_item_imports WHERE run_id=%s AND world_id=%s',
                        (run[0], world)).fetchone():
            raise RuntimeError('ENTITLEMENT_IMPORT_ALREADY_RECORDED')
        for target in cmap.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s '
                                'AND "dwCharID"=%s AND "bDelete"=0', (world, target)).fetchone():
                raise RuntimeError('MAPPED_CHARACTER_MISSING')
            if conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_CHARACTER_RECOVERY_CANNOT_BE_OVERWRITTEN')
            if conn.execute('SELECT 1 FROM app_world.exp_entitlements WHERE world_id=%s AND char_id=%s',
                            (world, target)).fetchone():
                raise RuntimeError('EXISTING_EXP_ENTITLEMENT_CANNOT_BE_OVERWRITTEN')
        for target in amap.values():
            if not conn.execute('SELECT 1 FROM app_global."TACCOUNT_PW" WHERE "dwUserID"=%s', (target,)).fetchone():
                raise RuntimeError('MAPPED_ACCOUNT_MISSING')
            if conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE user_id=%s', (target,)).fetchone():
                raise RuntimeError('EXISTING_ACCOUNT_RECOVERY_CANNOT_BE_OVERWRITTEN')
            if conn.execute('SELECT 1 FROM app_global.during_items WHERE user_id=%s', (target,)).fetchone():
                raise RuntimeError('EXISTING_ACCOUNT_ENTITLEMENT_CANNOT_BE_OVERWRITTEN')
        for row in exp_values:
            conn.execute('INSERT INTO app_world.exp_entitlements(world_id,char_id,item_id,item_type,remaining,expires) '
                         'VALUES(%s,%s,%s,%s,%s,%s)', (world, *row))
        for row in account_values:
            conn.execute('INSERT INTO app_global.during_items(user_id,item_id,item_type,remaining,expires) '
                         'VALUES(%s,%s,%s,%s,%s)', row)
        conn.execute('INSERT INTO reconstruction.during_item_imports(run_id,world_id,character_mapping_sha256,'
                     'account_mapping_sha256,source_timezone,exp_items,account_items) VALUES(%s,%s,%s,%s,%s,%s,%s)',
                     (run[0], world, char_hash, account_hash, source_timezone, len(exp), len(premium)))
    return dict(run_id=run[0], world_id=world, exp_items=len(exp), account_items=len(premium),
                source_timezone=source_timezone, character_mapping_sha256=char_hash, account_mapping_sha256=account_hash)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    parser.add_argument('--account-map', type=Path, required=True)
    parser.add_argument('--source-timezone', required=True)
    args = parser.parse_args()
    try:
        characters = json.loads(args.character_map.read_text())
        accounts = json.loads(args.account_map.read_text())
        with connect() as conn:
            result = import_items(conn, args.manifest, args.world, characters, accounts, args.source_timezone)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('During item import failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
