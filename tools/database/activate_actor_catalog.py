#!/usr/bin/env python3
"""Publish pinned Map actor charts, optionally certifying the additive v1 -> v2 upgrade.

Publication is offline and atomic. Historical imports and player graphs are never
rewritten. An explicit previous manifest is required to admit old graph receipts;
only the original two-table -> four-table extension with identical shared charts
is supported. A lost COMMIT response must be inspected, never automatically retried.
"""
import argparse
import json
from pathlib import Path
import secrets
import sys
from activate_catalog import MAPPING, PINNED
from generate_migrations import ACTOR_REFERENCE_KEYS
from migrate import connect, digest, load_snapshot, read_target, table_hash, target_bytes_hash

OLD_ACTOR_KEYS=frozenset(('TGAME_RAGEZONE','dbo',t) for t in ('TITEMMAGICCHART','TSKILLPOINTCHART'))
POLICY='actor-v1-to-v2-additive-statistics'
MAP_LOCK_NAMESPACE=0x344D4150


def actor_profile(path,mapping_path,pinned_path,expected):
    manifest,loaded=load_snapshot(path,mapping_path)
    pinned=[{k:a[k] for k in ('path','size_bytes','sha256')}
            for a in json.loads(pinned_path.read_text())['artifacts']
            if a['path'] in ('TGAME_RAGEZONE.bak','TGLOBAL_RAGEZONE.bak')]
    if len(pinned)!=2 or sorted(manifest['backups'],key=lambda x:x['path'])!=sorted(pinned,key=lambda x:x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    if {(t['database'],t['schema'],t['table']) for t,_,_ in loaded}!=expected:
        raise RuntimeError('COMPLETE_ACTOR_MANIFEST_REQUIRED' if expected==ACTOR_REFERENCE_KEYS else 'PREVIOUS_ACTOR_V1_MANIFEST_REQUIRED')
    return manifest,loaded,digest(json.dumps(manifest,sort_keys=True,separators=(',',':')).encode())


def verified_actor_import(conn,profile):
    manifest,loaded,fingerprint=profile
    run=conn.execute('SELECT id,status,source_manifest FROM reconstruction.import_runs '
                     'WHERE manifest_sha256=%s FOR UPDATE',(fingerprint,)).fetchone()
    if not run or run[1]!='verified' or run[2]!=manifest:raise RuntimeError('VERIFIED_IMPORT_REQUIRED')
    for t,model,rows in loaded:
        if not t.get('text_bytes_file'):raise RuntimeError('ORIGINAL_TEXT_BYTES_REQUIRED')
        key=(run[0],t['database'],t['schema'],t['table'])
        cp=conn.execute('SELECT row_count,source_sha256,target_sha256,source_bytes_sha256,target_bytes_sha256 '
                        'FROM reconstruction.checkpoints WHERE run_id=%s AND source_database=%s '
                        'AND source_schema=%s AND source_table=%s',key).fetchone()
        actual=read_target(conn,model,run[0])
        if (cp!=(len(rows),t['value_sha256'],t['value_sha256'],
                 t['text_bytes_value_sha256'],t['text_bytes_value_sha256']) or len(actual)!=len(rows)
            or table_hash(actual,model['column_mapping'])!=t['value_sha256']
            or target_bytes_hash(conn,key)!=t['text_bytes_value_sha256']):
            raise RuntimeError('CHECKPOINT_TARGET_DRIFT')
    return run[0]


def compatibility_proof(old,new):
    if old[0]['metadata_sha256']!=new[0]['metadata_sha256']:raise RuntimeError('ACTOR_METADATA_CHANGED')
    current={t['table']:t for t,_,_ in new[1]};shared={}
    for t,_,_ in old[1]:
        fields=('row_count','value_sha256','text_bytes_value_sha256')
        if any(t[k]!=current[t['table']][k] for k in fields):raise RuntimeError('ACTOR_SHARED_CHART_CHANGED')
        shared[t['table']]={k:t[k] for k in fields}
    return {'policy':POLICY,'metadata_sha256':old[0]['metadata_sha256'],
            'backups':old[0]['backups'],'shared_tables':shared,
            'added_tables':['TITEMATTRCHART','TITEMGRADECHART'],'transfer_codec_contract':2}


def offline_owner_barrier(conn):
    # The old runtime already locks this table for process replacement and every
    # fenced operation. Exclusive NOWAIT serializes old/new startup and workers
    # without needing a lock protocol that older binaries do not implement.
    conn.execute('LOCK TABLE app_world.map_runtime_owner IN ACCESS EXCLUSIVE MODE NOWAIT')
    live=conn.execute("SELECT 1 FROM pg_catalog.pg_locks WHERE locktype='advisory' "
                      'AND database=(SELECT oid FROM pg_catalog.pg_database WHERE datname=current_database()) '
                      'AND classid=%s AND objsubid=2 AND granted LIMIT 1',(MAP_LOCK_NAMESPACE,)).fetchone()
    if live:raise RuntimeError('ACTIVE_MAP_OWNER_BLOCKS_ACTOR_PUBLICATION')


def activate_actor_catalog(conn,manifest_path,mapping_path=MAPPING,pinned_path=PINNED,*,previous_manifest=None):
    new=actor_profile(manifest_path,mapping_path,pinned_path,ACTOR_REFERENCE_KEYS)
    old=actor_profile(previous_manifest,mapping_path,pinned_path,OLD_ACTOR_KEYS) if previous_manifest else None
    proof=compatibility_proof(old,new) if old else None
    from psycopg.types.json import Jsonb
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL READ COMMITTED')
        conn.execute("SET LOCAL lock_timeout='1s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040006)')
        # READ COMMITTED takes the verification snapshot after these locks. A
        # snapshot captured before waiting for a concurrent import could be stale.
        # Verification and certification must see stable underlying rows, not
        # just matching receipt hashes while another importer changes the data.
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes, '
                     'legacy_game."TITEMMAGICCHART",legacy_game."TSKILLPOINTCHART", '
                     'legacy_game."TITEMATTRCHART",legacy_game."TITEMGRADECHART" IN SHARE MODE')
        run_id=verified_actor_import(conn,new);old_id=verified_actor_import(conn,old) if old else None
        if conn.execute('SELECT 1 FROM legacy_game."TITEMGRADECHART" WHERE "_import_run_id"=%s '
                        'GROUP BY "bLevel" HAVING count(*)>1 LIMIT 1',(run_id,)).fetchone():
            raise RuntimeError('AMBIGUOUS_ITEM_GRADE_LEVEL')
        if not conn.execute('SELECT 1 FROM legacy_game."TITEMATTRCHART" WHERE "_import_run_id"=%s LIMIT 1',(run_id,)).fetchone():
            raise RuntimeError('EMPTY_ITEM_ATTRIBUTE_CHART')
        previous=conn.execute('SELECT run_id FROM runtime_control.actor_catalog WHERE singleton FOR UPDATE').fetchone()
        if old and (not previous or previous[0] not in (old_id,run_id)):
            raise RuntimeError('PREVIOUS_ACTOR_RELEASE_NOT_ACTIVE')
        changed=not previous or previous[0]!=run_id
        certificate=None
        if old:
            certificate=conn.execute('SELECT policy,proof,source_manifest_sha256,target_manifest_sha256 '
                'FROM runtime_control.actor_graph_compatibility WHERE source_run_id=%s AND target_run_id=%s',(old_id,run_id)).fetchone()
            if certificate and certificate!=(POLICY,proof,old[2],new[2]):raise RuntimeError('ACTOR_COMPATIBILITY_PROOF_CHANGED')
        if changed or (old and not certificate):offline_owner_barrier(conn)
        if old and not certificate:
            conn.execute('INSERT INTO runtime_control.actor_graph_compatibility '
                         '(source_run_id,target_run_id,source_manifest_sha256,target_manifest_sha256,policy,proof) '
                         'VALUES(%s,%s,%s,%s,%s,%s)',(old_id,run_id,old[2],new[2],POLICY,Jsonb(proof)))
        if changed:
            # Do not strand existing graph receipts by publishing an unrelated
            # release. Terminal transfer journals retain their original evidence.
            incompatible=conn.execute('''WITH needed AS (
                SELECT actor_manifest FROM app_world.map_checkpoints WHERE recovery_contract=2
                UNION SELECT actor_manifest FROM app_world.map_transfers WHERE phase='prepared'
              ) SELECT 1 FROM needed n WHERE n.actor_manifest<>%s AND NOT EXISTS (
                SELECT 1 FROM runtime_control.actor_graph_compatibility p
                JOIN reconstruction.import_runs s ON s.id=p.source_run_id AND s.status='verified'
                  AND s.manifest_sha256=p.source_manifest_sha256
                WHERE p.source_manifest_sha256=n.actor_manifest AND p.target_run_id=%s
                  AND p.target_manifest_sha256=%s AND p.policy=%s) LIMIT 1''',(new[2],run_id,new[2],POLICY)).fetchone()
            if incompatible:raise RuntimeError('ACTOR_GRAPH_COMPATIBILITY_REQUIRED')
            conn.execute('INSERT INTO runtime_control.actor_catalog(singleton,run_id) VALUES(true,%s) '
                         'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,activated_at=CURRENT_TIMESTAMP',(run_id,))
            activation=conn.execute('INSERT INTO runtime_control.actor_catalog_activations(previous_run_id,run_id,manifest_sha256) '
                         'VALUES(%s,%s,%s) RETURNING id',(previous[0] if previous else None,run_id,new[2])).fetchone()[0]
            # A process can have lost its advisory connection but retain worker
            # sessions. Rotate even inactive owner tokens so those workers fail
            # their next FOR SHARE identity check. Keep all player evidence intact.
            owners=conn.execute('SELECT world_id,server_id,owner_token FROM app_world.map_runtime_owner ORDER BY world_id,server_id').fetchall()
            for world,server,token in owners:
                replacement=secrets.token_hex(32)
                conn.execute('INSERT INTO runtime_control.actor_owner_retirements VALUES(%s,%s,%s,%s,%s)',
                             (activation,world,server,token,replacement))
                conn.execute('UPDATE app_world.map_runtime_owner SET owner_token=%s,backend_pid=0,acquired_at=clock_timestamp() '
                             'WHERE world_id=%s AND server_id=%s',(replacement,world,server))
        else:owners=[]
        return dict(run_id=run_id,manifest_sha256=new[2],changed=changed,reference_tables=len(new[1]),
                    verified_source_rows=sum(len(rows) for _,_,rows in new[1]),snapshot_values_and_original_text_bytes_verified=True,
                    compatible_previous_manifest=old[2] if old else None,compatibility_created=bool(old and not certificate),
                    retired_map_owners=len(owners))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--previous-manifest',type=Path,help='Explicit original two-table actor manifest; certifies only identical shared charts')
    p.add_argument('--report',type=Path)
    args=p.parse_args()
    try:
        with connect() as conn:result=activate_actor_catalog(conn,args.manifest,previous_manifest=args.previous_manifest)
        if args.report:args.report.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result))
    except Exception as ex:
        code=getattr(ex,'sqlstate',None)
        if not code and isinstance(ex,RuntimeError) and str(ex).replace('_','').isalnum():code=str(ex)
        print('Actor catalog activation failed: '+(code or 'ERROR_DETAIL_SUPPRESSED'),file=sys.stderr)
        sys.exit(1)

if __name__=='__main__':main()
