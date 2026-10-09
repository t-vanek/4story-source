"""Original ForceMaintain/EraseMaintainSkill packet oracle, real native PG/TCP.

Templates and rates come from the pinned backup. Packet layouts and ordering
are transcribed from original CSHandler, TPlayer, TObjBase and CSSender.
"""
import select
import struct
from psycopg import sql
from verify_login_wire import frame,read_packet
from verify_equipment_wire import descriptor
from verify_character_statistics_wire import source_statistics


def verify_postures(conn,cid,start,enter,login_port,map_port,until,restart):
    checks=[];client=None
    def check(ok,label):
        if not ok:raise RuntimeError('Posture wire: '+label)
        checks.append(label)
    def items():return [r[0] for r in conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,))]
    original=items()
    conn.execute('UPDATE app_world."TITEMTABLE" SET "dwStorageID"=255,"bItemID"=13 WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=1',(cid,))
    learned={r[0] for r in conn.execute('SELECT "wSkillID" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,))}
    weapon=next(i for i in original if i['dwStorageID']==254 and i['bItemID']==0)
    def insert(row):
        conn.execute(sql.SQL('INSERT INTO app_world."TITEMTABLE" ({}) VALUES ({})').format(sql.SQL(',').join(map(sql.Identifier,row)),sql.SQL(',').join(sql.Placeholder() for _ in row)),list(row.values()))
    for slot,template in ((10,1001),(11,401)):
        row=weapon.copy();row['dlID']=conn.execute('UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=1 RETURNING item_high_water').fetchone()[0]
        row.update(dwStorageID=255,bItemID=slot,wItemID=template,bCount=1);insert(row)
    for skill in (8,14):
        conn.execute('INSERT INTO app_world."TSKILLTABLE"("bWorldID","dwCharID","wSkillID","bLevel","dwRemainTick") VALUES(1,%s,%s,1,0) ON CONFLICT DO NOTHING',(cid,skill))
    cs,ss=3,4;equipment={};carried={};character={}
    def connect():
        nonlocal client,cs,ss,equipment,carried,character
        _,key=start(login_port,cid);client,character=enter(map_port,cid,key);cs,ss=3,4
        equipment={i[0]:(i,m) for b,i,m in character['items'] if b==254}
        carried={i[0]:(i,m) for b,i,m in character['items'] if b==255}
    def disconnect():
        nonlocal client
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'posture disconnect completes fenced save')
    def request(sb,sp,db,dp):
        nonlocal cs
        client.sendall(frame(bytes([sb,sp,db,dp,1]),0x52a8,cs));cs+=1
    def response(op,data,label):
        nonlocal ss
        actual=read_packet(client,ss);check(actual==(op,data),label);ss+=1
    def relocate(table,slot,target):
        item,magic=table.pop(slot);return (tuple([target,*item[1:]]),magic)
    def stats(effects,label):response(0x5324,source_statistics(conn,cid,effects=[(i,1) for i in effects]),label)
    def defend(skill):
        level,country=conn.execute('SELECT "bLevel","bCountry" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        values=[cid,cid,1,1,0,0,0,0,1,0,0,1,1,level,0,0,0,0,1,0,country,3,skill,1,0,1,*character['position'],*character['position'],0]
        body=struct.pack('<IIBBIBIIBIBBHB4I4BHBHB6fB',*values)
        check(len(body)==84,'ForceMaintain original DEFEND body is 84 bytes')
        response(0x52a1,body,'automatic posture DEFEND fields match original source')
    def end(skill,effects,label):
        response(0x52b7,struct.pack('<IBH',cid,1,skill),label+' SKILLEND')
        stats(effects,label+' post-erase CHARSTATINFO')
    def equip(effects,cancel=None):
        response(0x52ad,struct.pack('<IB',cid,len(equipment))+b''.join(descriptor(*equipment[k]) for k in sorted(equipment)),'complete EQUIP after posture creation')
        response(0x52a9,b'\0','first equipment MOVEITEM success')
        stats(effects,'intermediate source equipment statistics')
        hp,mp=conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        response(0x52a2,struct.pack('<IBIIII',cid,1,character['hpmp'][0],hp,character['hpmp'][2],mp),'equipment HPMP before posture cancellation')
        if cancel:end(cancel,[],'CheckEquipSkill')
        response(0x52a9,b'\0','final MOVEITEM follows all posture changes')
    def shield_on():
        request(255,10,254,1);equipment[1]=relocate(carried,10,1)
        response(0x52ac,b'\xff\x0a','shield removes carried source')
        response(0x52ab,b'\xfe'+descriptor(*equipment[1]),'shield adds exact equipment descriptor')
        defend(131);equip([131])
    def shield_off():
        request(254,1,255,10);carried[10]=relocate(equipment,1,10)
        response(0x52ac,b'\xfe\x01','shield unequip removes equipped source')
        response(0x52ab,b'\xff'+descriptor(*carried[10]),'shield unequip restores carried descriptor')
        equip([131],131)
    try:
        connect();check(character['effects']==[],'fresh character has no fabricated posture')
        shield_on()
        state=conn.execute('SELECT recovery_contract,maintain_state,app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=%s',(cid,)).fetchone()
        check(state[0]==4 and state[2] and state[1]==[[131,1,0,1,cid,1,cid,4]],'permanent posture commits original eight fields in valid v4 receipt')
        disconnect();connect()
        check(character['effects']==[(131,1,0,cid,1,cid,1,1,1,1,0,0,0,0,1,4,0.,0.,0.)],'CHARINFO restores all 51 maintained bytes with original CTSkill constructor defaults')
        shield_off()
        check(conn.execute('SELECT recovery_contract,maintain_state FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(3,None),'last posture cancellation restores empty native contract3')
        shield_on()
        # Two-handed weapon displaces the shield into first default-bag blank,
        # then swaps the old main weapon before replacing the active posture.
        destination=next(i for i in range(24) if i not in carried)
        request(255,11,254,0)
        carried[destination]=relocate(equipment,1,destination)
        response(0x52ac,b'\xfe\x01','two-handed weapon first displaces shield')
        response(0x52ab,b'\xff'+descriptor(*carried[destination]),'displaced shield uses source default-bag priority')
        old=relocate(equipment,0,11);equipment[0]=relocate(carried,11,0);carried[11]=old
        response(0x52aa,b'\xfe'+descriptor(*equipment[0]),'two-hand swap equips new source')
        response(0x52aa,b'\xff'+descriptor(*carried[11]),'two-hand swap retains old main weapon identity')
        end(131,[],'UpdateBuffSkill');defend(132);equip([132])
        check(conn.execute('SELECT before_effects,after_effects FROM app_world.equipment_operations WHERE char_id=%s ORDER BY operation_id DESC LIMIT 1',(cid,)).fetchone()==([[131,1,0,1,cid,1,cid,4]],[[132,1,0,1,cid,1,cid,4]]),'single item operation records exact posture replacement')
        # Crash with no logout: recovery must retain the immediately committed
        # permanent effect, even before the next periodic snapshot.
        map_port=restart();client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'replacement Map recovers posture owner')
        check(conn.execute('SELECT outcome,maintain_state,app_world.map_checkpoint_matches(p) FROM app_world.map_checkpoints p WHERE char_id=%s',(cid,)).fetchone()==('recovered',[[132,1,0,1,cid,1,cid,4]],True),'SIGKILL recovery retains native posture receipt')
        connect();check(character['effects'][0][:3]==(132,1,0),'permanent attack posture survives actual process crash and relogin')
        client.sendall(frame(struct.pack('<I',cid),0x5323,cs));cs+=1;stats([132],'recovered posture statistics match independent original formulas')
        conn.execute("CREATE FUNCTION public.delay_posture_commit() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.3); RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_posture_delay BEFORE INSERT ON app_world.equipment_operations FOR EACH ROW EXECUTE FUNCTION public.delay_posture_commit()')
        try:
            request(254,0,255,12)
            until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]>0,'posture cancellation waits in actual transaction')
            check(not select.select([client],[],[],.02)[0],'no item or effect success before complete PostgreSQL commit')
            client.close();client=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'disconnect during cancellation drains transaction without stale logout')
        finally:
            conn.execute('DROP TRIGGER synthetic_posture_delay ON app_world.equipment_operations');conn.execute('DROP FUNCTION public.delay_posture_commit()')
        connect();check(character['effects']==[] and 0 not in equipment and 12 in carried,'disconnect during commit preserves unequipped weapon and cancelled posture together')
        disconnect()
    finally:
        if client is not None:client.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'posture fixture is offline before cleanup')
        conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s',(cid,))
        for row in original:insert(row)
        for skill in (8,14):
            if skill not in learned:conn.execute('DELETE FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,skill))
    return {'status':'passed','checks':checks,'scope':'Permanent 131/132 equipment postures, original synthetic TCP, native PG, actual process SIGKILL; no original-client acceptance'},map_port
