#!/usr/bin/env python3
"""Phase B only: publish original World battle/time charts and explicit calendar.

Multiple mission rows require a private recorded source-order profile, never a
guessed sorted timetable. No row values, credentials or order evidence printed.
This publishes no mutable gameplay/character state and performs no source repair.
"""
import argparse
import json
from pathlib import Path
import sys
from zoneinfo import ZoneInfo
from activate_catalog import MAPPING, PINNED
from activate_actor_catalog import offline_owner_barrier
from migrate import connect, digest, load_snapshot, read_target, table_hash

TABLES=('TBATTLETIMECHART','TCUSTOMTIMECHART')
SOURCE_QUERY='SELECT bType, dwTime FROM TCUSTOMTIMECHART'

def import_world_battles(conn,manifest_path,world,source_timezone,custom_order=None):
    if isinstance(world,bool) or not 0<=world<=255:raise RuntimeError('WORLD_OUT_OF_RANGE')
    ZoneInfo(source_timezone)
    manifest,loaded=load_snapshot(manifest_path,MAPPING,include_historical=True)
    expected={(t['database'],t['schema'],t['table']) for t in json.loads(MAPPING.read_text())['tables']}
    if {(t['database'],t['schema'],t['table']) for t,_,_ in loaded}!=expected:
        raise RuntimeError('COMPLETE_HISTORICAL_MANIFEST_REQUIRED')
    pinned=[{k:a[k] for k in ('path','size_bytes','sha256')} for a in json.loads(PINNED.read_text())['artifacts']
            if a['path'] in ('TGAME_RAGEZONE.bak','TGLOBAL_RAGEZONE.bak')]
    if len(pinned)!=2 or sorted(manifest['backups'],key=lambda x:x['path'])!=sorted(pinned,key=lambda x:x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    fingerprint=digest(json.dumps(manifest,sort_keys=True,separators=(',',':')).encode())
    selected={t['table']:(t,model,rows) for t,model,rows in loaded
              if (t['database'],t['schema'])==('TGAME_RAGEZONE','dbo') and t['table'] in TABLES}
    if set(selected)!=set(TABLES):raise RuntimeError('COMPLETE_BATTLE_TIME_SOURCE_REQUIRED')
    battle,_,battles=selected[TABLES[0]];custom,_,times=selected[TABLES[1]]
    if (len(battles),len(times))!=(5,3):raise RuntimeError('PINNED_BATTLE_TIME_ROW_PROFILE_REQUIRES_RECONCILIATION')
    def byte(value):return not isinstance(value,bool) and isinstance(value,int) and 0<=value<=255
    def signed(value):return not isinstance(value,bool) and isinstance(value,int) and -2147483648<=value<=2147483647
    if len({r['bType'] for r in battles})!=len(battles):raise RuntimeError('DUPLICATE_BATTLE_TYPE')
    for row in battles:
        if (any(not byte(row[k]) for k in ('bType','bDay','bWeek')) or
                any(not signed(row[k]) for k in ('dwBattleDur','dwBattleStart','dwAlarmStart','dwAlarmEnd','dwPeaceDur'))):
            raise RuntimeError('SOURCE_BATTLE_BINDING_DOMAIN_UNAVAILABLE')
    if any(not byte(r['bType']) or not signed(r['dwTime']) for r in times):
        raise RuntimeError('SOURCE_CUSTOM_TIME_BYTE_DWORD_BINDING_UNAVAILABLE')
    if custom_order:
        encoded=custom_order.read_bytes();profile=json.loads(encoded)
        if (profile.get('source_manifest_sha256')!=fingerprint or profile.get('source_query')!=SOURCE_QUERY or
                not isinstance(profile.get('source_rows'),list)):
            raise RuntimeError('EXPLICIT_CUSTOM_ORDER_PROFILE_REQUIRED')
        order=profile['source_rows']
        if (any(isinstance(n,bool) or not isinstance(n,int) for n in order) or
                sorted(order)!=list(range(1,len(times)+1))):raise RuntimeError('CUSTOM_ORDER_MUST_REFERENCE_EVERY_REAL_ROW_ONCE')
        policy='explicit-source-order-profile';order_hash=digest(encoded)
    else:
        if sum(r['bType']==3 for r in times)>1:raise RuntimeError('MULTIPLE_MISSION_ROWS_REQUIRE_EXPLICIT_SOURCE_ORDER')
        order=list(range(1,len(times)+1));policy='single-mission-order-independent'
        order_hash=digest(json.dumps(order,separators=(',',':')).encode())
    from psycopg.types.json import Jsonb
    with conn.transaction():
        conn.execute("SET LOCAL lock_timeout='3s'");conn.execute("SET LOCAL statement_timeout='30s'")
        offline_owner_barrier(conn)
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,app_world.worlds,'
                     'legacy_game."TBATTLETIMECHART",legacy_game."TCUSTOMTIMECHART" IN SHARE MODE')
        conn.execute('LOCK TABLE runtime_control.world_battle_catalog IN SHARE ROW EXCLUSIVE MODE')
        run=conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs WHERE manifest_sha256=%s',(fingerprint,)).fetchone()
        if not run or run[1]!='verified' or run[2]!=manifest:raise RuntimeError('VERIFIED_ARCHIVAL_IMPORT_REQUIRED')
        if not conn.execute('SELECT 1 FROM pg_timezone_names WHERE name=%s',(source_timezone,)).fetchone():
            raise RuntimeError('POSTGRESQL_SOURCE_TIMEZONE_REQUIRED')
        if not conn.execute('SELECT 1 FROM world_compat.rps_release WHERE world_id=%s AND run_id=%s AND source_timezone=%s',
                            (world,run[0],source_timezone)).fetchone():raise RuntimeError('SAME_WORLD_SOURCE_PROFILE_AND_TIMEZONE_REQUIRED')
        for table,model,rows in selected.values():
            checkpoint=conn.execute('SELECT row_count,source_sha256,target_sha256 FROM reconstruction.checkpoints '
                'WHERE run_id=%s AND source_database=%s AND source_schema=%s AND source_table=%s',
                (run[0],'TGAME_RAGEZONE','dbo',table['table'])).fetchone()
            actual=read_target(conn,model,run[0])
            if (checkpoint!=(len(rows),table['value_sha256'],table['value_sha256']) or len(actual)!=len(rows) or
                    table_hash(actual,model['column_mapping'])!=table['value_sha256']):
                raise RuntimeError('ARCHIVED_BATTLE_TIME_VALUES_CHANGED')
            # The explicit order refers to archival ordinals, not their unordered
            # multiset hash. Verify those exact source-row identities as well.
            if table['table']=='TCUSTOMTIMECHART':
                ordinals=conn.execute('SELECT "_source_row_number","bType","dwTime" FROM legacy_game."TCUSTOMTIMECHART" '
                                      'WHERE "_import_run_id"=%s ORDER BY "_source_row_number"',(run[0],)).fetchall()
                if ordinals!=[(i,r['bType'],r['dwTime']) for i,r in enumerate(rows,1)]:
                    raise RuntimeError('CUSTOM_ORDER_ARCHIVAL_ORDINALS_CHANGED')
        if conn.execute('SELECT 1 FROM runtime_control.world_battle_catalog WHERE world_id=%s',(world,)).fetchone():
            raise RuntimeError('EXISTING_BATTLE_PROFILE_REQUIRES_OFFLINE_RECONCILIATION')
        conn.execute('INSERT INTO runtime_control.world_battle_catalog(world_id,run_id,manifest_sha256,battle_sha256,custom_sha256,'
                     'battle_rows,custom_rows,source_timezone,custom_order,order_policy,order_sha256) VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                     (world,run[0],fingerprint,battle['value_sha256'],custom['value_sha256'],len(battles),len(times),
                      source_timezone,Jsonb(order),policy,order_hash))
    return dict(world_id=world,manifest_sha256=fingerprint,battle_rows=len(battles),custom_rows=len(times),
                order_policy=policy,order_sha256=order_hash,source_timezone=source_timezone)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,required=True);parser.add_argument('--world',type=int,required=True)
    parser.add_argument('--source-timezone',required=True)
    parser.add_argument('--custom-order',type=Path,help='Private recorded original query-order profile bound to this manifest; compatibility still requires Phase B evidence')
    args=parser.parse_args()
    try:
        with connect() as conn:print(json.dumps(import_world_battles(conn,args.manifest,args.world,args.source_timezone,args.custom_order)))
    except Exception as error:
        code=getattr(error,'sqlstate',None)
        if not code and isinstance(error,RuntimeError) and str(error).replace('_','').isalnum():code=str(error)
        print(json.dumps({'status':'failed','error':code or 'ERROR_DETAIL_SUPPRESSED'}),file=sys.stderr);sys.exit(1)

if __name__=='__main__':main()
