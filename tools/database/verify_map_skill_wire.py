"""Native learned-skill resource gates using pinned content and synthetic ownership.

This seed belongs only to the disposable account. Historical charts are read-only;
skill learning and active effects are not exercised. Timing uses original charts.
"""
import math
import struct
import time
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
    # Source 102 rank2 has a +500ms rank increment and physical speed; 31 is
    # a free 600ms cast, used to prove actual expiration without timer injection.
    for skill,rank in ((102,2),(31,1)):
        conn.execute('INSERT INTO app_world."TSKILLTABLE"("bWorldID","dwCharID","wSkillID","bLevel","dwRemainTick") VALUES(1,%s,%s,%s,0) ON CONFLICT ("bWorldID","dwCharID","wSkillID") DO UPDATE SET "bLevel"=EXCLUDED."bLevel","dwRemainTick"=0',(cid,skill,rank))
    # Maximum MP is independently parsed from CHARINFO. This source fixture has
    # 351 MP (MEN 1+5+11, formula19 rate 20.66), so 8% costs 28; assert it again on the actual wire before charging.
    physical_cost=28
    conn.execute('UPDATE app_world."TCHARTABLE" SET "dwMP"=%s WHERE "dwCharID"=%s',(cost+physical_cost+5,cid))
    return {'skill':134,'rank':2,'cost':cost,'initial_mp':cost+physical_cost+5,'physical_cost':physical_cost}


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
    maximum_hp,hp,maximum_mp,initial_mp=character['hpmp']
    chart=conn.execute('SELECT "dwReuseDelay","nReuseDelayInc","bSpeedApply","bUseMPType","dwUseMP" FROM character_compat."TSKILLCHART" WHERE "wID"=102').fetchone()
    check(chart==(28000,500,1,2,8) and ((maximum_mp*8)&0xffffffff)//100==fixture['physical_cost'],
          'source physical rank2 timing and resource fixture match pinned chart and actual maxMP')
    physical_started=time.monotonic()
    send(request(102));data=verdict(0,'physical rank2 native cast succeeds with original weapon timing')
    check(data[19]==2,'physical success ACK retains rank2')
    op,bars=reply();check(op==0x52a2 and struct.unpack('<IBIIII',bars)==(cid,1,maximum_hp,hp,maximum_mp,initial_mp-fixture['physical_cost']),
          'physical cast charges original percentage MP once')
    send(request(102));verdict(6,'newly used native skill rejects immediate repeat without imported timer or optional chart')
    send(request());data=verdict(0,'learned rank2 native cast succeeds')
    check(data[19]==fixture['rank'] and struct.unpack_from('<I',data,1)[0]==cid and struct.unpack_from('<H',data,6)[0]==fixture['skill'],
          'original success packet carries authoritative caster skill and learned rank')
    op,bars=reply();maximum_hp,hp,maximum_mp,initial_mp=character['hpmp']
    check(op==0x52a2 and struct.unpack('<IBIIII',bars)==(cid,1,maximum_hp,hp,maximum_mp,initial_mp-fixture['physical_cost']-fixture['cost']),
          'native flat MP cost follows backup formula and exact HPMP layout')
    for _ in range(2):
        send(request());verdict(7,'unaffordable native cast returns original SKILL_NEEDMP without changed bars')
    until(lambda:conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(5,),
          'periodic checkpoint persists actual native cast cost')
    check(conn.execute('SELECT "bLevel" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,fixture['skill'])).fetchone()==(fixture['rank'],),
          'native cast cannot mutate its learned rank')
    delay=conn.execute('SELECT COALESCE(sum(t."dwSpeedInc"),0) FROM app_world."TITEMTABLE" i JOIN character_compat."TITEMCHART" t ON t."wItemID"=i."wItemID" WHERE i."dwOwnerID"=%s AND i."dwStorageID"=254 AND i."bItemID" IN (0,1) AND (i."dwDuraMax"=0 OR i."dwDuraCur"<>0)',(cid,)).fetchone()[0]
    expected_delay=28500+max(delay,0) # source NAS formula=0, no speed magic/passives in this fixture
    elapsed=int((time.monotonic()-physical_started)*1000)+20
    remaining=conn.execute('SELECT "wSkillID","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (102,134) ORDER BY "wSkillID"',(cid,)).fetchall()
    check(len(remaining)==2 and expected_delay-elapsed<=remaining[0][1]<=expected_delay and 700000<remaining[1][1]<=720000,
          'periodic checkpoint persists native rank and weapon cooldown plus long no-speed cooldown')
    send(request(31));verdict(0,'free 600ms source skill can start its own cooldown')
    send(request(31));verdict(6,'free native cast cannot bypass its new cooldown')
    time.sleep(.7)
    send(request(31));verdict(0,'source cooldown expires and a new cast rearms it')
    return client_sequence,{'status':'passed','checks':checks,'source_skill':fixture['skill'],'rank':fixture['rank'],'source_mp_cost':fixture['cost']}
