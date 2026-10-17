#!/usr/bin/env python3
"""Phase B only: atomically publish original Map AI producers after080.

Requires the complete pinned historical manifest, the existing079 publication
and private per-server original AI/condition result orders. Never edits archives,
invents command defaults or retries an unknown COMMIT. Private inputs stay out
of Git/stdout. Publication does not certify full AI, visibility or client support.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from import_map_battles import byte, dword
from import_map_monsters import TABLES as ALLOCATION_TABLES, check_table, order_list, unique_object
from migrate import connect, digest, load_snapshot

TABLES = ('TAICHART', 'TAICMDCHART', 'TAICONCHART')


def import_map_monster_ai(conn, manifest_path, world, server, order_path):
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
    checked = {t['table']: (t, model, rows) for t, model, rows in loaded
               if (t['database'], t['schema']) == ('TGAME_RAGEZONE', 'dbo') and t['table'] in TABLES+ALLOCATION_TABLES}
    if set(checked) != set(TABLES+ALLOCATION_TABLES):
        raise RuntimeError('COMPLETE_AI_AND_ALLOCATION_SOURCE_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    encoded = order_path.read_bytes()
    profile = json.loads(encoded, object_pairs_hook=unique_object)
    if (not isinstance(profile, dict) or set(profile) != {'source_manifest_sha256', 'server_id', 'bindings_query', 'condition_queries'}
            or profile['source_manifest_sha256'] != fingerprint or type(profile['server_id']) is not int
            or profile['server_id'] != server):
        raise RuntimeError('EXPLICIT_SERVER_AI_QUERY_ORDER_REQUIRED')
    commands = {}
    for row in checked['TAICMDCHART'][2]:
        key = dword(row['dwCmdID'])
        if key in commands:
            raise RuntimeError('AMBIGUOUS_SOURCE_AI_COMMAND_ID')
        commands[key] = byte(row['bCmdType'])  # unknown kinds use the real base command
    condition_index = {}
    for ordinal, row in enumerate(checked['TAICONCHART'][2], 1):
        condition_index.setdefault(dword(row['dwCmdID']), []).append(ordinal)
        dword(row['dwConditionID'])
        byte(row['bConditionType'])
    conditions = profile['condition_queries']
    if not isinstance(conditions, dict) or set(conditions) != {str(n) for n in commands}:
        raise RuntimeError('COMPLETE_AI_CONDITION_QUERY_ORDERS_REQUIRED')
    condition_orders = [(command, order_list(conditions[str(command)], condition_index.get(command, []))) for command in commands]
    bindings = checked['TAICHART'][2]
    binding_order = order_list(profile['bindings_query'], range(1, len(bindings)+1))
    for row in bindings:
        if dword(row['dwCmdID']) not in commands:
            raise RuntimeError('SOURCE_AI_COMMAND_TEMPLATE_ABSENT')
        if byte(row['bTriggerType']) >= 14:
            raise RuntimeError('SOURCE_AI_TRIGGER_ARRAY_OUT_OF_RANGE')
        byte(row['bAIType'])
        byte(row['bLoop'])
        dword(row['dwDelay'])
        dword(row['dwTriggerID'])
    condition_count = sum(len(order) for _, order in condition_orders)
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'runtime_control.active_catalog,runtime_control.character_catalog,runtime_control.routing_catalog,'
                     'runtime_control.map_battle_catalog,runtime_control.map_monster_catalog,runtime_control.map_monster_sources,'
                     'runtime_control.map_monster_spawn_order,runtime_control.map_monster_candidate_order,'
                     'runtime_control.map_monster_party_order IN SHARE MODE')
        for name in sorted(checked):
            conn.execute('LOCK TABLE legacy_game."'+name+'" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.map_monster_ai_catalog,runtime_control.map_monster_ai_sources,'
                     'runtime_control.map_monster_ai_binding_order,runtime_control.map_monster_ai_condition_order '
                     'IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s',
                           (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        allocation = conn.execute('SELECT order_sha256,content_manifest,character_manifest FROM monster_compat.catalog_release '
                                  'WHERE world_id=%s AND server_id=%s AND run_id=%s AND manifest_sha256=%s',
                                  (world, server, run[0], fingerprint)).fetchone()
        if not allocation:
            raise RuntimeError('MATCHED_079_ALLOCATION_PUBLICATION_REQUIRED')
        content = conn.execute("SELECT run_id FROM game_compat.catalog_release WHERE manifest_sha256=%s AND status='verified'",
                               (allocation[1],)).fetchone()
        character = conn.execute("SELECT run_id FROM character_compat.catalog_release WHERE manifest_sha256=%s AND status='verified'",
                                 (allocation[2],)).fetchone()
        if not content or not character:
            raise RuntimeError('MATCHED_ACTIVE_MONSTER_DEPENDENCIES_REQUIRED')
        for name, (table, model, rows) in checked.items():
            check_table(conn, run[0], table, model, rows, ordinals=True)
            if name in ALLOCATION_TABLES:
                check_table(conn, character[0] if name=='TLEVELCHART' else content[0], table, model, rows)
        if conn.execute('SELECT 1 FROM runtime_control.map_monster_ai_catalog WHERE world_id=%s AND server_id=%s', (world, server)).fetchone():
            raise RuntimeError('EXISTING_AI_PUBLICATION_REQUIRES_OFFLINE_RECONCILIATION')
        conn.execute('INSERT INTO runtime_control.map_monster_ai_catalog(world_id,server_id,run_id,manifest_sha256,'
                     'allocation_order_sha256,order_sha256,command_rows,condition_rows,binding_rows) '
                     'VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world, server, run[0], fingerprint, allocation[0], digest(encoded), len(commands), condition_count, len(bindings)))
        with conn.cursor() as cur:
            cur.executemany('INSERT INTO runtime_control.map_monster_ai_sources VALUES(%s,%s,%s,%s,%s,%s)',
                            [(world, server, name, len(checked[name][2]), checked[name][0]['value_sha256'],
                              checked[name][0].get('text_bytes_value_sha256')) for name in TABLES])
            cur.executemany('INSERT INTO runtime_control.map_monster_ai_binding_order VALUES(%s,%s,%s,%s)',
                            [(world, server, ordinal, order) for order, ordinal in enumerate(binding_order, 1)])
            cur.executemany('INSERT INTO runtime_control.map_monster_ai_condition_order VALUES(%s,%s,%s,%s,%s)',
                            [(world, server, command, ordinal, order) for command, ordinals in condition_orders
                             for order, ordinal in enumerate(ordinals, 1)])
        if not conn.execute('SELECT 1 FROM monster_compat.ai_release WHERE world_id=%s AND server_id=%s', (world, server)).fetchone():
            raise RuntimeError('AI_PUBLICATION_NOT_VISIBLE')
        for name, count in (('TAICMDCHART', len(commands)), ('TAICONCHART', condition_count), ('TAICHART', len(bindings))):
            actual = conn.execute('SELECT count(*) FROM monster_compat."'+name+'" WHERE world_id=%s AND server_id=%s', (world, server)).fetchone()[0]
            if actual != count:
                raise RuntimeError('AI_RUNTIME_VIEW_COUNTS_DIFFER')
    return dict(world_id=world, server_id=server, manifest_sha256=fingerprint, order_sha256=digest(encoded),
                command_rows=len(commands), condition_rows=condition_count, binding_rows=len(bindings))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--world', type=int, required=True)
    parser.add_argument('--server', type=int, required=True)
    parser.add_argument('--source-order', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(import_map_monster_ai(conn, args.manifest, args.world, args.server, args.source_order)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
