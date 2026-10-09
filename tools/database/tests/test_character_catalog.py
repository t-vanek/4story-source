import json
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import test_compatibility as fixtures
from activate_character_catalog import activate_character_catalog
from activate_catalog import MAPPING
from generate_migrations import CHARACTER_REFERENCE_KEYS
from migrate import import_snapshot

@unittest.skipUnless(os.getenv('FOURSTORY_DB_TESTS')=='1','requires isolated PostgreSQL')
class CharacterCatalogVerification(unittest.TestCase):
    setUpClass = classmethod(fixtures.CompatibilityVerification.setUpClass.__func__)
    tearDownClass = classmethod(fixtures.CompatibilityVerification.tearDownClass.__func__)
    setUp = fixtures.CompatibilityVerification.setUp
    fixture = fixtures.CompatibilityVerification.fixture

    def character_fixture(self, overrides=None):
        values={'TVETERANCHART':{'bID':1,'bLevel':1}, 'TLEVELCHART':{'bLevel':0}}
        values.update(overrides or {})
        return self.fixture(tables=[t for _,_,t in sorted(CHARACTER_REFERENCE_KEYS)],overrides=values)

    def test_complete_reconciled_release_and_reactivation(self):
        path=self.character_fixture()
        import_snapshot(self.conn,path,MAPPING)
        result=activate_character_catalog(self.conn,path)
        self.assertEqual(result['reference_tables'],22)
        self.assertEqual(result['verified_source_rows'],22)
        self.assertFalse(activate_character_catalog(self.conn,path)['changed'])
        self.assertEqual(self.conn.execute('SELECT "bLevel" FROM character_compat."TVETERANCHART"').fetchone()[0],1)

    def test_missing_reference_and_drift_cannot_replace_release(self):
        path=self.character_fixture()
        run=import_snapshot(self.conn,path,MAPPING)['run_id']
        activate_character_catalog(self.conn,path)
        incomplete=self.fixture()
        import_snapshot(self.conn,incomplete,MAPPING)
        with self.assertRaisesRegex(RuntimeError,'COMPLETE_CHARACTER_MANIFEST_REQUIRED'):
            activate_character_catalog(self.conn,incomplete)
        self.conn.execute('UPDATE legacy_game."TSTARTSKILL" SET "bLevel"=1 WHERE "_import_run_id"=%s',(run,))
        with self.assertRaisesRegex(RuntimeError,'CHECKPOINT_TARGET_DRIFT'):
            activate_character_catalog(self.conn,path)

    def test_missing_starter_is_not_fabricated(self):
        path=self.character_fixture({'TSTARTITEMCHART':{'bChartType':1,'wItemID':22}})
        import_snapshot(self.conn,path,MAPPING)
        old=self.conn.execute('SELECT run_id FROM runtime_control.character_catalog').fetchall()
        with self.assertRaisesRegex(RuntimeError,'CHARACTER_STARTER_GRAPH_INCOMPLETE'):
            activate_character_catalog(self.conn,path)
        self.assertEqual(self.conn.execute('SELECT run_id FROM runtime_control.character_catalog').fetchall(),old)
