"""Destructive fixture checks, only called inside the owned disposable PG harness."""
import copy
import json
import shutil
from activate_routing_catalog import activate_routing_catalog


def verify_routing_publication(conn,manifest,private):
    checks=[]
    selectors=conn.execute('SELECT (SELECT run_id FROM runtime_control.character_catalog),'
                           '(SELECT run_id FROM runtime_control.active_catalog),'
                           '(SELECT run_id FROM runtime_control.routing_catalog)').fetchone()
    history=conn.execute('SELECT count(*) FROM runtime_control.routing_catalog_activations').fetchone()[0]
    def check(ok,label):
        if not ok:raise RuntimeError('Routing publication verification failed: '+label)
        checks.append(label)
    def reject(path,code):
        try:activate_routing_catalog(conn,path)
        except RuntimeError as error:check(str(error)==code,code);return
        raise RuntimeError('Invalid routing publication accepted')
    check(not activate_routing_catalog(conn,manifest)['changed'],'same verified release activation is idempotent')
    copied=private/'routing-negative-fixture';shutil.copytree(manifest.parent,copied)
    altered=copied/'manifest.json';original=json.loads(altered.read_text())
    partial=copy.deepcopy(original);partial['tables'].pop();altered.write_text(json.dumps(partial))
    reject(altered,'COMPLETE_ROUTING_MANIFEST_REQUIRED')
    wrong=copy.deepcopy(original);wrong['backups'][0]['sha256']='0'*64;altered.write_text(json.dumps(wrong))
    reject(altered,'PINNED_BACKUP_FINGERPRINT_MISMATCH')
    altered.write_text(json.dumps(original))
    run=selectors[2]
    conn.execute("UPDATE reconstruction.checkpoints SET row_count=row_count+1 WHERE run_id=%s AND source_table='TMAPCHART'",(run,))
    try:reject(manifest,'CHECKPOINT_TARGET_DRIFT')
    finally:conn.execute("UPDATE reconstruction.checkpoints SET row_count=row_count-1 WHERE run_id=%s AND source_table='TMAPCHART'",(run,))
    check(conn.execute('SELECT (SELECT run_id FROM runtime_control.character_catalog),'
                       '(SELECT run_id FROM runtime_control.active_catalog),'
                       '(SELECT run_id FROM runtime_control.routing_catalog)').fetchone()==selectors,
          'failed publications preserve independent character, Map and routing selectors')
    check(conn.execute('SELECT count(*) FROM runtime_control.routing_catalog_activations').fetchone()[0]==history,
          'idempotent and failed publications add no activation history')
    check(conn.execute('SELECT count(*) FROM route_compat."TSVRCHART"').fetchone()[0]==358,
          'original TSVRCHART view join produces 358 routing rows')
    return dict(status='passed',checks=checks)
