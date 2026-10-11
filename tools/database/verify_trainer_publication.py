"""Publication adversarial checks in the explicitly owned PostgreSQL lab only."""
import copy
import json
import shutil
import psycopg
from activate_trainer_catalog import activate_trainer_catalog
from activate_actor_catalog import MAP_LOCK_NAMESPACE


def verify_trainer_publication(conn,manifest,private):
    checks=[]
    def check(ok,label):
        if not ok:raise RuntimeError('Trainer publication: '+label)
        checks.append(label)
    def reject(path,code):
        try:activate_trainer_catalog(conn,path)
        except RuntimeError as error:check(str(error)==code,code);return
        raise RuntimeError('Invalid trainer publication accepted')
    selected=conn.execute('SELECT run_id,character_manifest,actor_manifest FROM runtime_control.trainer_catalog').fetchone()
    run=selected[0]
    history=conn.execute('SELECT count(*) FROM runtime_control.trainer_catalog_activations').fetchone()
    check(not activate_trainer_catalog(conn,manifest)['changed'],'same verified trainer release is idempotent')
    copied=private/'trainer-negative-fixture';shutil.copytree(manifest.parent,copied)
    altered=copied/'manifest.json';original=json.loads(altered.read_text())
    partial=copy.deepcopy(original);partial['tables'].pop();altered.write_text(json.dumps(partial));reject(altered,'COMPLETE_TRAINER_MANIFEST_REQUIRED')
    wrong=copy.deepcopy(original);wrong['backups'][0]['sha256']='0'*64;altered.write_text(json.dumps(wrong));reject(altered,'PINNED_BACKUP_FINGERPRINT_MISMATCH')
    for table in ('TNPCITEMCHART','TTITLECHART'):
        conn.execute('UPDATE reconstruction.checkpoints SET row_count=row_count+1 WHERE run_id=%s AND source_table=%s',(run,table))
        try:reject(manifest,'CHECKPOINT_TARGET_DRIFT')
        finally:conn.execute('UPDATE reconstruction.checkpoints SET row_count=row_count-1 WHERE run_id=%s AND source_table=%s',(run,table))
    # Preserve and restore the exact source cell; no manufactured source fixture
    # is ever accepted as a replacement verified release.
    cell=conn.execute('SELECT "_source_row_number","dwRequirement" FROM legacy_game."TTITLECHART" WHERE "_import_run_id"=%s ORDER BY "_source_row_number" LIMIT 1',(run,)).fetchone()
    conn.execute('UPDATE legacy_game."TTITLECHART" SET "dwRequirement"="dwRequirement"+1 WHERE "_import_run_id"=%s AND "_source_row_number"=%s',(run,cell[0]))
    try:reject(manifest,'CHECKPOINT_TARGET_DRIFT')
    finally:conn.execute('UPDATE legacy_game."TTITLECHART" SET "dwRequirement"=%s WHERE "_import_run_id"=%s AND "_source_row_number"=%s',(cell[1],run,cell[0]))
    cell=conn.execute('SELECT source_row_number,column_name,source_bytes FROM reconstruction.source_text_bytes WHERE run_id=%s ORDER BY source_row_number,column_name LIMIT 1',(run,)).fetchone()
    conn.execute("UPDATE reconstruction.source_text_bytes SET source_bytes='\\x00' WHERE run_id=%s AND source_row_number=%s AND column_name=%s",(run,*cell[:2]))
    try:reject(manifest,'CHECKPOINT_TARGET_DRIFT')
    finally:conn.execute('UPDATE reconstruction.source_text_bytes SET source_bytes=%s WHERE run_id=%s AND source_row_number=%s AND column_name=%s',(cell[2],run,*cell[:2]))
    # An owner with a live session blocks a changed selection even if no runtime
    # owner row has been published yet. This is the startup race boundary.
    with psycopg.connect(conn.info.dsn,password=conn.info.password,autocommit=True) as owner:
        owner.execute('SELECT pg_advisory_lock(%s,1)',(MAP_LOCK_NAMESPACE,))
        conn.execute("UPDATE runtime_control.trainer_catalog SET character_manifest=repeat('0',64)")
        try:reject(manifest,'ACTIVE_MAP_OWNER_BLOCKS_ACTOR_PUBLICATION')
        finally:conn.execute('UPDATE runtime_control.trainer_catalog SET character_manifest=%s',(selected[1],))
    check(conn.execute('SELECT run_id,character_manifest,actor_manifest FROM runtime_control.trainer_catalog').fetchone()==selected,'failed trainer publications preserve all pinned release identities')
    check(conn.execute('SELECT count(*) FROM runtime_control.trainer_catalog_activations').fetchone()==history,'failed and idempotent publications append no activation receipts')
    check(not activate_trainer_catalog(conn,manifest)['changed'],'restored source values and original bytes still verify')
    check(conn.execute('SELECT count(*) FROM character_compat."TNPCITEMCHART"').fetchone()==(3364,),'all 3364 original NPC merchandise rows published')
    check(conn.execute('SELECT count(*) FROM character_compat."TTITLECHART"').fetchone()==(111,),'all 111 original title definitions published')
    return {'status':'passed','checks':checks}
