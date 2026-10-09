#!/usr/bin/env python3
"""Bounded 032 -> 033 migration/receipt verification in a new owned PG database.

Only synthetic structural rows are used here. Runtime owner, transfer codec and
gameplay evidence comes from run_native_verification, not these SQL fixtures.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import uuid
from disposable_environment import verify_container
from migrate import apply_migrations


def main():
    import psycopg
    from psycopg import sql
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--through-cancellation',action='store_true',help='Also verify additive migration034 preserves all four recovery contracts')
    args=parser.parse_args();repo=Path(__file__).resolve().parents[2]
    state=json.loads((args.work/'state.json').read_text());verify_container(state,state['pg_container'])
    values=dict(line.split('=',1) for line in (args.work/'postgres.env').read_text().splitlines())
    admin=dict(host='127.0.0.1',port=state['pg_port'],user='postgres',password=values['POSTGRES_PASSWORD'],autocommit=True)
    db='fourstory_posture_upgrade_'+uuid.uuid4().hex[:12];report={'status':'failed','checks':[]}
    def check(ok,label):
        if not ok:raise RuntimeError(label)
        report['checks'].append(label)
    with psycopg.connect(dbname='postgres',**admin) as conn:conn.execute(sql.SQL('CREATE DATABASE {}').format(sql.Identifier(db)))
    try:
        with psycopg.connect(dbname=db,**admin) as c,tempfile.TemporaryDirectory(prefix='fourstory-pre033-') as tmp:
            path=Path(tmp)
            for source in (repo/'database/postgresql').glob('[0-9][0-9][0-9]-*.sql'):
                if int(source.name[:3])<=32:shutil.copyfile(source,path/source.name)
            check(len(apply_migrations(c,path))==32,'unchanged migrations 001-032 apply')
            c.execute('INSERT INTO app_global."TACCOUNT_PW"("dwUserID","szUserID") VALUES(1,\'MigrationOnly\')')
            c.execute('INSERT INTO app_global."TGROUP"("bGroupID","bType","szNAME") VALUES(1,0,\'MigrationOnly\')')
            c.execute('INSERT INTO app_world.worlds VALUES(1,0,735812)')
            required=c.execute("SELECT column_name,data_type FROM information_schema.columns WHERE table_schema='app_world' AND table_name='TCHARTABLE' AND is_nullable='NO' AND column_default IS NULL AND is_identity='NO' ORDER BY ordinal_position").fetchall()
            for version in (1,2,3):
                row={name:('Migration'+str(version) if typ=='text' else 0) for name,typ in required}
                row.update(bWorldID=1,dwUserID=1,bSlot=version-1,bLevel=1,dwHP=1,dwMP=1)
                cid=c.execute(sql.SQL('INSERT INTO app_world."TCHARTABLE" ({}) VALUES ({}) RETURNING "dwCharID"').format(sql.SQL(',').join(map(sql.Identifier,row)),sql.SQL(',').join(sql.Placeholder() for _ in row)),list(row.values())).fetchone()[0]
                c.execute("INSERT INTO app_world.map_checkpoints(world_id,char_id,user_id,server_id,session_key,owner_token,connection_id,revision,fingerprint,recovery_contract,core_state,outcome) VALUES(1,%s,1,1,1,%s,1,0,%s,1,app_world.map_core_state(1::smallint,%s),'logout')",(cid,'a'*64,'b'*64,cid))
                if version==2:
                    body=b'0123456789'
                    c.execute('UPDATE app_world.map_checkpoints SET recovery_contract=2,transfer_body=%s,transfer_hash=%s,character_manifest=%s,routing_manifest=%s,actor_manifest=%s WHERE char_id=%s',(body,hashlib.sha256(body).hexdigest(),'c'*64,'d'*64,'e'*64,cid))
                if version==3:c.execute('UPDATE app_world.map_checkpoints SET recovery_contract=3,skill_state=app_world.map_skill_state(world_id,char_id) WHERE char_id=%s',(cid,))
            c.execute("INSERT INTO app_world.equipment_operations(operation_id,world_id,char_id,server_id,owner_token,connection_id,authority_epoch,state_contract,request,changed_items,core_fingerprint) VALUES(1,1,3,1,%s,1,0,3,decode('ff00fe0001','hex'),0,%s)",('a'*64,'b'*64))
            receipts=c.execute('SELECT * FROM migration_control.applied ORDER BY name').fetchall()
            checkpoints=c.execute('SELECT to_jsonb(p) FROM app_world.map_checkpoints p ORDER BY char_id').fetchall()
            equipment=c.execute('SELECT to_jsonb(o) FROM app_world.equipment_operations o').fetchall()
            source=repo/'database/postgresql/033-native-maintained-postures.sql';shutil.copyfile(source,path/source.name)
            check(apply_migrations(c,path)==[source.name],'upgrade applies only additive migration033')
            check(apply_migrations(c,path)==[],'repeated migration application is a no-op')
            check(c.execute("SELECT * FROM migration_control.applied WHERE name<'033' ORDER BY name").fetchall()==receipts,'all 32 prior hashes and timestamps remain exact')
            check(c.execute("SELECT to_jsonb(p)-'maintain_state' FROM app_world.map_checkpoints p ORDER BY char_id").fetchall()==checkpoints,'v1 v2 v3 checkpoint bytes fields and saved_at remain unchanged')
            check(c.execute("SELECT to_jsonb(o)-'before_effects'-'after_effects' FROM app_world.equipment_operations o").fetchall()==equipment,'historical equipment receipt remains unchanged')
            check(c.execute('SELECT bool_and(app_world.map_checkpoint_matches(p)) FROM app_world.map_checkpoints p').fetchone()==(True,),'all old contracts still match after upgrade')
            c.execute('INSERT INTO app_world.map_maintained_effects VALUES(1,3,0,131,1,0,1,3,1,3,4)')
            check(c.execute('SELECT app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=3').fetchone()==(False,),'old fresh receipt cannot ignore an added maintained effect')
            c.execute('UPDATE app_world.map_checkpoints SET recovery_contract=4,maintain_state=app_world.map_maintain_state(world_id,char_id) WHERE char_id=3')
            check(c.execute('SELECT app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=3').fetchone()==(True,),'v4 exactly binds permanent zero-remaining posture')
            for field in ('skill_id','skill_level','remaining','attack_type','attack_id','host_type','host_id','attack_country'):
                with c.transaction(force_rollback=True):
                    c.execute(sql.SQL('UPDATE app_world.map_maintained_effects SET {}={}+1 WHERE char_id=3').format(sql.Identifier(field),sql.Identifier(field)))
                    check(c.execute('SELECT app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=3').fetchone()==(False,),'v4 detects drift in '+field)
            c.execute('INSERT INTO app_world.map_maintained_effects VALUES(1,2,0,132,1,0,1,2,1,2,4)')
            check(c.execute('SELECT app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=2').fetchone()==(True,),'graph recovery continues to ignore stale normalized effects')
            check(c.execute("SELECT has_table_privilege('public','app_world.map_maintained_effects','INSERT') OR has_function_privilege('public','app_world.map_maintain_state(smallint,integer)','EXECUTE')").fetchone()==(False,),'new maintained mutation/comparison have no PUBLIC grant')
            if args.through_cancellation:
                receipts=c.execute('SELECT * FROM migration_control.applied ORDER BY name').fetchall()
                checkpoints=c.execute('SELECT to_jsonb(p) FROM app_world.map_checkpoints p ORDER BY char_id').fetchall()
                effects=c.execute('SELECT * FROM app_world.map_maintained_effects ORDER BY char_id,ordinal').fetchall()
                equipment=c.execute('SELECT to_jsonb(o) FROM app_world.equipment_operations o').fetchall()
                source=repo/'database/postgresql/034-native-effect-cancellation.sql';shutil.copyfile(source,path/source.name)
                check(apply_migrations(c,path)==[source.name],'effect upgrade applies only additive migration034')
                check(apply_migrations(c,path)==[],'effect upgrade reapplies without changing receipts')
                check(c.execute("SELECT * FROM migration_control.applied WHERE name<'034' ORDER BY name").fetchall()==receipts,'all 33 prior migration receipts remain exact')
                check(c.execute('SELECT to_jsonb(p) FROM app_world.map_checkpoints p ORDER BY char_id').fetchall()==checkpoints,'effect upgrade preserves complete core skill graph and maintained checkpoints')
                check(c.execute('SELECT * FROM app_world.map_maintained_effects ORDER BY char_id,ordinal').fetchall()==effects,'effect upgrade preserves ordered native maintained fields')
                check(c.execute('SELECT to_jsonb(o) FROM app_world.equipment_operations o').fetchall()==equipment,'effect upgrade preserves historical equipment operation bytes')
                check(c.execute("SELECT has_table_privilege('public','app_world.maintained_effect_operations','INSERT') OR has_sequence_privilege('public','app_world.maintained_effect_operations_operation_id_seq','USAGE')").fetchone()==(False,),'effect ledger and its identity allocator have no PUBLIC mutation grant')
            report.update(status='passed',scope='Synthetic structural migration/receipt invariants; native runtime and original-client acceptance are separate.')
    finally:
        with psycopg.connect(dbname='postgres',**admin) as conn:conn.execute(sql.SQL('DROP DATABASE {} WITH (FORCE)').format(sql.Identifier(db)))
        report['owned_database_removed']=True;args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('Posture upgrade:',len(report['checks']),'checks passed')


if __name__=='__main__':main()
