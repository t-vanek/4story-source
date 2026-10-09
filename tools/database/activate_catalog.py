#!/usr/bin/env python3
"""Atomically publish an explicit, reconciled backup-derived reference catalog.

Requires a complete 15-table reference manifest, exact pinned backup fingerprints,
verified checkpoints and fresh comparison of source rows/text bytes with PG.
The tool only writes runtime selection/history; never changes source snapshots.
"""
import argparse
import json
from pathlib import Path
import re
import sys

from forensics import cpp_literals
from generate_migrations import REFERENCE_TABLES
from migrate import (connect, digest, load_snapshot, read_target, table_hash,
                     target_bytes_hash)

ROOT = Path(__file__).resolve().parents[2]
MAPPING = ROOT / 'database/postgresql/mapping.json'
PINNED = ROOT / '_rewrite/docs/database-reconstruction/evidence/artifact-inventory.json'


def chart_queries(path):
    """Read the actual simple chart statements, including adjacent C++ literals."""
    queries = []
    for match in re.finditer(r'inline constexpr const char\* (All\w+)\s*=\s*(.*?);', path.read_text(), re.S):
        query = ''.join(x['value'] for x in cpp_literals(match.group(2)))
        table = re.search(r'\bFROM\s+(\w+)\b', query, re.I)
        if table and table.group(1) in REFERENCE_TABLES:
            queries.append((match.group(1), query))
    if len(queries) != 14:
        raise RuntimeError('CURRENT_CHART_QUERY_SET_CHANGED')
    return queries


def activate_catalog(conn, manifest_path, mapping_path=MAPPING, pinned_path=PINNED):
    manifest, loaded = load_snapshot(manifest_path, mapping_path)
    fingerprints = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
                    for a in json.loads(pinned_path.read_text())['artifacts']
                    if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(fingerprints) != 2 or sorted(manifest['backups'], key=lambda a: a['path']) != sorted(fingerprints, key=lambda a: a['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    if {t['table'] for t, _, _ in loaded} != set(REFERENCE_TABLES):
        raise RuntimeError('COMPLETE_REFERENCE_MANIFEST_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        conn.execute('SELECT pg_advisory_xact_lock(418040003)')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR UPDATE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_IMPORT_REQUIRED')
        run_id = run[0]
        verified_rows = 0
        for table, model, rows in loaded:
            if not table.get('text_bytes_file'):
                raise RuntimeError('ORIGINAL_TEXT_BYTES_REQUIRED')
            key = (run_id, table['database'], table['schema'], table['table'])
            cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                              'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                              'AND source_schema=%s AND source_table=%s', key).fetchone()
            actual = read_target(conn, model, run_id)
            actual_hash = table_hash(actual, model['column_mapping'])
            byte_hash = target_bytes_hash(conn, key)
            if cp != (len(rows), table['value_sha256'], actual_hash, table['text_bytes_value_sha256'], byte_hash) or len(actual) != len(rows):
                raise RuntimeError('CHECKPOINT_TARGET_DRIFT')
            verified_rows += len(rows)
        # Imported item projection must already exist; activation is read-only on content.
        item_count = conn.execute('SELECT count(*) FROM content.item_templates WHERE release_id=%s', (run_id,)).fetchone()[0]
        item = next(t for t, _, _ in loaded if t['table'] == 'TITEMCHART')
        if item_count != item['row_count']:
            raise RuntimeError('CANONICAL_ITEM_PROJECTION_REQUIRED')
        expected_items = sorted((int(r['wItemID']) & 65535, r['szNAME'], r['bType'], r['bKind'], r['bStack'], float(r['fPrice']))
                                for t, _, rows in loaded if t['table'] == 'TITEMCHART' for r in rows)
        actual_items = conn.execute('SELECT item_id,display_name,item_type,item_kind,max_stack,price '
                                    'FROM content.item_templates WHERE release_id=%s ORDER BY item_id', (run_id,)).fetchall()
        if expected_items != actual_items:
            raise RuntimeError('CANONICAL_ITEM_VALUE_MISMATCH')
        previous = conn.execute('SELECT run_id FROM runtime_control.active_catalog WHERE singleton').fetchone()
        changed = not previous or previous[0] != run_id
        if changed:
            conn.execute('INSERT INTO runtime_control.active_catalog(singleton,run_id) VALUES (true,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,activated_at=CURRENT_TIMESTAMP', (run_id,))
        conn.execute('SET LOCAL search_path TO pg_catalog, game_compat')
        checked_queries = []
        query_row_counts = {}
        for name, query in chart_queries(ROOT / 'Server/TMapSvrAsio/db/queries.h'):
            cursor = conn.execute(query)
            count = 0
            while batch := cursor.fetchmany(2048):
                count += len(batch)
            query_row_counts[name] = count
            checked_queries.append(name)
        monsters, mapped = conn.execute('SELECT '
            '(SELECT count(*) FROM game_compat."TMONSTERCHART"),'
            '(SELECT count(*) FROM game_compat."TMONATTRCHART")').fetchone()
        missing = conn.execute('SELECT count(*) FROM runtime_control.missing_monster_attributes').fetchone()[0]
        if mapped + missing != monsters:
            raise RuntimeError('MONSTER_ATTRIBUTE_CARDINALITY_MISMATCH')
        if changed:
            conn.execute('INSERT INTO runtime_control.catalog_activations(previous_run_id,run_id,manifest_sha256) '
                         'VALUES (%s,%s,%s)', (previous[0] if previous else None, run_id, fingerprint))
        return {'run_id': run_id, 'manifest_sha256': fingerprint, 'changed': changed,
                'reference_tables': len(loaded), 'verified_source_rows': verified_rows,
                'current_chart_queries_verified': checked_queries, 'query_row_counts': query_row_counts,
                'monster_templates': monsters,
                'monster_attribute_rows': mapped, 'source_monster_attribute_gaps': missing,
                'snapshot_values_and_original_text_bytes_verified': True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--mapping', type=Path, default=MAPPING)
    p.add_argument('--report', type=Path)
    args = p.parse_args()
    try:
        with connect() as conn:
            result = activate_catalog(conn, args.manifest, args.mapping)
        if args.report:
            args.report.write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Catalog activation failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
