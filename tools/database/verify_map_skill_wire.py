"""Native learned-skill resource gates using pinned content and synthetic ownership.

This seed belongs only to the disposable account. Historical charts are read-only;
skill learning and active effects are not exercised. Timing uses original charts.
"""
import math
import struct
import time
from verify_login_wire import frame, read_packet
from verify_cast_powers_wire import check_cast_fields


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
    loop_cost=17 # source skill34: five percent of the native 351 max MP
    for skill in (34,213,733,736,1329):
        conn.execute('INSERT INTO app_world."TSKILLTABLE"("bWorldID","dwCharID","wSkillID","bLevel","dwRemainTick") VALUES(1,%s,%s,1,0) ON CONFLICT DO NOTHING',(cid,skill))
    conn.execute('UPDATE app_world."TCHARTABLE" SET "dwMP"=%s WHERE "dwCharID"=%s',(cost+physical_cost+loop_cost+5,cid))
    # Use an existing synthetic-owned starter row; source charts stay untouched.
    reagent=conn.execute('SELECT "wItemID","dwWeaponID","dwReuseDelay","dwLoopDelay","bSpeedApply","bUseMPType","bUseHPType","dwKindDelay","wMapID","wPrevActiveID","wTargetActiveID" FROM character_compat."TSKILLCHART" WHERE "wID"=1623').fetchone()
    if reagent!=(31238,0,0,0,0,0,0,0,-1,0,0):raise RuntimeError('Pinned source reagent fixture changed')
    conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,1623,1,0) ON CONFLICT DO NOTHING',(cid,))
    rows=conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=31238,"bCount"=2,"dwDuraMax"=0,"dwDuraCur"=0 WHERE "dwOwnerID"=%s AND "dwStorageID"=255 AND "bItemID"=1 RETURNING "dlID"',(cid,)).fetchall()
    if len(rows)!=1:raise RuntimeError('Reagent requires exactly one synthetic starter row')
    return {'reagent_id':rows[0][0],'skill':134,'rank':2,'cost':cost,'initial_mp':cost+physical_cost+loop_cost+5,'physical_cost':physical_cost,'loop_cost':loop_cost}


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
    requirements=conn.execute('SELECT "wID","wPrevActiveID","wTargetActiveID","wMapID","dwWeaponID" FROM character_compat."TSKILLCHART" WHERE "wID" IN (736,1329) ORDER BY "wID"').fetchall()
    check(requirements==[(736,733,0,-1,0),(1329,0,0,550,0)],
          'ordinary map and active prerequisite requirements match immutable backup')
    send(request(1329));verdict(17,'ordinary cast outside source map550 returns original WRONGREGION')
    send(request(736));verdict(10,'learned skill733 is not an active prerequisite for ordinary skill736')
    chart=conn.execute('SELECT "dwReuseDelay","nReuseDelayInc","bSpeedApply","bUseMPType","dwUseMP" FROM character_compat."TSKILLCHART" WHERE "wID"=102').fetchone()
    check(chart==(28000,500,1,2,8) and ((maximum_mp*8)&0xffffffff)//100==fixture['physical_cost'],
          'source physical rank2 timing and resource fixture match pinned chart and actual maxMP')
    physical_started=time.monotonic()
    send(request(102));data=verdict(0,'physical rank2 native cast succeeds with original weapon timing')
    check(data[19]==2,'physical success ACK retains rank2')
    check(check_cast_fields(conn,cid,102,2,data),'ordinary physical cast sends source AL, powers, critical and aid country')
    op,bars=reply();check(op==0x52a2 and struct.unpack('<IBIIII',bars)==(cid,1,maximum_hp,hp,maximum_mp,initial_mp-fixture['physical_cost']),
          'physical cast charges original percentage MP once')
    send(request(102));verdict(6,'newly used native skill rejects immediate repeat without imported timer or optional chart')
    send(request());data=verdict(0,'learned rank2 native cast succeeds')
    check(data[19]==fixture['rank'] and struct.unpack_from('<I',data,1)[0]==cid and struct.unpack_from('<H',data,6)[0]==fixture['skill'],
          'original success packet carries authoritative caster skill and learned rank')
    check(check_cast_fields(conn,cid,134,fixture['rank'],data),'ordinary skill134 sends source instance powers and countries')
    op,bars=reply();maximum_hp,hp,maximum_mp,initial_mp=character['hpmp']
    check(op==0x52a2 and struct.unpack('<IBIIII',bars)==(cid,1,maximum_hp,hp,maximum_mp,initial_mp-fixture['physical_cost']-fixture['cost']),
          'native flat MP cost follows backup formula and exact HPMP layout')
    for _ in range(2):
        send(request());verdict(7,'unaffordable native cast returns original SKILL_NEEDMP without changed bars')
    until(lambda:conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(fixture['loop_cost']+5,),
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
    def loop_request(skill=31,caster=cid,kind=1,channel=1,map_id=0,x=0):
        return struct.pack('<IBBHHfffB',caster,kind,channel,map_id,skill,x,0,0,0)
    def loop_send(body):
        nonlocal client_sequence
        s.sendall(frame(body,0x5372,client_sequence));client_sequence+=1
    def loop_verdict(code,label):
        try:op,data=reply()
        except TimeoutError as error:raise RuntimeError('Native skill wire: LOOPSKILL_ACK missing') from error
        check(op==0x5373 and len(data)==45 and data[0]==code,label);return data
    loop_send(loop_request());loop_verdict(6,'loop cannot bypass a cooldown armed by normal use')
    loop_send(loop_request(65535));loop_verdict(1,'loop returns original NOTFOUND and its distinct 45-byte ACK')
    for body in (loop_request(caster=cid+1),loop_request(kind=2),loop_request(channel=2),loop_request(map_id=1),
                 loop_request(x=float('nan')),loop_request()[:-1]+b'\x01',loop_request()+b'\x00'):
        loop_send(body)
    loop_send(loop_request(65535));loop_verdict(1,'malformed and foreign loop requests have no response or mutation')
    loop_send(loop_request(134));loop_verdict(6,'loop checks live cooldown before insufficient MP unlike ordinary cast')
    loop_send(loop_request(213));loop_verdict(10,'loop requiring maintained skill209 rejects missing active effect')
    row=conn.execute('SELECT "dwLoopDelay","wTargetActiveID","wItemID","dwWeaponID","bSpeedApply","bUseMPType","dwUseMP" FROM character_compat."TSKILLCHART" WHERE "wID"=34').fetchone()
    check(row==(1200,0,0,0,3,2,5),'actual loop source magic timing and item-free cost contract match backup')
    loop_started=time.monotonic()
    loop_send(loop_request(34));data=loop_verdict(0,'native magic loop succeeds and uses original success layout')
    check(check_cast_fields(conn,cid,34,1,data,True),'magic loop sends original physical AL, powers, magic critical and aid country')
    check(data[8]==1 and data[29]==character['appearance'][3] and data[30]==character['appearance'][4],
          'loop ACK carries learned rank and original country and aid-country fields')
    op,bars=reply();check(op==0x52a2 and struct.unpack('<IBIIII',bars)==(cid,1,maximum_hp,hp,maximum_mp,5),
          'native loop deducts exact source percentage MP and broadcasts unchanged HPMP layout')
    loop_send(loop_request(34));loop_verdict(6,'loop repeat rejects before newly insufficient resources')
    until(lambda:conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(5,),
          'periodic checkpoint persists loop resource deduction')
    loop_delay=1200+max(delay,0) # same primary/secondary slots supply magic speed
    elapsed=int((time.monotonic()-loop_started)*1000)+20
    tick=conn.execute('SELECT "dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=34',(cid,)).fetchone()[0]
    check(max(0,loop_delay-elapsed)<=tick<=loop_delay,'native loop-generated duration reaches PostgreSQL checkpoint')
    time.sleep(max(0,loop_started+loop_delay/1000+.1-time.monotonic()))
    loop_send(loop_request(34));loop_verdict(7,'expired loop cooldown exposes NEEDMP without consuming or rearming')
    loop_send(loop_request());loop_verdict(0,'normal-use cooldown expires and free loop rearms successfully')
    send(request(31));verdict(6,'normal use cannot bypass a cooldown armed by loop use')
    check(conn.execute('SELECT "dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (736,1329)',(cid,)).fetchall()==[(0,),(0,)],
          'ordinary map and active-effect rejections checkpoint no fabricated cooldowns')
    item=next(item for bag,item,options in character['items'] if bag==255 and item[0]==1)
    check(item[1]==31238 and item[6]==2,'source reagent occupies exact synthetic default-bag slot')
    before=conn.execute('SELECT "dlID",to_jsonb(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
    expected=list(item);expected[6]=1
    expected_ack=b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*expected)
    # Source fixture has no magic options; compare the entire original descriptor.
    check(item[-1]==0,'source reagent descriptor has no synthetic magic options')
    send(request(1623))
    op,data=reply();check(op==0x52aa and data==expected_ack,'normal reagent cast sends exact UPDATEITEM bag and descriptor before cast ACK')
    check(reply()==(0x52a9,b'\x00'),'normal reagent cast sends original MOVEITEM success after item change')
    verdict(0,'ordinary reagent cast succeeds after confirmed inventory transaction')
    after=conn.execute('SELECT "dlID",to_jsonb(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
    expected_rows=[(ident,{**row,'bCount':1} if ident==fixture['reagent_id'] else row) for ident,row in before]
    check(after==expected_rows,'confirmed decrement changes only count and preserves all other full item rows')
    check(conn.execute('SELECT before_count,after_count FROM app_world.skill_item_consumptions WHERE char_id=%s ORDER BY consumption_id',(cid,)).fetchall()==[(2,1)],
          'receipt is already durable when ordinary client success arrives')
    loop_send(loop_request(1623))
    check(reply()==(0x52ac,b'\xff\x01'),'loop consuming last reagent sends exact DELITEM bag and slot before success')
    check(reply()==(0x52a9,b'\x00'),'loop last-item consumption preserves MOVEITEM success ordering')
    loop_verdict(0,'loop reagent cast succeeds only after confirmed last-row deletion')
    check(conn.execute('SELECT "dlID",to_jsonb(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()==[(i,r) for i,r in before if i!=fixture['reagent_id']],
          'last reagent deletes only its exact row while retaining every other item field')
    loop_send(loop_request(1623));loop_verdict(9,'missing loop reagent returns original UNSUITWEAPON without item or success ACK')
    send(request(1623));verdict(9,'missing ordinary reagent returns original UNSUITWEAPON without item or success ACK')
    check(conn.execute('SELECT before_count,after_count,after_hash IS NULL FROM app_world.skill_item_consumptions WHERE char_id=%s ORDER BY consumption_id',(cid,)).fetchall()==[(2,1,False),(1,0,True)],
          'rejected depleted-stack repeats cannot create another consumption receipt')
    check(conn.execute('SELECT "dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==(5,) and
          conn.execute('SELECT app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(True,),
          'reagent transactions retain original zero resource costs and valid core plus timer recovery receipt')
    return client_sequence,{'status':'passed','checks':checks,'source_skill':fixture['skill'],'rank':fixture['rank'],'source_mp_cost':fixture['cost'],'source_reagent_skill':1623,'source_reagent_item':31238}
