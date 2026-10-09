"""Whole-stack carried inventory moves through actual encrypted native daemons."""
import select
import struct
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent


def verify_inventory_moves(conn,cid,start,enter,login_port,map_port,until):
    checks=[];items=[];s=None
    def check(ok,label):
        if not ok:raise RuntimeError('Inventory move: '+label)
        checks.append(label)
    def inventory():
        return conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
    original=inventory()
    check(conn.execute('SELECT count(*) FROM app_world."TINVENTABLE" WHERE "dwCharID"=%s AND "bInvenID"=4',(cid,)).fetchone()==(0,),
          'synthetic extra bag is absent before fixture')
    try:
        conn.execute('INSERT INTO app_world."TINVENTABLE"("bWorldID","dwCharID","bInvenID","wItemID","dEndTime") VALUES(1,%s,4,4,\'1900-01-01\')',(cid,))
        second=seed_graph_reagent(conn,cid,True);items.append(second)
        conn.execute('UPDATE app_world."TITEMTABLE" SET "bItemID"=3,"wItemID"=11054,"bCount"=3 WHERE "dlID"=%s',(second,))
        first=seed_graph_reagent(conn,cid,True);items.append(first)
        conn.execute('UPDATE app_world."TITEMTABLE" SET "bCount"=8 WHERE "dlID"=%s',(first,))
        attributes={row[0]['dlID']:{k:v for k,v in row[0].items() if k not in ('dwStorageID','bItemID')} for row in inventory()}
        _,key=start(login_port,cid);s,char=enter(map_port,cid,key);cs,ss=3,4
        descriptors={item[1]:item for bag,item,magic in char['items'] if bag==255 and item[0] in (2,3)}
        def expected(template,bag,slot):
            desc=list(descriptors[template]);desc[0]=slot
            return bytes([bag])+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*desc)
        def request(src_bag,src_slot,dst_bag,dst_slot,count):
            nonlocal cs
            s.sendall(frame(bytes([src_bag,src_slot,dst_bag,dst_slot,count]),0x52a8,cs));cs+=1
        def reply():
            nonlocal ss
            result=read_packet(s,ss);ss+=1;return result
        def receipts():
            return conn.execute('SELECT count(*),count(DISTINCT operation_id),bool_and(state_contract=3) FROM app_world.inventory_movements WHERE char_id=%s AND item_id=ANY(%s)',(cid,items)).fetchone()
        def positions():
            return conn.execute('SELECT "dlID","dwStorageID","bItemID","bCount" FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s) ORDER BY "dlID"',(items,)).fetchall()
        for req,result in [((240,0,255,0,1),2),((255,9,255,0,1),3),((255,2,241,0,1),1),((255,2,255,2,1),4),((255,2,255,2,0),3)]:
            request(*req);check(reply()==(0x52a9,bytes([result])),'invalid move returns exact source error '+str(result))
        check(receipts()[0]==0,'rejected moves append no mutation receipts')
        request(255,2,4,3,255)
        check(reply()==(0x52ac,b'\xff\x02'),'whole-stack move deletes original source slot')
        check(reply()==(0x52ab,expected(8401,4,3)),'whole-stack move adds exact original descriptor at last valid destination slot')
        check(reply()==(0x52a9,b'\0'),'move success follows DEL and ADD')
        check(receipts()==(1,1,True),'whole-stack move has one durable receipt before success')
        check((first,4,3,8) in positions(),'whole-stack move preserves identity and count despite oversized request')
        conn.execute('CREATE FUNCTION public.delay_inventory_move() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.15); RETURN NEW; END $$')
        conn.execute('CREATE TRIGGER synthetic_inventory_move_delay BEFORE INSERT ON app_world.inventory_movements FOR EACH ROW EXECUTE FUNCTION public.delay_inventory_move()')
        try:
            request(4,3,255,3,1)
            check(not select.select([s],[],[],.08)[0],'swap publishes no private response before both receipts commit')
            check(reply()==(0x52aa,expected(8401,255,3)),'swap updates source at destination with its full count')
            check(reply()==(0x52aa,expected(11054,4,3)),'swap updates destination at source in original order')
            check(reply()==(0x52a9,b'\0'),'one swap success follows both full descriptors')
        finally:
            conn.execute('DROP TRIGGER synthetic_inventory_move_delay ON app_world.inventory_movements')
            conn.execute('DROP FUNCTION public.delay_inventory_move()')
        check(receipts()==(3,2,True),'two swapped rows share exactly one operation')
        check((first,255,3,8) in positions() and (second,4,3,3) in positions(),'cross-bag swap commits both original identities and quantities')
        check(conn.execute('SELECT app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(True,),
              'inventory transaction preserves a valid recovery receipt')
        s.close();s=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'inventory swap logout completes')
        _,key=start(login_port,cid);s,restored=enter(map_port,cid,key);cs,ss=3,4
        check(any(bag==255 and item[0]==3 and item[1]==8401 and item[6]==8 for bag,item,magic in restored['items']) and
              any(bag==4 and item[0]==3 and item[1]==11054 and item[6]==3 for bag,item,magic in restored['items']),
              'relogin restores both swapped positions and full stack descriptors')
        request(255,3,4,3,1)
        check(reply()==(0x52aa,expected(8401,4,3)) and reply()==(0x52aa,expected(11054,255,3)) and reply()==(0x52a9,b'\0'),
              'relogged character swaps the same identities back')
        request(4,3,255,2,8)
        check(reply()==(0x52ac,b'\x04\x03') and reply()==(0x52ab,expected(8401,255,2)) and reply()==(0x52a9,b'\0'),
              'return move preserves exact original inventory response order')
        check(receipts()==(6,4,True),'four complete operations record six moved item receipts without duplication')
        check(attributes=={row[0]['dlID']:{k:v for k,v in row[0].items() if k not in ('dwStorageID','bItemID')} for row in inventory()},
              'all original item fields except positions remain byte-for-value unchanged')
        s.close();s=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'inventory return logout completes')
        for req,label in [((255,2,4,4,8),'out-of-capacity destination'),((255,2,252,3,1),'unsupported drop')]:
            _,key=start(login_port,cid);s,_=enter(map_port,cid,key);cs,ss=3,4
            request(*req)
            check(s.recv(1)==b'',label+' closes before any success or item mutation')
            s.close();s=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),label+' closes with confirmed unchanged save')
            check(receipts()==(6,4,True),label+' cannot append a durable move')
    finally:
        if s is not None:s.close()
        if items:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s)',(items,))
        conn.execute('DELETE FROM app_world."TINVENTABLE" WHERE "dwCharID"=%s AND "bInvenID"=4',(cid,))
    check(inventory()==original,'fixture restores all original inventory rows after removing only synthetic items and bag')
    return {'status':'passed','checks':checks,'scope':'Actual encrypted fresh-primary whole-stack and different-template cross-bag moves/swaps, exact errors and inventory packets, delayed commit, capacities, no drop, full-field conservation and relogin. Graph transfer/recovery is additionally tested at the native service boundary; no original-client executable.'}
