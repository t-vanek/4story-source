"""Synthetic-owned reagent stack through actual encrypted primary handoffs.

Only runtime fixture rows are seeded. Historical catalogs stay read-only.
Graph inventory stays authoritative after transfer even when item rows are stale.
"""
import select
import struct
import time
from psycopg import sql
from verify_login_wire import frame, read_packet


def seed_graph_reagent(conn,cid,ammunition=False):
    row=conn.execute('SELECT * FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s ORDER BY "dlID" LIMIT 1',(cid,))
    item=dict(zip((c.name for c in row.description),row.fetchone()))
    item['dlID']=conn.execute('UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=1 RETURNING item_high_water').fetchone()[0]
    item.update(dwStorageID=255,bItemID=2,wItemID=8401 if ammunition else 31238,bCount=3,dwDuraMax=0,dwDuraCur=0,bRefineCur=0)
    for prefix in ('bMagic','wValue','dwTime'):
        for i in range(1,7):item[prefix+str(i)]=0
    conn.execute(sql.SQL('INSERT INTO app_world."TITEMTABLE" ({}) VALUES ({})').format(
        sql.SQL(',').join(map(sql.Identifier,item)),sql.SQL(',').join(sql.Placeholder() for _ in item)),list(item.values()))
    return item['dlID']


def graph_reagent_cast(conn,s,cid,item_id,descriptor,client_sequence,server_sequence,count,loop,check,delay=False,ammunition=False):
    if ammunition:time.sleep(1.6) # source skill32: 1200ms + bow701 300ms
    if loop:
        request=struct.pack('<IBBHHfffB',cid,1,1,0,32 if ammunition else 1623,0,0,0,0)
    else:
        request=struct.pack('<IBBHHBIIfffB',cid,1,1,0,32 if ammunition else 1623,0,0,0,0,0,0,0)
    if ammunition:request=request[:-1]+b'\x01'+struct.pack('<IBB',cid+100000,2,1)
    if delay:
        conn.execute("CREATE FUNCTION public.delay_graph_reagent() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.state_contract=2 THEN PERFORM pg_sleep(.3); END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_graph_reagent_delay BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW EXECUTE FUNCTION public.delay_graph_reagent()')
    try:
        s.sendall(frame(request,0x5372 if loop else 0x52b4,client_sequence))
        if delay:
            check(not select.select([s],[],[],.08)[0],'graph cast emits no item or success response before delayed transaction completes')
        try:op,data=read_packet(s,server_sequence)
        except (OSError,TimeoutError,RuntimeError) as error:
            raise RuntimeError('Graph reagent: original inventory ACK missing after primary transfer') from error
        if count>1:
            expected=list(descriptor);expected[6]=count-1
            check(op==0x52aa and data==b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*expected),
                  'graph cast sends complete original UPDATEITEM descriptor with exact decremented count')
        else:check((op,data)==(0x52ac,b'\xff\x02'),'graph last-item cast sends original DELITEM bag and slot')
        check(read_packet(s,server_sequence+1)==(0x52a9,b'\x00'),'graph cast preserves private item then MOVEITEM ordering')
        op,data=read_packet(s,server_sequence+2)
        check(op==(0x5373 if loop else 0x52b5) and len(data)==(45 if loop else 62)+(5 if ammunition else 0) and data[0]==0,
              'graph inventory commit precedes original ordinary or loop success')
        current=conn.execute('SELECT server_id,authority_epoch FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()
        receipt=conn.execute('SELECT server_id,authority_epoch,state_contract,before_count,after_count,before_graph_hash<>after_graph_hash,consumption_kind FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=%s ORDER BY consumption_id DESC LIMIT 1',(cid,item_id)).fetchone()
        check(receipt==(*current,2,count,count-1,True,'ammunition' if ammunition else 'reagent'),'graph consumption receipt is durable and bound to the current primary epoch before client success')
        check(conn.execute('SELECT "bCount" FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(item_id,)).fetchone()==(3,),
              'graph consumption does not overwrite or reload stale normalized item count')
        check(conn.execute('SELECT recovery_contract,app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(2,True),
              'graph item mutation retains a complete valid recovery checkpoint')
    finally:
        if delay:
            conn.execute('DROP TRIGGER synthetic_graph_reagent_delay ON app_world.skill_item_consumptions')
            conn.execute('DROP FUNCTION public.delay_graph_reagent()')
