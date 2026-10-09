import datetime
import pathlib
import sys
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from forensics import norm,definitions,cpp_literals
from migrate import table_hash,bytes_hash


class LexicalEvidenceTests(unittest.TestCase):
    def test_normalization_keeps_literal_case_and_space(self):
        self.assertEqual(norm("SELECT [Foo] -- noise\nFROM dbo.T"),norm('select "foo" from dbo.t'))
        self.assertNotEqual(norm("SELECT 'Mixed  Case'"),norm("SELECT 'mixed case'"))

    def test_batch_separator_does_not_change_object(self):
        a=definitions('CREATE VIEW [dbo].[V] AS SELECT 1\nGO\n-- exported footer')[0]
        b=definitions('CREATE VIEW dbo.V AS SELECT 1')[0]
        self.assertEqual(a['normalized_sha256'],b['normalized_sha256'])

    def test_fake_ddl_in_comments_or_literals_is_not_an_object(self):
        rows=definitions("-- CREATE TABLE bad(x int)\nCREATE VIEW dbo.good AS SELECT 'CREATE TABLE fake(x int)'")
        self.assertEqual([r['name'] for r in rows],['dbo.good'])

    def test_cpp_literals_handle_comments_continuations_and_raw_strings(self):
        source='// "FROM wrong"\n auto q="SELECT x " "FROM T"; auto r=R"sql(SELECT y FROM U)sql";'
        self.assertEqual([r['value'] for r in cpp_literals(source)],['SELECT x FROM T','SELECT y FROM U'])

    def test_multiset_hash_preserves_duplicate_and_null_information(self):
        cols=[{'source':'x','postgresql_type':'integer'}]
        rows=[{'x':1},{'x':None}]
        self.assertEqual(table_hash(rows,cols),table_hash(list(reversed(rows)),cols))
        self.assertNotEqual(table_hash(rows,cols),table_hash(rows+[rows[0]],cols))

    def test_real_hash_uses_float32_and_datetime_hash_keeps_precision(self):
        cols=[{'source':'x','postgresql_type':'real'}]
        self.assertEqual(table_hash([{'x':'0.10000000149011612'}],cols),table_hash([{'x':0.1}],cols))
        dates=[{'source':'x','postgresql_type':'timestamp without time zone'}]
        self.assertEqual(table_hash([{'x':'2019-01-27T16:01:55.003'}],dates),
                         table_hash([{'x':datetime.datetime(2019,1,27,16,1,55,3000)}],dates))

    def test_original_encoding_bytes_have_independent_fingerprint(self):
        a=[{'row_number':1,'column':'name','bytes':'6Q=='}]
        b=[{'row_number':1,'column':'name','bytes':b'\xe9'}]
        self.assertEqual(bytes_hash(a),bytes_hash(b))


if __name__=='__main__':unittest.main()
