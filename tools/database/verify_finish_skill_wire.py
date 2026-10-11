"""Original FINISHSKILL sender -> native PostgreSQL -> DEFEND/HPMP oracle.

Packet order comes from CSSender.cpp:2980 and TObjBase.cpp:1160. Cure arithmetic
comes from TSkillTemp::Calculate and PerformSkill SCT_HP, not the modern encoder.
Only the pinned instant self-cure scope is exercised here; no real client exists.
"""
import math
import struct
from verify_login_wire import frame,read_packet
from verify_cast_powers_wire import source_cast_fields


def verify_skill_finishes(conn,cid,start,enter,login_port,map_port,until,restart):
    checks=[];client=None;cs,ss=3,4
    def check(ok,label):
        if not ok:raise RuntimeError('Skill finish wire: '+label)
        checks.append(label)
    original=conn.execute('SELECT "bLevel","dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    old_skill=conn.execute('SELECT "bLevel","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=509',(cid,)).fetchone()
    def seed():
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=60,"dwHP"=100,"dwMP"=100000 WHERE "dwCharID"=%s',(cid,))
        conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,509,2,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "bLevel"=2,"dwRemainTick"=0',(cid,))
    def connect():
        nonlocal client,cs,ss
        _,key=start(login_port,cid);client,char=enter(map_port,cid,key);cs,ss=3,4;return char
    def send(op,body):
        nonlocal cs
        client.sendall(frame(body,op,cs));cs+=1
    def receive():
        nonlocal ss
        result=read_packet(client,ss);ss+=1;return result
    def disconnect():
        nonlocal client
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'finish logout saves character')
    def cast(char):
        raw=struct.pack('<IBBHHBIIfffBIBB',cid,1,1,0,509,0,0,0,*char['position'],1,cid,1,1)
        send(0x52b4,raw);op,data=receive()
        check(op==0x52b5 and data[0]==0 and data[19]==2 and data[-5:]==struct.pack('<IB',cid,1),'source learned Cure cast accepts one self target')
        op,cost=receive();check(op==0x52a2,'accepted Cure charges MP before finish')
        return struct.unpack('<IBIIII',cost)[2:]
    def finish(char,count=1):
        return struct.pack('<IIBfffHIIHB',cid,cid,1,*char['position'],509,0,0,0,count)+struct.pack('<IB',cid,1)*count
    try:
        seed();char=connect();before=cast(char);raw=finish(char)
        send(0x55f7,raw);op,defend=receive()
        power,countries=source_cast_fields(conn,cid,509,2)
        level,country=conn.execute('SELECT "bLevel","bCountry" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        al,_,pmin,pmax,mmin,mmax=struct.unpack('<HB4I',power)
        expected=struct.pack('<IIBBIBIIBIBBHB4I4BHBHB6fB',cid,cid,1,1,cid,1,0,0,0,0,countries[2],1,al,level,
            pmin,pmax,mmin,mmax,1,0,country,3,509,2,0,1,*char['position'],*char['position'],0)
        check(op==0x52a1 and defend==expected and len(defend)==84,'all 84 original DEFEND bytes precede the changed HPMP response')
        op,bars=receive();check(op==0x52a2,'healing HPMP follows DEFEND')
        hpmp=struct.unpack('<IBIIII',bars)[2:]
        value,start_level,next_level,rate=conn.execute('SELECT d."wValue",s."bLevel",s."bNextLevel",f."fRateX" FROM character_compat."TSKILLDATA" d JOIN character_compat."TSKILLCHART" s ON s."wID"=d."wSkillID" CROSS JOIN character_compat."TFORMULACHART" f WHERE d."wSkillID"=509 AND f."bID"=34').fetchone()
        # Original f1stRateX is FLOAT, promoted to double by pow.
        rate=struct.unpack('<f',struct.pack('<f',float(rate)))[0]
        amount=int(value*math.pow(rate,start_level+next_level)/100)
        possibilities={min(before[0],before[1]+amount+amount*r//100) for r in range(16)}
        check(hpmp[1] in possibilities and hpmp[1]>before[1] and hpmp[3]==before[3],'source rank-two cure rolls 0..15 percent without a second MP charge')
        receipt=conn.execute('SELECT request,responses,after_hp,after_mp FROM app_world.skill_finish_operations WHERE char_id=%s ORDER BY finish_id DESC LIMIT 1',(cid,)).fetchone()
        check(receipt==(raw,[[0x52a1,defend.hex()],[0x52a2,bars.hex()]],hpmp[1],hpmp[3]),'exact request and ordered responses are committed before client receives healing')
        check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(hpmp[1],hpmp[3]),'healing immediately updates native core')
        check(conn.execute('SELECT app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=%s',(cid,)).fetchone()==(True,),'healing commits valid recovery state')
        map_port=restart();client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'finish crash recovery clears old claim')
        char=connect();check(char['hpmp']==hpmp,'SIGKILL replacement and reconnect retain exact healed resources')
        count=conn.execute('SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id=%s',(cid,)).fetchone()[0]
        send(0x55f7,raw);check(client.recv(1)==b'','old-login finish replay closes without another success')
        map_port=restart();client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'rejected replay retains recoverable claim until replacement')
        check(conn.execute('SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id=%s',(cid,)).fetchone()==(count,),'replay does not create a second receipt')
        check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(hpmp[1],hpmp[3]),'replay cannot heal or overwrite committed resources')
        seed();char=connect();before=cast(char);raw=finish(char,0)
        send(0x55f7,raw);send(0x5323,struct.pack('<I',cid))
        check(receive()[0]==0x5324,'empty filtered target list finishes silently with no invented ACK')
        check(conn.execute('SELECT request,responses FROM app_world.skill_finish_operations WHERE char_id=%s ORDER BY finish_id DESC LIMIT 1',(cid,)).fetchone()==(raw,[]),'empty finish consumes its cast without changing resources')
        disconnect()
        seed();char=connect();before=cast(char);raw=finish(char)
        conn.execute("CREATE FUNCTION public.reject_finish_checkpoint() RETURNS trigger LANGUAGE plpgsql AS $$BEGIN RAISE EXCEPTION 'synthetic finish failure'; END$$")
        conn.execute('CREATE TRIGGER synthetic_finish_checkpoint BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_finish_checkpoint()')
        count=conn.execute('SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id=%s',(cid,)).fetchone()[0]
        send(0x55f7,raw);check(client.recv(1)==b'','late SQL failure closes without publishing cure')
        conn.execute('DROP TRIGGER synthetic_finish_checkpoint ON app_world.map_checkpoints');conn.execute('DROP FUNCTION public.reject_finish_checkpoint()')
        check(conn.execute('SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id=%s',(cid,)).fetchone()==(count,),'late SQL failure rolls back consumed cast receipt')
        check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(before[1],before[3]),'late SQL failure preserves the already committed cast costs without healing')
        map_port=restart();client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'failed finish owner replacement recovers cast state')
        char=connect();check(char['hpmp']==before,'failed finish restart restores pre-heal resources')
        # A native client cannot bypass FINISHSKILL with the prototype cure path.
        forged=struct.pack('<IIIBBHIIBHB4I2H5BHBHB6fI',cid+1,cid+1,cid,1,1,0,0,0,1,0,60,
            0xffffffff,0xffffffff,0xffffffff,0xffffffff,0,0,0,0,1,4,3,65535,255,509,255,*char['position'],*char['position'],0)
        send(0x52a0,forged);check(client.recv(1)==b'','native DEFEND cannot reach the client-authoritative prototype')
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'rejected prototype request tears down without gameplay mutation')
        check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(before[1],before[3]),'forged rank and powers leave native resources unchanged')
    finally:
        conn.execute('DROP TRIGGER IF EXISTS synthetic_finish_checkpoint ON app_world.map_checkpoints');conn.execute('DROP FUNCTION IF EXISTS public.reject_finish_checkpoint()')
        if client is not None:client.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'finish fixture offline before restoring mutable rows')
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=%s,"dwHP"=%s,"dwMP"=%s WHERE "dwCharID"=%s',(*original,cid))
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=509',(cid,))
        if old_skill:conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,509,%s,%s)',(cid,*old_skill))
    return {'status':'passed','checks':checks},map_port
