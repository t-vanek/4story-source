#!/usr/bin/env python3
"""Export an explicit gameplay-reference allowlist from disposable restored DBs.

No accounts, session, IP, mail or credential tables. Writes mode-0600 JSON outside
the repository. Float style 3 preserves SQL Server numeric round-trip precision.
"""
import argparse
import json
import os
import subprocess
from pathlib import Path
from generate_migrations import qi, REFERENCE_TABLES, CHARACTER_REFERENCE_KEYS, ROUTING_REFERENCE_KEYS, ACTOR_REFERENCE_KEYS
from inspect_backup import query
from migrate import digest,table_hash,bytes_hash


def si(name):return '['+name.replace(']',']]')+']'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--container',required=True)
    p.add_argument('--profile',choices=('map','characters','routing','actor'),default='map')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--mapping',type=Path,default=Path('database/postgresql/mapping.json'))
    p.add_argument('--backup-metadata',type=Path,default=Path('_rewrite/docs/database-reconstruction/evidence/artifact-inventory.json'))
    args=p.parse_args();output=args.output.resolve();repo=Path(__file__).resolve().parents[2]
    if output.is_relative_to(repo):raise RuntimeError('Extracted rows must stay outside the repository')
    output.mkdir(mode=0o700,parents=True,exist_ok=True);os.chmod(output,0o700)
    if (output/'manifest.json').exists():raise RuntimeError('Refusing to overwrite an existing snapshot; use a fresh directory')
    mapping=json.loads(args.mapping.read_text());artifacts=json.loads(args.backup_metadata.read_text())['artifacts']
    backups=[{k:a[k] for k in ('path','size_bytes','sha256')} for a in artifacts if a['path'] in ('TGAME_RAGEZONE.bak','TGLOBAL_RAGEZONE.bak')]
    if len(backups)!=2:raise RuntimeError('Expected both pinned backup fingerprints')
    for backup in backups:
        result=subprocess.check_output(['podman','exec',args.container,'sha256sum',
            '/var/opt/mssql/'+backup['path']],text=True).split()[0]
        if result!=backup['sha256']:raise RuntimeError('CONTAINER_BACKUP_FINGERPRINT_MISMATCH')
    manifest={'metadata_sha256':mapping['metadata_sha256'],'backups':backups,'tables':[]}
    for table in mapping['tables']:
        if not table['reference_import_allowed']:continue
        if args.profile == 'map' and (table['database'] != 'TGAME_RAGEZONE' or table['table'] not in REFERENCE_TABLES):continue
        key=(table['database'],table['schema'],table['table'])
        if args.profile == 'characters' and key not in CHARACTER_REFERENCE_KEYS:continue
        if args.profile == 'routing' and key not in ROUTING_REFERENCE_KEYS:continue
        if args.profile == 'actor' and key not in ACTOR_REFERENCE_KEYS:continue
        projections=[]
        text_columns=[]
        for col in table['column_mapping']:
            name=si(col['source'])
            projections.append(f'CONVERT(varchar(64),{name},3) AS {name}' if col['mssql_type'].startswith(('float','real')) else name)
            if col['postgresql_type']=='text':
                alias='__reconstruction_bytes_'+str(len(text_columns))
                projections.append(f'CONVERT(varbinary(max),{name}) AS {si(alias)}')
                text_columns.append((col['source'],alias))
        ordering=table['original_pk'] or [c['source'] for c in table['column_mapping']]
        statement='SELECT\n'+',\n'.join(projections)+'\nFROM '+si(table['schema'])+'.'+si(table['table'])+\
            '\nORDER BY '+','.join(si(c) for c in ordering)+' FOR JSON PATH, INCLUDE_NULL_VALUES'
        rows=query(args.container,statement,'forensic_'+table['database'])
        byte_records=[]
        for i,row in enumerate(rows,1):
            for name,alias in text_columns:
                byte_records.append({'row_number':i,'column':name,'bytes':row.pop(alias)})
        filename=table['database']+'.'+table['schema']+'.'+table['table']+'.json'
        data=(json.dumps(rows,ensure_ascii=False,separators=(',',':'))+'\n').encode()
        path=output/filename
        with path.open('xb') as f:os.chmod(path,0o600);f.write(data)
        entry={k:table[k] for k in ('database','schema','table')}
        entry.update(file=filename,row_count=len(rows),file_sha256=digest(data),value_sha256=table_hash(rows,table['column_mapping']))
        byte_filename=filename.replace('.json','.text-bytes.json')
        byte_data=(json.dumps(byte_records,separators=(',',':'))+'\n').encode()
        byte_path=output/byte_filename
        with byte_path.open('xb') as f:os.chmod(byte_path,0o600);f.write(byte_data)
        entry.update(text_bytes_file=byte_filename,text_bytes_file_sha256=digest(byte_data),
                     text_bytes_value_sha256=bytes_hash(byte_records))
        manifest['tables'].append(entry)
        print(f"{table['table']}: exported {len(rows)} reference rows")
    path=output/'manifest.json';path.write_text(json.dumps(manifest,indent=2)+'\n');os.chmod(path,0o600)


if __name__=='__main__':main()
