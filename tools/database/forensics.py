#!/usr/bin/env python3
"""Read-only repository inventory and conservative SQL/source cross-reference.

This is a lexical index, not a SQL/C++ compiler. Dynamic identifiers, generated
SQL and preprocessor reachability need review; counts are never runtime coverage.
No literal SQL, connection strings or data rows are emitted into reports.
"""
import argparse
import csv
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

SKIP = {'.git', '.idea', '.vs', 'vcpkg_installed', '__pycache__', 'node_modules'}
EXTENSIONS = {'.sql','.bak','.bacpac','.dacpac','.mdf','.ldf','.csv','.xml','.json','.md',
              '.zip','.7z','.rar','.tar','.gz','.bz2','.xz','.toml','.ini','.config','.yaml','.yml'}
SQL_TOKEN = re.compile(r"--[^\n]*|/\*[\s\S]*?\*/|N?'(?:''|[^'])*'|\[(?:\]\]|[^\]])*\]|\"(?:\"\"|[^\"])*\"|\w+|[^\s]", re.I)
IDENT = r'(?:\[[^\]]+\]|"[^"]+"|[A-Za-z_][\w$#]*)(?:\s*\.\s*(?:\[[^\]]+\]|"[^"]+"|[A-Za-z_][\w$#]*))*'
OBJ_RE = re.compile(r'\b(DELETE(?:\s+FROM)?|INSERT(?:\s+INTO)?|MERGE(?:\s+INTO)?|FROM|JOIN|UPDATE|EXEC(?:UTE)?(?:\s+@\w+\s*=)?|CALL)\s+('+IDENT+r')',re.I)
DDL_RE = re.compile(r'\b(CREATE(?:\s+OR\s+(?:ALTER|REPLACE))?|ALTER)\s+(PROC(?:EDURE)?|VIEW|TRIGGER|FUNCTION|TABLE|(?:UNIQUE\s+)?INDEX|SEQUENCE)\s+(?:IF\s+NOT\s+EXISTS\s+)?('+IDENT+r')',re.I)
CPP_TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|(?:u8|u|U|L)?R"(?P<delim>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delim)"|(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_text(path):
    data=path.read_bytes()
    if data.startswith((b'\xff\xfe',b'\xfe\xff')):
        return data.decode('utf-16')
    try:
        return data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return data.decode('cp1252',errors='replace')


def norm(sql):
    tokens=[]
    for match in SQL_TOKEN.finditer(sql):
        token=match.group()
        if token.startswith(('--','/*')):
            continue
        line=sql[sql.rfind('\n',0,match.start())+1:sql.find('\n',match.end()) if '\n' in sql[match.end():] else len(sql)]
        if token.lower()=='go' and line.strip().lower()=='go':
            continue
        if token.startswith("'") or token[:2].lower()=="n'":
            tokens.append(token)
        elif token.startswith(('[','"')):
            tokens.append(token[1:-1].lower())
        else:
            tokens.append(token.lower())
    return ' '.join(tokens)


def clean_ident(name):
    return re.sub(r'[\[\]"\s]','',name)


def sql_without_comments(sql):
    masked=list(sql)
    for match in SQL_TOKEN.finditer(sql):
        if match.group().startswith(('--','/*')):
            masked[match.start():match.end()]=' '*len(match.group())
    return ''.join(masked)


def definitions(sql):
    # Header offsets retain original SQL token boundaries and string contents.
    masked=list(sql)
    for m in SQL_TOKEN.finditer(sql):
        if m.group().startswith(('--','/*',"'")) or m.group()[:2].lower()=="n'":
            masked[m.start():m.end()]=' '*len(m.group())
    matches=list(DDL_RE.finditer(''.join(masked)))
    out=[]
    for i,m in enumerate(matches):
        end=matches[i+1].start() if i+1<len(matches) else len(sql)
        kind=m.group(2).upper().replace('PROCEDURE','PROC')
        name=clean_ident(m.group(3))
        body=sql[m.start():end]
        canonical=norm(re.sub(r'\bGO\s*$', '', body.strip(), flags=re.I))
        out.append({'kind':kind,'name':name,'line':sql.count('\n',0,m.start())+1,
                    'normalized_sha256':sha(canonical.encode()),'body':body})
    return out


def cpp_literals(text):
    result=[]
    end=-1
    for m in CPP_TOKEN.finditer(text):
        token=m.group()
        if token.startswith(('//','/*',"'")):
            continue
        if 'R"' in token[:4]:
            value=token.split('(',1)[1].rsplit(')',1)[0]
        else:
            value=token[token.index('"')+1:-1]
            value=re.sub(r'\\\r?\n','',value)
            value=re.sub(r'\\([\\"nrt])',lambda x:{'n':'\n','r':'\r','t':'\t'}.get(x[1],x[1]),value)
        if result and not text[end:m.start()].strip():
            result[-1]['value']+=value
        else:
            result.append({'value':value,'line':text.count('\n',0,m.start())+1,'start':m.start()})
        end=m.end()
    return result


def dump(path, data):
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')


def run(root, output, private):
    output.mkdir(parents=True,exist_ok=True)
    inventory=[]; excluded=Counter(); paths=[]
    for p in sorted(root.rglob('*')):
        if not p.is_file(): continue
        rel=p.relative_to(root)
        if any(part in SKIP or part.startswith(('build','cmake-build')) for part in rel.parts[:-1]) or p.is_relative_to(output.parent):
            excluded[rel.parts[0]]+=1; continue
        paths.append(p)
        if p.suffix.lower() not in EXTENSIONS: continue
        data=p.read_bytes()
        fmt=p.suffix.lower()[1:]
        if data.startswith(b'version https://git-lfs.github.com/spec/v1'):
            fmt='git-lfs-pointer (payload unavailable)'
        elif data.startswith(b'TAPE'):
            fmt='Microsoft Tape Format; SQL Server backup candidate'
        elif data.startswith(b'PK\x03\x04'):
            fmt='ZIP container'
        elif p.suffix.lower()=='.csv':
            fmt='TSV' if '\t' in next(iter(read_text(p).splitlines()),'') else 'CSV'
        elif str(p).endswith(('.cpp.bak','.h.bak')):
            fmt='C++ source-edit text backup'
        purpose=('legacy schema export' if str(rel).startswith('_rewrite/docs/schema') else
                 'development/migration SQL' if p.suffix.lower()=='.sql' else
                 'C++ source-edit backup; not a database backup' if str(p).endswith(('.cpp.bak','.h.bak')) else
                 'database backup candidate' if p.suffix.lower() in {'.bak','.mdf','.ldf','.bacpac','.dacpac'} else
                 'documentation' if p.suffix.lower()=='.md' else 'configuration/data (not assumed database evidence)')
        inventory.append({'path':str(rel),'size_bytes':len(data),'format':fmt,'sha256':sha(data),'suspected_purpose':purpose})
    dump(output/'artifact-inventory.json',{'root':str(root),'excluded_derived_or_internal_files':dict(excluded),'artifacts':inventory})
    exports={}; sql_objects=[]; source=[]; procedures=[]
    for folder in ('schema','schema.old-dump-2019'):
        export={}
        for db in ('TGLOBAL_RAGEZONE','TGAME_RAGEZONE'):
            tables=defaultdict(list)
            for r in csv.DictReader(read_text(root/f'_rewrite/docs/{folder}/{db}.tables.csv').splitlines(),delimiter='\t'):
                tables[r['table_name']].append(r)
            indexes=list(csv.DictReader(read_text(root/f'_rewrite/docs/{folder}/{db}.indexes.csv').splitlines(),delimiter='\t'))
            fks=list(csv.DictReader(read_text(root/f'_rewrite/docs/{folder}/{db}.fks.csv').splitlines(),delimiter='\t'))
            export[db]={'tables':dict(tables),'indexes':indexes,'foreign_keys':fks}
        exports[folder]=export
    for p in paths:
        if p.suffix.lower()!='.sql': continue
        rel=str(p.relative_to(root)); text=read_text(p)
        for obj in definitions(text):
            body=obj.pop('body'); obj['path']=rel
            obj['database']=next((db for db in ('TGLOBAL_RAGEZONE','TGAME_RAGEZONE') if db in rel),None)
            refs=[{'operation':m[1].upper(),'object':clean_ident(m[2])} for m in OBJ_RE.finditer(sql_without_comments(body))]
            obj['dependencies']=refs
            # Lexical flags are candidates; no transaction guarantee inferred.
            obj['flags']={flag:bool(re.search(pattern,sql_without_comments(body),re.I)) for flag,pattern in {
                'transaction':r'\bBEGIN\s+TRAN', 'write':r'\b(INSERT|UPDATE|DELETE|MERGE)\b',
                'dynamic_sql':r'\b(sp_executesql|EXEC\s*\()', 'cross_database':r'\b\w+\s*\.\s*dbo\s*\.',
                'maintenance':r'\b(TRUNCATE|DBCC|BACKUP|RESTORE)\b'}.items()}
            sql_objects.append(obj)
            if obj['kind'] in ('PROC','FUNCTION'): procedures.append(obj)
    entity_tables=defaultdict(set)
    dynamic_sites=[]
    for p in paths:
        if p.suffix.lower() not in {'.cpp','.h','.hpp'}:continue
        text=read_text(p)
        for m in re.finditer(r'EntityMapping\s*<\s*([\w:]+)\s*>[\s\S]*?\bTable\s*=\s*"(\w+)"',text):
            entity_tables[m[1].split('::')[-1]].add(m[2])
    for p in paths:
        if p.suffix.lower() not in {'.cpp','.h','.hpp','.c','.cs','.ps1','.py'}: continue
        rel=str(p.relative_to(root))
        if rel.startswith('tools/database/'): continue
        text=read_text(p)
        if p.suffix.lower() not in {'.cpp','.h','.hpp','.c'}: continue
        implementation=('archived_reference' if '/legacy_src/' in rel else
                        'portable' if 'Asio/' in rel else 'legacy' if rel.startswith(('Server/','Client/')) else 'shared')
        for m in re.finditer(r'\.Set\s*<\s*([\w:]+)\s*>\s*\(\s*\)',text):
            matches=entity_tables.get(m[1].split('::')[-1],set())
            for table in matches:
                source.append({'path':rel,'line':text.count('\n',0,m.start())+1,'object':table,
                    'operation':'ORM_REPOSITORY','entity_type':m[1],'evidence':'ENTITY_MAPPING_AND_SET_CALL',
                    'scope':'test' if '/tests/' in rel else 'source','implementation':implementation})
        for m in re.finditer(r'\b(?:BuildSql|SpCall|ExecNow)\s*\(|\b(?:m_table|TableName)\b',text):
            dynamic_sites.append({'path':rel,'line':text.count('\n',0,m.start())+1,
                                  'kind':'SQL_GENERATOR_REVIEW_SITE','implementation':implementation})
        literals=cpp_literals(text)
        for lit in literals:
            value=lit['value']
            sql_candidate=bool(re.match(r'\s*(?:SELECT|INSERT|UPDATE|DELETE|MERGE|EXEC|CALL|WITH|DECLARE|FROM|JOIN|\{)',value,re.I))
            for m in OBJ_RE.finditer(sql_without_comments(value)):
                obj=clean_ident(m[2]); op=m[1].upper()
                if not sql_candidate:
                    continue
                if not re.match(r'^(?:(?:\w+\.)*(?:T[A-Z_a-z]|CSP|OPTool|IPBLACKLIST|USERIPLOG|ITEMLOG|dtproperties|releaseDate|charkilling)|INFORMATION_SCHEMA\.|sys\.)',obj):
                    continue
                if obj.lower() in {'sys','information_schema'} or obj.startswith(('@','#')): continue
                source.append({'path':rel,'line':lit['line'],'object':obj,'operation':op,
                    'evidence':'SQL_LITERAL','scope':'test' if '/tests/' in rel else 'source',
                    'implementation':implementation})
            before=text[max(0,lit['start']-90):lit['start']]
            if re.search(r'(?:SpCall|\.Exec)\s*\(\s*$',before) and re.fullmatch(r'[\w.]+',value):
                source.append({'path':rel,'line':lit['line'],'object':value,'operation':'EXEC',
                    'evidence':'SP_BUILDER','scope':'test' if '/tests/' in rel else 'source','implementation':implementation})
            if re.search(r'\bTable\s*=\s*$',before) and re.fullmatch(r'\w+',value):
                source.append({'path':rel,'line':lit['line'],'object':value,'operation':'ORM_MAPPING',
                    'evidence':'ENTITY_MAPPING','scope':'source','implementation':implementation})
        # Schema validator names and requirements; no values are retained.
        for m in re.finditer(r'TableHasColumns\s*\([^;]*?"(\w+)"\s*,\s*\{([^}]+)\}',text,re.S):
            source.append({'path':rel,'line':text.count('\n',0,m.start())+1,'object':m[1],
                'operation':'VALIDATE','columns':re.findall(r'"(\w+)"',m[2]),'evidence':'SCHEMA_VALIDATOR',
                'scope':'source','implementation':'portable'})
        if 'schema_validator' in rel:
            by_table=defaultdict(list)
            for call in re.finditer(r'\bCheckColumns\s*\([\s\S]*?\}\s*\)',text):
                for m in re.finditer(r'\{\s*"(\w+)"\s*,\s*"(\w+)"\s*\}',call.group()):
                    by_table[m[1]].append((m[2],text.count('\n',0,call.start()+m.start())+1))
            for table,cols in by_table.items():
                source.append({'path':rel,'line':cols[0][1],'object':table,'operation':'VALIDATE',
                    'columns':list(dict.fromkeys(c[0] for c in cols)),'evidence':'CHECK_COLUMNS_PAIRS',
                    'scope':'source','implementation':'portable'})
    # A name match is evidence of availability, not proof of schema compatibility.
    known=defaultdict(list)
    for folder,dbs in exports.items():
        for db,exp in dbs.items():
            for table in exp['tables']:
                known[table.lower()].append(f'_rewrite/docs/{folder}/{db}.tables.csv')
    for obj in sql_objects:
        known[obj['name'].split('.')[-1].lower()].append(obj['path'])
    for row in source:
        row['available_artifacts']=sorted(set(known[row['object'].split('.')[-1].lower()]))
        row['availability']='NAME_MATCH_ONLY' if row['available_artifacts'] else 'MISSING_OR_DYNAMIC'
    source=list({json.dumps(r,sort_keys=True):r for r in source}.values())
    dump(output/'fragment-objects.json',sql_objects)
    dump(output/'dynamic-sql-review-sites.json',dynamic_sites)
    dump(output/'export-schema.json',exports)
    dump(output/'source-object-mapping.json',source)
    callers=defaultdict(list)
    for row in source:
        if row['operation'].startswith(('EXEC','CALL')):
            callers[row['object'].split('.')[-1].lower()].append({k:row[k] for k in ('path','line','implementation','scope')})
    for obj in procedures:
        obj['callers']=callers.get(obj['name'].split('.')[-1].lower(),[])
        name=obj['name'].lower()
        if any(x in name for x in ('clearall','test','snapshot','statistic','optool','batch','jobs')):
            obj['replacement']='reviewed administrative/background job; no automatic activation'
        elif any(x in name for x in ('login','passwd','account','protected','agreement')):
            obj['replacement']='application authentication/domain service + persistence transaction'
        else:
            obj['replacement']='application domain/persistence transaction; constraints for invariants'
        obj['migration_status']='UNPORTED_OR_PARTIAL; behavior and callers require review'
    dump(output/'procedure-modernization.json',procedures)
    if private:
        publish_backups(root,output,private,exports,sql_objects,source)
    print(json.dumps({'artifacts':len(inventory),'SQL_definitions':len(sql_objects),'source_edges':len(source),
                      'procedure_definitions':len(procedures),'missing_source_names':len({r['object'] for r in source if not r['available_artifacts']})}))


def publish_backups(root,output,private,exports,sql_objects,source):
    report=json.loads((private/'backup-report.json').read_text())
    dump(output/'backup-metadata.json',report)
    comparisons=[]; models={}
    for db in ('TGLOBAL_RAGEZONE','TGAME_RAGEZONE'):
        catalog=json.loads((private/(db+'.catalog.json')).read_text())
        # Module source can contain secrets: publish only hashes and dependencies.
        for m in catalog['modules']:
            definition=m.pop('definition')
            m['definition_available']=definition is not None
            m['normalized_sha256']=sha(norm(definition or '').encode())
        models[db]=catalog
        table_names=sorted({r['table_name'] for r in catalog['columns']})
        for folder,dbs in exports.items():
            exp=dbs[db]
            for table in sorted(set(table_names)|set(exp['tables'])):
                backup=[c for c in catalog['columns'] if c['table_name']==table]
                fragment=exp['tables'].get(table,[])
                differences=[]
                fields=['ordinal','column_name','type_name','max_length','precision','scale','is_nullable','is_identity','is_pk','default_definition']
                def cv(rows):
                    return [{f: norm(str(r.get(f) or '')) if f=='default_definition' else str(r.get(f) if r.get(f) is not None else '') for f in fields} for r in rows]
                if not backup: status='ONLY_IN_FRAGMENTS'
                elif not fragment: status='ONLY_IN_BACKUP'
                elif cv(backup)==cv(fragment): status='IDENTICAL'
                else:
                    status='CONFLICTING'
                    bc={c['column_name']:c for c in cv(backup)}; fc={c['column_name']:c for c in cv(fragment)}
                    for col in sorted(set(bc)|set(fc)):
                        if bc.get(col)!=fc.get(col): differences.append({'column':col,'backup':bc.get(col),'fragment':fc.get(col)})
                comparisons.append({'database':db,'kind':'TABLE_COLUMNS','object':table,'fragment':folder,'status':status,'differences':differences,
                                    'limitation':'CSV omits schema, collation, computed expressions, identity seed and increment'})
            fi=defaultdict(list)
            for idx in exp['indexes']: fi[idx['table_name']].append(idx)
            bi=defaultdict(list)
            for idx in catalog['indexes']: bi[idx['table_name']].append(idx)
            for table in sorted(set(fi)|set(bi)):
                b=sorted((i['type_desc'],str(i['is_unique']),str(i['is_primary_key']),','.join(c['column_name'] for c in i['column_details'] if not c['is_included'])) for i in bi[table])
                f=sorted((i['type_desc'],i['is_unique'],i['is_primary_key'],i['columns']) for i in fi[table])
                comparisons.append({'database':db,'kind':'INDEX_SET','object':table,'fragment':folder,
                    'status':'COMPATIBLE' if b==f else 'MODIFIED','backup_projection':b,'fragment_projection':f,
                    'limitation':'CSV omits direction, included columns, filters, disabled state; names ignored'})
            def fkproj(rows):
                return sorted(tuple(r[k] for k in ('parent_table','parent_column','referenced_table','referenced_column','delete_action','update_action')) for r in rows)
            comparisons.append({'database':db,'kind':'FK_SET','object':'database foreign keys','fragment':folder,
                'status':'COMPATIBLE' if fkproj(catalog['foreign_keys'])==fkproj(exp['foreign_keys']) else 'CONFLICTING',
                'limitation':'Export lacks FK trust and disabled flags, composite ordinal'})
            fragments={o['name'].split('.')[-1]:o for o in sql_objects if o['database']==db and o['path'].startswith(f'_rewrite/docs/{folder}/')}
            modules={m['object_name']:m for m in catalog['modules']}
            for name in sorted(set(fragments)|set(modules)):
                b=modules.get(name);f=fragments.get(name)
                status=('ONLY_IN_FRAGMENTS' if not b else 'ONLY_IN_BACKUP' if not f else
                        'IDENTICAL' if b['normalized_sha256']==f['normalized_sha256'] else 'MODIFIED')
                # Determine whether differences consist only of CRLF/LF in SQL
                # string literals. Report COMPATIBLE, never silently IDENTICAL.
                if b and f and status=='MODIFIED':
                    raw=json.loads((private/(db+'.catalog.json')).read_text())
                    bm=next(m for m in raw['modules'] if m['object_name']==name)
                    path=root/f['path']
                    fd=next(o for o in definitions(read_text(path)) if o['name'].split('.')[-1]==name)
                    if norm((bm['definition'] or '').replace('\r\n','\n'))==norm(fd['body'].replace('\r\n','\n')):
                        status='COMPATIBLE'
                comparisons.append({'database':db,'kind':'MODULE','object':name,'fragment':folder,'status':status,
                    'backup_sha256':b['normalized_sha256'] if b else None,'fragment_sha256':f['normalized_sha256'] if f else None,
                    'fragment_path':f['path'] if f else None,'limitation':'Lexical normalization preserves strings; modified does not imply behavioral equivalence'})
    dump(output/'backup-schema.json',models)
    dump(output/'schema-comparison.json',comparisons)
    for row in source:
        name=row['object'].split('.')[-1].lower()
        matches=[]
        for db,model in models.items():
            tables=[c for c in model['columns'] if c['table_name'].lower()==name]
            modules=[m for m in model['modules'] if m['object_name'].lower()==name]
            if tables or modules:
                match={'database':db,'kind':'TABLE' if tables else 'MODULE'}
                if row['operation']=='VALIDATE' and tables:
                    present={c['column_name'].lower() for c in tables}
                    match['missing_required_columns']=[c for c in row['columns'] if c.lower() not in present]
                matches.append(match)
        row['backup_matches']=matches
    dump(output/'source-object-mapping.json',source)
    modernization=[]
    for db,model in models.items():
        original=json.loads((private/(db+'.catalog.json')).read_text())
        for module in original['modules']:
            if module['type'].strip() not in ('P','FN','IF','TF'):continue
            name=module['object_name'];body=sql_without_comments(module['definition'] or '')
            calls=[{k:r[k] for k in ('path','line','implementation','scope')} for r in source
                   if r['object'].split('.')[-1].lower()==name.lower() and r['operation'].startswith(('EXEC','CALL'))]
            deps=[d for d in model['dependencies'] if d['referencing_object']==name]
            params=[p for p in model['parameters'] if p['object_name']==name]
            writes=sorted({clean_ident(m[2]) for m in OBJ_RE.finditer(body) if m[1].upper().startswith(('UPDATE','INSERT','DELETE','MERGE'))})
            role=('authentication/domain + persistence' if any(s in name.lower() for s in ('login','passwd','account','protected','agreement')) else
                  'reviewed admin/background worker' if any(s in name.lower() for s in ('clearall','test','snapshot','statistic','optool','batch','jobs')) else
                  'application domain + persistence transaction')
            modernization.append({'database':db,'schema':module['schema_name'],'procedure':name,
                'classification':'CONFIRMED original definition; PROPOSED replacement responsibility',
                'definition_sha256':next(m['normalized_sha256'] for m in model['modules'] if m['object_name']==name),
                'parameters':params,'callers':calls,'dependencies':deps,'lexical_write_targets':writes,
                'explicit_begin_transaction':bool(re.search(r'\bBEGIN\s+TRAN',body,re.I)),
                'has_return':bool(re.search(r'\bRETURN\b',body,re.I)),
                'replacement':role,'migration_status':'NOT_PORTED_TO_POSTGRESQL',
                'review_requirement':'Preserve branches, return/output contracts, concurrency and rollback behavior; lexical flags are not behavioral proof'})
    dump(output/'backup-procedure-modernization.json',modernization)
    print('comparison:',dict(Counter(r['status'] for r in comparisons)))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('.'))
    parser.add_argument('--output',type=Path,default=Path('_rewrite/docs/database-reconstruction/evidence'))
    parser.add_argument('--private-backup-metadata',type=Path)
    args=parser.parse_args()
    run(args.root.resolve(),args.output.resolve(),args.private_backup_metadata)
