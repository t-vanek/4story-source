"""Native learned-skill resource gates using pinned content and synthetic ownership.

This seed belongs only to the disposable account. Historical charts are read-only;
skill learning, effects and attack-speed/kind cooldown generation are not exercised.
"""
import math
import struct
from verify_login_wire import frame, read_packet


def seed_skill_cast(conn,cid):
    # Backup skill 134, rank 2: FLOAT-rounded exponential MP cost = 88.
    row=conn.execute('SELECT "bUseMPType","dwUseMP","bUseHPType","bLevel","bNextLevel","bMaxLevel" FROM character_compat."TSKILLCHART" WHERE "wID"=134').fetchone()
    if row is None or row[0]!=1 or row[2]!=0 or row[5]<2:
        raise RuntimeError('Pinned source skill134 cost fixture changed')
    rate=conn.execute('SELECT "fRateX" FROM character_compat."TFORMULACHART" WHERE "bID"=34').fetchone()[0]
    f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
    cost=int(f32(f32(row[1]&0xffffffff)*f32(math.pow(f32(rate),row[3]+row[4])/100)))
    if cost!=88:raise RuntimeError('Backup skill134 rank2 golden cost changed')
    conn.execute('INSERT INTO app_world."TSKILLTABLE"("bWorldID","dwCharID","wSkillID","bLevel","dwRemainTick") VALUES(1,%s,134,2,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "bLevel"=2,"dwRemainTick"=0',(cid,))
    conn.execute('UPDATE app_world."TCHARTABLE" SET "dwMP"=%s WHERE "dwCharID"=%s',(cost+5,cid))
    return {'skill':134,'rank':2,'cost':cost,'initial_mp':cost+5}


def verify_skill_cast(conn,s,cid,character,fixture,until):
    checks=[];client_sequence=3;server_sequence=4
    def check(ok,label):
        if not ok:raise RuntimeError('Native skill wire: '+label)
        checks.append(label)
    def request(skill=fixture['skill'],caster=cid,kind=1,channel=1,map_id=0,x=0):
        return struct.pack('<IBBHHBIIfffB',caster,kind,channel,map_id,skill,0,0,0,x,0,0,0)
    def send(body):
        nonlocal client_sequence
        s.sendall(frame(body,0x52b4,client_sequence));client_sequence+=1
    def reply():
        nonlocal server_sequence
        packet=read_packet(s,server_sequence);server_sequence+=1;return packet
    def verdict(code,label):
        op,data=reply();check(op==0x52b5 and len(data)==62 and data[0]==code,label);return data
    check(any(i==fixture['skill'] and level==fixture['rank'] for i,level,tick in character['skills']),
          'synthetic learned ownership uses an actual backup template at rank2')
    check(character['hpmp'][3]==fixture['initial_mp'],'native initial MP matches bounded cost fixture')
    check(all(i!=65535 for i,level,tick in character['skills']),'unknown-skill fixture is not learned')
    send(request(65535));verdict(1,'unlearned native skill returns original SKILL_NOTFOUND')
    malformed=request()[:-1]+b'\x01'
    for body in (request(caster=cid+1),request(kind=2),request(channel=2),request(map_id=1),
                 request(x=float('nan')),malformed,request()+b'\x00'):
        send(body)
    send(request(65535));verdict(1,'spoofed identity route and malformed targets have no cast response or side effects')
    send(request());data=verdict(0,'learned rank2 native cast succeeds')
    check(data[19]==fixture['rank'] and struct.unpack_from('<I',data,1)[0]==cid and struct.unpack_from('<H',data,6)[0]==fixture['skill'],
          'original success packet carries authoritative caster skill and learned rank')
    op,bars=reply();maximum_hp,hp,maximum_mp,initial_mp=character['hpmp']
    check(op==0x52a2 and struct.unpack('<IBIIII',bars)==(cid,1,maximum_hp,hp,maximum_mp,initial_mp-fixture['cost']),
          'native flat MP cost follows backup formula and exact HPMP layout')
    for _ in range(2):
        send(request());verdict(7,'unaffordable native cast returns original SKILL_NEEDMP without changed bars')
    until(lambda:conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(5,),
          'periodic checkpoint persists actual native cast cost')
    check(conn.execute('SELECT "bLevel" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,fixture['skill'])).fetchone()==(fixture['rank'],),
          'native cast cannot mutate its learned rank')
    return client_sequence,{'status':'passed','checks':checks,'source_skill':fixture['skill'],'rank':fixture['rank'],'source_mp_cost':fixture['cost']}
