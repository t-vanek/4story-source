#!/usr/bin/env python3
"""Publish aggregate-only profiles from the fixed disposable restored databases."""
import argparse
import json
from pathlib import Path
from inspect_backup import query


def ident(name):
    return '['+name.replace(']',']]')+']'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--container',required=True)
    p.add_argument('--private-metadata',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args(); out={}
    for db in ('TGLOBAL_RAGEZONE','TGAME_RAGEZONE'):
        catalog=json.loads((args.private_metadata/(db+'.catalog.json')).read_text())
        target='forensic_'+db; rows=[]; profiles=[]
        for rc in catalog['row_counts']:
            table=ident(rc['schema_name'])+'.'+ident(rc['table_name'])
            count=query(args.container,f'SELECT COUNT_BIG(*) exact_count FROM {table} FOR JSON PATH',target)[0]['exact_count']
            rows.append(dict(rc,exact_count=count,partition_count_matches=count==rc['row_count']))
        # Numeric ranges of account/player identifiers can disclose individual
        # records in sparse snapshots. Publish ranges only for static catalogs.
        for table in ('TITEMCHART','TQUESTCHART','TNPCCHART','TMONSTERCHART','TSKILLCHART'):
            cols=[c for c in catalog['columns'] if c['table_name']==table]
            if not cols: continue
            numeric=[c for c in cols if c['type_name'] in ('tinyint','smallint','int','bigint','real','float')]
            projections=[]
            for i,c in enumerate(numeric):
                n=ident(c['column_name'])
                projections.append(f'MIN({n}) [min_{i}],MAX({n}) [max_{i}],\n'
                    f'SUM(CONVERT(bigint,CASE WHEN {n}<0 THEN 1 ELSE 0 END)) [negative_{i}]')
            vals=query(args.container,'SELECT\n'+',\n'.join(projections)+
                f"\nFROM {ident(cols[0]['schema_name'])}.{ident(table)} FOR JSON PATH, INCLUDE_NULL_VALUES",target)[0]
            profiles.append({'table':table,'columns':[{'column':c['column_name'],'min':vals[f'min_{i}'],
                'max':vals[f'max_{i}'],'negative_count':vals[f'negative_{i}']} for i,c in enumerate(numeric)]})
        out[db]={'exact_row_counts':rows,'numeric_profiles':profiles,
            'codepage':query(args.container,"SELECT COLLATIONPROPERTY(CONVERT(varchar(100),DATABASEPROPERTYEX(DB_NAME(),'Collation')),'CodePage') codepage FOR JSON PATH",target)}
        print(f'{db}: recounted {len(rows)} tables; mismatches '+str(sum(not r['partition_count_matches'] for r in rows)))
    args.output.write_text(json.dumps(out,indent=2)+'\n')


if __name__=='__main__':
    main()
