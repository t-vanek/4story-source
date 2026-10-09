#!/usr/bin/env python3
"""Checksummed migrations and resumable, value-verified reference-data imports.

PG connection is supplied via libpq PG* environment variables (never printed).
Requires psycopg 3. This standalone tool shares the server's SQL schema design;
it does not introduce an application ORM. Extracted rows stay outside Git.
"""
import argparse
import base64
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
from decimal import Decimal

from generate_migrations import qi


def digest(data):return hashlib.sha256(data).hexdigest()


class SourceValidationError(RuntimeError):
    def __init__(self,code,table,row_number):
        super().__init__(code)
        self.source_table=table
        self.source_row_number=row_number


def bytes_hash(records):
    values=sorted((r['row_number'],r['column'],None if r['bytes'] is None else
        (bytes(r['bytes']).hex() if not isinstance(r['bytes'],str) else base64.b64decode(r['bytes'],validate=True).hex())) for r in records)
    return digest(json.dumps(values,separators=(',',':')).encode())


def table_hash(rows, columns):
    """Order-independent multiset hash; preserves NULL, duplicates and float bits."""
    hashes=[]
    for row in rows:
        values=[]
        for c in columns:
            value=row[c['source']]; typ=c['postgresql_type']
            if value is None: encoded=['null']
            elif typ in ('real','double precision'):
                encoded=[typ,struct.pack('!f' if typ=='real' else '!d',float(value)).hex()]
            elif typ.startswith('timestamp'):
                date=value if isinstance(value,dt.datetime) else dt.datetime.fromisoformat(value)
                encoded=['timestamp',date.isoformat(timespec='microseconds')]
            elif typ=='bytea':
                encoded=['bytea',bytes(value).hex() if not isinstance(value,str) else base64.b64decode(value).hex()]
            elif typ.startswith('numeric'):
                encoded=['numeric',str(Decimal(str(value)).normalize())]
            else: encoded=[typ,value]
            values.append(encoded)
        hashes.append(digest(json.dumps(values,ensure_ascii=False,separators=(',',':')).encode()))
    return digest(('\n'.join(sorted(hashes))+'\n').encode())


def connect():
    import psycopg
    # PGHOST/PGPORT/PGDATABASE/PGUSER/PGPASSWORD or PGPASSFILE.
    return psycopg.connect('',autocommit=True)


def apply_migrations(conn, directory):
    applied=[]
    with conn.transaction():
        conn.execute('SELECT pg_advisory_xact_lock(418040001)')
        conn.execute('CREATE SCHEMA IF NOT EXISTS migration_control')
        conn.execute('''CREATE TABLE IF NOT EXISTS migration_control.applied (
            name text PRIMARY KEY, sha256 text NOT NULL, applied_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP)''')
        for path in sorted(directory.glob('[0-9][0-9][0-9]-*.sql')):
            checksum=digest(path.read_bytes())
            old=conn.execute('SELECT sha256 FROM migration_control.applied WHERE name=%s',(path.name,)).fetchone()
            if old:
                if old[0]!=checksum:raise RuntimeError('APPLIED_MIGRATION_CHECKSUM_CHANGED')
                continue
            conn.execute(path.read_text())
            conn.execute('INSERT INTO migration_control.applied(name,sha256) VALUES (%s,%s)',(path.name,checksum))
            applied.append(path.name)
    return applied


def adapt(value, column):
    if value is None:return None
    typ=column['postgresql_type']
    if typ.startswith('timestamp'):return dt.datetime.fromisoformat(value)
    if typ=='bytea':return base64.b64decode(value,validate=True)
    if typ in ('real','double precision'):return float(value)
    if typ.startswith('numeric'):return Decimal(str(value))
    return value


def load_snapshot(manifest_path, mapping_path):
    manifest=json.loads(manifest_path.read_text());mapping=json.loads(mapping_path.read_text())
    if manifest['metadata_sha256']!=mapping['metadata_sha256']:raise RuntimeError('METADATA_FINGERPRINT_MISMATCH')
    allowed={(t['database'],t['schema'],t['table']):t for t in mapping['tables'] if t['reference_import_allowed']}
    loaded=[];seen=set()
    for table in manifest['tables']:
        # Keep the validated public manifest immutable. Private byte records
        # belong only to the loaded working entry, never to provenance hashes.
        table=dict(table)
        key=(table['database'],table['schema'],table['table'])
        if key not in allowed:raise RuntimeError('TABLE_NOT_ON_REFERENCE_ALLOWLIST')
        if key in seen:raise RuntimeError('DUPLICATE_TABLE_IN_MANIFEST')
        seen.add(key)
        path=(manifest_path.parent/table['file']).resolve()
        if not path.is_relative_to(manifest_path.parent.resolve()):raise RuntimeError('SNAPSHOT_PATH_ESCAPES_DIRECTORY')
        data=path.read_bytes()
        if digest(data)!=table['file_sha256']:raise RuntimeError('SNAPSHOT_FILE_CHANGED')
        rows=json.loads(data)
        model=allowed[key];columns=model['column_mapping'];expected={c['source'] for c in columns}
        if len(rows)!=table['row_count']:raise RuntimeError('SOURCE_ROW_COUNT_MISMATCH')
        pk_seen=set()
        for row_number,row in enumerate(rows,1):
            def invalid(code):raise SourceValidationError(code,table['table'],row_number)
            if set(row)!=expected:invalid('SOURCE_COLUMN_SET_MISMATCH')
            for col in columns:
                value=row[col['source']]; typ=col['postgresql_type']
                if value is None and not col.get('nullable',True):invalid('SOURCE_NOT_NULL_VIOLATION')
                if value is not None and typ in ('smallint','integer','bigint'):
                    lo,hi={'smallint':(-32768,32767),'integer':(-2147483648,2147483647),
                           'bigint':(-9223372036854775808,9223372036854775807)}[typ]
                    if not isinstance(value,int) or not lo<=value<=hi:invalid('SOURCE_INTEGER_RANGE_VIOLATION')
                    if col['mssql_type']=='tinyint' and not 0<=value<=255:invalid('SOURCE_TINYINT_RANGE_VIOLATION')
            if model['original_pk']:
                key_values=tuple(row[k] for k in model['original_pk'])
                if key_values in pk_seen:invalid('SOURCE_DUPLICATE_PRIMARY_KEY')
                pk_seen.add(key_values)
        if table_hash(rows,columns)!=table['value_sha256']:raise RuntimeError('SOURCE_VALUE_HASH_MISMATCH')
        if table.get('text_bytes_file'):
            byte_path=(manifest_path.parent/table['text_bytes_file']).resolve()
            if not byte_path.is_relative_to(manifest_path.parent.resolve()):raise RuntimeError('BYTE_SNAPSHOT_PATH_ESCAPES_DIRECTORY')
            byte_data=byte_path.read_bytes()
            if digest(byte_data)!=table['text_bytes_file_sha256']:raise RuntimeError('BYTE_SNAPSHOT_FILE_CHANGED')
            records=json.loads(byte_data)
            if bytes_hash(records)!=table['text_bytes_value_sha256']:raise RuntimeError('SOURCE_BYTE_HASH_MISMATCH')
            text_cols={c['source'] for c in columns if c['postgresql_type']=='text'}
            expected_keys={(i,c) for i in range(1,len(rows)+1) for c in text_cols}
            actual_keys=[(r['row_number'],r['column']) for r in records]
            if set(actual_keys)!=expected_keys or len(actual_keys)!=len(expected_keys):raise RuntimeError('SOURCE_BYTE_COLUMN_SET_MISMATCH')
            table['_byte_records']=records
        loaded.append((table,model,rows))
    return manifest,loaded


def read_target(conn, model, run_id):
    cols=model['column_mapping']
    rows=conn.execute('SELECT '+', '.join(qi(c['target']) for c in cols)+' FROM '+
        qi(model['target_schema'])+'.'+qi(model['table'])+' WHERE "_import_run_id"=%s',(run_id,)).fetchall()
    return [{c['source']:v for c,v in zip(cols,row,strict=True)} for row in rows]


def import_snapshot(conn, manifest_path, mapping_path):
    from psycopg.types.json import Jsonb
    manifest,loaded=load_snapshot(manifest_path,mapping_path)
    # Loaded byte records are private payloads, never part of the journal manifest.
    public_manifest=json.loads(manifest_path.read_text())
    fingerprint=digest(json.dumps(public_manifest,sort_keys=True,separators=(',',':')).encode())
    with conn.transaction():
        conn.execute('SELECT pg_advisory_xact_lock(418040002)')
        run=conn.execute('''INSERT INTO reconstruction.import_runs(manifest_sha256,source_manifest,status)
            VALUES (%s,%s,'running') ON CONFLICT(manifest_sha256) DO UPDATE SET status='running', verified_at=NULL
            RETURNING id''',(fingerprint,Jsonb(public_manifest))).fetchone()[0]
    verified=[];skipped=[]
    for table,model,rows in loaded:
        key=(run,table['database'],table['schema'],table['table'])
        try:
            with conn.transaction():
                conn.execute('SELECT id FROM reconstruction.import_runs WHERE id=%s FOR UPDATE',(run,))
                cp=conn.execute('''SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 FROM reconstruction.checkpoints
                    WHERE run_id=%s AND source_database=%s AND source_schema=%s AND source_table=%s''',key).fetchone()
                if cp:
                    actual=read_target(conn,model,run)
                    checksum=table_hash(actual,model['column_mapping'])
                    byte_checksum=target_bytes_hash(conn,key) if table.get('text_bytes_file') else None
                    if cp!=(len(rows),table['value_sha256'],checksum,table.get('text_bytes_value_sha256'),byte_checksum) or len(actual)!=len(rows):
                        raise RuntimeError('CHECKPOINT_TARGET_DRIFT')
                    skipped.append(table['table']);continue
                if read_target(conn,model,run):raise RuntimeError('UNJOURNALED_TARGET_ROWS')
                cols=model['column_mapping']
                statement='INSERT INTO '+qi(model['target_schema'])+'.'+qi(model['table'])+' ('+\
                    ','.join(['"_import_run_id"','"_source_row_number"']+[qi(c['target']) for c in cols])+') VALUES ('+\
                    ','.join(['%s']*(2+len(cols)))+')'
                with conn.cursor() as cur:
                    cur.executemany(statement,((run,i,*[adapt(row[c['source']],c) for c in cols])
                                              for i,row in enumerate(rows,1)))
                    if table.get('text_bytes_file'):
                        cur.executemany('''INSERT INTO reconstruction.source_text_bytes
                            (run_id,source_database,source_schema,source_table,source_row_number,column_name,source_bytes)
                            VALUES (%s,%s,%s,%s,%s,%s,%s)''',
                            ((*key,r['row_number'],r['column'],None if r['bytes'] is None else base64.b64decode(r['bytes'],validate=True))
                             for r in table['_byte_records']))
                target=read_target(conn,model,run); checksum=table_hash(target,cols)
                if len(target)!=len(rows) or checksum!=table['value_sha256']:
                    raise RuntimeError('TARGET_VALUE_RECONCILIATION_FAILED')
                byte_checksum=target_bytes_hash(conn,key) if table.get('text_bytes_file') else None
                if byte_checksum!=table.get('text_bytes_value_sha256'):raise RuntimeError('TARGET_BYTE_RECONCILIATION_FAILED')
                conn.execute('''INSERT INTO reconstruction.checkpoints
                    (run_id,source_database,source_schema,source_table,row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256)
                    VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)''',(*key,len(rows),table['value_sha256'],checksum,
                                                      table.get('text_bytes_value_sha256'),byte_checksum))
                verified.append({'table':table['table'],'rows':len(rows),'value_hash_matches':True,'source_text_bytes_verified':byte_checksum is not None})
        except Exception as ex:
            code=getattr(ex,'sqlstate',None) or (str(ex) if isinstance(ex,RuntimeError) else 'IMPORT_ERROR')
            # Neither SQL error detail nor source row values may escape.
            if not code.replace('_','').isalnum():code='IMPORT_ERROR'
            with conn.transaction():
                conn.execute("UPDATE reconstruction.import_runs SET status='failed' WHERE id=%s",(run,))
                conn.execute('INSERT INTO reconstruction.failures(run_id,source_table,error_code) VALUES (%s,%s,%s)',
                             (run,table['table'],code))
            raise RuntimeError('IMPORT_FAILED_'+code) from None
    try:
        with conn.transaction():
            conn.execute('SELECT id FROM reconstruction.import_runs WHERE id=%s FOR UPDATE',(run,))
            projection=project_items(conn,run,loaded)
            conn.execute("UPDATE reconstruction.import_runs SET status='verified',verified_at=CURRENT_TIMESTAMP WHERE id=%s",(run,))
    except Exception:
        with conn.transaction():
            conn.execute("UPDATE reconstruction.import_runs SET status='failed' WHERE id=%s",(run,))
            conn.execute("INSERT INTO reconstruction.failures(run_id,source_table,error_code) VALUES (%s,'content.item_templates','PROJECTION_FAILED')",(run,))
        raise RuntimeError('PROJECTION_FAILED') from None
    return {'run_id':run,'manifest_sha256':fingerprint,'verified_tables':verified,'skipped_verified_tables':skipped,
            'source_rows':sum(len(rows) for _,_,rows in loaded),'canonical_items':projection}


def target_bytes_hash(conn,key):
    rows=conn.execute('''SELECT source_row_number,column_name,source_bytes FROM reconstruction.source_text_bytes
        WHERE run_id=%s AND source_database=%s AND source_schema=%s AND source_table=%s''',key).fetchall()
    return bytes_hash([{'row_number':r[0],'column':r[1],'bytes':r[2]} for r in rows])


def project_items(conn, run, loaded):
    item=next(((table,model,rows) for table,model,rows in loaded if table['table']=='TITEMCHART'),None)
    if not item:return None
    _,model,rows=item
    conn.execute('INSERT INTO content.releases(id,source_database) VALUES (%s,%s) ON CONFLICT DO NOTHING',
                 (run,model['database']))
    expected=sorted((int(r['wItemID']) & 65535,r['szNAME'],r['bType'],r['bKind'],r['bStack'],float(r['fPrice'])) for r in rows)
    existing=conn.execute('SELECT item_id,display_name,item_type,item_kind,max_stack,price FROM content.item_templates WHERE release_id=%s ORDER BY item_id',(run,)).fetchall()
    if not existing:
        conn.execute('''INSERT INTO content.item_templates(release_id,item_id,display_name,item_type,item_kind,max_stack,price)
            SELECT "_import_run_id",("wItemID"::integer & 65535),"szNAME","bType","bKind","bStack","fPrice"
            FROM legacy_game."TITEMCHART" WHERE "_import_run_id"=%s''',(run,))
        existing=conn.execute('SELECT item_id,display_name,item_type,item_kind,max_stack,price FROM content.item_templates WHERE release_id=%s ORDER BY item_id',(run,)).fetchall()
    if expected!=existing:raise RuntimeError('CANONICAL_ITEM_VALUE_MISMATCH')
    return {'rows':len(existing),'six_fields_verified':True}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='command',required=True)
    migrations=sub.add_parser('schema');migrations.add_argument('--directory',type=Path,default=Path('database/postgresql'))
    imp=sub.add_parser('import-reference');imp.add_argument('--manifest',type=Path,required=True)
    imp.add_argument('--mapping',type=Path,default=Path('database/postgresql/mapping.json'))
    p.add_argument('--report',type=Path)
    args=p.parse_args()
    try:
        with connect() as conn:
            result={'applied_migrations':apply_migrations(conn,args.directory)} if args.command=='schema' else import_snapshot(conn,args.manifest,args.mapping)
        if args.report:args.report.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result))
    except Exception as ex:
        code=getattr(ex,'sqlstate',None)
        if not code and isinstance(ex,RuntimeError) and str(ex).replace('_','').isalnum():code=str(ex)
        failure={'error_code':code or 'ERROR_DETAIL_SUPPRESSED'}
        if isinstance(ex,SourceValidationError):
            failure.update(source_table=ex.source_table,source_row_number=ex.source_row_number,stage='source_preflight')
        if args.report:args.report.write_text(json.dumps(failure,indent=2)+'\n')
        print('Database migration failed: '+(code or 'ERROR_DETAIL_SUPPRESSED'),file=sys.stderr)
        sys.exit(1)


if __name__=='__main__':main()
