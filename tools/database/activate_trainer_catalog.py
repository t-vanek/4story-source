#!/usr/bin/env python3
"""Publish verified backup trainer merchandise while native Maps are offline.

The source list is also used by other NPC types; retain every original row.
Learning interprets it only for skill-master/rental NPCs, as the original did.
No player state, existing manifests or imported source values are rewritten.
"""
import argparse
import json
import secrets
import sys
from pathlib import Path
from activate_catalog import MAPPING,PINNED
from activate_actor_catalog import offline_owner_barrier,verified_actor_import
from generate_migrations import TRAINER_REFERENCE_KEYS
from migrate import connect,digest,load_snapshot


def activate_trainer_catalog(conn,manifest_path,mapping_path=MAPPING,pinned_path=PINNED):
    manifest,loaded=load_snapshot(manifest_path,mapping_path)
    pinned=[{k:a[k] for k in ('path','size_bytes','sha256')} for a in json.loads(pinned_path.read_text())['artifacts']
            if a['path'] in ('TGAME_RAGEZONE.bak','TGLOBAL_RAGEZONE.bak')]
    if len(pinned)!=2 or sorted(manifest['backups'],key=lambda x:x['path'])!=sorted(pinned,key=lambda x:x['path']):
        raise RuntimeError('PINNED_BACKUP_FINGERPRINT_MISMATCH')
    if {(t['database'],t['schema'],t['table']) for t,_,_ in loaded}!=TRAINER_REFERENCE_KEYS:
        raise RuntimeError('COMPLETE_TRAINER_MANIFEST_REQUIRED')
    fingerprint=digest(json.dumps(manifest,sort_keys=True,separators=(',',':')).encode())
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL READ COMMITTED')
        conn.execute("SET LOCAL lock_timeout='1s'")
        conn.execute('SELECT pg_advisory_xact_lock(418040006)')
        conn.execute('LOCK TABLE reconstruction.import_runs,reconstruction.checkpoints,reconstruction.source_text_bytes, '
                     'legacy_game."TNPCITEMCHART",legacy_game."TTITLECHART",runtime_control.character_catalog,runtime_control.actor_catalog IN SHARE MODE')
        # Existing generic checkpoint verifier compares every value and byte,
        # rather than trusting the import receipt alone.
        run=verified_actor_import(conn,(manifest,loaded,fingerprint))
        catalogs=conn.execute("SELECT c.manifest_sha256,a.manifest_sha256 FROM character_compat.catalog_release c "
            "CROSS JOIN actor_compat.catalog_release a JOIN actor_compat.statistics_release s ON s.run_id=a.run_id "
            "WHERE c.status='verified' AND a.status='verified'").fetchall()
        if len(catalogs)!=1:raise RuntimeError('VERIFIED_CHARACTER_AND_STATISTICS_RELEASES_REQUIRED')
        character,actor=catalogs[0]
        previous=conn.execute('SELECT run_id,character_manifest,actor_manifest FROM runtime_control.trainer_catalog WHERE singleton FOR UPDATE').fetchone()
        changed=previous!=(run,character,actor)
        retired=0
        if changed:
            offline_owner_barrier(conn)
            conn.execute('INSERT INTO runtime_control.trainer_catalog(singleton,run_id,character_manifest,actor_manifest) VALUES(true,%s,%s,%s) '
                'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id,character_manifest=EXCLUDED.character_manifest, '
                'actor_manifest=EXCLUDED.actor_manifest,activated_at=CURRENT_TIMESTAMP',(run,character,actor))
            activation=conn.execute('INSERT INTO runtime_control.trainer_catalog_activations(previous_run_id,run_id,manifest_sha256,character_manifest,actor_manifest) '
                'VALUES(%s,%s,%s,%s,%s) RETURNING id',(previous[0] if previous else None,run,fingerprint,character,actor)).fetchone()[0]
            for world,server,token in conn.execute('SELECT world_id,server_id,owner_token FROM app_world.map_runtime_owner ORDER BY world_id,server_id').fetchall():
                replacement=secrets.token_hex(32)
                conn.execute('INSERT INTO runtime_control.trainer_owner_retirements VALUES(%s,%s,%s,%s,%s)',(activation,world,server,token,replacement))
                conn.execute('UPDATE app_world.map_runtime_owner SET owner_token=%s,backend_pid=0,acquired_at=clock_timestamp() WHERE world_id=%s AND server_id=%s',
                             (replacement,world,server));retired+=1
        return {'run_id':run,'manifest_sha256':fingerprint,'changed':changed,'character_manifest':character,'actor_manifest':actor,
                'verified_source_rows':sum(len(rows) for _,_,rows in loaded),'snapshot_values_and_original_text_bytes_verified':True,'retired_map_owners':retired}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--report',type=Path)
    args=p.parse_args()
    try:
        with connect() as conn:result=activate_trainer_catalog(conn,args.manifest)
        if args.report:args.report.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result))
    except Exception as ex:
        code=getattr(ex,'sqlstate',None)
        if not code and isinstance(ex,RuntimeError) and str(ex).replace('_','').isalnum():code=str(ex)
        print('Trainer catalog activation failed: '+(code or 'ERROR_DETAIL_SUPPRESSED'),file=sys.stderr)
        sys.exit(1)


if __name__=='__main__':main()
