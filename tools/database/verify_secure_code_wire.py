"""Independent original SecuritySystem wire oracle (CSHandler/CSSender and client).
All credential bytes below are synthetic test data. No captured client is used.
"""
import struct
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent


def verify_secure_code(conn,cid,start,enter,login_port,map_port,until,restart):
    checks=[];client=None;cs,ss=3,4;synthetic_item=None
    uid=conn.execute('SELECT "dwUserID" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()[0]
    def check(ok,label):
        if not ok:raise RuntimeError('Secure wire: '+label)
        checks.append(label)
    def string(value):return struct.pack('<i',len(value))+value
    def connect(expected):
        nonlocal client,cs,ss
        _,key=start(login_port,cid);client,char=enter(map_port,cid,key);cs,ss=3,4
        check(char['security']==expected,'CHARINFO original created/unlocked/disabled bytes match persisted account and new Login')
        return char
    def disconnect():
        nonlocal client
        client.close();client=None
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'secure disconnect saves checkpoint')
    def send(op,body=b'',expected=None):
        nonlocal cs,ss
        client.sendall(frame(body,op,cs));cs+=1
        if expected:
            actual=read_packet(client,ss);ss+=1
            check(actual==expected,'original secure opcode 0x%04x and exact response bytes'%op)
    def count():return conn.execute('SELECT count(*) FROM app_world.secure_code_operations WHERE user_id=%s',(uid,)).fetchone()[0]
    try:
        check(conn.execute('SELECT count(*) FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=255 AND "bItemID" IN (2,3)',(cid,)).fetchone()==(0,),'secure move fixture slots are initially empty')
        synthetic_item=seed_graph_reagent(conn,cid,True)
        char=connect((0,0,0))
        descriptor=next(item for bag,item,magic in char['items'] if bag==255 and item[0]==2)
        def move(source,destination):
            nonlocal ss
            send(0x52a8,bytes([255,source,255,destination,3]))
            moved=list(descriptor);moved[0]=destination
            check(read_packet(client,ss)==(0x52ac,bytes([255,source])),'unlocked secure move emits original DELITEM');ss+=1
            check(read_packet(client,ss)==(0x52ab,b'\xff'+struct.pack('<BHBBHHBIIBBBqBBBHHBB',*moved)),'unlocked secure move emits exact original ADDITEM descriptor');ss+=1
            check(read_packet(client,ss)==(0x52a9,b'\x00'),'unlocked secure move emits success after item packets');ss+=1
            check(conn.execute('SELECT "dwStorageID","bItemID","bCount" FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(synthetic_item,)).fetchone()==(255,destination,3),'secured native inventory move commits exact identity and whole stack')
        send(0x5590,string(b'Absent000')) # source silently ignores missing object
        send(0x5580,string(b'Synthetic0'),(0x5581,b'\x02'))
        send(0x5580,string(b'Different0'),(0x5581,b'\x01'))
        send(0x5588,string(b'Synthetic0'),(0x5589,b'\x07\x01')) # already unlocked!
        send(0x5584,string(b'Synthetic0'),(0x5585,b'\x06\x00'))
        disconnect();connect((1,0,1))
        send(0x5586,b'',(0x5587,b'\x04'))
        # Source guards before decoding even these empty mutation bodies.
        send(0x52b2,b'',(0x5589,b'\x00\x00'))
        send(0x52a8,b'',(0x5589,b'\x00\x00')) # MOVEITEM_REQ (source CSProtocol)
        check(read_packet(client,ss)==(0x52a9,b'\x04'),'blocked MOVEITEM sends source MI_SAMEPOS after secure unlock prompt');ss+=1
        send(0x5590,string(b'Synthetic0'),(0x5591,b'\x09\x00'))
        send(0x5588,string(b'Synthetic0'),(0x5589,b'\x08\x00'))
        move(2,3);move(3,2)
        send(0x5582,string(b'synthetic0')+string(b'NextCode00'),(0x5583,b'\x07\x01'))
        send(0x5582,string(b'Synthetic0')+string(b'NextCode00'),(0x5583,b'\x05\x00'))
        disconnect();connect((1,0,0))
        for n in range(1,10):send(0x5590,string(b'Incorrect0'),(0x5591,bytes([7,n])))
        send(0x5588,string(b'Incorrect0'),(0x5591,b'\x0a\x0a'))
        send(0x5584,string(b'NextCode00'),(0x5591,b'\x0a\x0a'))
        send(0x52a8,b'',(0x5591,b'\x0a\x0a'))
        check(read_packet(client,ss)==(0x52a9,b'\x04'),'locked-out MOVEITEM still sends MI_SAMEPOS after redirected CHECK');ss+=1
        before=count();deadline=conn.execute('SELECT deadline_ms FROM app_global.secure_codes WHERE user_id=%s',(uid,)).fetchone()[0]
        send(0x5592)
        op,body=read_packet(client,ss);ss+=1
        check(op==0x5593 and body==b'\x17\x3b','24-hour lock returns original BYTE 23 hours / 59 minutes after elapsed subsecond')
        map_port=restart();client.close();client=None;connect((1,0,0))
        check(count()==before,'SIGKILL recovery never replays security operation')
        check(conn.execute('SELECT tries,deadline_ms FROM app_global.secure_codes WHERE user_id=%s',(uid,)).fetchone()==(10,deadline),'tries and absolute deadline survive replacement Map')
        send(0x5588,string(b'NextCode00'),(0x5591,b'\x0a\x0a'))
        disconnect()
        # Explicit fixture time advance, while no live writer owns the character.
        conn.execute('UPDATE app_global.secure_codes SET deadline_ms=floor(extract(epoch FROM clock_timestamp())*1000)::bigint-1000 WHERE user_id=%s',(uid,))
        connect((1,0,0));send(0x5592,b'',(0x5589,b'\x00\x00'))
        send(0x5588,string(b'NextCode00'),(0x5589,b'\x08\x00'))
        # Enabled and unlocked must reach the existing native repositories.
        send(0x52a8,bytes([253,0,253,1,1]),(0x52a9,b'\x02'))
        send(0x52b2,struct.pack('<HH',65535,131))
        op,body=read_packet(client,ss);ss+=1
        check(op==0x52b3 and len(body)==30 and body[:4]==b'\x01\x83\x00\x00','unlocked native trainer path returns original missing-NPC result instead of security gate')
        before=count()
        conn.execute("CREATE FUNCTION public.synthetic_secure_fault() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic secure checkpoint fault'; END $$")
        conn.execute('CREATE TRIGGER synthetic_secure_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.synthetic_secure_fault()')
        send(0x5582,string(b'NextCode00')+string(b'Rollback00'))
        check(client.recv(1)==b'','late secure SQL error closes TCP without successful response')
        conn.execute('DROP TRIGGER synthetic_secure_fault ON app_world.map_checkpoints');conn.execute('DROP FUNCTION public.synthetic_secure_fault()')
        check(count()==before,'late SQL rollback leaves append history unchanged')
        check(bytes(conn.execute('SELECT code FROM app_global.secure_codes WHERE user_id=%s',(uid,)).fetchone()[0])==b'NextCode00','late SQL rollback retains previous credential')
        map_port=restart();client.close();client=None;connect((1,0,0))
        send(0x5582,string(b'NextCode00')+string(b'Rollback00'),(0x5583,b'\x05\x00'))
        send(0x5584,string(b'Rollback00'),(0x5585,b'\x06\x00'))
        disconnect();connect((1,0,1));disconnect()
    finally:
        if client:client.close()
        conn.execute('DROP TRIGGER IF EXISTS synthetic_secure_fault ON app_world.map_checkpoints');conn.execute('DROP FUNCTION IF EXISTS public.synthetic_secure_fault()')
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'secure fixture release')
        # Disposable fixture cleanup only. Runtime role cannot delete this state.
        conn.execute('DELETE FROM app_global.secure_codes WHERE user_id=%s',(uid,))
        if synthetic_item:conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dlID"=%s',(synthetic_item,))
    return {'passed':len(checks),'tests':checks,'client_acceptance':'BLOCKED: original EXE/assets unavailable'},map_port
