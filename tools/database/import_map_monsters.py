#!/usr/bin/env python3
"""Phase B only: publish native source Map allocation after079 and skill proof082.

Requires the complete pinned historical manifest and original query order for
one server. Does not generate historical rows or certify AI/client support.
Offline, atomic, no automatic retry of an unknown COMMIT. No private output.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from import_map_battles import word, byte, dword
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash

TABLES = ('TMONSPAWNCHART', 'TMAPMONCHART', 'TMONSTERCHART', 'TMONATTRCHART',
          'TCLASSCHART', 'TRACECHART', 'TLEVELCHART')


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RuntimeError('DUPLICATE_QUERY_ORDER_JSON_KEY')
        result[key] = value
    return result


def order_list(order, expected):
    if (not isinstance(order, list) or any(isinstance(n, bool) or not isinstance(n, int) for n in order)
            or len(order) != len(expected) or set(order) != set(expected)):
        raise RuntimeError('EXACT_ORIGINAL_QUERY_RESULT_ORDINALS_REQUIRED')
    return order


def check_table(conn, run, table, model, source, ordinals=False):
    from psycopg import sql
    key = (run, 'TGAME_RAGEZONE', 'dbo', table['table'])
    cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                      'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                      'AND source_schema=%s AND source_table=%s', key).fetchone()
    byte_hash = target_bytes_hash(conn, key) if table.get('text_bytes_file') else None
    expected_bytes = table.get('text_bytes_value_sha256')
    actual = read_target(conn, model, run)
    if (cp != (len(source), table['value_sha256'], table['value_sha256'], expected_bytes, expected_bytes)
            or len(actual) != len(source) or table_hash(actual, model['column_mapping']) != table['value_sha256']
            or byte_hash != expected_bytes):
        raise RuntimeError('MAP_MONSTER_SOURCE_OR_ACTIVE_VALUES_CHANGED')
    if ordinals:
        columns = [c['target'] for c in model['column_mapping']]
        ordered = conn.execute(sql.SQL('SELECT "_source_row_number",{} FROM {}.{} WHERE "_import_run_id"=%s ORDER BY "_source_row_number"').format(
            sql.SQL(',').join(map(sql.Identifier, columns)), sql.Identifier(model['target_schema']),
            sql.Identifier(model['table'])), (run,)).fetchall()
        normalized = [dict(zip((c['source'] for c in model['column_mapping']), values[1:])) for values in ordered]
        if ([values[0] for values in ordered] != list(range(1, len(source)+1)) or
                any(table_hash([a], model['column_mapping']) != table_hash([b], model['column_mapping'])
                    for a, b in zip(source, normalized))):
            raise RuntimeError('MAP_MONSTER_ARCHIVAL_ORDINALS_CHANGED')


def import_map_monsters(conn, manifest_path, world, server, order_path):
    byte(world)
    byte(server)
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
        raise RuntimeError('COMPLETE_MAP_MONSTER_SOURCE_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    encoded = order_path.read_bytes()
    profile = json.loads(encoded, object_pairs_hook=unique_object)
    if (not isinstance(profile, dict) or set(profile) != {'source_manifest_sha256', 'server_id', 'spawn_query', 'map_mon_queries', 'party_queries'}
            or profile['source_manifest_sha256'] != fingerprint or type(profile['server_id']) is not int
            or profile['server_id'] != server):
        raise RuntimeError('EXPLICIT_SERVER_SCOPED_QUERY_ORDER_REQUIRED')
    # Validate the fields allocation consumes, without inventing missing IDs.
    for name, keys in (('TMONSPAWNCHART', ('wID',)), ('TMONSTERCHART', ('wID',)),
                       ('TMONATTRCHART', ('wID', 'bLevel')), ('TCLASSCHART', ('bClassID',)),
                       ('TRACECHART', ('bRaceID',)), ('TLEVELCHART', ('bLevel',))):
        seen = set()
        for row in selected[name][2]:
            identity = tuple(word(row[k]) if k.startswith('w') else byte(row[k]) for k in keys)
            if identity in seen:
                raise RuntimeError('AMBIGUOUS_MAP_MONSTER_SOURCE_KEY')
            seen.add(identity)
    for row in selected['TMONSPAWNCHART'][2]:
        for key in ('wGroup', 'wMapID', 'wLocalID', 'wDir', 'wPartyID'):
            word(row[key])
        for key in ('bCountry', 'bCount', 'bRange', 'bArea', 'bLink', 'bProb', 'bRoamType', 'bEvent'):
            byte(row[key])
        dword(row['dwRegion'])
        dword(row['dwDelay'])
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'runtime_control.map_battle_catalog,runtime_control.routing_catalog,'
                     'runtime_control.character_catalog,runtime_control.active_catalog IN SHARE MODE')
        for name in sorted(TABLES):
            conn.execute('LOCK TABLE legacy_game."' + name + '" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.map_monster_catalog,runtime_control.map_monster_sources,'
                     'runtime_control.map_monster_spawn_order,runtime_control.map_monster_candidate_order,'
                     'runtime_control.map_monster_party_order IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s',
                           (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        battle = conn.execute('SELECT routing_manifest,character_manifest,content_manifest FROM battle_compat.catalog_release '
                              'WHERE world_id=%s AND run_id=%s AND manifest_sha256=%s', (world, run[0], fingerprint)).fetchone()
        if not battle:
            raise RuntimeError('MATCHED_078_BATTLE_PUBLICATION_REQUIRED')
        active = conn.execute("SELECT run_id FROM game_compat.catalog_release WHERE manifest_sha256=%s AND status='verified'",
                              (battle[2],)).fetchone()
        if not active:
            raise RuntimeError('MATCHED_ACTIVE_CONTENT_REQUIRED')
        character = conn.execute("SELECT run_id FROM character_compat.catalog_release WHERE manifest_sha256=%s AND status='verified'",
                                 (battle[1],)).fetchone()
        if not character:
            raise RuntimeError('MATCHED_ACTIVE_CHARACTER_REQUIRED')
        for table, model, rows in selected.values():
            check_table(conn, run[0], table, model, rows, ordinals=True)
            check_table(conn, character[0] if table['table']=='TLEVELCHART' else active[0], table, model, rows)
        # SQL Server FLOAT division then truncating SMALLINT cast; sets are
        # independent server-only sets, not a correlated map/unit projection.
        source_ordinals = [r[0] for r in conn.execute('SELECT "_source_row_number" FROM legacy_game."TMONSPAWNCHART" '
            'WHERE "_import_run_id"=%s AND "wMapID" IN '
            '(SELECT "wMapID" FROM route_compat.source_map_regions WHERE "bServerID"=%s) AND '
            'trunc(("fPosZ"/1024::real)::double precision)::smallint::integer*256+'
            'trunc(("fPosX"/1024::real)::double precision)::smallint::integer IN '
            '(SELECT "wUnitID" FROM route_compat.source_map_regions WHERE "bServerID"=%s)', (run[0], server, server))]
        spawn_order = order_list(profile['spawn_query'], source_ordinals)
        spawn_source = selected['TMONSPAWNCHART'][2]
        spawns = {word(spawn_source[n-1]['wID']): spawn_source[n-1] for n in spawn_order}
        candidates, parties = profile['map_mon_queries'], profile['party_queries']
        party_queries = {spawn: word(row['wPartyID']) for spawn, row in spawns.items() if word(row['wPartyID']) != 0}
        if (not isinstance(candidates, dict) or set(candidates) != {str(n) for n in spawns}
                or not isinstance(parties, dict) or set(parties) != {str(n) for n in party_queries}):
            raise RuntimeError('COMPLETE_PARAMETERIZED_MONSTER_QUERY_ORDERS_REQUIRED')
        candidate_index, party_index = {}, {}
        for ordinal, row in enumerate(selected['TMAPMONCHART'][2], 1):
            candidate_index.setdefault(word(row['wSpawnID']), []).append(ordinal)
            word(row['wMonID'])
            for key in ('bEssential', 'bLeader', 'bProb'):
                byte(row[key])
        for ordinal, row in enumerate(spawn_source, 1):
            party_index.setdefault(word(row['wPartyID']), []).append(ordinal)
        candidate_orders = [(spawn, order_list(candidates[str(spawn)], candidate_index.get(spawn, []))) for spawn in spawns]
        # Repeated same-party queries may have different unspecified order;
        # retain each source invocation separately, keyed by its owning spawn.
        party_orders = [(spawn, order_list(parties[str(spawn)], party_index[party]))
                        for spawn, party in party_queries.items()]
        candidate_count = sum(len(order) for _, order in candidate_orders)
        party_count = sum(len(order) for _, order in party_orders)
        if conn.execute('SELECT 1 FROM runtime_control.map_monster_catalog WHERE world_id=%s AND server_id=%s', (world, server)).fetchone():
            raise RuntimeError('EXISTING_MAP_MONSTER_PUBLICATION_REQUIRES_OFFLINE_RECONCILIATION')
        conn.execute('INSERT INTO runtime_control.map_monster_catalog(world_id,server_id,run_id,manifest_sha256,order_sha256,'
                     'routing_manifest,character_manifest,content_manifest,spawn_rows,candidate_rows,party_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, server, run[0], fingerprint, digest(encoded), *battle, len(spawn_order), candidate_count, party_count))
        with conn.cursor() as cur:
            cur.executemany('INSERT INTO runtime_control.map_monster_sources VALUES(%s,%s,%s,%s,%s,%s)',
                            [(world, server, name, len(rows), table['value_sha256'], table.get('text_bytes_value_sha256'))
                             for name, (table, _, rows) in selected.items()])
            cur.executemany('INSERT INTO runtime_control.map_monster_spawn_order VALUES(%s,%s,%s,%s)',
                            [(world, server, ordinal, order) for order, ordinal in enumerate(spawn_order, 1)])
            cur.executemany('INSERT INTO runtime_control.map_monster_candidate_order VALUES(%s,%s,%s,%s,%s)',
                            [(world, server, spawn, ordinal, order) for spawn, ordinals in candidate_orders
                             for order, ordinal in enumerate(ordinals, 1)])
            cur.executemany('INSERT INTO runtime_control.map_monster_party_order VALUES(%s,%s,%s,%s,%s)',
                            [(world, server, spawn, ordinal, order) for spawn, ordinals in party_orders
                             for order, ordinal in enumerate(ordinals, 1)])
        if not conn.execute('SELECT 1 FROM monster_compat.catalog_release WHERE world_id=%s AND server_id=%s', (world, server)).fetchone():
            raise RuntimeError('MAP_MONSTER_PUBLICATION_NOT_VISIBLE')
        actual = conn.execute('SELECT spawn_rows,candidate_rows,party_rows FROM monster_compat.allocation_counts '
                              'WHERE world_id=%s AND server_id=%s', (world, server)).fetchone()
        if actual != (len(spawn_order), candidate_count, party_count):
            raise RuntimeError('MAP_MONSTER_RUNTIME_SOURCE_COUNTS_DIFFER')
        #082 derives skill/formula/class/race readers from this same complete
        # archive. No new live skills/hosts or historical rows are seeded.
        if not conn.execute('SELECT 1 FROM monster_compat.skill_release WHERE world_id=%s AND server_id=%s',
                            (world, server)).fetchone():
            raise RuntimeError('MAP_MONSTER_SKILL_SOURCE_PROOF_REQUIRED')
    return dict(world_id=world, server_id=server, manifest_sha256=fingerprint, order_sha256=digest(encoded),
                spawn_rows=len(spawn_order), candidate_rows=candidate_count, party_rows=party_count)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--server', type=int, required=True)
    parser.add_argument('--source-order', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(import_map_monsters(conn, args.manifest, args.world, args.server, args.source_order)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
