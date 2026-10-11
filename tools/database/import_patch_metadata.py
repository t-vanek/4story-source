#!/usr/bin/env python3
"""Initialize native Patch metadata from a value/byte-verified full archive.

Only runs explicitly; never downloads or publishes patch assets. Original
varchar bytes are kept for the MultiByte Patch server protocol. Existing native
metadata, even an empty initialized catalog, is never replaced implicitly.
"""
import argparse
import base64
import json
from pathlib import Path
import sys

from activate_actor_catalog import verified_actor_import
from activate_catalog import MAPPING, PINNED
from migrate import connect, digest, load_snapshot

MIN_BETA_DEFINITION = '9e408fea8bb877a570af49e58b40568896688fcb77ed8428c51a8aee091ba24f'


def import_metadata(conn, manifest_path):
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    names = {'TVERSION', 'TPREVERSION', 'TUSER_INTERFACE'}
    selected = [(t, m, r) for t, m, r in loaded
                if t['database'] == 'TGLOBAL_RAGEZONE' and t['schema'] == 'dbo' and t['table'] in names]
    if {t['table'] for t, _, _ in selected} != names:
        raise RuntimeError('COMPLETE_PATCH_TABLES_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda a: a['path']) != sorted(pinned, key=lambda a: a['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    counts = {}
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        conn.execute('LOCK TABLE app_patch.runtime_contract,app_patch."TVERSION",app_patch."TPREVERSION",'
                     'app_patch."TUSER_INTERFACE" IN ACCESS EXCLUSIVE MODE')
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     'legacy_global."TVERSION",legacy_global."TPREVERSION",legacy_global."TUSER_INTERFACE" IN SHARE MODE')
        run = verified_actor_import(conn, (manifest, selected, fingerprint))
        if conn.execute('SELECT 1 FROM app_patch.runtime_contract').fetchone():
            raise RuntimeError('PATCH_CATALOG_ALREADY_INITIALIZED')
        for table, _, rows in selected:
            name = table['table']
            if conn.execute('SELECT 1 FROM app_patch."' + name + '" LIMIT 1').fetchone():
                raise RuntimeError('UNJOURNALED_PATCH_METADATA')
            raw = {(b['row_number'], b['column']): None if b['bytes'] is None else base64.b64decode(b['bytes'], validate=True)
                   for b in table['_byte_records']}
            for number, row in enumerate(rows, 1):
                required = ['szName'] if name == 'TUSER_INTERFACE' else ['szName', 'szPath']
                if any(raw.get((number, col)) is None for col in required):
                    raise RuntimeError('PATCH_ORIGINAL_TEXT_BYTES_REQUIRED')
                if name == 'TVERSION':
                    conn.execute('INSERT INTO app_patch."TVERSION" VALUES(%s,%s,%s,%s,%s,%s,%s)',
                                 (row['dwVersion'], row['szPath'], row['szName'], raw[number, 'szPath'],
                                  raw[number, 'szName'], row['dwSize'], row['dwBetaVer']))
                elif name == 'TPREVERSION':
                    conn.execute('INSERT INTO app_patch."TPREVERSION" VALUES(%s,%s,%s,%s,%s,%s)',
                                 (row['dwBetaVer'], row['szPath'], row['szName'], raw[number, 'szPath'],
                                  raw[number, 'szName'], row['dwSize']))
                else:
                    conn.execute('INSERT INTO app_patch."TUSER_INTERFACE" VALUES(%s,%s,%s,%s)',
                                 (row['bOption'], row['szName'], raw[number, 'szName'], row['dwSize']))
            counts[name] = len(rows)
        # Backup dbo.TMinBetaVer consists of SET @dwMinVer = 2. This value is
        # its OUT contract, not the erroneous C++ return binding or a fixture.
        conn.execute('INSERT INTO app_patch.runtime_contract(singleton,contract,source_run_id,source_manifest,'
                     'min_beta,min_beta_definition_sha256) VALUES(true,1,%s,%s,2,%s)',
                     (run, fingerprint, MIN_BETA_DEFINITION))
    return dict(run_id=run, source_manifest=fingerprint, rows=counts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = import_metadata(conn, args.manifest)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Patch import failed: ' + (code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
