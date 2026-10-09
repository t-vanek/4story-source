"""Independent original MOVEITEM/ChangeEquipItem wire expectations on native PG."""
import select
import struct
from psycopg import sql
from verify_login_wire import frame,read_packet
from verify_character_statistics_wire import source_statistics


def descriptor(item,magic):
    return struct.pack('<BHBBHHBIIBBBqBBBHHBB',*item)+b''.join(struct.pack('<BH',*x) for x in magic)


def verify_equipment(conn,cid,start,enter,login_port,map_port,until):
    checks=[];client=None;created=None
    def check(ok,label):
        if not ok:raise RuntimeError('Equipment wire: '+label)
        checks.append(label)
    def inventory():return conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
    original=inventory()
    row=conn.execute('SELECT * FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=3',(cid,))
    item=dict(zip((c.name for c in row.description),row.fetchone()))
    old_id=item['dlID']
    item['dlID']=conn.execute('UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=1 RETURNING item_high_water').fetchone()[0]
    created=item['dlID'];item.update(dwStorageID=255,bItemID=2,bGem=item['bGem']+1)
    conn.execute(sql.SQL('INSERT INTO app_world."TITEMTABLE" ({}) VALUES ({})').format(sql.SQL(',').join(map(sql.Identifier,item)),sql.SQL(',').join(sql.Placeholder() for _ in item)),list(item.values()))
    try:
        _,key=start(login_port,cid);client,character=enter(map_port,cid,key);cs,ss=3,4
        equipment={i[0]:(i,m) for bag,i,m in character['items'] if bag==254}
        carried={i[0]:(i,m) for bag,i,m in character['items'] if bag==255}
        baseline=inventory();initial_stats=source_statistics(conn,cid);initial_hpmp=character['hpmp']
        def request(sb,sp,db,dp,count=1):
            nonlocal cs
            client.sendall(frame(bytes([sb,sp,db,dp,count]),0x52a8,cs));cs+=1
        def response(value,label):
            nonlocal ss
            check(read_packet(client,ss)==value,label);ss+=1
        def full_equipment(label):
            response((0x52ad,struct.pack('<IB',cid,len(equipment))+b''.join(descriptor(*equipment[k]) for k in sorted(equipment))),label+' complete original EQUIP')
            response((0x52a9,b'\0'),label+' first MOVEITEM success')
            response((0x5324,source_statistics(conn,cid)),label+' independently derived 87-byte statistics')
            hp,mp=conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
            response((0x52a2,struct.pack('<IBIIII',cid,1,initial_hpmp[0],hp,initial_hpmp[2],mp)),label+' original HPMP')
            response((0x52a9,b'\0'),label+' final source MOVEITEM success')
        for req,result in [((254,3,254,3,1),4),((254,19,255,3,1),3),((255,2,254,2,1),5)]:
            request(*req);response((0x52a9,bytes([result])),'source equipment rejection '+str(result))
        check(inventory()==baseline,'rejected equipment requests leave every item field unchanged')
        conn.execute("CREATE FUNCTION public.delay_native_equipment() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.2); RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_equipment_delay BEFORE INSERT ON app_world.equipment_operations FOR EACH ROW EXECUTE FUNCTION public.delay_native_equipment()')
        try:
            request(255,2,254,3)
            check(not select.select([client],[],[],.08)[0],'no item, EQUIP or success before complete delayed commit')
            new,old=carried.pop(2),equipment.pop(3)
            new=(tuple([3,*new[0][1:]]),new[1]);old=(tuple([2,*old[0][1:]]),old[1]);equipment[3]=new;carried[2]=old
            response((0x52aa,b'\xfe'+descriptor(*new)),'swap puts incoming full descriptor in equipment first')
            response((0x52aa,b'\xff'+descriptor(*old)),'swap puts displaced descriptor into original carried slot second')
            full_equipment('swap:')
        finally:
            conn.execute('DROP TRIGGER synthetic_equipment_delay ON app_world.equipment_operations')
            conn.execute('DROP FUNCTION public.delay_native_equipment()')
        check(conn.execute('SELECT "dwStorageID","bItemID" FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(created,)).fetchone()==(254,3),'fresh equipment identity is durably equipped')
        check(conn.execute('SELECT state_contract,changed_items FROM app_world.equipment_operations WHERE char_id=%s ORDER BY operation_id DESC LIMIT 1',(cid,)).fetchone()==(3,2),'equipment swap receipt identifies both atomic changes')
        # Reverse request is normalized by the original source before validation.
        request(254,3,255,2)
        old,new=carried.pop(2),equipment.pop(3)
        old=(tuple([3,*old[0][1:]]),old[1]);new=(tuple([2,*new[0][1:]]),new[1]);equipment[3]=old;carried[2]=new
        response((0x52aa,b'\xfe'+descriptor(*old)),'reverse swap equips carried item first')
        response((0x52aa,b'\xff'+descriptor(*new)),'reverse swap stores previously equipped item second')
        full_equipment('reverse:')
        check(source_statistics(conn,cid)==initial_stats and inventory()==baseline,'reverse swap restores original stats and exact item rows')
        request(254,3,255,3,255);head=equipment.pop(3);head=(tuple([3,*head[0][1:]]),head[1]);carried[3]=head
        response((0x52ac,b'\xfe\x03'),'unequip deletes exact equipped slot')
        response((0x52ab,b'\xff'+descriptor(*head)),'unequip adds source descriptor to carried bag')
        full_equipment('unequip:')
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'equipment logout completes')
        _,key=start(login_port,cid);client,restored=enter(map_port,cid,key);cs,ss=3,4
        check(not any(b==254 and i[0]==3 for b,i,m in restored['items']) and any(b==255 and i==head[0] and m==head[1] for b,i,m in restored['items']),'fresh relogin retains unequipped item')
        request(255,3,254,3,255);equipment[3]=head;carried.pop(3)
        response((0x52ac,b'\xff\x03'),'reequip deletes original carried slot')
        response((0x52ab,b'\xfe'+descriptor(*head)),'reequip restores exact equipped descriptor')
        full_equipment('reequip:')
        check(inventory()==baseline,'complete equipment lifecycle preserves every raw item attribute and identity')
        check(conn.execute('SELECT app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(True,),'equipment changes retain valid core/skill recovery receipt')
        check(conn.execute('SELECT count(*) FROM (SELECT c.operation_id FROM app_world.equipment_item_changes c JOIN app_world.equipment_operations o USING(operation_id) WHERE o.char_id=%s GROUP BY c.operation_id HAVING sum(before_count)<>sum(after_count)) q',(cid,)).fetchone()==(0,),'every equipment operation conserves total quantity')
        conn.execute("CREATE FUNCTION public.delay_native_equipment() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.3); RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_equipment_delay BEFORE INSERT ON app_world.equipment_operations FOR EACH ROW EXECUTE FUNCTION public.delay_native_equipment()')
        try:
            request(254,3,255,3)
            until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]>0,'equipment commit is pending when client disconnects')
            client.close();client=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'disconnect drains equipment commit and final save')
            check(conn.execute('SELECT "dwStorageID","bItemID" FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(old_id,)).fetchone()==(255,3),
                  'disconnect during commit preserves committed unequip instead of stale teardown overwrite')
        finally:
            conn.execute('DROP TRIGGER synthetic_equipment_delay ON app_world.equipment_operations')
            conn.execute('DROP FUNCTION public.delay_native_equipment()')
        _,key=start(login_port,cid);client,recovered=enter(map_port,cid,key);cs,ss=3,4
        check(any(b==255 and i==head[0] and m==head[1] for b,i,m in recovered['items']),
              'next real Login restores equipment committed during disconnect')
        request(255,3,254,3)
        response((0x52ac,b'\xff\x03'),'post-disconnect reequip removes carried item')
        response((0x52ab,b'\xfe'+descriptor(*head)),'post-disconnect reequip restores equipped item')
        full_equipment('post-disconnect:')
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'equipment fixture final save completes')
    finally:
        if client:client.close()
        if created:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(created,))
    check(inventory()==original,'equipment fixture removes only its own synthetic item')
    return dict(status='passed',checks=checks,scope='Native encrypted TCP armor equip/unequip/swap, exact source reply ordering and bytes, delayed commit, PostgreSQL item/core/skill receipts and relogin; active-effect and original-client acceptance remain open.')
