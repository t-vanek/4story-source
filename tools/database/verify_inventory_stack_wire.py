"""Independent source MOVEITEM expectations for native split/merge/equality paths."""
import select
import struct
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent


def verify_inventory_stacks(conn,cid,start,enter,login_port,map_port,until):
    checks=[];owned=[];s=None
    def check(ok,label):
        if not ok:raise RuntimeError('Inventory stack: '+label)
        checks.append(label)
    def inventory():return conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
    original=inventory()
    limit=conn.execute('SELECT "bStack" FROM character_compat."TITEMCHART" WHERE "wItemID"=8401').fetchone()[0]
    check(8<=limit<=255,'source arrow template has a verified BYTE stack capacity')
    try:
        # Clone only synthetic mutable rows. The source catalog is read-only.
        for slot,count,gem in [(4,3,1),(3,limit-2,0),(2,8,0)]:
            item=seed_graph_reagent(conn,cid,True);owned.append(item)
            conn.execute('UPDATE app_world."TITEMTABLE" SET "bItemID"=%s,"bCount"=%s,"bGem"=%s WHERE "dlID"=%s',(slot,count,gem,item))
        variant,dest,source=owned
        baseline=inventory()
        _,key=start(login_port,cid);s,char=enter(map_port,cid,key);cs,ss=3,4
        descriptors={item[0]:item for bag,item,magic in char['items'] if bag==255 and item[0] in (2,3,4)}
        def descriptor(origin,slot,count):
            row=list(descriptors[origin]);row[0]=slot;row[6]=count
            return b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*row)
        def request(src,dst,count):
            nonlocal cs
            s.sendall(frame(bytes([255,src,255,dst,count]),0x52a8,cs));cs+=1
        def response(expected,label):
            nonlocal ss
            check(read_packet(s,ss)==expected,label);ss+=1
        def counts():return conn.execute('SELECT "bItemID","bCount" FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s) ORDER BY "bItemID"',(owned,)).fetchall()
        request(2,3,255)
        response((0x52aa,descriptor(2,2,6)),'capacity-clamped merge updates remaining source first')
        response((0x52aa,descriptor(3,3,limit)),'merge fills destination only to pinned bStack')
        response((0x52a9,b'\0'),'clamped merge succeeds after both item responses')
        check(counts()==[(2,6),(3,limit),(4,3)],'clamped merge conserves total quantity in PostgreSQL')
        request(2,3,255)
        response((0x52aa,descriptor(2,2,6)),'full destination retains original unchanged source UPDATE')
        response((0x52aa,descriptor(3,3,limit)),'full destination retains original unchanged destination UPDATE')
        response((0x52a9,b'\0'),'full destination preserves original successful no-op result')
        request(3,2,2)
        response((0x52aa,descriptor(3,3,limit-2)),'partial merge honors requested quantity')
        response((0x52aa,descriptor(2,2,8)),'partial merge preserves original destination identity')
        response((0x52a9,b'\0'),'partial merge completes')
        request(2,4,1)
        response((0x52aa,descriptor(2,4,8)),'same template with different gem swaps the full source stack')
        response((0x52aa,descriptor(4,2,3)),'same-template unequal swap retains destination attributes and count')
        response((0x52a9,b'\0'),'same-template unequal swap completes')
        request(4,2,1)
        response((0x52aa,descriptor(2,2,8)),'reverse unequal swap returns original stack')
        response((0x52aa,descriptor(4,4,3)),'reverse unequal swap returns gem-bearing item')
        response((0x52a9,b'\0'),'reverse unequal swap completes')
        high=conn.execute('SELECT item_high_water FROM app_world.worlds WHERE group_id=1').fetchone()[0]
        conn.execute('CREATE FUNCTION public.delay_inventory_stack() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN PERFORM pg_sleep(.15); RETURN NEW; END $$')
        conn.execute('CREATE TRIGGER synthetic_stack_delay BEFORE INSERT ON app_world.inventory_stack_changes FOR EACH ROW EXECUTE FUNCTION public.delay_inventory_stack()')
        try:
            request(2,5,3)
            check(not select.select([s],[],[],.08)[0],'split sends no item or success response before both receipts commit')
            response((0x52aa,descriptor(2,2,5)),'split updates surviving source before adding destination')
            response((0x52ab,descriptor(2,5,3)),'split copies exact complete original descriptor to new slot')
            response((0x52a9,b'\0'),'split succeeds after UPDATE and ADD')
        finally:
            conn.execute('DROP TRIGGER synthetic_stack_delay ON app_world.inventory_stack_changes')
            conn.execute('DROP FUNCTION public.delay_inventory_stack()')
        created=conn.execute('SELECT "dlID" FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=255 AND "bItemID"=5',(cid,)).fetchone()[0];owned.append(created)
        check(created==high+1 and created!=source,'split uses the same atomic allocator as native character creation')
        check(conn.execute('SELECT before_count,after_count,parent_id FROM app_world.inventory_stack_changes WHERE char_id=%s AND item_id=%s',(cid,created)).fetchone()==(0,3,source),'split creation receipt links the new identity to its original source')
        s.close();s=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'split logout completes')
        _,key=start(login_port,cid);s,char=enter(map_port,cid,key);cs,ss=3,4
        check(any(bag==255 and item[0]==5 and item[1]==8401 and item[6]==3 for bag,item,magic in char['items']),'fresh relogin restores the split stack')
        request(5,2,255)
        response((0x52ac,b'\xff\x05'),'complete merge deletes emptied source first')
        response((0x52aa,descriptor(2,2,8)),'complete merge updates original destination with full quantity')
        response((0x52a9,b'\0'),'complete merge succeeds after DEL and UPDATE')
        check(conn.execute('SELECT count(*) FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(created,)).fetchone()==(0,),'deleted split identity cannot remain as an empty duplicate')
        check(inventory()==baseline,'split and recombination preserve every original database item field')
        check(conn.execute('SELECT count(*) FROM (SELECT operation_id FROM app_world.inventory_stack_changes WHERE char_id=%s GROUP BY operation_id HAVING sum(before_count)<>sum(after_count) OR count(*)<>2) q',(cid,)).fetchone()==(0,),'every TCP stack operation records an atomic balanced pair')
        check(conn.execute('SELECT app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(True,),'stack mutations leave a valid recovery receipt')
        s.close();s=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'stack fixture logout completes')
    finally:
        if s is not None:s.close()
        if owned:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=ANY(%s)',(owned,))
    check(inventory()==original,'stack fixture removes only its own synthetic items')
    return {'status':'passed','checks':checks,'scope':'Actual encrypted native fresh split/merge, stack cap/no-op, same-template unequal swaps, original ACK bytes/order, allocator, delayed commit and relogin. No original client executable.'}


def graph_stack_packet(conn,s,cid,descriptor,cs,ss,count,stage,check):
    # Independent source descriptors: split then move on Map2; merge on Map1
    # after transferring the resulting graph back over the actual World link.
    def expected(slot,quantity):
        row=list(descriptor);row[0]=slot;row[6]=quantity
        return b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*row)
    if stage=='split':
        request=bytes([255,2,255,3,1]);responses=[(0x52aa,expected(2,count-1)),(0x52ab,expected(3,1)),(0x52a9,b'\0')]
    elif stage=='move':
        request=bytes([255,3,255,4,1]);responses=[(0x52ac,b'\xff\x03'),(0x52ab,expected(4,1)),(0x52a9,b'\0')]
    else:
        request=bytes([255,4,255,2,255]);responses=[(0x52ac,b'\xff\x04'),(0x52aa,expected(2,count)),(0x52a9,b'\0')]
    s.sendall(frame(request,0x52a8,cs))
    for i,response in enumerate(responses):check(read_packet(s,ss+i)==response,'graph '+stage+' emits exact original ordered inventory response '+str(i))
    check(conn.execute('SELECT recovery_contract,app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(2,True),'graph '+stage+' commits a valid full-state recovery receipt')
    check(conn.execute('SELECT count(*) FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=255 AND "bItemID" IN (3,4)',(cid,)).fetchone()==(0,),'graph '+stage+' never materializes stale normalized child rows')
