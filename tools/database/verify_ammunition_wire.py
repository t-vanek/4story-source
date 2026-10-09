"""Single-hit bow/crossbow consumption on disposable characters and real daemons."""
import struct
import time
from psycopg import sql
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent


def verify_ammunition(conn,cid,start,enter,login_port,map_port,until):
    checks=[]
    def check(ok,label):
        if not ok:raise RuntimeError('Native ammunition: '+label)
        checks.append(label)
    for weapon,ammo,delay in [(701,8401,1.6),(801,8402,1.9)]:
        old_rows=conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
        row=conn.execute('SELECT * FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=2',(cid,))
        old=dict(zip((c.name for c in row.description),row.fetchone()))
        item_id=None;s=None
        try:
            conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=%s,"dwDuraMax"=100,"dwDuraCur"=100 WHERE "dlID"=%s',(weapon,old['dlID']))
            item_id=seed_graph_reagent(conn,cid,True)
            conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=%s,"bCount"=2 WHERE "dlID"=%s',(ammo,item_id))
            conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,32,1,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "dwRemainTick"=0',(cid,))
            _,key=start(login_port,cid);s,char=enter(map_port,cid,key)
            desc=next(item for bag,item,magic in char['items'] if bag==255 and item[0]==2)
            cs,ss=3,4
            def cast(loop):
                nonlocal cs
                header=struct.pack('<IBBHHfffB',cid,1,1,0,32,0,0,0,1) if loop else struct.pack('<IBBHHBIIfffB',cid,1,1,0,32,0,0,0,0,0,0,1)
                s.sendall(frame(header+struct.pack('<IBB',cid+100000,2,1),0x5372 if loop else 0x52b4,cs));cs+=1
            def reply():
                nonlocal ss
                result=read_packet(s,ss);ss+=1;return result
            cast(False)
            expected=list(desc);expected[6]=1
            check(reply()==(0x52aa,b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*expected)),f'weapon{weapon} emits exact ammo UPDATEITEM after confirmed commit')
            check(reply()==(0x52a9,b'\x00'),'ammo MOVEITEM follows private inventory response')
            op,body=reply();check(op==0x52b5 and len(body)==67 and body[0]==0 and body[-5:]==struct.pack('<IB',cid+100000,2),
                                 'ordinary single-hit success retains original target identity')
            check(conn.execute('SELECT "bCount" FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(item_id,)).fetchone()==(1,),
                  'single-shot ammunition decrement is durable before success')
            cast(True);op,body=reply();check(op==0x5373 and body[0]==6,'loop cannot bypass ordinary ammunition cooldown')
            time.sleep(delay);cast(True)
            check(reply()==(0x52ac,b'\xff\x02'),'loop deletes last arrow or bolt with exact source DELITEM')
            check(reply()==(0x52a9,b'\x00'),'last-ammo MOVEITEM precedes loop response')
            op,body=reply();check(op==0x5373 and len(body)==50 and body[0]==0,'loop single-hit success follows durable ammo deletion')
            time.sleep(delay);cast(True);op,body=reply()
            check(op==0x5373 and len(body)==45 and body[0]==9,'exhausted single-hit loop returns source UNSUITWEAPON')
            check(conn.execute('SELECT count(*),min(state_contract),max(state_contract) FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=%s AND consumption_kind=\'ammunition\'',(cid,item_id)).fetchone()==(2,3,3),
                  'fresh ammunition appends exactly two contract-3 consumption receipts')
            s.close();s=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,), 'ammo logout completes')
            _,key=start(login_port,cid);s,restored=enter(map_port,cid,key)
            check(all(not(bag==255 and item[0]==2) for bag,item,magic in restored['items']), 'ammo relogin never restores a depleted stack')
            s.close();s=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,), 'ammo relogin closes')
        finally:
            if s is not None:s.close()
            if item_id is not None:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(item_id,))
            conn.execute(sql.SQL('UPDATE app_world."TITEMTABLE" SET {} WHERE "dlID"=%s').format(
                sql.SQL(',').join(sql.SQL('{}=%s').format(sql.Identifier(c)) for c in old)),[*old.values(),old['dlID']])
        check(old_rows==conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall(),
              'ammo fixture restores exact original inventory after deleting only its synthetic stack')
    return {'status':'passed','checks':checks,'scope':'Original source single-hit skill32, bow701/arrow8401 and crossbow801/bolt8402 on a synthetic account; no target damage or original-client execution.'}
