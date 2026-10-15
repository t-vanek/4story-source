#!/usr/bin/env python3
"""Offline publication of complete source guild-skill membership after072.

Phase B only. Verifies the complete pinned historical manifest and actual archive,
then publishes a registry bound to current character/routing releases. Does not
import, seed skills or run gameplay. Unknown COMMIT outcomes require reconciliation
without automatic retry. Private inputs remain outside Git.
"""
import argparse
import json
from pathlib import Path
import sys

from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash


def backup_identity(manifest):
    return sorted(manifest['backups'], key=lambda artifact: artifact['path'])


def activate_skill_context(conn, manifest_path):
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    expected = {(t['database'], t['schema'], t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned = [{key: artifact[key] for key in ('path', 'size_bytes', 'sha256')}
              for artifact in json.loads(PINNED.read_text())['artifacts']
              if artifact['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or backup_identity(manifest) != sorted(pinned, key=lambda artifact: artifact['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    selected = [(table, model, rows) for table, model, rows in loaded
                if (table['database'], table['schema'], table['table']) == ('TGAME_RAGEZONE', 'dbo', 'TGUILDSKILLCHART')]
    if len(selected) != 1:
        raise RuntimeError('COMPLETE_GUILD_SKILL_CHART_REQUIRED')
    table, model, rows = selected[0]
    if any(row['wSkillID'] is None or row['bType'] is None for row in rows):
        raise RuntimeError('NULL_SOURCE_GUILD_SKILL_SEMANTICS_UNSUPPORTED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='1s'")
        conn.execute("SET LOCAL statement_timeout='30s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040008)')
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes, '
                     'legacy_game."TGUILDSKILLCHART",runtime_control.character_catalog, '
                     'runtime_control.routing_catalog IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.skill_context_catalog, '
                     'runtime_control.skill_context_activations IN SHARE ROW EXCLUSIVE MODE')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        releases = []
        for view in ('character_compat.catalog_release', 'route_compat.catalog_release'):
            current = conn.execute('SELECT r.manifest_sha256,r.source_manifest FROM ' + view + ' c '
                                   "JOIN reconstruction.import_runs r ON r.id=c.run_id WHERE c.status='verified'").fetchall()
            if (len(current) != 1 or current[0][1].get('metadata_sha256') != manifest['metadata_sha256']
                    or backup_identity(current[0][1]) != backup_identity(manifest)):
                raise RuntimeError('MATCHED_CURRENT_CHARACTER_ROUTING_RELEASES_REQUIRED')
            releases.append(current[0][0])
        key = (run[0], 'TGAME_RAGEZONE', 'dbo', 'TGUILDSKILLCHART')
        checkpoint = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                                  'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                                  'AND source_schema=%s AND source_table=%s', key).fetchone()
        actual = read_target(conn, model, run[0])
        byte_hash = target_bytes_hash(conn, key) if table.get('text_bytes_file') else None
        source_bytes = table.get('text_bytes_value_sha256')
        if (checkpoint != (len(rows), table['value_sha256'], table['value_sha256'], source_bytes, source_bytes)
                or len(actual) != len(rows) or table_hash(actual, model['column_mapping']) != table['value_sha256']
                or byte_hash != source_bytes):
            raise RuntimeError('ARCHIVED_GUILD_SKILL_VALUES_CHANGED')
        release = (run[0], fingerprint, *releases, manifest['metadata_sha256'],
                   len(rows), table['value_sha256'], source_bytes)
        previous = conn.execute('SELECT run_id,manifest_sha256,character_manifest,routing_manifest,metadata_sha256, '
                                'guild_rows,guild_sha256,guild_bytes_sha256 '
                                'FROM runtime_control.skill_context_catalog WHERE singleton FOR UPDATE').fetchone()
        changed = previous != release
        if changed:
            conn.execute('INSERT INTO runtime_control.skill_context_catalog(singleton,run_id,manifest_sha256,character_manifest,routing_manifest,metadata_sha256, '
                         'guild_rows,guild_sha256,guild_bytes_sha256) VALUES(true,%s,%s,%s,%s,%s,%s,%s,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,manifest_sha256=EXCLUDED.manifest_sha256, '
                         'character_manifest=EXCLUDED.character_manifest,routing_manifest=EXCLUDED.routing_manifest,metadata_sha256=EXCLUDED.metadata_sha256, '
                         'guild_rows=EXCLUDED.guild_rows,guild_sha256=EXCLUDED.guild_sha256,guild_bytes_sha256=EXCLUDED.guild_bytes_sha256, '
                         'activated_at=clock_timestamp()', release)
            conn.execute('INSERT INTO runtime_control.skill_context_activations(previous_run_id,run_id,manifest_sha256,character_manifest,routing_manifest,metadata_sha256, '
                         'guild_rows,guild_sha256,guild_bytes_sha256) VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                         (previous[0] if previous else None, *release))
        published = conn.execute('SELECT run_id,manifest_sha256,character_manifest,routing_manifest,guild_rows '
                                 "FROM skill_context_compat.catalog_release WHERE status='verified'").fetchall()
        if published != [(run[0], fingerprint, *releases, len(rows))]:
            raise RuntimeError('SKILL_CONTEXT_PUBLICATION_NOT_VISIBLE')
    return {'run_id': run[0], 'manifest_sha256': fingerprint, 'character_manifest': releases[0],
            'routing_manifest': releases[1], 'guild_rows': len(rows), 'changed': changed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            print(json.dumps(activate_skill_context(conn, args.manifest)))
    except Exception as error:
        code = getattr(error, 'sqlstate', None)
        if not code and isinstance(error, RuntimeError) and str(error).replace('_', '').isalnum():
            code = str(error)
        print(json.dumps({'status': 'failed', 'error': code or 'ERROR_DETAIL_SUPPRESSED'}), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
