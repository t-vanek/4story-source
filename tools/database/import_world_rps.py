#!/usr/bin/env python3
"""Phase B only: publish the full RPS chart and project its source win history.

Requires074, a complete verified historical manifest and an explicitly chosen
IANA source timezone. Never replaces an existing projection or live ledger.
Source character INTs are audit values, not a claimed native identity mapping.
No connection details, source records or timestamps are printed.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import sys
from zoneinfo import ZoneInfo

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from import_during_items import source_epoch
from migrate import connect, digest, load_snapshot, read_target, table_hash

TABLES = ('TRPSGAMECHART', 'TRPSGAMERECORDTABLE')


def import_world_rps(conn, manifest_path, world, source_timezone):
    if isinstance(world, bool) or not 0 <= world <= 255:
        raise RuntimeError('WORLD_OUT_OF_RANGE')
    zone = ZoneInfo(source_timezone)
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if (len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) !=
            sorted(pinned, key=lambda x: x['path'])):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    selected = {t['table']: (t, model, rows) for t, model, rows in loaded
                if (t['database'], t['schema']) == ('TGAME_RAGEZONE', 'dbo') and t['table'] in TABLES}
    if set(selected) != set(TABLES):
        raise RuntimeError('COMPLETE_RPS_SOURCE_REQUIRED')
    chart, _, games = selected[TABLES[0]]
    history, _, records = selected[TABLES[1]]
    # Historical restored-profile evidence for these exact two pinned backups
    # records48 chart rows and0 ledger rows. A new profile needs reconciliation,
    # not a manifest that repeats the backup names but invents played rounds.
    if (len(games), len(records)) != (48, 0):
        raise RuntimeError('PINNED_RPS_ROW_PROFILE_REQUIRES_RECONCILIATION')
    keys = {(g['bType'], g['bWinCount']) for g in games}
    if len(keys) != len(games) or len(games) > (65534 - 16 - 4) // 9:
        raise RuntimeError('RPS_SOURCE_CHART_KEY_OR_WIRE_DOMAIN_UNAVAILABLE')
    # Source WORD bindings preserve signed SQL SMALLINT bits, not positive-only
    # caps. Do not normalize negative historical SMALLINT values to zero.
    for game in games:
        if any(isinstance(game[c], bool) or not isinstance(game[c], int) or not 0 <= game[c] <= 255
               for c in ('bType', 'bWinCount', 'bProb_Win', 'bProb_Draw', 'bProb_Lose')):
            raise RuntimeError('RPS_SOURCE_BYTE_DOMAIN_MISMATCH')
        if any(isinstance(game[c], bool) or not isinstance(game[c], int) or not -32768 <= game[c] <= 32767
               for c in ('wWinKeep', 'wWinPeriod')):
            raise RuntimeError('RPS_SOURCE_WORD_BINDING_MISMATCH')
    seconds = []
    for row in records:
        raw = dt.datetime.fromisoformat(row['dWinDate'])
        if raw.tzinfo is not None or raw.second or raw.microsecond:
            raise RuntimeError('RPS_SOURCE_SMALLDATETIME_REPRESENTATION_MISMATCH')
        if not dt.datetime(1900, 1, 1) <= raw <= dt.datetime(2079, 6, 6, 23, 59):
            raise RuntimeError('RPS_SOURCE_SMALLDATETIME_RANGE_MISMATCH')
        second = 0 if raw.year < 2000 else source_epoch(row['dWinDate'], zone)
        if second % 60 or not -3000000000 <= second <= 4000000000:
            raise RuntimeError('RPS_SOURCE_TIME_REQUIRES_EXPLICIT_RECONCILIATION')
        seconds.append(second)
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        # Reuse the existing offline publication boundary; all Map/World owners
        # must be absent. Catalog and history become visible in one transaction.
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,app_world.worlds,'
                     'legacy_game."TRPSGAMECHART",legacy_game."TRPSGAMERECORDTABLE" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.world_rps_catalog,app_world.rps_records,'
                     'app_world.rps_operations IN SHARE ROW EXCLUSIVE MODE')
        if not conn.execute('SELECT 1 FROM app_world.worlds WHERE group_id=%s', (world,)).fetchone():
            raise RuntimeError('NATIVE_WORLD_REQUIRED')
        if not conn.execute('SELECT 1 FROM pg_timezone_names WHERE name=%s', (source_timezone,)).fetchone():
            raise RuntimeError('POSTGRESQL_SOURCE_TIMEZONE_REQUIRED')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        for table, model, rows in selected.values():
            checkpoint = conn.execute('SELECT row_count,source_sha256,target_sha256 FROM reconstruction.checkpoints '
                                      'WHERE run_id=%s AND source_database=%s AND source_schema=%s AND source_table=%s',
                                      (run[0], 'TGAME_RAGEZONE', 'dbo', table['table'])).fetchone()
            actual = read_target(conn, model, run[0])
            if (checkpoint != (len(rows), table['value_sha256'], table['value_sha256']) or
                    len(actual) != len(rows) or table_hash(actual, model['column_mapping']) != table['value_sha256']):
                raise RuntimeError('ARCHIVED_RPS_VALUES_CHANGED')
        if (conn.execute('SELECT 1 FROM runtime_control.world_rps_catalog WHERE world_id=%s', (world,)).fetchone() or
                conn.execute('SELECT 1 FROM app_world.rps_records WHERE world_id=%s LIMIT 1', (world,)).fetchone() or
                conn.execute('SELECT 1 FROM app_world.rps_operations WHERE world_id=%s LIMIT 1', (world,)).fetchone()):
            raise RuntimeError('EXISTING_RPS_STATE_REQUIRES_OFFLINE_RECONCILIATION')
        # Preserve duplicates and unknown chart keys; the original boot skips
        # unknown keys in RAM but retains them in the source database.
        for number, (row, second) in enumerate(zip(records, seconds), 1):
            raw = dt.datetime.fromisoformat(row['dWinDate'])
            if raw.year >= 2000:
                resolved = conn.execute('SELECT win_time,hydrate_second FROM app_world.rps_source_minute(%s,%s)',
                                        (second, source_timezone)).fetchone()
                if resolved != (raw, second):
                    raise RuntimeError('RPS_PYTHON_POSTGRESQL_TIMEZONE_MISMATCH')
            conn.execute('INSERT INTO app_world.rps_records(world_id,game_type,win_count,win_time,hydrate_second,character_id,'
                         'source_run_id,source_row) VALUES(%s,%s,%s,%s,%s,%s,%s,%s)',
                         (world, row['bType'], row['bWinCount'], raw,
                          second, row['dwCharID'], run[0], number))
        conn.execute('INSERT INTO runtime_control.world_rps_catalog(world_id,run_id,manifest_sha256,chart_sha256,'
                     'records_sha256,chart_rows,record_rows,source_timezone) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, run[0], fingerprint, chart['value_sha256'], history['value_sha256'],
                      len(games), len(records), source_timezone))
    return dict(world_id=world, run_id=run[0], manifest_sha256=fingerprint, chart_rows=len(games),
                source_history_rows=len(records), source_timezone=source_timezone)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--source-timezone', required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(import_world_rps(conn, args.manifest, args.world, args.source_timezone)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
