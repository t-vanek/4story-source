#!/usr/bin/env python3
"""Observe original unordered skill-data reads on an owned restored SQL Server.

This records an observation, not a SQL ordering guarantee. The PostgreSQL port
explicitly stabilizes the recovered clustered-key order. No runtime MS SQL use.
"""
import argparse
import hashlib
import json
from pathlib import Path
from inspect_backup import query,CATALOG
from disposable_environment import verify_container


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--snapshot',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args();state=json.loads((args.work/'state.json').read_text())
    name=state['mssql_container'];verify_container(state,name);database='forensic_TGAME_RAGEZONE'
    indexes=query(name,CATALOG['indexes']+' FOR JSON PATH, INCLUDE_NULL_VALUES',database)
    index=next(i for i in indexes if i['table_name']=='TSKILLDATA' and i['is_primary_key'])
    key=[c['column_name'] for c in index['column_details'] if c['key_ordinal']]
    assert key==['wSkillID','bAction','bType','bAttr','bExec'] and index['type_desc']=='CLUSTERED'
    raw=json.loads((args.snapshot/'TGAME_RAGEZONE.dbo.TSKILLDATA.json').read_text())
    skills={r['wSkillID'] for r in raw}
    ambiguous=[sid for sid in sorted(skills) if any(r['wSkillID']==sid and r['bAttr'] in (1,2) for r in raw) and any(r['wSkillID']==sid and r['bAttr']==3 for r in raw)]
    assert ambiguous==[637,640,814,1343]
    observations=[]
    for sid in ambiguous:
        ordered=sorted((r for r in raw if r['wSkillID']==sid),key=lambda r:tuple(r[k] for k in key))
        expected=[{k:r[k] for k in ('bAction','bType','bAttr','bExec','bInc','wValue','wValueInc','bCalc')} for r in ordered]
        for _ in range(3):
            # Same projection and parameter type as CTBLSkillData. No ORDER BY.
            sql="EXEC sp_executesql N'SELECT bAction,bType,bAttr,bExec,bInc,wValue,wValueInc,bCalc FROM TSKILLDATA WHERE wSkillID=@P1', N'@P1 smallint', @P1="+str(sid)
            actual=[{k:int(v) for k,v in r.items()} for r in query(name,sql,database,tabular=True)]
            assert actual==expected,(sid,'original query differs from source key order')
        early=next(r['bAttr'] for r in ordered if r['bAttr'] in (1,2,3))
        observations.append({'skill':sid,'executions':3,'rows':len(expected),'matches_source_clustered_key':True,'attack_type':1 if early in (1,2) else 3})
    result={'status':'passed','scope':'Offline restored backup source query observation, not ordering guarantee or original-client proof',
            'engine':query(name,"SELECT CONVERT(varchar(100),SERVERPROPERTY('ProductVersion')) version FOR JSON PATH"),
            'source_query':'Server/TMapSvr/DBAccess.h CTBLSkillData; exact projection, parameterized smallint, no ORDER BY',
            'clustered_primary_key':key,'skill_count':len(skills),'order_sensitive_skills':observations,
            'modern_correction':'Explicit ascending recovered clustered-key order, independent of PostgreSQL access plan. Original SQL without ORDER BY is not guaranteed on every engine/plan; observed agreement is bounded to this restored source and engine.',
            'backups':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path('TGAME_RAGEZONE.bak'),Path('TGLOBAL_RAGEZONE.bak'))}}
    args.report.write_text(json.dumps(result,indent=2)+'\n');print('Original skill order: four mixed-attribute skills, twelve parameterized reads match restored clustered-key order.')


if __name__=='__main__':main()
