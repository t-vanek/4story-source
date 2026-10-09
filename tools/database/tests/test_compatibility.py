import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import test_postgresql as fixtures
from activate_catalog import activate_catalog, MAPPING, PINNED
from generate_compatibility import generate
from generate_migrations import qi, REFERENCE_TABLES
from migrate import apply_migrations, connect, import_snapshot, load_snapshot

ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(os.getenv('FOURSTORY_DB_TESTS') == '1', 'requires explicit disposable PostgreSQL test database')
class CompatibilityVerification(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.getenv('PGDATABASE', '').startswith('fourstory_synthetic_'):
            raise RuntimeError('Refusing tests outside fourstory_synthetic_*')
        cls.conn = connect()
        cls.mapping = json.loads(MAPPING.read_text())
        apply_migrations(cls.conn, ROOT / 'database/postgresql')

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='fourstory-compatibility-synthetic-')
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)

    def fixture(self, tables=REFERENCE_TABLES, overrides=None):
        base = self.directory
        self.directory = base / uuid.uuid4().hex
        self.directory.mkdir()
        try:
            path = fixtures.PostgreSQLVerification.fixture(self, tables, overrides, raw_bytes=True)
        finally:
            self.directory = base
        manifest = json.loads(path.read_text())
        manifest['backups'] = [{k: a[k] for k in ('path', 'size_bytes', 'sha256')}
                               for a in json.loads(PINNED.read_text())['artifacts']
                               if a['path'] in ('TGAME_RAGEZONE.bak', 'TGLOBAL_RAGEZONE.bak')]
        path.write_text(json.dumps(manifest))
        return path

    def test_derived_monster_key_and_both_identifier_styles(self):
        path = self.fixture(overrides={'TMONSTERCHART': {'wID': 1234, 'wMonAttr': 56, 'bLevel': 7},
                                       'TMONATTRCHART': {'wID': 56, 'bLevel': 7, 'dwMaxHP': 1500},
                                       'TITEMCHART': {'wItemID': -1}})
        run = import_snapshot(self.conn, path, MAPPING)['run_id']
        manifest, loaded = load_snapshot(path, MAPPING)
        self.assertTrue(all('_byte_records' not in t for t in manifest['tables']))
        self.assertTrue(all('_byte_records' in t for t, _, _ in loaded))
        result = activate_catalog(self.conn, path)
        self.assertEqual((result['monster_attribute_rows'], result['source_monster_attribute_gaps']), (1, 0))
        self.assertEqual(len(result['current_chart_queries_verified']), 14)
        self.assertEqual(self.conn.execute('SELECT "wID","dwMaxHP" FROM game_compat."TMONATTRCHART"').fetchone(), (1234, 1500))
        self.assertEqual(self.conn.execute('SELECT wID,dwMaxHP FROM game_compat.TMONATTRCHART').fetchone(), (1234, 1500))
        self.assertEqual(self.conn.execute('SELECT "wID" FROM legacy_game."TMONATTRCHART" WHERE "_import_run_id"=%s', (run,)).fetchone()[0], 56)
        self.assertEqual(self.conn.execute('SELECT wItemID FROM game_compat.TITEMCHART').fetchone()[0], -1)
        self.assertEqual(activate_catalog(self.conn, path)['changed'], False)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM runtime_control.catalog_activations WHERE run_id=%s', (run,)).fetchone()[0], 1)
        # Activation has not changed source rows or byte sidecars.
        self.assertEqual(len(import_snapshot(self.conn, path, MAPPING)['skipped_verified_tables']), 15)

    def test_explicit_release_switch_and_missing_source_stats(self):
        first = self.fixture(overrides={'TMONSTERCHART': {'wID': 5}})
        original = import_snapshot(self.conn, first, MAPPING)['run_id']
        activate_catalog(self.conn, first)
        second = self.fixture(overrides={'TMONSTERCHART': {'wID': 18, 'wMonAttr': 58}})
        candidate = import_snapshot(self.conn, second, MAPPING)['run_id']
        self.assertEqual(self.conn.execute('SELECT wID FROM game_compat.TMONSTERCHART').fetchone()[0], 5)
        result = activate_catalog(self.conn, second)
        self.assertEqual((result['monster_attribute_rows'], result['source_monster_attribute_gaps']), (0, 1))
        self.assertEqual(self.conn.execute('SELECT monster_id,attribute_id FROM runtime_control.missing_monster_attributes').fetchone(), (18, 58))
        self.assertEqual(self.conn.execute('SELECT previous_run_id FROM runtime_control.catalog_activations WHERE run_id=%s', (candidate,)).fetchone()[0], original)
        # Rollback selects the previous verified manifest; never reverses original data.
        activate_catalog(self.conn, first)
        self.assertEqual(self.conn.execute('SELECT wID FROM game_compat.TMONSTERCHART').fetchone()[0], 5)

    def test_partial_unverified_and_wrong_backup_imports_are_rejected(self):
        partial = self.fixture(['TCLASSCHART'])
        import_snapshot(self.conn, partial, MAPPING)
        with self.assertRaisesRegex(RuntimeError, 'COMPLETE_REFERENCE_MANIFEST_REQUIRED'):
            activate_catalog(self.conn, partial)
        full = self.fixture()
        with self.assertRaisesRegex(RuntimeError, 'VERIFIED_IMPORT_REQUIRED'):
            activate_catalog(self.conn, full)
        run = import_snapshot(self.conn, full, MAPPING)['run_id']
        self.conn.execute("UPDATE reconstruction.import_runs SET status='failed' WHERE id=%s", (run,))
        with self.assertRaisesRegex(RuntimeError, 'VERIFIED_IMPORT_REQUIRED'):
            activate_catalog(self.conn, full)
        manifest = json.loads(full.read_text())
        manifest['backups'][0]['sha256'] = '0' * 64
        full.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(RuntimeError, 'PINNED_BACKUP_FINGERPRINT_MISMATCH'):
            activate_catalog(self.conn, full)

    def test_repeatable_read_catalog_survives_concurrent_publication(self):
        first = self.fixture(overrides={'TMONSTERCHART': {'wID': 5}})
        original = import_snapshot(self.conn, first, MAPPING)['run_id']
        activate_catalog(self.conn, first)
        second = self.fixture(overrides={'TMONSTERCHART': {'wID': 18}})
        candidate = import_snapshot(self.conn, second, MAPPING)['run_id']
        with connect() as reader:
            with reader.transaction():
                reader.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY')
                self.assertEqual(reader.execute('SELECT run_id FROM game_compat.catalog_release').fetchone()[0], original)
                self.assertEqual(reader.execute('SELECT wID FROM game_compat.TMONSTERCHART').fetchone()[0], 5)
                activate_catalog(self.conn, second)
                self.assertEqual(reader.execute('SELECT run_id FROM game_compat.catalog_release').fetchone()[0], original)
                self.assertEqual(reader.execute('SELECT wID FROM game_compat.TMONSTERCHART').fetchone()[0], 5)
            self.assertEqual(reader.execute('SELECT run_id FROM game_compat.catalog_release').fetchone()[0], candidate)
            self.assertEqual(reader.execute('SELECT wID FROM game_compat.TMONSTERCHART').fetchone()[0], 18)

    def test_target_drift_is_rejected_before_release_change(self):
        first = self.fixture()
        original = import_snapshot(self.conn, first, MAPPING)['run_id']
        activate_catalog(self.conn, first)
        second = self.fixture(overrides={'TMONSTERCHART': {'wID': 2}})
        candidate = import_snapshot(self.conn, second, MAPPING)['run_id']
        self.conn.execute('UPDATE legacy_game."TMONATTRCHART" SET "dwMaxHP"=99 WHERE "_import_run_id"=%s', (candidate,))
        with self.assertRaisesRegex(RuntimeError, 'CHECKPOINT_TARGET_DRIFT'):
            activate_catalog(self.conn, second)
        self.assertEqual(self.conn.execute('SELECT run_id FROM runtime_control.active_catalog').fetchone()[0], original)

    def test_query_contract_failure_rolls_back_selection_and_history(self):
        import psycopg
        first = self.fixture()
        original = import_snapshot(self.conn, first, MAPPING)['run_id']
        activate_catalog(self.conn, first)
        second = self.fixture(overrides={'TMONSTERCHART': {'wID': 2}})
        candidate = import_snapshot(self.conn, second, MAPPING)['run_id']
        with patch('activate_catalog.chart_queries', return_value=[('SyntheticBadColumn', 'SELECT unknown_contract_column FROM TMONSTERCHART')]):
            with self.assertRaises(psycopg.errors.UndefinedColumn):
                activate_catalog(self.conn, second)
        self.assertEqual(self.conn.execute('SELECT run_id FROM runtime_control.active_catalog').fetchone()[0], original)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM runtime_control.catalog_activations WHERE run_id=%s', (candidate,)).fetchone()[0], 0)

    def test_catalog_reader_cannot_write_or_access_original_layer(self):
        import psycopg
        path = self.fixture()
        import_snapshot(self.conn, path, MAPPING)
        activate_catalog(self.conn, path)
        role = qi('fourstory_synthetic_reader_' + uuid.uuid4().hex)
        self.conn.execute('CREATE ROLE ' + role + ' NOLOGIN')
        try:
            self.conn.execute('GRANT USAGE ON SCHEMA game_compat TO ' + role)
            self.conn.execute('GRANT SELECT ON ALL TABLES IN SCHEMA game_compat TO ' + role)
            with self.conn.transaction():
                self.conn.execute('SET LOCAL ROLE ' + role)
                self.assertEqual(self.conn.execute('SELECT count(*) FROM game_compat.TMONSTERCHART').fetchone()[0], 1)
            for statement in ('SELECT * FROM legacy_game."TMONATTRCHART"',
                              'UPDATE legacy_game."TMONATTRCHART" SET "dwMaxHP"=42',
                              'SELECT * FROM runtime_control.active_catalog'):
                with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                    with self.conn.transaction():
                        self.conn.execute('SET LOCAL ROLE ' + role)
                        self.conn.execute(statement)
            with self.assertRaises((psycopg.errors.InsufficientPrivilege, psycopg.errors.ObjectNotInPrerequisiteState)):
                with self.conn.transaction():
                    self.conn.execute('SET LOCAL ROLE ' + role)
                    self.conn.execute('UPDATE game_compat.TMONATTRCHART SET dwMaxHP=42')
        finally:
            self.conn.execute('DROP OWNED BY ' + role)
            self.conn.execute('DROP ROLE ' + role)


class CompatibilityArtifactVerification(unittest.TestCase):
    def test_generated_views_match_reviewed_migration(self):
        self.assertEqual(generate(json.loads(MAPPING.read_text())),
                         (ROOT / 'database/postgresql/006-current-chart-contracts.sql').read_text())


if __name__ == '__main__':
    unittest.main()
