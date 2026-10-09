"""Destructive-free integration checks ONLY in an explicitly selected synthetic DB."""
import base64
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from migrate import apply_migrations,connect,digest,import_snapshot,load_snapshot,table_hash,bytes_hash
from generate_migrations import qi

ROOT=Path(__file__).resolve().parents[3]
MAPPING=ROOT/'database/postgresql/mapping.json'


@unittest.skipUnless(os.getenv('FOURSTORY_DB_TESTS')=='1','requires explicit disposable PostgreSQL test database')
class PostgreSQLVerification(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.getenv('PGDATABASE','').startswith('fourstory_synthetic_'):
            raise RuntimeError('Refusing tests outside fourstory_synthetic_*')
        cls.conn=connect();cls.mapping=json.loads(MAPPING.read_text())
        apply_migrations(cls.conn,ROOT/'database/postgresql')

    @classmethod
    def tearDownClass(cls):cls.conn.close()

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='fourstory-synthetic-')
        self.addCleanup(self.tmp.cleanup);self.directory=Path(self.tmp.name)

    def fixture(self,tables,overrides=None,raw_bytes=False):
        manifest={'metadata_sha256':self.mapping['metadata_sha256'],'fixture':str(uuid.uuid4()),'backups':[],'tables':[]}
        for name in tables:
            model=next(t for t in self.mapping['tables'] if t['reference_import_allowed'] and t['table']==name)
            row={}
            for col in model['column_mapping']:
                typ=col['postgresql_type']
                row[col['source']]=None if col['nullable'] else ('Synthetic Žluťoučký 韓' if typ=='text' else
                    '2000-01-01T00:00:00.003' if typ.startswith('timestamp') else '1.25' if typ in ('real','double precision') else 0)
            row.update((overrides or {}).get(name,{}));rows=[row]
            data=json.dumps(rows,ensure_ascii=False).encode();filename=name+'.json';(self.directory/filename).write_bytes(data)
            table={k:model[k] for k in ('database','schema','table')}
            table.update(file=filename,row_count=1,file_sha256=digest(data),value_sha256=table_hash(rows,model['column_mapping']))
            if raw_bytes:
                records=[{'row_number':1,'column':c['source'],'bytes':None if row[c['source']] is None else
                          base64.b64encode(row[c['source']].encode('utf-16le')).decode()}
                         for c in model['column_mapping'] if c['postgresql_type']=='text']
                byte_data=json.dumps(records).encode();byte_file=name+'.bytes.json';(self.directory/byte_file).write_bytes(byte_data)
                table.update(text_bytes_file=byte_file,text_bytes_file_sha256=digest(byte_data),text_bytes_value_sha256=bytes_hash(records))
            manifest['tables'].append(table)
        path=self.directory/'manifest.json';path.write_text(json.dumps(manifest));return path

    def test_migrations_are_repeatable_and_detect_checksum_drift(self):
        self.assertEqual(apply_migrations(self.conn,ROOT/'database/postgresql'),[])
        path=self.directory/'001-import-control.sql';path.write_text('-- changed')
        with self.assertRaisesRegex(RuntimeError,'CHECKSUM_CHANGED'):apply_migrations(self.conn,self.directory)

    def test_byte_preservation_unicode_and_unsigned_word_projection(self):
        path=self.fixture(['TITEMCHART'],{'TITEMCHART':{'wItemID':-1,'bStack':255}},raw_bytes=True)
        result=import_snapshot(self.conn,path,MAPPING)
        self.assertEqual(result['canonical_items']['rows'],1)
        item=self.conn.execute('SELECT item_id,display_name FROM content.item_templates WHERE release_id=%s',(result['run_id'],)).fetchone()
        self.assertEqual(item,(65535,'Synthetic Žluťoučký 韓'))
        self.assertTrue(result['verified_tables'][0]['source_text_bytes_verified'])
        repeated=import_snapshot(self.conn,path,MAPPING)
        self.assertEqual(repeated['run_id'],result['run_id'])
        self.assertEqual(repeated['skipped_verified_tables'],['TITEMCHART'])

    def test_file_tampering_and_duplicate_primary_key_are_rejected(self):
        path=self.fixture(['TCLASSCHART']);manifest=json.loads(path.read_text());table=manifest['tables'][0]
        file=self.directory/table['file'];rows=json.loads(file.read_text());file.write_text('[]')
        with self.assertRaisesRegex(RuntimeError,'SNAPSHOT_FILE_CHANGED'):load_snapshot(path,MAPPING)
        rows*=2;data=json.dumps(rows).encode();file.write_bytes(data)
        table.update(row_count=2,file_sha256=digest(data),value_sha256=table_hash(rows,next(t for t in self.mapping['tables'] if t['table']=='TCLASSCHART')['column_mapping']))
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(RuntimeError,'DUPLICATE_PRIMARY_KEY'):load_snapshot(path,MAPPING)

    def test_source_integer_ranges_and_nulls_are_validated(self):
        path=self.fixture(['TCLASSCHART'],{'TCLASSCHART':{'bClassID':256}})
        with self.assertRaisesRegex(RuntimeError,'TINYINT_RANGE'):load_snapshot(path,MAPPING)
        path=self.fixture(['TCLASSCHART'],{'TCLASSCHART':{'bClassID':None}})
        with self.assertRaisesRegex(RuntimeError,'NOT_NULL'):load_snapshot(path,MAPPING)

    def test_table_failure_rolls_back_and_resume_preserves_checkpoint(self):
        path=self.fixture(['TCLASSCHART','TRACECHART'],{'TRACECHART':{'bRaceID':7}})
        self.conn.execute('ALTER TABLE legacy_game."TRACECHART" ADD CONSTRAINT synthetic_failure CHECK ("bRaceID"<>7)')
        try:
            with self.assertRaisesRegex(RuntimeError,'IMPORT_FAILED_23514'):import_snapshot(self.conn,path,MAPPING)
            run=self.conn.execute("SELECT id FROM reconstruction.import_runs WHERE status='failed' ORDER BY id DESC LIMIT 1").fetchone()[0]
            self.assertEqual(self.conn.execute('SELECT count(*) FROM reconstruction.checkpoints WHERE run_id=%s',(run,)).fetchone()[0],1)
            self.assertEqual(self.conn.execute('SELECT count(*) FROM legacy_game."TRACECHART" WHERE "_import_run_id"=%s',(run,)).fetchone()[0],0)
            code=self.conn.execute('SELECT error_code FROM reconstruction.failures WHERE run_id=%s',(run,)).fetchone()[0]
            self.assertEqual(code,'23514')
        finally:
            self.conn.execute('ALTER TABLE legacy_game."TRACECHART" DROP CONSTRAINT synthetic_failure')
        result=import_snapshot(self.conn,path,MAPPING)
        self.assertEqual(result['run_id'],run)
        self.assertEqual(result['skipped_verified_tables'],['TCLASSCHART'])
        self.assertEqual(result['verified_tables'][0]['table'],'TRACECHART')

    def test_modified_checkpoint_target_is_rejected(self):
        path=self.fixture(['TCLASSCHART']);result=import_snapshot(self.conn,path,MAPPING)
        self.conn.execute('UPDATE legacy_game."TCLASSCHART" SET "wSTR"=99 WHERE "_import_run_id"=%s',(result['run_id'],))
        with self.assertRaisesRegex(RuntimeError,'CHECKPOINT_TARGET_DRIFT'):import_snapshot(self.conn,path,MAPPING)

    def test_postgresql_tinyint_constraint_is_enforced(self):
        model=next(t for t in self.mapping['tables'] if t['table']=='TCLASSCHART')
        path=self.fixture(['TCLASSCHART']);result=import_snapshot(self.conn,path,MAPPING)
        import psycopg
        with self.assertRaises(psycopg.errors.CheckViolation):
            with self.conn.transaction():
                self.conn.execute('UPDATE legacy_game."TCLASSCHART" SET "bClassID"=256 WHERE "_import_run_id"=%s',(result['run_id'],))


if __name__=='__main__':unittest.main()
