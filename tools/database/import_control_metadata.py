#!/usr/bin/env python3
"""Initialize native Control projections from the verified, complete archive.

Offline and explicit. Never contacts peers, invokes service managers, sends SMS,
or runs Control. Credentials are transformed only in memory and never printed.
The original source rows and bytes stay immutable in the archival schemas.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import secrets
import sys
from zoneinfo import ZoneInfo

from activate_actor_catalog import verified_actor_import
from activate_catalog import MAPPING, PINNED
from migrate import connect, digest, load_snapshot


# (native column, original column, preserve original bytes)
PROJECTIONS = {
    'TGROUP': [('bGroupID', 'bGroupID', False), ('name_bytes', 'szNAME', True)],
    'TMACHINE': [('bMachineID', 'bMachineID', False), ('name_bytes', 'szNAME', True), ('bRouteID', 'bRouteID', False)],
    'TNETWORK': [('bMachineID', 'bMachineID', False), ('network_bytes', 'szNetwork', True)],
    'TIPADDR': [('bMachineID', 'bMachineID', False), ('public_bytes', 'szIPAddr', True),
               ('private_bytes', 'szPriAddr', True), ('bActive', 'bActive', False)],
    'TSVRTYPE': [('bType', 'bType', False), ('name_bytes', 'szName', True), ('bControl', 'bControl', False)],
    'TSERVER': [(c, c, False) for c in ('bGroupID', 'bServerID', 'bType', 'bMachineID', 'wPort')]
               + [('name_bytes', 'szName', True)],
    'TCASHSHOPITEMCHART': [('wID', 'wID', False), ('name_bytes', 'szName', True), ('bCanSell', 'bCanSell', False)],
    'TEVENTCHART': [(c, c, False) for c in ('dwIndex', 'bID', 'bGroupID', 'bSvrType', 'bSvrID',
                     'dStartDate', 'dEndDate', 'wValue', 'wMapID', 'dwStartAlarm', 'dwEndAlarm', 'bPartTime')]
        + [(target, source, True) for target, source in (
            ('title_bytes', 'szTitle'), ('start_bytes', 'szStartMsg'), ('mid_bytes', 'szMidMsg'),
            ('end_bytes', 'szEndMsg'), ('value_bytes', 'szValue'))],
}


def import_metadata(conn, manifest_path, legacy_timezone):
    ZoneInfo(legacy_timezone)  # Require an explicit, real timezone, even for empty event history.
    manifest, loaded = load_snapshot(manifest_path, MAPPING, include_historical=True)
    names = set(PROJECTIONS) | {'TMANAGER'}
    selected = [(t, m, rows) for t, m, rows in loaded
                if t['database'] == 'TGLOBAL_RAGEZONE' and t['schema'] == 'dbo' and t['table'] in names]
    if {t['table'] for t, _, _ in selected} != names:
        raise RuntimeError('COMPLETE_CONTROL_TABLES_REQUIRED')
    pinned = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
              for a in json.loads(PINNED.read_text())['artifacts']
              if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
    if len(pinned) != 2 or sorted(manifest['backups'], key=lambda a: a['path']) != sorted(pinned, key=lambda a: a['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    fingerprint = digest(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode())
    counts = {}
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='5s'")
        if not conn.execute('SELECT pg_try_advisory_xact_lock(1413698130)').fetchone()[0]:
            raise RuntimeError('CONTROL_MUST_BE_STOPPED')
        if not conn.execute('SELECT 1 FROM pg_timezone_names WHERE name=%s', (legacy_timezone,)).fetchone():
            raise RuntimeError('POSTGRESQL_TIMEZONE_REQUIRED')
        targets = ['app_control.runtime_contract', 'app_control.operators'] + ['app_control."'+n+'"' for n in PROJECTIONS]
        conn.execute('LOCK TABLE '+','.join(targets)+' IN ACCESS EXCLUSIVE MODE')
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes,'
                     + ','.join('legacy_global."'+n+'"' for n in sorted(names))+' IN SHARE MODE')
        run = verified_actor_import(conn, (manifest, selected, fingerprint))
        if conn.execute('SELECT 1 FROM app_control.runtime_contract').fetchone():
            raise RuntimeError('CONTROL_ALREADY_INITIALIZED')
        if any(conn.execute('SELECT 1 FROM '+target+' LIMIT 1').fetchone() for target in targets[1:]):
            raise RuntimeError('UNJOURNALED_CONTROL_STATE')
        for table, _, rows in selected:
            name = table['table']
            raw = {(b['row_number'], b['column']): None if b['bytes'] is None else base64.b64decode(b['bytes'], validate=True)
                   for b in table['_byte_records']}

            def original(number, column):
                value = raw.get((number, column))
                if value is None or b'\0' in value:
                    raise RuntimeError('CONTROL_ORIGINAL_BYTES_REQUIRED')
                return value

            for number, row in enumerate(rows, 1):
                if name == 'TMANAGER':
                    identity = original(number, 'szID')
                    password = original(number, 'szPasswd')
                    if len(identity) > 50 or len(password) > 50:
                        raise RuntimeError('UNREPRESENTABLE_OPERATOR_CREDENTIAL')
                    # Keep the historical ID's decoded representation; require
                    # its stated Windows code-page projection before activation.
                    if identity.decode('cp1252') != row['szID']:
                        raise RuntimeError('OPERATOR_CODEPAGE_MISMATCH')
                    salt = secrets.token_bytes(16)
                    credential = hashlib.pbkdf2_hmac('sha256', password.rstrip(b' '), salt, 210000, 32)
                    conn.execute('INSERT INTO app_control.operators(name,name_bytes,password_salt,password_hash,password_iterations,authority) '
                                 'VALUES(%s,%s,%s,%s,210000,%s)', (row['szID'], identity, salt, credential, row['bAuthority']))
                else:
                    fields = PROJECTIONS[name]
                    columns = ','.join('"'+target+'"' for target, _, _ in fields)
                    values = [original(number, source) if binary else row[source] for _, source, binary in fields]
                    conn.execute('INSERT INTO app_control."'+name+'"('+columns+') VALUES('+','.join(['%s']*len(fields))+')', values)
            counts[name] = len(rows)
        # Original LoadData excludes servers without a loaded group, machine or
        # bControl=1 type. Retain every record and report those exact exclusions.
        excluded = conn.execute('SELECT s."bGroupID",s."bType",s."bServerID" FROM app_control."TSERVER" s LEFT JOIN app_control."TGROUP" g USING("bGroupID") '
                        'LEFT JOIN app_control."TMACHINE" m USING("bMachineID") LEFT JOIN app_control."TSVRTYPE" t USING("bType") '
                        'WHERE s."bType"=6 OR g."bGroupID" IS NULL OR m."bMachineID" IS NULL OR t."bType" IS NULL OR t."bControl"<>1 '
                        'ORDER BY s."bGroupID",s."bType",s."bServerID"').fetchall()
        conn.execute('INSERT INTO app_control.runtime_contract(singleton,contract,source_run_id,source_manifest,legacy_timezone,text_encoding) '
                     "VALUES(true,1,%s,%s,%s,'WIN1252')", (run, fingerprint, legacy_timezone))
    return dict(run_id=run, source_manifest=fingerprint, rows=counts, legacy_timezone=legacy_timezone,
                source_defined_runtime_exclusions=excluded)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--legacy-timezone', required=True, help='Source server IANA zone, explicitly chosen by the operator')
    args = parser.parse_args()
    try:
        with connect() as conn:
            result = import_metadata(conn, args.manifest, args.legacy_timezone)
        print(json.dumps(result))
    except Exception as ex:
        code = getattr(ex, 'sqlstate', None)
        if not code and isinstance(ex, RuntimeError) and str(ex).replace('_', '').isalnum():
            code = str(ex)
        print('Control import failed: '+(code or 'ERROR_DETAIL_SUPPRESSED'), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
