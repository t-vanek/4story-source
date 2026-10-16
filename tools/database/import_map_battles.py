#!/usr/bin/env python3
"""Phase B only: project source Map battle-region boot state after078.

Requires the complete pinned historical manifest,077 calendar and private
original-query order evidence. No source data, inferred native guild mapping,
guessed empty regions or replacement publication is generated. Never retries
an unknown commit. All row/text/time input stays outside Git and stdout.
"""
import argparse
import base64
import datetime as dt
import json
from pathlib import Path
import sys
from zoneinfo import ZoneInfo

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash

TABLES = ('TBATTLEZONECHART', 'TLOCALTABLE', 'TCASTLETABLE', 'TMISSIONTABLE',
          'TSKYGARDENTABLE', 'TLOCALOCCUPYTABLE', 'TGUILDTABLE', 'TINDUNCHART')
# These are CTBL query result ordinals, not an invented ORDER BY local ID.
KINDS = ((3, 'TMISSIONTABLE', 'wMissionID'), (2, 'TCASTLETABLE', 'wCastle'),
         (4, 'TSKYGARDENTABLE', 'wID'), (1, 'TLOCALTABLE', 'wLocalID'))
DEPENDENCIES = (('TMAPCHART', 'TUNITCHART', 'TCHANNELCHART', 'TSPAWNPOSCHART'),
                ('TITEMCHART', 'TNPCCHART', 'TSKILLCHART', 'TSKILLDATA'), ('TMONSPAWNCHART',))


def word(value):
    if isinstance(value, bool) or not isinstance(value, int) or not -32768 <= value <= 32767:
        raise RuntimeError('SOURCE_SMALLINT_WORD_REQUIRED')
    return value & 65535


def dword(value):
    if isinstance(value, bool) or not isinstance(value, int) or not -2147483648 <= value <= 2147483647:
        raise RuntimeError('SOURCE_INT_DWORD_REQUIRED')
    return value & 4294967295


def byte(value):
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 255:
        raise RuntimeError('SOURCE_BYTE_REQUIRED')
    return value


def dbtime(value, zone):
    source = dt.datetime.fromisoformat(value)
    if (source.tzinfo is not None or source.second or source.microsecond or
            not dt.datetime(1900, 1, 1) <= source <= dt.datetime(2079, 6, 6, 23, 59)):
        raise RuntimeError('SOURCE_SMALLDATETIME_REPRESENTATION_REQUIRED')
    if source.year < 2000:
        return 0  # __DBTOTIME's explicit sentinel branch, not UNIX conversion.
    first, second = (source.replace(tzinfo=zone, fold=n) for n in (0, 1))
    if (first.utcoffset() != second.utcoffset() or
            first.astimezone(dt.timezone.utc).astimezone(zone).replace(tzinfo=None) != source):
        raise RuntimeError('SOURCE_DB_TIME_REQUIRES_EXPLICIT_DST_RECONCILIATION')
    epoch = int((first.astimezone(dt.timezone.utc) - dt.datetime(1970, 1, 1, tzinfo=dt.timezone.utc)).total_seconds())
    if not 0 <= epoch <= 253402300799:
        raise RuntimeError('SOURCE_DB_TIME_OUT_OF_SUPPORTED_RANGE')
    return epoch


def original_bytes(table, ordinal, column, value):
    records = [r for r in table.get('_byte_records', ()) if r['row_number'] == ordinal and r['column'] == column]
    if len(records) != 1 or records[0]['bytes'] is None:
        raise RuntimeError('COMPLETE_ORIGINAL_ANSI_TEXT_BYTES_REQUIRED')
    raw = base64.b64decode(records[0]['bytes'], validate=True)
    if len(raw) > 50 or b'\0' in raw or value is None:
        raise RuntimeError('SOURCE_MAX_NAME_OR_NUL_REQUIRES_RECONCILIATION')
    return raw


def import_map_battles(conn, manifest_path, world, order_path):
    if isinstance(world, bool) or not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')} for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda a: a['path']) != sorted(pinned, key=lambda a: a['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    selected = {t['table']: (t, model, rows) for t, model, rows in loaded
                if (t['database'], t['schema']) == ('TGAME_RAGEZONE', 'dbo') and t['table'] in TABLES}
    if set(selected) != set(TABLES):
        raise RuntimeError('COMPLETE_MAP_BATTLE_SOURCE_REQUIRED')
    # Public, previously reconciled counts for these exact backups. These are
    # profile checks, never substitutes for loading/verifying the actual rows.
    counts = dict(TBATTLEZONECHART=26, TLOCALTABLE=12, TCASTLETABLE=4, TMISSIONTABLE=8,
                  TSKYGARDENTABLE=1, TLOCALOCCUPYTABLE=72, TGUILDTABLE=0, TINDUNCHART=20)
    if any(len(selected[name][2]) != count for name, count in counts.items()):
        raise RuntimeError('PINNED_MAP_BATTLE_ROW_PROFILE_REQUIRES_RECONCILIATION')
    dependency_tables = {name for names in DEPENDENCIES for name in names}
    checked = {t['table']: (t, model, rows) for t, model, rows in loaded
               if (t['database'], t['schema']) == ('TGAME_RAGEZONE', 'dbo') and t['table'] in set(TABLES) | dependency_tables}
    if set(checked) != set(TABLES) | dependency_tables:
        raise RuntimeError('COMPLETE_MAP_BATTLE_DEPENDENCY_SOURCE_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    encoded = order_path.read_bytes()
    profile = json.loads(encoded)
    orders = profile.get('queries')
    if (profile.get('source_manifest_sha256') != fingerprint or not isinstance(orders, dict) or
            set(orders) != {name for _, name, _ in KINDS}):
        raise RuntimeError('EXPLICIT_ORIGINAL_MAP_QUERY_ORDER_REQUIRED')
    for _, name, _ in KINDS:
        order = orders[name]
        if (not isinstance(order, list) or any(isinstance(n, bool) or not isinstance(n, int) for n in order) or
                sorted(order) != list(range(1, len(selected[name][2]) + 1))):
            raise RuntimeError('QUERY_ORDER_MUST_REFERENCE_EVERY_REAL_SOURCE_ROW_ONCE')
    zones = {}
    for ordinal, row in enumerate(selected['TBATTLEZONECHART'][2], 1):
        key = word(row['wID'])
        if key in zones:
            raise RuntimeError('AMBIGUOUS_SOURCE_BATTLE_ZONE_KEY')
        zones[key] = row
        original_bytes(selected['TBATTLEZONECHART'][0], ordinal, 'szName', row['szName'])
    guilds = {}
    guild_table, _, guild_rows = selected['TGUILDTABLE']
    for ordinal, row in enumerate(guild_rows, 1):
        key = dword(row['dwID'])
        if key in guilds:
            raise RuntimeError('AMBIGUOUS_SOURCE_GUILD_JOIN')
        guilds[key] = original_bytes(guild_table, ordinal, 'szName', row['szName'])
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'runtime_control.world_battle_catalog,runtime_control.routing_catalog,'
                     'runtime_control.character_catalog,runtime_control.active_catalog IN SHARE MODE')
        for name in sorted(checked):  # identifiers come exclusively from fixed contract lists
            conn.execute('LOCK TABLE legacy_game."' + name + '" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.map_battle_catalog,runtime_control.map_battle_sources,'
                     'app_world.battle_regions,app_world.battle_occupation_days IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        calendar = conn.execute('SELECT source_timezone FROM world_compat.battle_release '
                                'WHERE world_id=%s AND run_id=%s AND manifest_sha256=%s', (world, run[0], fingerprint)).fetchone()
        if not calendar:
            raise RuntimeError('SAME_WORLD_077_SOURCE_CALENDAR_REQUIRED')
        zone = ZoneInfo(calendar[0])
        releases = []
        for schema, dependencies in zip(('route_compat', 'character_compat', 'game_compat'), DEPENDENCIES):
            rows = conn.execute('SELECT r.manifest_sha256,r.source_manifest,r.id FROM ' + schema + '.catalog_release c '
                                "JOIN reconstruction.import_runs r ON r.id=c.run_id WHERE c.status='verified'").fetchall()
            if (len(rows) != 1 or rows[0][1].get('metadata_sha256') != manifest['metadata_sha256'] or
                    sorted(rows[0][1]['backups'], key=lambda a: a['path']) != sorted(manifest['backups'], key=lambda a: a['path'])):
                raise RuntimeError('MATCHED_CURRENT_MAP_CATALOGS_REQUIRED')
            releases.append(rows[0][0])
            # Equal backup/metadata names alone do not prove table values.
            for name in dependencies:
                table, model, source = checked[name]
                key = (rows[0][2], 'TGAME_RAGEZONE', 'dbo', name)
                cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                                  'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s AND source_schema=%s AND source_table=%s', key).fetchone()
                actual = read_target(conn, model, rows[0][2])
                expected_bytes = table.get('text_bytes_value_sha256')
                byte_hash = target_bytes_hash(conn, key) if table.get('text_bytes_file') else None
                if (cp != (len(source), table['value_sha256'], table['value_sha256'], expected_bytes, expected_bytes) or
                        len(actual) != len(source) or table_hash(actual, model['column_mapping']) != table['value_sha256'] or byte_hash != expected_bytes):
                    raise RuntimeError('ACTIVE_MAP_BATTLE_DEPENDENCY_DIFFERS_FROM_FULL_SOURCE')
        for table, model, rows in checked.values():
            key = (run[0], 'TGAME_RAGEZONE', 'dbo', table['table'])
            cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                              'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s AND source_schema=%s AND source_table=%s', key).fetchone()
            actual = read_target(conn, model, run[0])
            byte_hash = target_bytes_hash(conn, key) if table.get('text_bytes_file') else None
            expected_bytes = table.get('text_bytes_value_sha256')
            if (cp != (len(rows), table['value_sha256'], table['value_sha256'], expected_bytes, expected_bytes) or
                    len(actual) != len(rows) or table_hash(actual, model['column_mapping']) != table['value_sha256'] or byte_hash != expected_bytes):
                raise RuntimeError('ARCHIVED_MAP_BATTLE_VALUES_CHANGED')
            # Check exact ordinals, including text-byte row identities, because
            # order-independent table_hash alone cannot validate the order profile.
            from psycopg import sql
            columns = [c['target'] for c in model['column_mapping']]
            ordered = conn.execute(sql.SQL('SELECT {} FROM {}.{} WHERE "_import_run_id"=%s ORDER BY "_source_row_number"').format(
                sql.SQL(',').join(map(sql.Identifier, columns)), sql.Identifier(model['target_schema']), sql.Identifier(model['table'])), (run[0],)).fetchall()
            normalized = [dict(zip((c['source'] for c in model['column_mapping']), values)) for values in ordered]
            if any(table_hash([a], model['column_mapping']) != table_hash([b], model['column_mapping']) for a, b in zip(rows, normalized)):
                raise RuntimeError('SOURCE_MAP_BATTLE_ORDINALS_CHANGED')
        if conn.execute('SELECT 1 FROM runtime_control.map_battle_catalog WHERE world_id=%s', (world,)).fetchone():
            raise RuntimeError('EXISTING_MAP_BATTLE_STATE_REQUIRES_OFFLINE_RECONCILIATION')
        regions = []
        seen = set()
        for kind, name, id_column in KINDS:
            table, _, rows = selected[name]
            for order, ordinal in enumerate(orders[name], 1):
                row = rows[ordinal - 1]
                local = word(row[id_column])
                if local not in zones:
                    continue  # exact source missing-zone branch, archive retained
                if local in seen:
                    raise RuntimeError('SOURCE_CROSS_TYPE_LOCAL_KEY_COLLISION')
                seen.add(local)
                guild = dword(row['dwGuild' if kind == 1 else 'dwGuildID']) if kind in (1, 2) else 0
                hero = original_bytes(table, ordinal, 'szHero', row['szHero']) if kind in (1, 2) else b''
                hero_time = dbtime(row['dateHero'], zone) if kind in (1, 2) else 0
                next_defend = dbtime(row['dateDefend' if kind == 1 else 'dateWarTime'], zone) if kind != 3 else 0
                occupied = dbtime(row['dateOccupy'], zone) if kind == 1 else None
                # Check Python and PG agree before materializing an instant.
                for key in ('dateHero', 'dateDefend', 'dateWarTime', 'dateOccupy'):
                    if key not in row or dt.datetime.fromisoformat(row[key]).year < 2000:
                        continue
                    resolved = conn.execute("SELECT extract(epoch FROM (date_trunc('second',%s::timestamp) AT TIME ZONE %s))::bigint", (row[key], calendar[0])).fetchone()[0]
                    if resolved != dbtime(row[key], zone):
                        raise RuntimeError('PYTHON_POSTGRESQL_SOURCE_TIMEZONE_MISMATCH')
                regions.append((world, local, kind, run[0], ordinal, order, byte(row['bCountry']), guild,
                                guilds.get(guild) if kind in (1, 2) else b'', hero, hero_time, next_defend, occupied))
        if any(sum(r[2] == kind for r in regions) > 255 for kind in (1, 2)):
            raise RuntimeError('SOURCE_HERO_LIST_BYTE_COUNT_OVERFLOW')
        days = []
        for ordinal, row in enumerate(selected['TLOCALOCCUPYTABLE'][2], 1):
            local, day = word(row['wLocalID']), byte(row['bDay'])
            if local not in seen or day == 0:
                continue
            if day > 7:
                raise RuntimeError('SOURCE_OCCUPATION_DAY_OUT_OF_BOUNDS')
            days.append((world, local, day, dword(row['dwGuildID']), byte(row['bType']), ordinal))
        conn.execute('INSERT INTO runtime_control.map_battle_catalog(world_id,run_id,manifest_sha256,routing_manifest,'
                     'character_manifest,content_manifest,source_timezone,order_sha256,region_rows,occupation_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, run[0], fingerprint, *releases, calendar[0], digest(encoded), len(regions), len(days)))
        for name, (table, _, rows) in selected.items():
            conn.execute('INSERT INTO runtime_control.map_battle_sources VALUES(%s,%s,%s,%s,%s)',
                         (world, name, len(rows), table['value_sha256'], table.get('text_bytes_value_sha256')))
        with conn.cursor() as cur:
            cur.executemany('INSERT INTO app_world.battle_regions VALUES(' + ','.join(['%s'] * 13) + ')', regions)
            cur.executemany('INSERT INTO app_world.battle_occupation_days VALUES(%s,%s,%s,%s,%s,%s)', days)
        if not conn.execute('SELECT 1 FROM battle_compat.catalog_release WHERE world_id=%s', (world,)).fetchone():
            raise RuntimeError('MAP_BATTLE_PUBLICATION_NOT_VISIBLE')
    return dict(world_id=world, manifest_sha256=fingerprint, region_rows=len(regions), occupation_rows=len(days), order_sha256=digest(encoded))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--source-order', type=Path, required=True, help='Private original CTBL query result ordinals; not proof of client compatibility')
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(import_map_battles(conn, args.manifest, args.world, args.source_order)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
