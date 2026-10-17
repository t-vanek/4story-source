#!/usr/bin/env python3
"""Phase B only: publish the source Map cell inputs after081.

Requires the complete pinned historical manifest, matching079/routing releases,
the original CTBLChannelList result order and explicit TGLOBAL_GSP alias. No
archive changes, synthetic channels, private stdout or unknown-COMMIT retry.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from import_map_battles import byte
from import_map_monsters import order_list, unique_object
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash

ROUTING = ('TMAPCHART', 'TUNITCHART', 'TCHANNELCHART', 'TSPAWNPOSCHART')
KEYS = {('TGAME_RAGEZONE', name) for name in ROUTING} | {('TGLOBAL_RAGEZONE', 'TCHANNEL')}


def check_source(conn, run, table, model, source, *, ordinals=False):
    from psycopg import sql
    key = (run, table['database'], 'dbo', table['table'])
    receipt = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                           'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                           'AND source_schema=%s AND source_table=%s', key).fetchone()
    byte_hash = target_bytes_hash(conn, key) if table.get('text_bytes_file') else None
    expected_bytes = table.get('text_bytes_value_sha256')
    actual = read_target(conn, model, run)
    if (receipt != (len(source), table['value_sha256'], table['value_sha256'], expected_bytes, expected_bytes)
            or len(actual) != len(source) or table_hash(actual, model['column_mapping']) != table['value_sha256']
            or byte_hash != expected_bytes):
        raise RuntimeError('MAP_CELL_SOURCE_OR_ROUTING_VALUES_CHANGED')
    if ordinals:
        columns = [c['target'] for c in model['column_mapping']]
        ordered = conn.execute(sql.SQL('SELECT "_source_row_number",{} FROM {}.{} WHERE "_import_run_id"=%s ORDER BY "_source_row_number"').format(
            sql.SQL(',').join(map(sql.Identifier, columns)), sql.Identifier(model['target_schema']),
            sql.Identifier(model['table'])), (run,)).fetchall()
        normalized = [dict(zip((c['source'] for c in model['column_mapping']), values[1:])) for values in ordered]
        if ([values[0] for values in ordered] != list(range(1, len(source)+1)) or
                any(table_hash([a], model['column_mapping']) != table_hash([b], model['column_mapping'])
                    for a, b in zip(source, normalized))):
            raise RuntimeError('MAP_CELL_ARCHIVAL_ORDINALS_CHANGED')


def import_map_cells(conn, manifest_path, world, server, order_path, *, global_database_alias):
    byte(world)
    byte(server)
    if global_database_alias != 'TGLOBAL_GSP=TGLOBAL_RAGEZONE':
        raise RuntimeError('EXPLICIT_SOURCE_TCHANNEL_DATABASE_ALIAS_REQUIRED')
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')} for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda a: a['path']) != sorted(pinned, key=lambda a: a['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    checked = {(t['database'], t['table']): (t, model, rows) for t, model, rows in loaded
               if t['schema']=='dbo' and (t['database'], t['table']) in KEYS}
    if set(checked) != KEYS:
        raise RuntimeError('COMPLETE_SOURCE_CELL_INPUTS_REQUIRED')
    for table, model, _ in checked.values():
        if any(c['postgresql_type']=='text' for c in model['column_mapping']) and not table.get('text_bytes_file'):
            raise RuntimeError('ORIGINAL_CELL_SOURCE_TEXT_BYTES_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    encoded = order_path.read_bytes()
    profile = json.loads(encoded, object_pairs_hook=unique_object)
    if (not isinstance(profile, dict) or set(profile) != {'source_manifest_sha256', 'server_id', 'channel_query'}
            or profile['source_manifest_sha256'] != fingerprint or type(profile['server_id']) is not int
            or profile['server_id'] != server):
        raise RuntimeError('EXPLICIT_ORIGINAL_CHANNEL_QUERY_ORDER_REQUIRED')
    channels = checked[('TGLOBAL_RAGEZONE', 'TCHANNEL')][2]
    order = order_list(profile['channel_query'], range(1, len(channels)+1))
    for row in channels:
        byte(row['bGroupID'])
        if byte(row['bChannel']) >= 51:
            raise RuntimeError('SOURCE_TCHANNEL_EXCEEDS_MAP_MAX_CHANNEL51')
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'runtime_control.active_catalog,runtime_control.character_catalog,runtime_control.routing_catalog,'
                     'runtime_control.map_battle_catalog,runtime_control.map_monster_catalog,runtime_control.map_monster_sources,'
                     'runtime_control.map_monster_spawn_order,runtime_control.map_monster_candidate_order,'
                     'runtime_control.map_monster_party_order IN SHARE MODE')
        for table, model, _ in sorted(checked.values(), key=lambda value: (value[0]['database'], value[0]['table'])):
            conn.execute('LOCK TABLE '+model['target_schema']+'."'+table['table']+'" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.map_cell_catalog,runtime_control.map_cell_sources,'
                     'runtime_control.map_cell_channel_order IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s',
                           (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        allocation = conn.execute('SELECT order_sha256,routing_manifest FROM monster_compat.catalog_release '
                                  'WHERE world_id=%s AND server_id=%s AND run_id=%s AND manifest_sha256=%s',
                                  (world, server, run[0], fingerprint)).fetchone()
        if not allocation:
            raise RuntimeError('MATCHED_079_ALLOCATION_PUBLICATION_REQUIRED')
        routing = conn.execute("SELECT run_id FROM route_compat.catalog_release WHERE manifest_sha256=%s AND status='verified'",
                               (allocation[1],)).fetchone()
        if not routing:
            raise RuntimeError('MATCHED_ACTIVE_ROUTING_REQUIRED')
        for table, model, rows in checked.values():
            check_source(conn, run[0], table, model, rows, ordinals=table['table']=='TCHANNEL')
            if table['database']=='TGAME_RAGEZONE':
                check_source(conn, routing[0], table, model, rows)
        conn.execute('DELETE FROM runtime_control.map_cell_channel_order WHERE world_id=%s AND server_id=%s', (world, server))
        conn.execute('DELETE FROM runtime_control.map_cell_sources WHERE world_id=%s AND server_id=%s', (world, server))
        conn.execute('DELETE FROM runtime_control.map_cell_catalog WHERE world_id=%s AND server_id=%s', (world, server))
        conn.execute('INSERT INTO runtime_control.map_cell_catalog(world_id,server_id,run_id,manifest_sha256,'
                     'routing_manifest,allocation_order_sha256,order_sha256,global_database_alias,channel_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, server, run[0], fingerprint, allocation[1], allocation[0], digest(encoded), global_database_alias, len(order)))
        with conn.cursor() as cur:
            cur.executemany('INSERT INTO runtime_control.map_cell_sources VALUES(%s,%s,%s,%s,%s,%s,%s)',
                            [(world, server, table['database'], table['table'], len(rows), table['value_sha256'],
                              table.get('text_bytes_value_sha256')) for table, _, rows in checked.values()])
            cur.executemany('INSERT INTO runtime_control.map_cell_channel_order VALUES(%s,%s,%s,%s)',
                            [(world, server, ordinal, position) for position, ordinal in enumerate(order, 1)])
        if not conn.execute('SELECT 1 FROM route_compat.cell_catalog_release WHERE world_id=%s AND server_id=%s', (world, server)).fetchone():
            raise RuntimeError('SOURCE_CELL_PUBLICATION_NOT_VISIBLE')
        actual = conn.execute('SELECT count(*) FROM route_compat.source_cell_channels WHERE world_id=%s AND server_id=%s', (world, server)).fetchone()[0]
        if actual != len(order):
            raise RuntimeError('SOURCE_CHANNEL_RUNTIME_VIEW_COUNT_DIFFERS')
    return dict(world_id=world, server_id=server, manifest_sha256=fingerprint, order_sha256=digest(encoded), channel_rows=len(order))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--server', type=int, required=True)
    parser.add_argument('--source-order', type=Path, required=True)
    parser.add_argument('--global-database-alias', required=True, choices=['TGLOBAL_GSP=TGLOBAL_RAGEZONE'])
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(import_map_cells(conn, args.manifest, args.world, args.server, args.source_order,
                                             global_database_alias=args.global_database_alias)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
