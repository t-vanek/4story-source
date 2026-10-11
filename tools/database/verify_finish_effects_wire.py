"""Original ACTION/FINISHSKILL with real equipment postures, PostgreSQL and TCP.

Oracle: CSHandler::OnCS_FINISHSKILL_ACK, TObjBase::Defend/CheckEternalBuff,
EraseMaintainSkill and TPlayer::CheckEquipSkill. No modern packet codecs.
"""
import math
import struct
import time
from psycopg import sql
from verify_login_wire import frame,read_packet
from verify_cast_powers_wire import source_cast_fields,check_cast_fields
from verify_character_statistics_wire import source_statistics


def verify_finish_effects(conn,cid,start,enter,login_port,map_port,until,restart):
    checks=[];client=None;cs,ss=3,4;char={}
    def check(ok,label):
        if not ok:raise RuntimeError('Finish effects wire: '+label)
        checks.append(label)
    original=conn.execute('SELECT "bLevel","bAftermath","dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    items=[r[0] for r in conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s',(cid,))]
    skills=conn.execute('SELECT "wSkillID","bLevel","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (8,14,240,509)',(cid,)).fetchall()
    check(conn.execute('SELECT count(*) FROM app_world.map_maintained_effects WHERE char_id=%s',(cid,)).fetchone()==(0,),'fixture starts without normalized effects')
    def insert(row):
        conn.execute(sql.SQL('INSERT INTO app_world."TITEMTABLE" ({}) VALUES ({})').format(sql.SQL(',').join(map(sql.Identifier,row)),sql.SQL(',').join(sql.Placeholder() for _ in row)),list(row.values()))
    def seed(mp=100000):
        # Only our offline synthetic character; source catalogs stay immutable.
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=60,"bAftermath"=0,"dwHP"=100,"dwMP"=%s WHERE "dwCharID"=%s',(mp,cid))
        conn.execute('DELETE FROM app_world.map_maintained_effects WHERE char_id=%s',(cid,))
        conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s',(cid,))
        for item in items:
            row=item.copy()
            if row['dwStorageID']==254 and row['bItemID']==1:row.update(dwStorageID=255,bItemID=13)
            insert(row)
        weapon=next(i for i in items if i['dwStorageID']==254 and i['bItemID']==0)
        for slot,template in ((10,1001),(11,401)):
            row=weapon.copy();row['dlID']=conn.execute('UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=1 RETURNING item_high_water').fetchone()[0]
            row.update(dwStorageID=255,bItemID=slot,wItemID=template,bCount=1);insert(row)
        for skill in (8,14,240,509):
            conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,2,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "bLevel"=2,"dwRemainTick"=0',(cid,skill))
    def connect():
        nonlocal client,cs,ss,char
        _,key=start(login_port,cid);client,char=enter(map_port,cid,key);cs,ss=3,4
    def send(op,body):
        nonlocal cs
        client.sendall(frame(body,op,cs));cs+=1
    def receive():
        nonlocal ss
        out=read_packet(client,ss);ss+=1;return out
    def disconnect():
        nonlocal client
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'effect finish disconnect saves')
    def replace():
        nonlocal client,map_port
        map_port=restart()
        if client is not None:client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'effect finish owner replacement recovers')
    def equip(slot,target,posture):
        send(0x52a8,bytes([255,slot,254,target,1]));success=0;stats=[]
        for _ in range(16):
            op,body=receive()
            if op==0x52a9:
                check(body==b'\0','actual equipment operation succeeds');success+=1
            if op==0x5324:stats.append(body)
            if success==2:break
        check(success==2 and stats[-1]==source_statistics(conn,cid,effects=[(posture,1)]),'equipment creates source posture and exact statistics before finish')
        check(conn.execute('SELECT maintain_state FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==([[posture,1,0,1,cid,1,cid,4]],),'equipped posture is durable before skill action')
    def action():
        send(0x52ae,struct.pack('<IBBIIBHH',cid,1,2,3,4,1,0,240))
        check(receive()==(0x52af,struct.pack('<BIBBIIH',0,cid,1,2,3,4,240)),'ACTION preserves posture and returns exact original success')
        time.sleep(.35)
    def cast(skill,effects):
        send(0x52b4,struct.pack('<IBBHHBIIfffBIBB',cid,1,1,0,skill,0,0,0,*char['position'],1,cid,1,1))
        op,body=receive();check(op==0x52b5 and check_cast_fields(conn,cid,skill,2,body,effects=effects),'accepted cast uses source posture attack profile')
        op,body=receive();check(op==0x52a2,'accepted cast commits MP cost before pending finish')
        return struct.unpack('<IBIIII',body)[2:]
    def finish(skill,empty=False):
        body=struct.pack('<IIBfffHIIHB',cid,cid,1,*char['position'],skill,0,0,0,0 if empty else 1)
        if not empty:body+=struct.pack('<IB',cid,1)
        send(0x55f7,body)
    def expected_defend(skill,posture):
        power,countries=source_cast_fields(conn,cid,skill,2,effects=[(posture,1)])
        al,_,pmin,pmax,mmin,mmax=struct.unpack('<HB4I',power)
        level,country=conn.execute('SELECT "bLevel","bCountry" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        return struct.pack('<IIBBIBIIBIBBHB4I4BHBHB6fB',cid,cid,1,1,cid,1,0,0,0,0,countries[2],1,al,level,
            pmin,pmax,mmin,mmax,1,0,country,3,skill,2,0,1,*char['position'],*char['position'],0)
    def healed(skill,before):
        op,body=receive();check(op==0x52a2,'finished cure sends HPMP after DEFEND')
        bars=struct.unpack('<IBIIII',body)[2:]
        value,level,next_level,rate=conn.execute('SELECT d."wValue",s."bLevel",s."bNextLevel",f."fRateX" FROM character_compat."TSKILLDATA" d JOIN character_compat."TSKILLCHART" s ON s."wID"=d."wSkillID" CROSS JOIN character_compat."TFORMULACHART" f WHERE d."wSkillID"=%s AND f."bID"=34',(skill,)).fetchone()
        amount=int(value*math.pow(struct.unpack('<f',struct.pack('<f',float(rate)))[0],level+next_level)/100)
        check(bars[0]==before[0] and bars[2:]==before[2:] and bars[1] in {min(before[0],before[1]+amount+amount*r//100) for r in range(16)},'posture cure follows original rank formula and resource clamp')
        return bars,body
    try:
        for posture in (131,132):
            seed();connect();equip(10,1,131)
            if posture==132:equip(11,0,132)
            action();before=cast(240,[(posture,1)]);expected=expected_defend(240,posture)
            finish(240);check(receive()==(0x52a1,expected),'charged cure with posture returns all 84 original DEFEND bytes')
            healed(240,before)
            check(conn.execute('SELECT before_effects,after_effects,response_audiences FROM app_world.skill_finish_operations WHERE char_id=%s ORDER BY finish_id DESC LIMIT 1',(cid,)).fetchone()==([[posture,1,0,1,cid,1,cid,4]],[[posture,1,0,1,cid,1,cid,4]],[2,2]),'charged finish preserves stance with nearby DEFEND and HPMP audiences')
            disconnect()
        use_type,value,level,next_level,rate=conn.execute('SELECT s."bUseMPType",s."dwUseMP",s."bLevel",s."bNextLevel",f."fRateX" FROM character_compat."TSKILLCHART" s CROSS JOIN character_compat."TFORMULACHART" f WHERE s."wID"=509 AND f."bID"=34').fetchone()
        check(use_type==1,'source Cure has a rank-scaled flat MP cost')
        cost=int(value*math.pow(struct.unpack('<f',struct.pack('<f',float(rate)))[0],level+next_level)/100)
        for mode in ('empty','heal','rollback'):
            seed(cost);connect();before=cast(509,[]);check(before[3]==0,'accepted Cure spends exactly all MP')
            equip(10,1,131);expected=expected_defend(509,131)
            if mode=='empty':
                finish(509,True);send(0x5323,struct.pack('<I',cid))
                check(receive()==(0x5324,source_statistics(conn,cid,effects=[(131,1)])),'empty filtered finish skips Defend eternal cleanup and emits no unsolicited reply')
                check(conn.execute('SELECT responses,response_audiences,before_effects=after_effects FROM app_world.skill_finish_operations WHERE char_id=%s ORDER BY finish_id DESC LIMIT 1',(cid,)).fetchone()==([],[],True),'empty finish still accepts valid equipment and consumes the cast without healing')
                disconnect();continue
            if mode=='rollback':
                count=conn.execute('SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id=%s',(cid,)).fetchone()[0]
                conn.execute("CREATE FUNCTION public.reject_finish_effect() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic finish effect failure'; END $$")
                conn.execute('CREATE TRIGGER synthetic_finish_effect_fault BEFORE INSERT ON app_world.skill_finish_operations FOR EACH ROW EXECUTE FUNCTION public.reject_finish_effect()')
                finish(509);check(client.recv(1)==b'','late SQL failure sends no END, private STAT, DEFEND or HPMP')
                conn.execute('DROP TRIGGER synthetic_finish_effect_fault ON app_world.skill_finish_operations');conn.execute('DROP FUNCTION public.reject_finish_effect()')
                check(conn.execute('SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id=%s',(cid,)).fetchone()==(count,),'failed effect finish is not automatically retried')
                replace();connect()
                check(char['hpmp'][1:4:2]==(100,0) and char['effects'][0][:3]==(131,1,0),'rollback and SIGKILL preserve unhealed resources and posture together')
            else:
                finish(509);end=struct.pack('<IBH',cid,1,131);stats=source_statistics(conn,cid,effects=[])
                check(receive()==(0x52b7,end),'Defend zero-MP cleanup emits original END before healing reply')
                check(receive()==(0x5324,stats),'posture removal sends exact private source statistics')
                check(receive()==(0x52a1,expected),'DEFEND retains instance powers computed before posture removal')
                bars,hpmp=healed(509,before)
                responses=[[0x52b7,end.hex()],[0x5324,stats.hex()],[0x52a1,expected.hex()],[0x52a2,hpmp.hex()]]
                check(conn.execute('SELECT responses,response_audiences,before_effects,after_effects FROM app_world.skill_finish_operations WHERE char_id=%s ORDER BY finish_id DESC LIMIT 1',(cid,)).fetchone()==(responses,[1,0,2,2],[[131,1,0,1,cid,1,cid,4]],[]),'atomic receipt preserves exact source ordering, bytes, audiences and removed stance')
                replace();connect();check(char['hpmp']==bars and char['effects']==[],'committed healing and erased posture survive actual SIGKILL and reconnect')
            disconnect()
    finally:
        conn.execute('DROP TRIGGER IF EXISTS synthetic_finish_effect_fault ON app_world.skill_finish_operations');conn.execute('DROP FUNCTION IF EXISTS public.reject_finish_effect()')
        if client is not None:client.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'finish effect fixture offline before restoration')
        conn.execute('DELETE FROM app_world.map_maintained_effects WHERE char_id=%s',(cid,))
        conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s',(cid,))
        for row in items:insert(row)
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=%s,"bAftermath"=%s,"dwHP"=%s,"dwMP"=%s WHERE "dwCharID"=%s',(*original,cid))
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (8,14,240,509)',(cid,))
        for skill,rank,timer in skills:conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,%s,%s)',(cid,skill,rank,timer))
    return {'status':'passed','checks':checks,'scope':'Original-source synthetic TCP with actual equipment, native PostgreSQL and process replacement; original client unavailable'},map_port
