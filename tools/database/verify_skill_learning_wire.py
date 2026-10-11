"""Original CS_SKILLBUY packet oracle: Client/CSSender, CSHandler, TSkillTemp,
TPlayer::UseMoney/GetTitle/IsEnoughSkillPoint and Server/CSSender. Runtime
responses are compared with independently calculated source rules, not receipts.
Synthetic account/level/funds only; all skills, NPC offers and prices are backed
by the verified original .bak charts. An actual client remains unavailable.
"""
import struct
from verify_login_wire import frame,read_packet


def verify_skill_learning(conn,cid,start,enter,login_port,map_port,until,restart):
    checks=[];client=None;cs,ss=3,4;selected=0;name=b''
    def check(ok,label):
        if not ok:raise RuntimeError('Skill learning wire: '+label)
        checks.append(label)
    if conn.execute('SELECT count(*) FROM character_compat.trainer_release').fetchone()!=(1,):
        return {'skipped':'Verified trainer manifest not supplied','tests':[]},map_port
    fields='"bLevel","dwHP","dwMP","dwGold","dwSilver","dwCooper","wSkillPoint","bStartAct","bClass"'
    original=conn.execute('SELECT '+fields+' FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    skills_saved=conn.execute('SELECT "wSkillID","bLevel","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,)).fetchall()
    titles_saved=conn.execute('SELECT "wTitleID","bSelected" FROM app_world."TTITLETABLE" WHERE "dwCharID"=%s',(cid,)).fetchall()
    definitions={r[0]:r[1:] for r in conn.execute('SELECT "wID","bKind","bLevel","bNextLevel","fPrice" FROM character_compat."TSKILLCHART"')}
    points={(r[0],r[1]):r[2] for r in conn.execute('SELECT "wID","bLevel","bSkillPoint" FROM actor_compat."TSKILLPOINTCHART"')}
    prices=dict(conn.execute('SELECT "bLevel","dwMoney" FROM character_compat."TLEVELCHART"').fetchall())
    gold_titles=dict(conn.execute('SELECT "wTitleID","dwRequirement" FROM character_compat."TTITLECHART" WHERE "bKind"=8 ORDER BY ("wTitleID"::integer&65535)').fetchall())
    state=[11,0,0,200];learned={s:r for s,r,_ in skills_saved if s not in (101,102,104,131)};titles={};character_class=0
    def connect():
        nonlocal client,cs,ss,selected,name
        _,key=start(login_port,cid);client,char=enter(map_port,cid,key);cs,ss=3,4;selected=char['title'];name=char['name'];return char
    def disconnect():
        nonlocal client
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'learning disconnect saves checkpoint')
    def gold_packets():
        nonlocal titles,selected
        eligible=[i for i,v in gold_titles.items() if v<state[0]]
        if len(eligible)>1 and eligible[-2]==selected:selected=eligible[-1]
        if any(i in titles for i in eligible):return []
        titles={i:s for i,s in titles.items() if i not in gold_titles};packets=[]
        for i in eligible:
            titles[i]=int(selected in gold_titles)
            packets.append(struct.pack('<B',len(titles))+b''.join(struct.pack('<HB',i,s) for i,s in sorted(titles.items()))+struct.pack('<HB',i,1))
        return packets
    def title_list():
        nonlocal cs,ss
        client.sendall(frame(b'',0x54f4,cs));cs+=1
        op,body=read_packet(client,ss);ss+=1
        expected=bytes([len(titles)])+b''.join(struct.pack('<HB',i,v) for i,v in sorted(titles.items()))
        check(op==0x54f5 and body==expected,'original TITLELIST returns exact ordered owned IDs and selected flags')
    def select_title(title,changed=True):
        nonlocal cs,ss,selected,titles
        client.sendall(frame(struct.pack('<H',title),0x54f6,cs));cs+=1
        if changed:
            op,body=read_packet(client,ss);ss+=1
            expected=struct.pack('<BIBBi',0,cid,103,0,len(name))+name+struct.pack('<HI',title,0)
            check(op==0x544a and body==expected,'original World round trip returns exact CHANGECHARBASE bytes and source string width')
            selected=title;titles={i:int(i==title) for i in titles}
        # A list is an ordered protocol barrier, proving no fabricated ACK for
        # repeated/unowned selection without relying on a short socket timeout.
        title_list()
        check(dict(conn.execute('SELECT "wTitleID","bSelected" FROM app_world."TTITLETABLE" WHERE "dwCharID"=%s',(cid,)).fetchall())==titles,'selection flags are committed before client notification')
    def purchase(npc,skill,code,rank):
        nonlocal cs,ss
        raw=struct.pack('<HH',npc,skill);expected_titles=[]
        if code==0:
            kind,first,next_level,multiplier=definitions[skill]
            def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
            price=int(f32(f32(prices[(first+learned.get(skill,0)*next_level)&255])*f32(float(multiplier))))
            if price:
                expected_titles+=gold_packets() # source UseMoney(price,FALSE)
                total=state[0]*1000000+state[1]*1000+state[2]-price
                state[:3]=[total//1000000,total//1000%1000,total%1000]
                expected_titles+=gold_packets() # source UseMoney(price,TRUE)
            state[3]-=points.get((skill,rank),0);learned[skill]=rank
        kind_points=[0]*4
        for i,r in learned.items():
            kind=definitions[i][0];index=kind-character_class*3 if kind else 0
            if 0<=index<4:kind_points[index]=(kind_points[index]+sum(points.get((i,n),0) for n in range(1,r+1)))&65535
        expected=struct.pack('<BHBI3IH4H',code,skill,rank,0,*state,*kind_points)
        client.sendall(frame(raw,0x52b2,cs));cs+=1
        for body in expected_titles:
            op,packet=read_packet(client,ss);ss+=1
            check(op==0x54f7 and packet==body,'source title-gain packet precedes skill purchase ACK with exact title list')
        op,packet=read_packet(client,ss);ss+=1
        check(op==0x52b3 and packet==expected and len(packet)==30,'all 30 original SKILLBUY ACK bytes including Tick and kind points')
        check(dict(conn.execute('SELECT "wTitleID","bSelected" FROM app_world."TTITLETABLE" WHERE "dwCharID"=%s',(cid,)).fetchall())==titles,'source gold-title ownership commits with purchase')
        actual=conn.execute('SELECT "dwGold","dwSilver","dwCooper","wSkillPoint" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        check(actual==tuple(state),'committed money and points match independently calculated original price')
        check(dict(conn.execute('SELECT "wSkillID","bLevel" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,)).fetchall())==learned,'committed skill membership and ranks match original learning result')
    try:
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=60,"dwHP"=100,"dwMP"=10000,"dwGold"=11,"dwSilver"=0,"dwCooper"=0,"wSkillPoint"=200,"bStartAct"=0,"bClass"=0 WHERE "dwCharID"=%s',(cid,))
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID" IN (101,102,104,131)',(cid,))
        conn.execute('DELETE FROM app_world."TTITLETABLE" WHERE "dwCharID"=%s',(cid,))
        char=connect();purchase(65535,131,1,0);purchase(22047,509,11,0)
        purchase(22047,131,0,1);purchase(22047,131,2,1)
        purchase(22047,101,0,1);purchase(22047,101,12,0)
        purchase(22047,104,0,1);purchase(22047,102,0,1)
        # Source upgrades require a talkable NPC, not its trainer merchandise.
        npc=conn.execute('SELECT ("wID"::integer&65535) FROM character_compat."TNPCCHART" WHERE "bCountryID"=3 AND "wLocalID"=0 AND "bDiscountRate"=0 AND "bType" NOT IN (1,19) ORDER BY "wID" LIMIT 1').fetchone()[0]
        title_list();select_title(85);select_title(85,False);select_title(65535,False)
        purchase(npc,101,0,2)
        check(selected==86 and titles[85]==1 and titles[86]==0,'source gold promotion keeps live ID separate from persisted selected flag')
        title_list()
        row=conn.execute('SELECT request,acknowledgement,spent FROM app_world.skill_purchase_operations WHERE char_id=%s ORDER BY purchase_id DESC LIMIT 1',(cid,)).fetchone()
        check(bytes(row[0])==struct.pack('<HH',npc,101) and len(row[1])==30 and row[2]>0,'atomic purchase receipt retains source request and positive charge')
        before=conn.execute('SELECT count(*) FROM app_world.skill_purchase_operations WHERE char_id=%s',(cid,)).fetchone()
        # Crash after committed ACK and reconnect must not buy a second rank.
        map_port=restart();client.close();client=None;char=connect()
        check(char['title']==85,'SIGKILL reload reconstructs title from source flags, not transient gold promotion')
        title_list();select_title(87)
        check(conn.execute('SELECT count(*) FROM app_world.skill_purchase_operations WHERE char_id=%s',(cid,)).fetchone()==before,'SIGKILL and reconnect never replay accepted purchase')
        check(dict(conn.execute('SELECT "wSkillID","bLevel" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,)).fetchall())==learned,'purchased ranks survive process replacement')
        disconnect();char=connect()
        # A late SQL fault occurs after rank/money/title writes and receipt insert.
        conn.execute("CREATE FUNCTION public.synthetic_learning_fault() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic learning checkpoint fault'; END $$")
        conn.execute('CREATE TRIGGER synthetic_learning_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.synthetic_learning_fault()')
        client.sendall(frame(struct.pack('<HH',22047,104),0x52b2,cs));cs+=1
        check(client.recv(1)==b'','late SQL failure closes TCP without a successful purchase ACK')
        conn.execute('DROP TRIGGER synthetic_learning_fault ON app_world.map_checkpoints');conn.execute('DROP FUNCTION public.synthetic_learning_fault()')
        check(conn.execute('SELECT count(*) FROM app_world.skill_purchase_operations WHERE char_id=%s',(cid,)).fetchone()==before,'failed purchase rolls its receipt back')
        check(conn.execute('SELECT "dwGold","dwSilver","dwCooper","wSkillPoint" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()==tuple(state),'failed purchase leaves coins and points unchanged')
        map_port=restart();client.close();client=None;char=connect()
        # The identical request succeeds after removing the injected fault,
        # proving the rollback exercised a payable rank change, not a rejection.
        purchase(22047,104,0,2)
        title_before=conn.execute('SELECT count(*) FROM app_world.title_selection_operations WHERE char_id=%s',(cid,)).fetchone()
        conn.execute("CREATE FUNCTION public.synthetic_title_fault() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic title checkpoint fault'; END $$")
        conn.execute('CREATE TRIGGER synthetic_title_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.synthetic_title_fault()')
        client.sendall(frame(struct.pack('<H',84),0x54f6,cs));cs+=1
        check(client.recv(1)==b'','late title SQL failure closes TCP without World/client success')
        conn.execute('DROP TRIGGER synthetic_title_fault ON app_world.map_checkpoints');conn.execute('DROP FUNCTION public.synthetic_title_fault()')
        check(conn.execute('SELECT count(*) FROM app_world.title_selection_operations WHERE char_id=%s',(cid,)).fetchone()==title_before,'failed title change rolls its append receipt back')
        check(dict(conn.execute('SELECT "wTitleID","bSelected" FROM app_world."TTITLETABLE" WHERE "dwCharID"=%s',(cid,)).fetchall())==titles,'failed title change preserves every old selected flag')
        map_port=restart();client.close();client=None;char=connect()
        check(char['title']==87,'restart after failed title selection recovers the last committed title')
        select_title(84)
        # Strict decoding leaves durable ownership unchanged.
        client.sendall(frame(struct.pack('<HHB',22047,101,0),0x52b2,cs));cs+=1
        check(client.recv(1)==b'','malformed purchase with trailing byte closes TCP')
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'malformed learning teardown completes')
    finally:
        if client:client.close()
        conn.execute('DROP TRIGGER IF EXISTS synthetic_title_fault ON app_world.map_checkpoints');conn.execute('DROP FUNCTION IF EXISTS public.synthetic_title_fault()')
        conn.execute('DROP TRIGGER IF EXISTS synthetic_learning_fault ON app_world.map_checkpoints');conn.execute('DROP FUNCTION IF EXISTS public.synthetic_learning_fault()')
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'learning fixture release')
        names=fields.split(',');conn.execute('UPDATE app_world."TCHARTABLE" SET '+','.join(n+'=%s' for n in names)+' WHERE "dwCharID"=%s',(*original,cid))
        conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,))
        for s,r,t in skills_saved:conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,%s,%s,%s)',(cid,s,r,t))
        conn.execute('DELETE FROM app_world."TTITLETABLE" WHERE "dwCharID"=%s',(cid,))
        for i,s in titles_saved:conn.execute('INSERT INTO app_world."TTITLETABLE" VALUES(1,%s,%s,%s)',(cid,i,s))
    return {'passed':len(checks),'tests':checks,'source_trainer':22047,'client_acceptance':'BLOCKED: original client EXE/assets unavailable'},map_port
