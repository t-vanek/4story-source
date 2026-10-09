"""Source-derived encrypted multi-target, multi-stack arrow and bolt casts."""
import select
import struct
import time
from psycopg import sql
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent


def verify_ammunition_batches(conn,cid,start,enter,login_port,map_port,until):
    checks=[]
    def check(ok,label):
        if not ok:raise RuntimeError('Ammunition batch: '+label)
        checks.append(label)
    for weapon,ammo,hits,delay in [(701,8401,4,1.6),(801,8402,16,1.9)]:
        old_rows=conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
        row=conn.execute('SELECT * FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=2',(cid,))
        old=dict(zip((c.name for c in row.description),row.fetchone()))
        item_ids=[];s=None
        try:
            conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=%s,"dwDuraMax"=100,"dwDuraCur"=100 WHERE "dlID"=%s',(weapon,old['dlID']))
            # Move the first synthetic stack to slot3 before seeding slot2.
            second=seed_graph_reagent(conn,cid,True);item_ids.append(second)
            conn.execute('UPDATE app_world."TITEMTABLE" SET "bItemID"=3,"wItemID"=%s,"bCount"=%s WHERE "dlID"=%s',(11054 if ammo==8401 else ammo,2*hits-1,second))
            first=seed_graph_reagent(conn,cid,True);item_ids.append(first)
            conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=%s,"bCount"=1 WHERE "dlID"=%s',(ammo,first))
            conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,32,1,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "dwRemainTick"=0',(cid,))
            _,key=start(login_port,cid);s,char=enter(map_port,cid,key)
            desc=next(item for bag,item,magic in char['items'] if bag==255 and item[0]==3)
            cs,ss=3,4
            targets=b''.join(struct.pack('<IB',cid+100000+i,2) for i in range(hits))
            def cast(loop):
                nonlocal cs
                # Unflagged tuple must not charge ammo. The >16th flagged
                # tuple in the 16-hit case must be decoded but excluded.
                requested=hits+(1 if hits==16 else 0)
                encoded=struct.pack('<IBB',cid+99999,2,0)+b''.join(struct.pack('<IBB',cid+100000+i,2,1) for i in range(requested))
                header=struct.pack('<IBBHHfffB',cid,1,1,0,32,0,0,0,requested+1) if loop else struct.pack('<IBBHHBIIfffB',cid,1,1,0,32,0,0,0,0,0,0,requested+1)
                s.sendall(frame(header+encoded,0x5372 if loop else 0x52b4,cs));cs+=1
            def reply():
                nonlocal ss
                result=read_packet(s,ss);ss+=1;return result
            conn.execute("CREATE FUNCTION public.delay_ammo_batch() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.15); RETURN NEW; END $$")
            conn.execute('CREATE TRIGGER synthetic_ammo_batch_delay BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW EXECUTE FUNCTION public.delay_ammo_batch()')
            try:
                cast(False)
                check(not select.select([s],[],[],.08)[0],'no stack or success ACK escapes the delayed batch transaction')
                check(reply()==(0x52ac,b'\xff\x02'),'first stack deletion uses exact source bag and slot')
                expected=list(desc);expected[6]=hits
                check(reply()==(0x52aa,b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*expected)),f'weapon{weapon} updates remaining mixed or same-template stack exactly')
                check(reply()==(0x52a9,b'\x00'),'one MOVEITEM follows both ordered item ACKs')
                op,body=reply();check(op==0x52b5 and len(body)==62+5*hits and body[0]==0 and body[-5*hits:]==targets,'ordinary success preserves flagged targets and original 16-target cap')
            finally:
                conn.execute('DROP TRIGGER synthetic_ammo_batch_delay ON app_world.skill_item_consumptions')
                conn.execute('DROP FUNCTION public.delay_ammo_batch()')
            receipt=conn.execute('SELECT count(*),count(DISTINCT cast_id),sum(before_count-after_count),min(hit_count),max(hit_count),min(state_contract) FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=ANY(%s)',(cid,item_ids)).fetchone()
            check(receipt==(2,1,hits,hits,hits,3),'two durable stack receipts identify one complete cast before success')
            check(conn.execute('SELECT "dlID","bCount" FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s)',(item_ids,)).fetchall()==[(second,hits)],'batch deletion and partial decrement are durable together')
            cast(True);op,body=reply();check(op==0x5373 and body[0]==6,'loop repeat cannot bypass batch cooldown')
            time.sleep(delay);cast(True)
            check(reply()==(0x52ac,b'\xff\x03'),'loop deletes the final multi-unit stack')
            check(reply()==(0x52a9,b'\x00'),'loop batch has one MOVEITEM')
            op,body=reply();check(op==0x5373 and len(body)==45+5*hits and body[0]==0 and body[-5*hits:]==targets,'loop multi-target success preserves original target list')
            time.sleep(delay);cast(True);op,body=reply()
            check(op==0x5373 and len(body)==45 and body[0]==9,'exhausted multi-target loop returns source UNSUITWEAPON')
            check(conn.execute('SELECT count(*),count(DISTINCT cast_id),sum(before_count-after_count) FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=ANY(%s)',(cid,item_ids)).fetchone()==(3,2,2*hits),'rejected retry adds no batch or item charge')
            s.close();s=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,), 'batch logout completes')
            _,key=start(login_port,cid);s,restored=enter(map_port,cid,key)
            check(all(not(bag==255 and item[0] in (2,3)) for bag,item,magic in restored['items']),'relogin never resurrects either consumed stack')
            s.close();s=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,), 'batch relogin closes')
        finally:
            if s is not None:s.close()
            if item_ids:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s)',(item_ids,))
            conn.execute(sql.SQL('UPDATE app_world."TITEMTABLE" SET {} WHERE "dlID"=%s').format(
                sql.SQL(',').join(sql.SQL('{}=%s').format(sql.Identifier(c)) for c in old)),[*old.values(),old['dlID']])
        check(old_rows==conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall(),'batch fixture restores exact original inventory')
    return {'status':'passed','checks':checks,'scope':'Synthetic owned-PC skill32, arrow and bolt ordinary/loop batches, 4 and 16 flagged targets with ignored unflagged/overflow tuples; original encrypted packet layouts. No damage or original-client execution.'}
