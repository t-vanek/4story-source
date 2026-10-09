import json
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import test_compatibility as fixtures
from activate_actor_catalog import activate_actor_catalog
from activate_catalog import MAPPING
from generate_migrations import ACTOR_REFERENCE_KEYS
from migrate import import_snapshot, digest, table_hash

@unittest.skipUnless(os.getenv('FOURSTORY_DB_TESTS')=='1','requires isolated PostgreSQL')
class ActorCatalogVerification(unittest.TestCase):
    setUpClass=classmethod(fixtures.CompatibilityVerification.setUpClass.__func__)
    tearDownClass=classmethod(fixtures.CompatibilityVerification.tearDownClass.__func__)
    setUp=fixtures.CompatibilityVerification.setUp
    fixture=fixtures.CompatibilityVerification.fixture
    def actor_fixture(self,overrides=None):
        return self.fixture(tables=[t for _,_,t in sorted(ACTOR_REFERENCE_KEYS)],overrides=overrides)
    def test_independent_idempotent_publication(self):
        query='SELECT (SELECT run_id FROM runtime_control.active_catalog),(SELECT run_id FROM runtime_control.character_catalog),(SELECT run_id FROM runtime_control.routing_catalog)'
        before=self.conn.execute(query).fetchone()
        path=self.actor_fixture();import_snapshot(self.conn,path,MAPPING)
        result=activate_actor_catalog(self.conn,path)
        self.assertEqual((result['reference_tables'],result['verified_source_rows']),(4,4))
        self.assertFalse(activate_actor_catalog(self.conn,path)['changed'])
        self.assertEqual(self.conn.execute(query).fetchone(),before)
    def test_partial_and_wrong_backup_are_rejected(self):
        partial=self.fixture(tables=['TITEMMAGICCHART']);import_snapshot(self.conn,partial,MAPPING)
        with self.assertRaisesRegex(RuntimeError,'COMPLETE_ACTOR_MANIFEST_REQUIRED'):activate_actor_catalog(self.conn,partial)
        path=self.actor_fixture();manifest=json.loads(path.read_text());manifest['backups'][0]['sha256']='0'*64;path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(RuntimeError,'PINNED_BACKUP_FINGERPRINT_MISMATCH'):activate_actor_catalog(self.conn,path)
    def test_target_drift_cannot_switch_selector_or_append_history(self):
        first=self.actor_fixture();import_snapshot(self.conn,first,MAPPING);previous=activate_actor_catalog(self.conn,first)['run_id']
        history=self.conn.execute('SELECT count(*) FROM runtime_control.actor_catalog_activations').fetchone()[0]
        second=self.actor_fixture({"TSKILLPOINTCHART":{"bSkillPoint":7}});result=import_snapshot(self.conn,second,MAPPING)
        self.conn.execute('UPDATE legacy_game."TITEMMAGICCHART" SET "wMaxValue"=123 WHERE "_import_run_id"=%s',(result['run_id'],))
        with self.assertRaisesRegex(RuntimeError,'CHECKPOINT_TARGET_DRIFT'):activate_actor_catalog(self.conn,second)
        self.assertEqual(self.conn.execute('SELECT run_id FROM runtime_control.actor_catalog').fetchone()[0],previous)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM runtime_control.actor_catalog_activations').fetchone()[0],history)

    def replace_rows(self,path,table_name,rows):
        manifest=json.loads(path.read_text())
        t=next(t for t in manifest['tables'] if t['table']==table_name)
        model=next(t for t in self.mapping['tables'] if t['database']=='TGAME_RAGEZONE' and t['table']==table_name)
        data=json.dumps(rows).encode();(path.parent/t['file']).write_bytes(data)
        t.update(row_count=len(rows),file_sha256=digest(data),value_sha256=table_hash(rows,model['column_mapping']))
        path.write_text(json.dumps(manifest))
    def test_ambiguous_grades_cannot_be_published(self):
        path=self.actor_fixture();manifest=json.loads(path.read_text())
        t=next(t for t in manifest['tables'] if t['table']=='TITEMGRADECHART')
        row=json.loads((path.parent/t['file']).read_text())[0]
        self.replace_rows(path,'TITEMGRADECHART',[row,dict(row,bGrade=7)])
        import_snapshot(self.conn,path,MAPPING)
        with self.assertRaisesRegex(RuntimeError,'AMBIGUOUS_ITEM_GRADE_LEVEL'):activate_actor_catalog(self.conn,path)
    def test_empty_attribute_fallback_cannot_be_published(self):
        path=self.actor_fixture();self.replace_rows(path,'TITEMATTRCHART',[])
        import_snapshot(self.conn,path,MAPPING)
        with self.assertRaisesRegex(RuntimeError,'EMPTY_ITEM_ATTRIBUTE_CHART'):activate_actor_catalog(self.conn,path)
    def test_old_actor_release_retained_but_not_stat_capable(self):
        path=self.fixture(tables=['TITEMMAGICCHART','TSKILLPOINTCHART'])
        run=import_snapshot(self.conn,path,MAPPING)['run_id']
        with self.conn.transaction(force_rollback=True):
            self.conn.execute('INSERT INTO runtime_control.actor_catalog(singleton,run_id) VALUES(true,%s) '
                              'ON CONFLICT(singleton) DO UPDATE SET run_id=EXCLUDED.run_id',(run,))
            self.assertEqual(self.conn.execute('SELECT count(*) FROM actor_compat.statistics_release').fetchone(),(0,))
            self.assertEqual(self.conn.execute('SELECT count(*) FROM actor_compat."TITEMMAGICCHART"').fetchone(),(1,))
        with self.assertRaisesRegex(RuntimeError,'COMPLETE_ACTOR_MANIFEST_REQUIRED'):activate_actor_catalog(self.conn,path)
