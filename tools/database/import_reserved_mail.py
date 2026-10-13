#!/usr/bin/env python3
"""Offline source TRESERVEDPOST projection after migration063 (Phase B only).

Requires full pinned archives, exact recipient identity mappings, source clock
and ANSI output profiles. Preserves all status/item fields and never allocates a
delivery item, changes old posts or reconstructs terminal delivery identities.
"""
import argparse
import base64
import codecs
import datetime as dt
import json
from pathlib import Path
import sys
from zoneinfo import ZoneInfo

from activate_catalog import MAPPING, PINNED
from generate_migrations import qi
from import_during_items import identity_map, source_epoch
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash


def import_reserved(conn, manifest_path, world, characters, source_timezone, varchar_encoding, wire_encoding):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    zone = ZoneInfo(source_timezone)
    for encoding in (varchar_encoding, wire_encoding):
        if 'Mail'.encode(codecs.lookup(encoding).name) != b'Mail':
            raise RuntimeError('EXPLICIT_ANSI_COMPATIBLE_ENCODING_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    matching = [(t, model, rows) for t, model, rows in loaded
                if (t['database'], t['schema'], t['table']) == ('TGAME_RAGEZONE', 'dbo', 'TRESERVEDPOST')]
    if len(matching) != 1:
        raise RuntimeError('EXACT_SOURCE_RESERVED_TABLE_REQUIRED')
    table, model, rows = matching[0]
    mapping = identity_map(characters, {r['dwRecverID'] for r in rows})
    if len({r['dwSeq'] for r in rows}) != len(rows) or any(r['bSend'] not in (0, 1, 2) for r in rows):
        raise RuntimeError('INVALID_SOURCE_RESERVED_ID_OR_STATUS_RETAINED_IN_ARCHIVE')
    if any(r['bSend'] == 0 and (not r['wItemID'] or not r['bCount']) for r in rows):
        raise RuntimeError('PENDING_RESERVED_ITEM_NOT_REPRESENTABLE_IN_NATIVE_STATE')
    fields = ('szSender', 'szTitle', 'szMessage')
    types = {c['source']: c['mssql_type'].lower() for c in model['column_mapping'] if c['source'] in fields}
    byte_map = {(r['row_number'], r['column']): r['bytes'] for r in table.get('_byte_records', [])}
    wires = []
    for n, row in enumerate(rows, 1):
        output = []
        for field, maximum in zip(fields, (50, 256, 2048)):
            raw = byte_map.get((n, field))
            if raw is None:
                raise RuntimeError('ORIGINAL_RESERVED_TEXT_BYTES_REQUIRED')
            raw = base64.b64decode(raw, validate=True) if isinstance(raw, str) else bytes(raw)
            encoding = 'utf-16-le' if types[field].startswith(('nvarchar', 'nchar')) else varchar_encoding
            if raw.decode(encoding, errors='strict') != row[field]:
                raise RuntimeError('SOURCE_RESERVED_TEXT_ENCODING_MISMATCH')
            wire = row[field].encode(wire_encoding, errors='strict')
            # Stored strings already contain any TQuestSendPost length prefix.
            # Never prefix again or truncate a multibyte output by guesswork.
            if b'\0' in wire or len(wire) > maximum:
                raise RuntimeError('SOURCE_RESERVED_OUTPUT_REQUIRES_EXPLICIT_RECONCILIATION')
            output.append(wire)
        wires.append(output)
    timestamps = [dt.datetime.fromtimestamp(source_epoch(r['dEndTime'], zone), dt.timezone.utc).replace(tzinfo=None)
                  for r in rows]
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    map_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        if not conn.execute('SELECT pg_try_advisory_xact_lock(%s)', (0x52504F0000000000 | world,)).fetchone()[0]:
            raise RuntimeError('ACTIVE_RESERVED_MAIL_BATCH')
        conn.execute('LOCK TABLE app_world.worlds,app_global."TCURRENTUSER" IN SHARE MODE')
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
        conn.execute('LOCK TABLE app_world."TCHARTABLE",app_world."TRESERVEDPOST",app_world.reserved_mail_clock '
                     'IN SHARE ROW EXCLUSIVE MODE')
        conn.execute('LOCK TABLE legacy_game."TRESERVEDPOST",reconstruction.source_text_bytes IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        archived = read_target(conn, model, run[0])
        if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
            raise RuntimeError('ARCHIVED_RESERVED_VALUES_CHANGED')
        if rows and target_bytes_hash(conn, (run[0], 'TGAME_RAGEZONE', 'dbo', 'TRESERVEDPOST')) != table['text_bytes_value_sha256']:
            raise RuntimeError('ARCHIVED_RESERVED_TEXT_BYTES_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.reserved_mail_imports WHERE run_id=%s AND world_id=%s', (run[0], world)).fetchone():
            raise RuntimeError('RESERVED_IMPORT_ALREADY_RECORDED')
        if conn.execute('SELECT 1 FROM app_world."TRESERVEDPOST" WHERE "bWorldID"=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('EXISTING_RESERVED_QUEUE_CANNOT_BE_OVERWRITTEN')
        if conn.execute('SELECT 1 FROM app_world.reserved_mail_clock WHERE world_id=%s', (world,)).fetchone():
            raise RuntimeError('EXISTING_RESERVED_DELIVERY_CLOCK_CANNOT_BE_OVERWRITTEN')
        for target in mapping.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s AND "dwCharID"=%s', (world, target)).fetchone():
                raise RuntimeError('MAPPED_RESERVED_RECIPIENT_MISSING_REQUIRES_RECONCILIATION')
        source_columns = [c['source'] for c in model['column_mapping']]
        columns = ['bWorldID', *source_columns, 'sender_bytes', 'title_bytes', 'message_bytes', 'source_run_id', 'source_row_number']
        statement = 'INSERT INTO app_world."TRESERVEDPOST"(' + ','.join(map(qi, columns)) + ') VALUES(' + ','.join(['%s'] * len(columns)) + ')'
        for n, (row, wire, timestamp) in enumerate(zip(rows, wires, timestamps), 1):
            values = dict(row)
            values['dwRecverID'] = mapping[row['dwRecverID']]
            values['dEndTime'] = timestamp
            conn.execute(statement, [world, *(values[c] for c in source_columns), *wire, run[0], n])
        conn.execute('INSERT INTO reconstruction.reserved_mail_imports(run_id,world_id,character_mapping_sha256,'
                     'source_timezone,source_varchar_encoding,wire_encoding,source_rows) VALUES(%s,%s,%s,%s,%s,%s,%s)',
                     (run[0], world, map_hash, source_timezone, varchar_encoding, wire_encoding, len(rows)))
    return dict(run_id=run[0], world_id=world, source_rows=len(rows), character_mapping_sha256=map_hash,
                source_timezone=source_timezone, source_varchar_encoding=varchar_encoding, wire_encoding=wire_encoding)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    parser.add_argument('--source-timezone', required=True)
    parser.add_argument('--source-varchar-encoding', required=True)
    parser.add_argument('--wire-encoding', required=True)
    args = parser.parse_args()
    # No automatic retry, including an unknown COMMIT result.
    with connect() as conn:
        print(json.dumps(import_reserved(conn, args.manifest, args.world, json.loads(args.character_map.read_text()),
                                        args.source_timezone, args.source_varchar_encoding, args.wire_encoding), indent=2))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)
