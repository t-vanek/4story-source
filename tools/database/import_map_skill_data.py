#!/usr/bin/env python3
"""Phase B only: publish complete original CTBLSkillData query order after083.

Requires the full pinned archive and079/082, matching active character data and
private source query-order evidence. Never executes during Phase A, rewrites
historical rows, invents an empty query, or retries an uncertain COMMIT.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from import_map_battles import byte, word
from import_map_monsters import check_table, order_list, unique_object
from migrate import connect, digest, load_snapshot

SOURCE_REVISION = 'c77f79017d7940c1500ba36a5dbf8bda6fefb67c'
TABLES = ('TSKILLCHART', 'TSKILLDATA', 'TPREMIUMSKILLCHART')


def import_map_skill_data(conn, manifest_path, world, server, order_path):
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
    tables = {t['table']: (t, model, rows) for t, model, rows in loaded
              if (t['database'], t['schema']) == ('TGAME_RAGEZONE', 'dbo') and t['table'] in TABLES}
    if set(tables) != set(TABLES):
        raise RuntimeError('COMPLETE_SKILL_DATA_SOURCE_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    encoded = order_path.read_bytes()
    profile = json.loads(encoded, object_pairs_hook=unique_object)
    if (not isinstance(profile, dict) or set(profile) != {'source_manifest_sha256', 'server_id',
            'allocation_order_sha256', 'source_revision', 'skill_data_queries'}
            or profile['source_manifest_sha256'] != fingerprint or type(profile['server_id']) is not int
            or profile['server_id'] != server or profile['source_revision'] != SOURCE_REVISION):
        raise RuntimeError('EXPLICIT_ORIGINAL_SKILL_DATA_ORDER_REQUIRED')
    skills = set()
    for row in tables['TSKILLCHART'][2]:
        skill = word(row['wID'])
        if skill in skills:
            raise RuntimeError('AMBIGUOUS_SOURCE_SKILL_ID')
        skills.add(skill)
    by_skill = {}
    for ordinal, row in enumerate(tables['TSKILLDATA'][2], 1):
        skill = word(row['wSkillID'])
        for field in ('bAction', 'bType', 'bAttr', 'bExec', 'bInc', 'bCalc'):
            byte(row[field])
        word(row['wValue'])
        word(row['wValueInc'])
        by_skill.setdefault(skill, []).append(ordinal)
    queries = profile['skill_data_queries']
    for row in tables['TPREMIUMSKILLCHART'][2]:
        byte(row['bType'])
        word(row['wSkillID'])
        word(row['wMedals'])
    if not isinstance(queries, dict) or set(queries) != {str(skill) for skill in skills}:
        raise RuntimeError('COMPLETE_PER_SKILL_QUERY_ORDERS_REQUIRED')
    # Empty results must be explicit. Orphan data stays archived, as original
    # LoadData only executes CTBLSkillData for an existing skill template.
    orders = [(skill, order_list(queries[str(skill)], by_skill.get(skill, []))) for skill in sorted(skills)]
    data_count = sum(len(order) for _, order in orders)
    if data_count > 1_000_000 or len(tables['TPREMIUMSKILLCHART'][2]) > 1_000_000:
        raise RuntimeError('NATIVE_SKILL_DATA_RESOURCE_LIMIT')
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'runtime_control.active_catalog,runtime_control.character_catalog,runtime_control.routing_catalog,'
                     'runtime_control.map_battle_catalog,runtime_control.map_monster_catalog,runtime_control.map_monster_sources,'
                     'runtime_control.map_monster_spawn_order,runtime_control.map_monster_candidate_order,'
                     'runtime_control.map_monster_party_order IN SHARE MODE')
        for name in TABLES:
            conn.execute('LOCK TABLE legacy_game."' + name + '" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.map_skill_data_catalog,runtime_control.map_skill_data_queries,'
                     'runtime_control.map_skill_data_order IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s',
                           (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        allocation = conn.execute('SELECT order_sha256,character_manifest FROM monster_compat.skill_release '
                                  'WHERE world_id=%s AND server_id=%s AND run_id=%s AND manifest_sha256=%s',
                                  (world, server, run[0], fingerprint)).fetchone()
        if not allocation or profile['allocation_order_sha256'] != allocation[0]:
            raise RuntimeError('MATCHED_079_082_SKILL_PUBLICATION_REQUIRED')
        active = conn.execute("SELECT run_id FROM character_compat.catalog_release WHERE manifest_sha256=%s AND status='verified'",
                              (allocation[1],)).fetchone()
        if not active:
            raise RuntimeError('MATCHED_ACTIVE_CHARACTER_REQUIRED')
        for table, model, rows in tables.values():
            check_table(conn, run[0], table, model, rows, ordinals=True)
            if table['table'] != 'TPREMIUMSKILLCHART':
                check_table(conn, active[0], table, model, rows)
        if conn.execute('SELECT 1 FROM runtime_control.map_skill_data_catalog WHERE world_id=%s AND server_id=%s',
                        (world, server)).fetchone():
            raise RuntimeError('EXISTING_SKILL_DATA_PUBLICATION_REQUIRES_OFFLINE_RECONCILIATION')
        source = tables['TSKILLDATA'][0]
        premium = tables['TPREMIUMSKILLCHART'][0]
        conn.execute('INSERT INTO runtime_control.map_skill_data_catalog(world_id,server_id,run_id,manifest_sha256,'
                     'allocation_order_sha256,character_manifest,order_sha256,source_revision,source_rows,'
                     'source_value_sha256,source_bytes_sha256,premium_rows,premium_value_sha256,premium_bytes_sha256,skill_queries,data_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, server, run[0], fingerprint, *allocation, digest(encoded), SOURCE_REVISION,
                      len(tables['TSKILLDATA'][2]), source['value_sha256'], source.get('text_bytes_value_sha256'),
                      len(tables['TPREMIUMSKILLCHART'][2]), premium['value_sha256'], premium.get('text_bytes_value_sha256'),
                      len(skills), data_count))
        with conn.cursor() as cur:
            cur.executemany('INSERT INTO runtime_control.map_skill_data_queries VALUES(%s,%s,%s,%s)',
                            [(world, server, skill, len(order)) for skill, order in orders])
            cur.executemany('INSERT INTO runtime_control.map_skill_data_order VALUES(%s,%s,%s,%s,%s)',
                            [(world, server, skill, ordinal, order) for skill, ordinals in orders
                             for order, ordinal in enumerate(ordinals, 1)])
        result = conn.execute('SELECT skill_queries,data_rows FROM monster_compat.skill_data_release '
                              'WHERE world_id=%s AND server_id=%s', (world, server)).fetchone()
        if result != (len(skills), data_count):
            raise RuntimeError('SKILL_DATA_PUBLICATION_NOT_VISIBLE_OR_INCOMPLETE')
    return dict(world_id=world, server_id=server, manifest_sha256=fingerprint,
                order_sha256=digest(encoded), skill_queries=len(skills), data_rows=data_count)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--server', type=int, required=True)
    parser.add_argument('--source-order', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(import_map_skill_data(conn, args.manifest, args.world, args.server, args.source_order)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
