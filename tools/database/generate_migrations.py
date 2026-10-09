#!/usr/bin/env python3
"""Generate typed historical snapshots and an explicitly proposed content model.

Never translates business procedures or invents data. Output is versioned SQL;
review/regenerate before the first application, never rewrite applied migrations.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path

REFERENCE_TABLES = (
    'TITEMCHART','TCLASSCHART','TRACECHART','TFORMULACHART','TQUESTCHART',
    'TQUESTTERMCHART','TQREWARDCHART','TSKILLCHART','TSKILLDATA','TNPCCHART',
    'TMONSTERCHART','TMONSPAWNCHART','TMAPMONCHART','TMONATTRCHART','TMONITEMCHART',
)


# Additional immutable charts required by TGAME.TCreateChar. Never include
# historical player/account/reservation tables in the reference allowlist.
CHARACTER_EXTRA_GAME_TABLES = ('TQUESTITEMCHART', 'TSTARTITEMCHART', 'TSTARTSKILL',
                              'TSTARTHOTKEY', 'TSTARTRECALL', 'TLEVELCHART')
CHARACTER_REFERENCE_KEYS = frozenset(
    [('TGAME_RAGEZONE', 'dbo', t) for t in (*REFERENCE_TABLES, *CHARACTER_EXTRA_GAME_TABLES)]
    + [('TGLOBAL_RAGEZONE', 'dbo', 'TVETERANCHART')])

ROUTING_REFERENCE_KEYS = frozenset(
    ('TGAME_RAGEZONE', 'dbo', t) for t in ('TMAPCHART','TUNITCHART','TCHANNELCHART','TSPAWNPOSCHART'))

ACTOR_REFERENCE_KEYS = frozenset(
    ('TGAME_RAGEZONE','dbo',t) for t in ('TITEMMAGICCHART','TSKILLPOINTCHART','TITEMATTRCHART','TITEMGRADECHART'))


def qi(name):
    return '"'+name.replace('"','""')+'"'


def legacy_schema(db, schema):
    base='legacy_global' if db=='TGLOBAL_RAGEZONE' else 'legacy_game'
    return base if schema=='dbo' else base+'_'+schema


def pg_type(c):
    t=c['type_name']
    types={'tinyint':'smallint','smallint':'smallint','int':'integer','bigint':'bigint',
           'real':'real','datetime':'timestamp without time zone',
           'smalldatetime':'timestamp without time zone','date':'date','bit':'boolean',
           'image':'bytea','binary':'bytea','varbinary':'bytea',
           'varchar':'text','nvarchar':'text','char':'text','nchar':'text','text':'text','ntext':'text'}
    if t=='float':return 'real' if c['precision']<=24 else 'double precision'
    if t in ('decimal','numeric'):return f"numeric({c['precision']},{c['scale']})"
    if t not in types: raise ValueError(f'Unsupported evidenced type {t}')
    return types[t]


def mssql_type(c):
    t=c['type_name']
    if t in ('varchar','nvarchar','char','nchar','binary','varbinary'):
        size='max' if c['max_length']==-1 else str(c['max_length']//(2 if t.startswith('n') else 1))
        return f'{t}({size})'
    if t in ('numeric','decimal'):return f"{t}({c['precision']},{c['scale']})"
    if t=='float':return f"float({c['precision']})"
    return t


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--metadata',type=Path,default=Path('_rewrite/docs/database-reconstruction/evidence/backup-schema.json'))
    p.add_argument('--output',type=Path,default=Path('database/postgresql'))
    args=p.parse_args();models=json.loads(args.metadata.read_text()); args.output.mkdir(parents=True,exist_ok=True)
    fingerprint=hashlib.sha256(args.metadata.read_bytes()).hexdigest()
    header=f'-- Generated from restored metadata SHA256 {fingerprint}.\n-- Historical snapshot layer; not a drop-in game-server schema.\n'
    one='''-- Migration 001: import control plane (PROPOSED).
CREATE SCHEMA reconstruction;
CREATE TABLE reconstruction.import_runs (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    manifest_sha256 text NOT NULL UNIQUE CHECK (manifest_sha256 ~ '^[0-9a-f]{64}$'),
    source_manifest jsonb NOT NULL,
    status text NOT NULL CHECK (status IN ('running','verified','failed')),
    started_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    verified_at timestamptz
);
CREATE TABLE reconstruction.checkpoints (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_database text NOT NULL, source_schema text NOT NULL, source_table text NOT NULL,
    row_count bigint NOT NULL CHECK (row_count >= 0),
    source_sha256 text NOT NULL, target_sha256 text NOT NULL,
    completed_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (run_id, source_database, source_schema, source_table),
    CHECK (source_sha256 = target_sha256)
);
CREATE TABLE reconstruction.failures (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_table text NOT NULL, error_code text NOT NULL,
    source_row_number bigint, -- no row payload, login, IP or credentials
    occurred_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
'''
    (args.output/'001-import-control.sql').write_text(one)
    schemas=sorted({legacy_schema(db,c['schema_name']) for db,m in models.items() for c in m['columns']})
    ddl=[header]+['CREATE SCHEMA '+qi(s)+';' for s in schemas]
    mapping=[]
    for db,model in models.items():
        groups=defaultdict(list)
        for c in model['columns']:groups[(c['schema_name'],c['table_name'])].append(c)
        mssql=['-- CONFIRMED table metadata from this backup; no module behavior or rows.\n-- Apply ONLY to a new empty disposable database, never an existing database.']
        for schema in sorted({s for s,t in groups}):
            if schema!='dbo':mssql.append(f'CREATE SCHEMA [{schema}];\nGO')
        for (schema,table),cols in sorted(groups.items()):
            cols.sort(key=lambda c:c['ordinal']); pg_schema=legacy_schema(db,schema)
            entries=['    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id)',
                     '    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0)']
            original=[]
            for c in cols:
                entry='    '+qi(c['column_name'])+' '+pg_type(c)+(' NOT NULL' if not c['is_nullable'] else '')
                if c['type_name']=='tinyint':entry+=' CHECK ('+qi(c['column_name'])+' BETWEEN 0 AND 255)'
                entries.append(entry)
                raw='    ['+c['column_name'].replace(']',']]')+'] '+mssql_type(c)
                if c['is_identity']:raw+=f" IDENTITY({c['identity_seed']},{c['identity_increment']})"
                if c['collation_name'] and c['type_name'] in ('varchar','nvarchar','char','nchar','text','ntext'):
                    raw+=' COLLATE '+c['collation_name']
                raw+=(' NULL' if c['is_nullable'] else ' NOT NULL')
                if c['default_definition']:raw+=' DEFAULT '+c['default_definition']
                if c['is_computed']:raise ValueError('Computed column needs a separate reconstruction rule')
                original.append(raw)
            pk=next((i for i in model['indexes'] if i['table_name']==table and i['schema_name']==schema and i['is_primary_key']),None)
            entries.append('    PRIMARY KEY ("_import_run_id", "_source_row_number")')
            if pk:
                keys=[c['column_name'] for c in pk['column_details'] if not c['is_included']]
                entries.append('    UNIQUE ("_import_run_id", '+', '.join(qi(k) for k in keys)+')')
                original.append('    PRIMARY KEY ('+', '.join('['+k.replace(']',']]')+']' for k in keys)+')')
            ddl.append('CREATE TABLE '+qi(pg_schema)+'.'+qi(table)+' (\n'+',\n'.join(entries)+'\n);')
            mssql.append('CREATE TABLE ['+schema+'].['+table+'] (\n'+',\n'.join(original)+'\n);\nGO')
            mapping.append({'database':db,'schema':schema,'table':table,'target_schema':pg_schema,
                'classification':'CONFIRMED columns; PROPOSED import provenance and PG types',
                'column_mapping':[{'source':c['column_name'],'target':c['column_name'],'mssql_type':mssql_type(c),
                    'postgresql_type':pg_type(c),'nullable':bool(c['is_nullable']),
                    'source_is_identity':bool(c['is_identity'])} for c in cols],
                'original_pk': [c['column_name'] for c in pk['column_details']] if pk else [],
                'reference_import_allowed': (db, schema, table) in (CHARACTER_REFERENCE_KEYS | ROUTING_REFERENCE_KEYS | ACTOR_REFERENCE_KEYS)})
        legacy_dir=args.output.parent/'legacy-reconstructed';legacy_dir.mkdir(exist_ok=True)
        (legacy_dir/(db+'.tables.sql')).write_text('\n\n'.join(mssql)+'\n')
    (args.output/'002-legacy-snapshots.sql').write_text('\n\n'.join(ddl)+'\n')
    three='''-- Migration 003: canonical content projection (PROPOSED, evidence-linked).
-- Full source columns remain in immutable versioned legacy snapshots.
CREATE SCHEMA content;
CREATE TABLE content.releases (
    id bigint PRIMARY KEY REFERENCES reconstruction.import_runs(id),
    game_version text, -- UNKNOWN; backup date is not a game version
    server_protocol integer CHECK (server_protocol BETWEEN 0 AND 65535),
    source_database text NOT NULL
);
CREATE TABLE content.item_templates (
    release_id bigint NOT NULL REFERENCES content.releases(id),
    item_id integer NOT NULL CHECK (item_id BETWEEN 0 AND 65535),
    display_name text NOT NULL,
    item_type smallint NOT NULL CHECK (item_type BETWEEN 0 AND 255),
    item_kind smallint NOT NULL CHECK (item_kind BETWEEN 0 AND 255),
    max_stack smallint NOT NULL CHECK (max_stack BETWEEN 0 AND 255),
    price double precision NOT NULL, -- source is FLOAT, not an invented money unit
    PRIMARY KEY (release_id, item_id)
);
COMMENT ON TABLE content.item_templates IS
  'PROPOSED canonical projection of TGAME_RAGEZONE.dbo.TITEMCHART; source wItemID is a protocol WORD';
'''
    (args.output/'003-canonical-content.sql').write_text(three)
    (args.output/'mapping.json').write_text(json.dumps({'metadata_sha256':fingerprint,'tables':mapping},indent=2)+'\n')
    print(f'Generated {len(mapping)} typed historical snapshot tables and canonical item projection.')


if __name__=='__main__':main()
