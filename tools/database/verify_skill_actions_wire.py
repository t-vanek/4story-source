"""Original ACTION -> cast -> charged FINISHSKILL over encrypted native TCP.

Oracle: CSSenderAll.cpp:319, CSHandler.cpp:1248/20197, TObjBase::PerformSkill.
The fixed numeric opcodes, layouts and result codes do not import modern codecs.
"""
import math
import struct
import time
from verify_login_wire import frame,read_packet
from verify_cast_powers_wire import source_cast_fields


def verify_skill_actions(conn,cid,start,enter,login_port,map_port,until,restart):
    checks=[];client=None;cs,ss=3,4
    def check(ok,label):
        if not ok:raise RuntimeError('Skill action wire: '+label)
        checks.append(label)
    original=conn.execute('SELECT "bLevel","bAftermath","dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    skills=conn.execute('SELECT "wSkillID","bLevel","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (34,240,629)',(cid,)).fetchall()
    def seed(mp=100000,aftermath=0):
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=60,"bAftermath"=%s,"dwHP"=100,"dwMP"=%s WHERE "dwCharID"=%s',(aftermath,mp,cid))
        for skill,rank in ((34,1),(240,2),(629,2)):
            conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,%s,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "bLevel"=EXCLUDED."bLevel","dwRemainTick"=0',(cid,skill,rank))
    def connect():
        nonlocal client,cs,ss
        _,key=start(login_port,cid);client,char=enter(map_port,cid,key);cs,ss=3,4;return char
    def send(op,body):
        nonlocal cs
        client.sendall(frame(body,op,cs));cs+=1
    def receive():
        nonlocal ss
        out=read_packet(client,ss);ss+=1;return out
    def disconnect():
        nonlocal client
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'action fixture disconnect saves')
    def replace():
        nonlocal client,map_port
        map_port=restart()
        if client is not None:client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'action owner replacement recovers')
    def action(skill,result=0):
        raw=struct.pack('<IBBIIBHH',cid,1,2,3,4,1,0,skill)
        send(0x52ae,raw);reply=receive()
        check(reply==(0x52af,struct.pack('<BIBBIIH',result,cid,1,2,3,4,skill)),f'source ACTION {skill} returns exact 17 bytes and result {result}')
        return raw
    def cast(char,skill,rank=2,loop=False):
        raw=(struct.pack('<IBBHHfffBIBB',cid,1,1,0,skill,*char['position'],1,cid,1,1) if loop else
             struct.pack('<IBBHHBIIfffBIBB',cid,1,1,0,skill,0,0,0,*char['position'],1,cid,1,1))
        send(0x5372 if loop else 0x52b4,raw);op,data=receive()
        check(op==(0x5373 if loop else 0x52b5) and data[0]==0 and data[8 if loop else 19]==rank,'skill cast is accepted at learned rank')
        op,cost=receive();check(op==0x52a2,'cast commits its MP cost before finish')
        return struct.unpack('<IBIIII',cost)[2:]
    def finish(char,skill):
        send(0x55f7,struct.pack('<IIBfffHIIHBIB',cid,cid,1,*char['position'],skill,0,0,0,1,cid,1))
    try:
        # TObjBase.cpp:1450,1855,2001 and TSkill.cpp:202: pure MEN growth
        # excludes aftermath, equipment and effects. No modern stat code used.
        seed(aftermath=100)
        race,klass,level=conn.execute('SELECT r."wMEN",k."wMEN",c."bLevel" FROM app_world."TCHARTABLE" c JOIN character_compat."TRACECHART" r ON r."bRaceID"=c."bRace" JOIN character_compat."TCLASSCHART" k ON k."bClassID"=c."bClass" WHERE c."dwCharID"=%s',(cid,)).fetchone()
        formulas=dict(conn.execute('SELECT "bID",row_to_json(f) FROM character_compat."TFORMULACHART" f WHERE "bID" IN (19,34)').fetchall())
        f32=lambda x:struct.unpack('<f',struct.pack('<f',float(x)))[0]
        men=f32(((1+race+klass)&65535)*math.pow(f32(formulas[34]['fRateX']),level-1))
        pure_mp=(formulas[19]['dwinit']+int(f32(men*f32(formulas[19]['fRateX']))))&0xffffffff
        check(conn.execute('SELECT "bUseMPType","dwUseMP","bUseHPType","dwUseHP" FROM character_compat."TSKILLCHART" WHERE "wID"=34').fetchone()==(2,5,0,0),'backup Magic Attack requires five percent of pure maximum MP')
        cost=(pure_mp*5&0xffffffff)//100
        seed(cost-1,100);char=connect()
        check(pure_mp>char['hpmp'][2] and char['hpmp'][2]*5//100<cost-1,'aftermath creates a decisive difference between pure and visible MP maxima')
        action(34,7);disconnect()
        for loop in (False,True):
            seed(aftermath=100);char=connect();action(34)
            bars=cast(char,34,1,loop)
            check(bars==(char['hpmp'][0],char['hpmp'][1],char['hpmp'][2],char['hpmp'][3]-cost),
                  ('LOOPSKILL' if loop else 'SKILLUSE')+' charges source pure MP despite aftermath')
            check(conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(bars[3],),'pure percentage cost commits before HPMP reply')
            disconnect()
        seed(0);connect();action(240,7);disconnect()
        for skill in (240,):
            seed();char=connect();raw=action(skill)
            ledger=conn.execute('SELECT action_id,request,charge_ms FROM app_world.skill_action_operations WHERE char_id=%s ORDER BY action_id DESC LIMIT 1',(cid,)).fetchone()
            duration=conn.execute('SELECT "dwActionTime" FROM character_compat."TSKILLCHART" WHERE "wID"=%s',(skill,)).fetchone()[0]
            check(ledger[1:]==(raw,duration),'ACTION stores original request and backup charge duration before success')
            check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(char['hpmp'][1],char['hpmp'][3]),'ACTION does not debit HP or MP')
            action(65000,1) # Failed action must not replace a valid pending charge.
            time.sleep(duration/1000+.05)
            before=cast(char,skill)
            check(conn.execute('SELECT charge_action_id FROM app_world.accepted_skill_casts WHERE char_id=%s ORDER BY cast_id DESC LIMIT 1',(cid,)).fetchone()==(ledger[0],),'cast consumes the same successful charge despite an intervening rejected action')
            finish(char,skill);op,defend=receive()
            power,countries=source_cast_fields(conn,cid,skill,2)
            level,country=conn.execute('SELECT "bLevel","bCountry" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
            al,_,pmin,pmax,mmin,mmax=struct.unpack('<HB4I',power)
            expected=struct.pack('<IIBBIBIIBIBBHB4I4BHBHB6fB',cid,cid,1,1,cid,1,0,0,0,0,countries[2],1,al,level,
                pmin,pmax,mmin,mmax,1,0,country,3,skill,2,0,1,*char['position'],*char['position'],0)
            check((op,defend)==(0x52a1,expected),'charged cure returns all original DEFEND fields before HPMP')
            op,bars=receive();check(op==0x52a2,'charged cure sends HPMP after DEFEND');hpmp=struct.unpack('<IBIIII',bars)[2:]
            value,level0,next_level,rate=conn.execute('SELECT d."wValue",s."bLevel",s."bNextLevel",f."fRateX" FROM character_compat."TSKILLDATA" d JOIN character_compat."TSKILLCHART" s ON s."wID"=d."wSkillID" CROSS JOIN character_compat."TFORMULACHART" f WHERE d."wSkillID"=%s AND f."bID"=34',(skill,)).fetchone()
            rate=struct.unpack('<f',struct.pack('<f',float(rate)))[0];amount=int(value*math.pow(rate,level0+next_level)/100)
            check(hpmp[1] in {min(before[0],before[1]+amount+amount*r//100) for r in range(16)} and hpmp[1]>before[1] and hpmp[3]==before[3],'charged cure uses source rank formula and never charges MP twice')
            action(skill,6)
            replace();char=connect();check(char['hpmp']==hpmp,'charged cure resources survive SIGKILL and reconnect');disconnect()
        # A prior login's pending charge cannot authorize a cast in a new login.
        seed();connect();action(240);disconnect();seed();char=connect();before=cast(char,240)
        finish(char,240);check(client.recv(1)==b'','previous login charge cannot authorize healing');replace()
        check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(before[1],before[3]),'rejected old charge preserves only the committed cast cost')
        seed();char=connect();action(240);action(0);before=cast(char,240)
        finish(char,240);check(client.recv(1)==b'','explicit zero-skill ACTION cancellation prevents later healing');replace()
        check(conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(before[1],before[3]),'cancelled charge cannot change persistent resources')
        seed();connect();count=conn.execute('SELECT count(*) FROM app_world.skill_action_operations WHERE char_id=%s',(cid,)).fetchone()[0]
        conn.execute("CREATE FUNCTION public.reject_action_checkpoint() RETURNS trigger LANGUAGE plpgsql AS $$BEGIN RAISE EXCEPTION 'synthetic action failure'; END$$")
        conn.execute('CREATE TRIGGER synthetic_action_checkpoint BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_action_checkpoint()')
        send(0x52ae,struct.pack('<IBBIIBHH',cid,1,2,3,4,1,0,240));check(client.recv(1)==b'','late ACTION SQL failure closes without publishing charge success')
        conn.execute('DROP TRIGGER synthetic_action_checkpoint ON app_world.map_checkpoints');conn.execute('DROP FUNCTION public.reject_action_checkpoint()')
        check(conn.execute('SELECT count(*) FROM app_world.skill_action_operations WHERE char_id=%s',(cid,)).fetchone()==(count,),'failed ACTION rolls back its receipt and charge');replace()
    finally:
        conn.execute('DROP TRIGGER IF EXISTS synthetic_action_checkpoint ON app_world.map_checkpoints');conn.execute('DROP FUNCTION IF EXISTS public.reject_action_checkpoint()')
        if client is not None:client.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'action fixture offline before restoring rows')
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=%s,"bAftermath"=%s,"dwHP"=%s,"dwMP"=%s WHERE "dwCharID"=%s',(*original,cid))
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (34,240,629)',(cid,))
        for skill,rank,timer in skills:conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,%s,%s)',(cid,skill,rank,timer))
    return {'status':'passed','checks':checks},map_port
