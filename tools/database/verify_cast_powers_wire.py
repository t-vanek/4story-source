"""Outgoing cast fields derived independently from the original server and backup.

Tests only outgoing powers, AL/critical/countries and projection isolation. They
are not acceptance of effects, damage, skill learning or an original client.
"""
import struct
from verify_character_statistics_wire import source_statistics
from verify_login_wire import frame,read_packet


def source_cast_fields(conn,cid,skill,rank,loop=False,aid_country=3,**state):
    data=conn.execute('SELECT "bType","bAttr","bExec" FROM character_compat."TSKILLDATA" WHERE "wSkillID"=%s',(skill,)).fetchall()
    physical=any(attr in (1,2) for _,attr,_ in data)
    magic=any(attr==3 for _,attr,_ in data)
    if physical and magic:
        # Original query has no ORDER BY. These are the four order-sensitive
        # backup skills; restored SQL Server observations are recorded separately.
        attack_type={637:1,640:1,814:1,1343:3}[skill]
    else:attack_type=1 if physical else 3 if any(3<=attr<=9 for _,attr,_ in data) else 0
    ranged=any(typ==1 and (target==9 or attr==2) for typ,attr,target in data)
    values=struct.unpack('<I6H11I2HB3I2H3BHB',source_statistics(conn,cid,instance=(skill,rank),**state))
    level,country=conn.execute('SELECT "bLevel","bCountry" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    attack_level=values[18 if loop or attack_type==1 else 24]
    physical_min,physical_max=values[10:12] if ranged else values[7:9]
    power=struct.pack('<HB4I',attack_level,level,physical_min,physical_max,values[21],values[22])
    countries=bytes((country,aid_country,values[28 if attack_type==3 else 20]))
    return power,countries


def check_cast_fields(conn,cid,skill,rank,data,loop=False,**state):
    power,countries=source_cast_fields(conn,cid,skill,rank,loop,**state)
    return data[(9 if loop else 20):(28 if loop else 39)]==power and data[(29 if loop else 46):(32 if loop else 49)]==countries


def verify_cast_powers(conn,cid,start,enter,login_port,map_port,until):
    checks=[];client=None
    def check(ok,label):
        if not ok:raise RuntimeError('Native cast powers: '+label)
        checks.append(label)
    baseline=conn.execute('SELECT "bLevel","bAftermath","dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    saved_skills=conn.execute('SELECT "wSkillID","bLevel","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,)).fetchall()
    weapon=conn.execute('SELECT "dlID","bLevel","bGem","dwDuraMax","dwDuraCur" FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=0',(cid,)).fetchone()
    # Real backup data: buff-action exponential AL/PAP/LAP, rank-linear crit,
    # percent PAP/MAP, elemental magic, no-attribute mixed power and mixed attr.
    selected=((217,2),(218,2),(219,2),(327,2),(733,1),(790,1),(917,1),(1343,1))
    try:
        for mode in ('level60','aftermath and graded weapon','broken weapon'):
            conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=60,"bAftermath"=%s,"dwHP"=100000,"dwMP"=100000 WHERE "dwCharID"=%s',(0 if mode=='level60' else 65,cid))
            conn.execute('UPDATE app_world."TITEMTABLE" SET "bLevel"=7,"bGem"=2,"dwDuraMax"=100,"dwDuraCur"=%s WHERE "dlID"=%s',(0 if mode=='broken weapon' else 100,weapon[0]))
            for skill,rank in selected:
                conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,%s,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "bLevel"=EXCLUDED."bLevel","dwRemainTick"=0',(cid,skill,rank))
            expected_stat=source_statistics(conn,cid)
            _,key=start(login_port,cid);client,char=enter(map_port,cid,key);cs,ss=3,4
            def send(body,opcode):
                nonlocal cs
                client.sendall(frame(body,opcode,cs));cs+=1
            def receive():
                nonlocal ss
                packet=read_packet(client,ss);ss+=1;return packet
            for skill,rank in selected:
                body=struct.pack('<IBBHHBIIfffB',cid,1,1,0,skill,0,0,0,0,0,0,0)
                send(body,0x52b4);op,data=receive()
                check(op==0x52b5 and len(data)==62 and data[0]==0 and data[19]==rank and
                      check_cast_fields(conn,cid,skill,rank,data),mode+': ordinary skill'+str(skill)+' matches source rank, AL, four powers, critical and countries')
                cost=conn.execute('SELECT "dwUseHP","dwUseMP" FROM character_compat."TSKILLCHART" WHERE "wID"=%s',(skill,)).fetchone()
                if any(cost):check(receive()[0]==0x52a2,mode+': charged skill'+str(skill)+' retains following HPMP packet')
                # No instance modifiers may leak into persistent inspection.
                send(struct.pack('<I',cid),0x5323)
                check(receive()==(0x5324,expected_stat),mode+': skill'+str(skill)+' instance modifiers do not leak into ordinary statistics')
            # Zero-loop-delay source 917 uses ranged power despite SAT_NONE;
            # ordinary and loop must choose distinct AL on this WIS/DEX fixture.
            send(struct.pack('<IBBHHfffB',cid,1,1,0,917,0,0,0,0),0x5372);op,data=receive()
            check(op==0x5373 and len(data)==45 and data[0]==0 and check_cast_fields(conn,cid,917,1,data,True),mode+': no-attribute loop uses physical AL and instance ranged power')
            client.close();client=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'cast projection disconnect checkpoints resources and timers')
            check(conn.execute('SELECT app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(True,),mode+': existing resource/timer persistence remains valid')
    finally:
        if client is not None:client.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'cast projection fixture offline before restoring mutable rows')
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=%s,"bAftermath"=%s,"dwHP"=%s,"dwMP"=%s WHERE "dwCharID"=%s',(*baseline,cid))
        conn.execute('UPDATE app_world."TITEMTABLE" SET "bLevel"=%s,"bGem"=%s,"dwDuraMax"=%s,"dwDuraCur"=%s WHERE "dlID"=%s',(*weapon[1:],weapon[0]))
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,))
        for skill,rank,tick in saved_skills:conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,%s,%s)',(cid,skill,rank,tick))
    return {'status':'passed','checks':checks,'scope':'Outgoing instance-skill profile only; full effects and accepted hit persistence pending; original client unavailable'}
