#!/usr/bin/env python3
"""Publish original Map actor item-magic/skill-point metadata without switching other releases."""
import argparse
import json
from pathlib import Path
import sys
from activate_catalog import MAPPING, PINNED
from generate_migrations import ACTOR_REFERENCE_KEYS
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash


def activate_actor_catalog(conn, manifest_path, mapping_path=MAPPING, pinned_path=PINNED):
    manifest, loaded = load_snapshot(manifest_path, mapping_path)
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(pinned_path.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda x: x['path']) != sorted(pinned, key=lambda x: x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    if {(t['database'], t['schema'], t['table']) for t, _, _ in loaded} != ACTOR_REFERENCE_KEYS:
        raise RuntimeError('COMPLETE_ACTOR_MANIFEST_REQUIRED')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        conn.execute('SELECT pg_advisory_xact_lock(418040006)')
        run = conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                           'WHERE manifest_sha256=%s FOR UPDATE', (fingerprint,)).fetchone()
        if not run or run[1] != 'verified' or run[2] != manifest:
            raise RuntimeError('VERIFIED_IMPORT_REQUIRED')
        run_id = run[0]
        for t, model, rows in loaded:
            if not t.get('text_bytes_file'):
                raise RuntimeError('ORIGINAL_TEXT_BYTES_REQUIRED')
            key = (run_id, t['database'], t['schema'], t['table'])
            cp = conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                              'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                              'AND source_schema=%s AND source_table=%s', key).fetchone()
            actual = read_target(conn, model, run_id)
            if cp != (len(rows), t['value_sha256'], table_hash(actual, model['column_mapping']),
                      t['text_bytes_value_sha256'], target_bytes_hash(conn, key)) or len(actual) != len(rows):
                raise RuntimeError('CHECKPOINT_TARGET_DRIFT')
        previous = conn.execute('SELECT run_id FROM runtime_control.actor_catalog WHERE singleton').fetchone()
        changed = not previous or previous[0] != run_id
        if changed:
            conn.execute('INSERT INTO runtime_control.actor_catalog(singleton,run_id) VALUES (true,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,activated_at=CURRENT_TIMESTAMP', (run_id,))
        if changed:
            conn.execute('INSERT INTO runtime_control.actor_catalog_activations(previous_run_id,run_id,manifest_sha256) '
                         'VALUES (%s,%s,%s)', (previous[0] if previous else None, run_id, fingerprint))
        return dict(run_id=run_id, manifest_sha256=fingerprint, changed=changed,
                    reference_tables=len(loaded), verified_source_rows=sum(len(rows) for _, _, rows in loaded),
                    snapshot_values_and_original_text_bytes_verified=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--report', type=Path)
    args = p.parse_args()
    try:
        with connect() as conn:
            result = activate_actor_catalog(conn, args.manifest)
        if args.report: args.report.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum(): code = str(ex)
        print('Actor catalog activation failed: '+(code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__': main()
