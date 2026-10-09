import json
import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import test_compatibility as fixtures
from activate_catalog import MAPPING
from activate_routing_catalog import activate_routing_catalog
from generate_migrations import ROUTING_REFERENCE_KEYS
from migrate import import_snapshot, digest, table_hash

@unittest.skipUnless(os.getenv('FOURSTORY_DB_TESTS')=='1','requires isolated PostgreSQL')
class RoutingCatalogVerification(unittest.TestCase):
    setUpClass=classmethod(fixtures.CompatibilityVerification.setUpClass.__func__)
    tearDownClass=classmethod(fixtures.CompatibilityVerification.tearDownClass.__func__)
    setUp=fixtures.CompatibilityVerification.setUp
    fixture=fixtures.CompatibilityVerification.fixture

    def routing_fixture(self,overrides=None):
        return self.fixture(tables=[t for _,_,t in sorted(ROUTING_REFERENCE_KEYS)],overrides=overrides)

    def test_complete_release_is_independent_and_idempotent(self):
        before=self.conn.execute('SELECT (SELECT run_id FROM runtime_control.active_catalog),'
                                 '(SELECT run_id FROM runtime_control.character_catalog)').fetchone()
        path=self.routing_fixture();import_snapshot(self.conn,path,MAPPING)
        result=activate_routing_catalog(self.conn,path)
        self.assertEqual(result['reference_tables'],4)
        self.assertEqual(result['unmapped_source_channel_cells'],0)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM route_compat."TSVRCHART"').fetchone()[0],1)
        self.assertFalse(activate_routing_catalog(self.conn,path)['changed'])
        self.assertEqual(self.conn.execute('SELECT (SELECT run_id FROM runtime_control.active_catalog),'
                                          '(SELECT run_id FROM runtime_control.character_catalog)').fetchone(),before)

    def test_missing_cell_is_preserved_without_synthetic_owner(self):
        path=self.routing_fixture({'TCHANNELCHART':{'wUnitID':17}})
        import_snapshot(self.conn,path,MAPPING);result=activate_routing_catalog(self.conn,path)
        self.assertEqual(result['unmapped_source_channel_cells'],1)
        self.assertEqual(self.conn.execute('SELECT "wUnitID" FROM route_compat."TCHANNELCHART"').fetchone()[0],17)

    def test_ambiguous_cell_rolls_back_publication_and_history(self):
        first=self.routing_fixture();import_snapshot(self.conn,first,MAPPING)
        previous=activate_routing_catalog(self.conn,first)['run_id']
        history=self.conn.execute('SELECT count(*) FROM runtime_control.routing_catalog_activations').fetchone()[0]
        candidate=self.routing_fixture();manifest=json.loads(candidate.read_text())
        for entry in manifest['tables']:
            if entry['table'] not in ('TMAPCHART','TUNITCHART'):continue
            path=candidate.parent/entry['file'];row=json.loads(path.read_text())[0]
            rows=[dict(row,bServerID=1),dict(row,bServerID=2)]
            data=json.dumps(rows).encode();path.write_bytes(data)
            model=next(t for t in self.mapping['tables'] if t['table']==entry['table'])
            entry.update(row_count=2,file_sha256=digest(data),value_sha256=table_hash(rows,model['column_mapping']))
        candidate.write_text(json.dumps(manifest));import_snapshot(self.conn,candidate,MAPPING)
        with self.assertRaisesRegex(RuntimeError,'AMBIGUOUS_ROUTING_OWNERSHIP'):
            activate_routing_catalog(self.conn,candidate)
        self.assertEqual(self.conn.execute('SELECT run_id FROM runtime_control.routing_catalog').fetchone()[0],previous)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM runtime_control.routing_catalog_activations').fetchone()[0],history)
