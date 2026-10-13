#!/usr/bin/env python3
"""Offline historical mail projection after migration060; execute in Phase B.

Requires the complete pinned archive, exact character mapping, explicit source
clock and ANSI binding profiles. Preserves post/item IDs and original values;
never creates accounts, characters or invented attachment attributes. Nonempty
obsolete TPOSTITEMTABLE has no durable item ID/expiry/durability: reject that
unresolved contract while retaining it in the complete archive.
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


def import_mailboxes(conn, manifest_path, world, characters, source_timezone, varchar_encoding, wire_encoding):
    if not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    zone = ZoneInfo(source_timezone)
    for encoding in (varchar_encoding, wire_encoding):
        codec = codecs.lookup(encoding)
        # The supported source is an ANSI TCHAR build, not an arbitrary UTF-16
        # wire protocol. Do not accept a codec that inserts a BOM or NUL bytes.
        if 'Mail'.encode(codec.name) != b'Mail':
            raise RuntimeError('ANSI_COMPATIBLE_EXPLICIT_ENCODING_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    names = {'TPOSTTABLE', 'TPOSTITEMTABLE', 'TITEMTABLE', 'TDBITEMINDEXTABLE'}
    tables = {t['table']: (t, model, rows) for t, model, rows in loaded
              if t['database'] == 'TGAME_RAGEZONE' and t['schema'] == 'dbo' and t['table'] in names}
    if set(tables) != names:
        raise RuntimeError('ALL_SOURCE_MAIL_TABLES_REQUIRED')
    if tables['TPOSTITEMTABLE'][2]:
        raise RuntimeError('OBSOLETE_POST_ITEM_ROWS_REQUIRE_EXPLICIT_CONTRACT')
    posts = tables['TPOSTTABLE'][2]
    attachments = [r for r in tables['TITEMTABLE'][2] if r['bStorageType'] == 2]
    if len({r['dwPostID'] for r in posts}) != len(posts) or any(r['dwPostID'] <= 0 for r in posts):
        raise RuntimeError('INVALID_OR_DUPLICATE_SOURCE_POST_ID')
    post_by_id = {r['dwPostID']: r for r in posts}
    required = {r['dwCharID'] for r in posts} | {r['dwSendID'] for r in posts if r['dwSendID']}
    mapping = identity_map(characters, required)
    slots = set()
    for item in attachments:
        post = post_by_id.get(item['dwStorageID'])
        if (item['bOwnerType'] != 0 or not post or post['dwCharID'] != item['dwOwnerID'] or
                item['dlID'] <= 0 or not item['bCount']):
            raise RuntimeError('INVALID_SOURCE_ATTACHMENT_RETAINED_ONLY_IN_ARCHIVE')
        slot = (item['dwStorageID'], item['bItemID'])
        if slot in slots:
            raise RuntimeError('DUPLICATE_SOURCE_ATTACHMENT_SLOT_RETAINED_ONLY_IN_ARCHIVE')
        slots.add(slot)
    source_post, post_model, _ = tables['TPOSTTABLE']
    records = source_post.get('_byte_records', [])
    if posts and not records:
        raise RuntimeError('ORIGINAL_POST_TEXT_BYTES_REQUIRED')
    byte_map = {(r['row_number'], r['column']): r['bytes'] for r in records}
    text_fields = ('szRecvName', 'szSender', 'szTitle', 'szMessage')
    text_types = {c['source']: c['mssql_type'].lower() for c in post_model['column_mapping'] if c['source'] in text_fields}
    wire_values = []
    for n, post in enumerate(posts, 1):
        values = []
        for field, maximum in zip(text_fields, (50, 50, 256, 2048)):
            raw = byte_map.get((n, field))
            if raw is None:
                raise RuntimeError('NULL_SOURCE_POST_TEXT_BYTES')
            raw = base64.b64decode(raw, validate=True) if isinstance(raw, str) else bytes(raw)
            encoding = 'utf-16-le' if text_types[field].startswith(('nvarchar', 'nchar')) else varchar_encoding
            decoded = raw.decode(encoding, errors='strict')
            if decoded != post[field]:
                raise RuntimeError('SOURCE_POST_TEXT_ENCODING_MISMATCH')
            wire = decoded.encode(wire_encoding, errors='strict')
            if b'\0' in wire or len(wire) > maximum:
                raise RuntimeError('SOURCE_POST_TEXT_NOT_REPRESENTABLE_IN_NATIVE_WIRE')
            values.append(wire)
        wire_values.append(values)
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    map_hash = digest(json.dumps(characters, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
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
        conn.execute('LOCK TABLE app_world."TCHARTABLE",app_world."TPOSTTABLE",app_world."TITEMTABLE" IN SHARE ROW EXCLUSIVE MODE')
        conn.execute('LOCK TABLE legacy_game."TPOSTTABLE",legacy_game."TPOSTITEMTABLE",legacy_game."TITEMTABLE",'
                     'legacy_game."TDBITEMINDEXTABLE",reconstruction.source_text_bytes IN SHARE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR SHARE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        for table, model, rows in tables.values():
            archived = read_target(conn, model, run[0])
            if len(archived) != len(rows) or table_hash(archived, model['column_mapping']) != table['value_sha256']:
                raise RuntimeError('ARCHIVED_MAIL_VALUES_CHANGED')
        if posts and target_bytes_hash(conn, (run[0], 'TGAME_RAGEZONE', 'dbo', 'TPOSTTABLE')) != source_post['text_bytes_value_sha256']:
            raise RuntimeError('ARCHIVED_MAIL_TEXT_BYTES_CHANGED')
        if conn.execute('SELECT 1 FROM reconstruction.mailbox_imports WHERE run_id=%s AND world_id=%s', (run[0], world)).fetchone():
            raise RuntimeError('MAILBOX_IMPORT_ALREADY_RECORDED')
        if conn.execute('SELECT 1 FROM app_world."TPOSTTABLE" WHERE "bWorldID"=%s LIMIT 1', (world,)).fetchone():
            raise RuntimeError('EXISTING_MAILBOXES_CANNOT_BE_OVERWRITTEN')
        for target in mapping.values():
            if not conn.execute('SELECT 1 FROM app_world."TCHARTABLE" WHERE "bWorldID"=%s '
                                'AND "dwCharID"=%s AND "bDelete"=0', (world, target)).fetchone():
                raise RuntimeError('MAPPED_CHARACTER_MISSING')
            if conn.execute('SELECT 1 FROM app_world.map_checkpoints WHERE world_id=%s AND char_id=%s', (world, target)).fetchone():
                raise RuntimeError('EXISTING_CHARACTER_RECOVERY_CANNOT_BE_OVERWRITTEN')
        for post, wire in zip(posts, wire_values):
            timestamp = dt.datetime.fromtimestamp(source_epoch(post['timeRecv'], zone), dt.timezone.utc).replace(tzinfo=None)
            values = (world, mapping[post['dwCharID']], post['dwPostID'], post['szRecvName'], post['bType'], post['bRead'], timestamp,
                      mapping[post['dwSendID']] if post['dwSendID'] else 0, post['szSender'], post['szTitle'], post['szMessage'],
                      post['dwGold'], post['dwSilver'], post['dwCooper'], post['bContain'], *wire)
            conn.execute('INSERT INTO app_world."TPOSTTABLE"("bWorldID","dwCharID","dwPostID","szRecvName","bType","bRead",'
                         '"timeRecv","dwSendID","szSender","szTitle","szMessage","dwGold","dwSilver","dwCooper","bContain",'
                         'recipient_bytes,sender_bytes,title_bytes,message_bytes) OVERRIDING SYSTEM VALUE '
                         'VALUES(' + ','.join(['%s'] * len(values)) + ')', values)
        item_world, high_water = conn.execute('SELECT item_world,item_high_water FROM app_world.worlds WHERE group_id=%s FOR UPDATE', (world,)).fetchone()
        indices = [r['dlID'] for r in tables['TDBITEMINDEXTABLE'][2] if r['bWorld'] == item_world]
        if len(indices) != 1:
            raise RuntimeError('EXACT_SOURCE_ITEM_ALLOCATION_INDEX_REQUIRED')
        if any((r['dlID'] >> 56) != item_world or r['dlID'] > indices[0] for r in attachments):
            raise RuntimeError('SOURCE_ATTACHMENT_ALLOCATION_LINEAGE_MISMATCH')
        item_columns = [c['source'] for c in tables['TITEMTABLE'][1]['column_mapping']]
        for item in attachments:
            values = dict(item)
            values['dwOwnerID'] = mapping[item['dwOwnerID']]
            values['dEndTime'] = dt.datetime.fromtimestamp(source_epoch(item['dEndTime'], zone), dt.timezone.utc).replace(tzinfo=None)
            columns = ['bWorldID', *item_columns, 'post_id', 'wCustomTex']
            params = [world, *(values[c] for c in item_columns), item['dwStorageID'], item['dwTime6'] & 65535]
            conn.execute('INSERT INTO app_world."TITEMTABLE"(' + ','.join(map(qi, columns)) + ') VALUES(' + ','.join(['%s'] * len(params)) + ')', params)
        conn.execute('UPDATE app_world.worlds SET item_high_water=GREATEST(item_high_water,%s) WHERE group_id=%s', (indices[0], world))
        conn.execute('INSERT INTO reconstruction.mailbox_imports(run_id,world_id,character_mapping_sha256,source_timezone,'
                     'source_varchar_encoding,wire_encoding,source_post_rows,source_attachment_rows,source_postitem_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s,0)', (run[0], world, map_hash, source_timezone, varchar_encoding, wire_encoding, len(posts), len(attachments)))
        if posts:
            # All native worlds are offline. Sequence advancement is monotonic;
            # PostgreSQL sequence gaps can remain after rollback and are harmless.
            conn.execute('SELECT setval(\'app_world."TPOSTTABLE_dwPostID_seq"\',GREATEST('
                         '(SELECT last_value FROM app_world."TPOSTTABLE_dwPostID_seq"),'
                         '(SELECT max("dwPostID") FROM app_world."TPOSTTABLE")),true)')
    return dict(run_id=run[0], world_id=world, imported_posts=len(posts), imported_attachments=len(attachments),
                character_mapping_sha256=map_hash, source_timezone=source_timezone,
                source_varchar_encoding=varchar_encoding, wire_encoding=wire_encoding)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--character-map', type=Path, required=True)
    parser.add_argument('--source-timezone', required=True)
    parser.add_argument('--source-varchar-encoding', required=True)
    parser.add_argument('--wire-encoding', required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = import_mailboxes(conn, args.manifest, args.world, json.loads(args.character_map.read_text()),
                                      args.source_timezone, args.source_varchar_encoding, args.wire_encoding)
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
