"""Pinned source multi-attacks on disposable owned PCs through encrypted TCP."""
import math
import select
import struct
import time
from psycopg import sql
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent


def verify_multi_attack(conn,cid,start,enter,login_port,map_port,until):
    checks=[];s=None;items=[]
    def check(ok,label):
        if not ok:raise RuntimeError('Multi-attack: '+label)
        checks.append(label)
    old_mp=conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()[0]
    cursor=conn.execute('SELECT * FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=2',(cid,))
    weapon=dict(zip((c.name for c in cursor.description),cursor.fetchone()))
    old_skills=conn.execute('SELECT * FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (324,1407)',(cid,)).fetchall()
    check(not old_skills,'synthetic multi-attack skills are absent before fixture')
    try:
        chart=conn.execute('SELECT "bUseMPType","dwUseMP","bLevel","bNextLevel","bTargetHit" FROM character_compat."TSKILLCHART" WHERE "wID"=324').fetchone()
        rate=conn.execute('SELECT "fRateX" FROM character_compat."TFORMULACHART" WHERE "bID"=34').fetchone()[0]
        f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
        cost=int(f32(f32(chart[1])*f32(math.pow(f32(rate),chart[2])/100)))
        check(cost==169 and chart[0]==1 and chart[4]==3,'source skill324 rank1 costs169 MP and distributes three hits')
        conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=701,"dwDuraMax"=100,"dwDuraCur"=100 WHERE "dlID"=%s',(weapon['dlID'],))
        second=seed_graph_reagent(conn,cid,True);items.append(second)
        conn.execute('UPDATE app_world."TITEMTABLE" SET "bItemID"=3,"wItemID"=11054,"bCount"=5 WHERE "dlID"=%s',(second,))
        first=seed_graph_reagent(conn,cid,True);items.append(first)
        conn.execute('UPDATE app_world."TITEMTABLE" SET "bCount"=1 WHERE "dlID"=%s',(first,))
        conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,324,1,0),(1,%s,1407,1,0)',(cid,cid))
        conn.execute('UPDATE app_world."TCHARTABLE" SET "dwMP"=351 WHERE "dwCharID"=%s',(cid,))
        _,key=start(login_port,cid);s,char=enter(map_port,cid,key);cs,ss=3,4
        check(char['hpmp'][3]==351,'native fixture has351 MP for two source-cost casts')
        descriptor=next(item for bag,item,magic in char['items'] if bag==255 and item[0]==3)
        def request(skill,loop=False,targets=None):
            nonlocal cs
            targets=targets if targets is not None else [(cid+100000,2,1)]
            body=struct.pack('<IBBHHfffB',cid,1,1,0,skill,0,0,0,len(targets)) if loop else struct.pack('<IBBHHBIIfffB',cid,1,1,0,skill,0,0,0,0,0,0,len(targets))
            body+=b''.join(struct.pack('<IBB',*t) for t in targets)
            s.sendall(frame(body,0x5372 if loop else 0x52b4,cs));cs+=1
        def reply():
            nonlocal ss
            result=read_packet(s,ss);ss+=1;return result
        def bars(expected):
            op,body=reply()
            check(op==0x52a2 and len(body)==21 and struct.unpack_from('<I',body,17)[0]==expected,'one expanded cast charges MP once and sends original HPMP')
        # A non-ammunition source skill: cap one means input order followed
        # by first-target padding is deterministic for every random draw.
        targets=[(cid+200000,2,1),(cid+300000,1,1),(cid+400000,2,0)]
        request(1407,targets=targets);op,body=reply()
        expected=struct.pack('<IB',targets[0][0],2)+struct.pack('<IB',targets[1][0],1)+struct.pack('<IB',targets[0][0],2)*4
        check(op==0x52b5 and len(body)==92 and body[0]==0 and body[-30:]==expected,'six-hit non-ammunition skill preserves type/order then pads first target; unflagged target omitted')
        request(1407,True,targets);op,body=reply();check(op==0x5373 and body[0]==6,'expanded non-ammunition loop respects ordinary cooldown')
        time.sleep(2.1);request(1407,True,targets);op,body=reply()
        check(op==0x5373 and len(body)==75 and body[0]==0 and body[-30:]==expected,'loop uses the same source expansion for non-ammunition skill')
        conn.execute("CREATE FUNCTION public.delay_multi_attack() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.15); RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_multi_attack_delay BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW EXECUTE FUNCTION public.delay_multi_attack()')
        try:
            request(324)
            check(not select.select([s],[],[],.08)[0],'expanded cast emits nothing before both delayed stack receipts commit')
            check(reply()==(0x52ac,b'\xff\x02'),'expanded ordinary cast deletes first stack')
            desc=list(descriptor);desc[6]=3
            check(reply()==(0x52aa,b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*desc)),'one requested target consumes three arrows across two templates')
            check(reply()==(0x52a9,b'\0'),'one MOVEITEM follows all expanded stack responses')
            op,body=reply();expected=struct.pack('<IB',cid+100000,2)*3
            check(op==0x52b5 and len(body)==77 and body[0]==0 and body[19]==1 and body[-15:]==expected,'ordinary source packet carries three duplicates and authoritative rank')
            bars(351-cost)
        finally:
            conn.execute('DROP TRIGGER synthetic_multi_attack_delay ON app_world.skill_item_consumptions')
            conn.execute('DROP FUNCTION public.delay_multi_attack()')
        check(conn.execute("SELECT count(*),count(DISTINCT cast_id),sum(before_count-after_count),min(hit_count),bool_and(hit_mode='expanded') FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=ANY(%s)",(cid,items)).fetchone()==(2,1,3,3,True),'expanded three-hit charge and mode are durable before success')
        request(324,True);op,body=reply();check(op==0x5373 and body[0]==6,'expanded ammo cannot bypass its reuse gate')
        s.close();s=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'multi-attack first logout completes')
        _,key=start(login_port,cid);s,restored=enter(map_port,cid,key);cs,ss=3,4
        check(any(bag==255 and item[0]==3 and item[6]==3 for bag,item,magic in restored['items']),'relogin keeps exact expanded stack remainder')
        request(324,True);op,body=reply();check(op==0x5373 and body[0]==6,'relogin restores multi-attack cooldown without offline decay')
        time.sleep(12.5);request(324,True)
        check(reply()==(0x52ac,b'\xff\x03'),'expanded loop deletes final three arrows')
        check(reply()==(0x52a9,b'\0'),'expanded loop keeps inventory response order')
        op,body=reply();check(op==0x5373 and len(body)==60 and body[0]==0 and body[-15:]==expected,'expanded loop success has original three-target layout')
        bars(351-2*cost)
        check(conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(13,),'two expanded casts charge169 MP each, not once per hit')
        check(conn.execute("SELECT count(*),count(DISTINCT cast_id),sum(before_count-after_count),bool_and(hit_mode='expanded') FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=ANY(%s)",(cid,items)).fetchone()==(3,2,6,True),'all six arrows are charged exactly once across two expanded casts')
        s.close();s=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'multi-attack final logout completes')
    finally:
        if s is not None:s.close()
        if items:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s)',(items,))
        conn.execute(sql.SQL('UPDATE app_world."TITEMTABLE" SET {} WHERE "dlID"=%s').format(sql.SQL(',').join(sql.SQL('{}=%s').format(sql.Identifier(c)) for c in weapon)),[*weapon.values(),weapon['dlID']])
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (324,1407)',(cid,))
        conn.execute('UPDATE app_world."TCHARTABLE" SET "dwMP"=%s WHERE "dwCharID"=%s',(old_mp,cid))
    check(conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(old_mp,),'fixture restores original MP after confirmed logout')
    return {'status':'passed','checks':checks,'scope':'Actual encrypted owned-PC ordinary/loop multi-attack324 rank1 and non-ammunition1407 rank1, atomic mixed-stack debit, cost once per cast, cooldown and relogin. Synthetic targets; no damage or original-client execution.'}
