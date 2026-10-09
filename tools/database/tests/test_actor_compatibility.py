"""Real PostgreSQL certificate, permission, rollback and old-process fence tests."""
import json
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import test_compatibility as fixtures
from activate_actor_catalog import activate_actor_catalog,MAP_LOCK_NAMESPACE
from activate_catalog import MAPPING
from generate_migrations import ACTOR_REFERENCE_KEYS
from migrate import connect,import_snapshot,table_hash,read_target,load_snapshot


@unittest.skipUnless(os.getenv('FOURSTORY_DB_TESTS')=='1','requires isolated PostgreSQL')
class ActorCompatibilityVerification(unittest.TestCase):
    setUpClass=classmethod(fixtures.CompatibilityVerification.setUpClass.__func__)
    tearDownClass=classmethod(fixtures.CompatibilityVerification.tearDownClass.__func__)
    fixture=fixtures.CompatibilityVerification.fixture
    def setUp(self):
        fixtures.CompatibilityVerification.setUp(self)
        self.old=self.fixture(tables=['TITEMMAGICCHART','TSKILLPOINTCHART'])
        self.new=self.fixture(tables=[t for _,_,t in sorted(ACTOR_REFERENCE_KEYS)])
        self.old_run=import_snapshot(self.conn,self.old,MAPPING)['run_id']
        self.new_run=import_snapshot(self.conn,self.new,MAPPING)['run_id']
        self.conn.execute('INSERT INTO runtime_control.actor_catalog(singleton,run_id) VALUES(true,%s) ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id',(self.old_run,))
    def publish(self):return activate_actor_catalog(self.conn,self.new,previous_manifest=self.old)
    def state(self):
        return [self.conn.execute(q).fetchall() for q in (
            'SELECT * FROM runtime_control.actor_catalog','SELECT * FROM runtime_control.actor_catalog_activations ORDER BY id',
            'SELECT * FROM runtime_control.actor_graph_compatibility ORDER BY source_run_id,target_run_id',
            'SELECT * FROM runtime_control.actor_owner_retirements ORDER BY activation_id,world_id,server_id',
            'SELECT * FROM app_world.map_runtime_owner ORDER BY world_id,server_id')]
    def owner(self):
        self.conn.execute('INSERT INTO app_global."TGROUP"("bGroupID","bType","szNAME") VALUES(250,0,\'Synthetic owner fence\') ON CONFLICT DO NOTHING')
        self.conn.execute('INSERT INTO app_world.worlds VALUES(250,126,9079256848778919936) ON CONFLICT DO NOTHING')
        self.conn.execute('INSERT INTO app_world.map_runtime_owner VALUES(250,1,%s,0,clock_timestamp()) ON CONFLICT(world_id,server_id) DO UPDATE SET owner_token=EXCLUDED.owner_token',('a'*64,))
        self.addCleanup(lambda:self.conn.execute('DELETE FROM app_world.map_runtime_owner WHERE world_id=250'))
    def test_explicit_directional_idempotent_certificate(self):
        first=self.publish();state=self.state();again=self.publish()
        self.assertTrue(first['changed'] and first['compatibility_created'])
        self.assertFalse(again['changed'] or again['compatibility_created'])
        self.assertEqual(self.state(),state)
        pair=self.conn.execute('SELECT source_manifest_sha256,target_manifest_sha256 FROM actor_compat.transfer_catalog_compatibility').fetchall()
        self.assertEqual(pair,[(first['compatible_previous_manifest'],first['manifest_sha256'])])
        proof=self.conn.execute('SELECT proof FROM runtime_control.actor_graph_compatibility WHERE source_run_id=%s',(self.old_run,)).fetchone()[0]
        self.assertEqual(set(proof['shared_tables']),{'TITEMMAGICCHART','TSKILLPOINTCHART'})
        self.assertEqual(proof['added_tables'],['TITEMATTRCHART','TITEMGRADECHART'])
        with self.conn.transaction(force_rollback=True):
            self.conn.execute('UPDATE runtime_control.actor_catalog SET run_id=%s',(self.old_run,))
            self.assertEqual(self.conn.execute('SELECT count(*) FROM actor_compat.transfer_catalog_compatibility').fetchone(),(0,))
    def test_changed_shared_values_cannot_certify(self):
        self.new=self.fixture(tables=[t for _,_,t in sorted(ACTOR_REFERENCE_KEYS)],overrides={'TSKILLPOINTCHART':{'bSkillPoint':7}})
        import_snapshot(self.conn,self.new,MAPPING);before=self.state()
        with self.assertRaisesRegex(RuntimeError,'ACTOR_SHARED_CHART_CHANGED'):self.publish()
        self.assertEqual(self.state(),before)
    def test_old_target_drift_cannot_certify(self):
        self.conn.execute('UPDATE legacy_game."TITEMMAGICCHART" SET "wMaxValue"=123 WHERE "_import_run_id"=%s',(self.old_run,))
        before=self.state()
        with self.assertRaisesRegex(RuntimeError,'CHECKPOINT_TARGET_DRIFT'):self.publish()
        self.assertEqual(self.state(),before)
    def test_matching_tampered_target_receipt_is_not_source_truth(self):
        self.conn.execute('UPDATE legacy_game."TITEMMAGICCHART" SET "wMaxValue"=123 WHERE "_import_run_id"=%s',(self.new_run,))
        model=next(m for t,m,r in load_snapshot(self.new,MAPPING)[1] if t['table']=='TITEMMAGICCHART')
        changed=table_hash(read_target(self.conn,model,self.new_run),model['column_mapping'])
        self.conn.execute('UPDATE reconstruction.checkpoints SET source_sha256=%s,target_sha256=%s WHERE run_id=%s AND source_table=\'TITEMMAGICCHART\'',(changed,changed,self.new_run))
        with self.assertRaisesRegex(RuntimeError,'CHECKPOINT_TARGET_DRIFT'):self.publish()
    def test_unverified_old_import_cannot_certify(self):
        self.conn.execute('UPDATE reconstruction.import_runs SET status=\'failed\' WHERE id=%s',(self.old_run,))
        with self.assertRaisesRegex(RuntimeError,'VERIFIED_IMPORT_REQUIRED'):self.publish()
    def test_unselected_old_import_cannot_certify(self):
        self.conn.execute('DELETE FROM runtime_control.actor_catalog')
        with self.assertRaisesRegex(RuntimeError,'PREVIOUS_ACTOR_RELEASE_NOT_ACTIVE'):self.publish()
    def test_live_old_advisory_owner_blocks_atomically(self):
        before=self.state()
        with connect() as owner:
            owner.execute('SELECT pg_advisory_lock(%s,64001)',(MAP_LOCK_NAMESPACE,))
            with self.assertRaisesRegex(RuntimeError,'ACTIVE_MAP_OWNER_BLOCKS_ACTOR_PUBLICATION'):self.publish()
        self.assertEqual(self.state(),before)
        self.assertTrue(self.publish()['changed'])
    def test_inflight_old_worker_blocks_atomically(self):
        from psycopg.errors import LockNotAvailable
        self.owner();before=self.state()
        with connect() as worker,worker.transaction():
            worker.execute('SELECT owner_token FROM app_world.map_runtime_owner WHERE world_id=250 FOR SHARE')
            with self.assertRaises(LockNotAvailable):self.publish()
            self.assertEqual(self.state(),before)
    def test_concurrent_publishers_create_one_certificate_and_activation(self):
        from concurrent.futures import ThreadPoolExecutor
        before=self.conn.execute('SELECT count(*) FROM runtime_control.actor_catalog_activations').fetchone()[0]
        def publish():
            with connect() as c:return activate_actor_catalog(c,self.new,previous_manifest=self.old)
        with ThreadPoolExecutor(max_workers=2) as pool:
            first=pool.submit(publish);second=pool.submit(publish)
            results=[first.result(),second.result()]
        self.assertEqual(sum(r['changed'] for r in results),1)
        self.assertEqual(sum(r['compatibility_created'] for r in results),1)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM runtime_control.actor_catalog_activations').fetchone()[0],before+1)
    def test_detached_old_worker_cannot_reuse_retired_token(self):
        self.owner();result=self.publish()
        self.assertEqual(result['retired_map_owners'],1)
        self.assertIsNone(self.conn.execute('SELECT 1 FROM app_world.map_runtime_owner WHERE world_id=250 AND server_id=1 AND owner_token=%s FOR SHARE',('a'*64,)).fetchone())
        before,after=self.conn.execute('SELECT previous_token,replacement_token FROM runtime_control.actor_owner_retirements WHERE world_id=250 ORDER BY activation_id DESC LIMIT 1').fetchone()
        self.assertEqual(before,'a'*64);self.assertNotEqual(before,after)
        self.assertEqual(self.conn.execute('SELECT owner_token,backend_pid FROM app_world.map_runtime_owner WHERE world_id=250').fetchone(),(after,0))
    def test_late_failure_rolls_back_proof_selector_history_and_fence(self):
        from psycopg.errors import RaiseException
        self.owner();before=self.state()
        self.conn.execute("CREATE FUNCTION public.fail_actor_fence() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic late fence failure'; END $$")
        self.conn.execute('CREATE TRIGGER synthetic_actor_fence BEFORE UPDATE ON app_world.map_runtime_owner FOR EACH ROW EXECUTE FUNCTION public.fail_actor_fence()')
        try:
            with self.assertRaises(RaiseException):self.publish()
            self.assertEqual(self.state(),before)
        finally:
            self.conn.execute('DROP TRIGGER synthetic_actor_fence ON app_world.map_runtime_owner')
            self.conn.execute('DROP FUNCTION public.fail_actor_fence()')
    def test_runtime_can_only_read_current_target_projection(self):
        from psycopg.errors import InsufficientPrivilege
        self.publish()
        with self.conn.transaction(force_rollback=True):
            self.conn.execute('CREATE ROLE synthetic_actor_reader NOLOGIN')
            self.conn.execute('GRANT USAGE ON SCHEMA actor_compat TO synthetic_actor_reader')
            self.conn.execute('GRANT SELECT ON ALL TABLES IN SCHEMA actor_compat TO synthetic_actor_reader')
            self.conn.execute('SET LOCAL ROLE synthetic_actor_reader')
            self.assertEqual(self.conn.execute('SELECT count(*) FROM actor_compat.transfer_catalog_compatibility').fetchone(),(1,))
            self.assertFalse(self.conn.execute("SELECT has_table_privilege(current_user,'actor_compat.transfer_catalog_compatibility','UPDATE')").fetchone()[0])
            for query in ('SELECT * FROM runtime_control.actor_graph_compatibility',
                          "UPDATE runtime_control.actor_graph_compatibility SET target_manifest_sha256=repeat('0',64)"):
                with self.assertRaises(InsufficientPrivilege),self.conn.transaction():self.conn.execute(query)


if __name__=='__main__':unittest.main()
