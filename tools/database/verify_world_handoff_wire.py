"""Independent SS bytes exercise the actual World's main handoff coordinator.

Map peers and released state are synthetic. World forwards the opaque state;
this is not a native Map transfer codec or PostgreSQL ownership-transfer test.
"""
import socket
import struct


def verify_world_handoff(port):
    checks=[];sockets=[]
    cid=0x7ffe0101;key=0x1234aabb;uid=0x7ffe0201
    ids=(211,212,213)
    def check(ok,label):
        if not ok:raise RuntimeError('World handoff wire: '+label)
        checks.append(label)
    def checksum(body):
        result=0;end=len(body)//4*4
        for i in range(0,end,4):result^=struct.unpack_from('<I',body,i)[0]
        for byte in body[end:]:result^=byte
        return result
    def exact(s,n):
        out=b''
        while len(out)<n:
            part=s.recv(n-len(out))
            if not part:raise RuntimeError('World handoff wire: unexpected EOF')
            out+=part
        return out
    def send(s,op,body):s.sendall(struct.pack('<HHI',len(body)+8,op,checksum(body))+body)
    def expect(s,op,body,label):
        while True:
            length,actual,crc=struct.unpack('<HHI',exact(s,8));check(length>=8,'valid independent SS frame length')
            payload=exact(s,length-8);check(checksum(payload)==crc,'independent SS checksum matches')
            if actual==0x911e:
                check(payload==b'\0'*5,'exact registration broadcast');continue
            check(actual==op and (body(payload) if callable(body) else payload==body),
                  f'{label} (expected {op:#06x}, got {actual:#06x}, {len(payload)} bytes)')
            return payload
    def identity():return struct.pack('<II',cid,key)
    def position():return identity()+struct.pack('<BHfff',1,7,80,90,100)
    def string(value):return struct.pack('<i',len(value))+value
    def add(sid):return identity()+struct.pack('<IHI',0x0100007f,4000+sid,uid)
    def enter(result=0):
        return identity()+string(b'TransferHero')+bytes([10,0,1,0,0,0,0,0,0,3])+struct.pack('<IBHfffBBB HII',0,1,7,80,90,100,0,0,result,0,0,0)
    def release():return b'\0'+identity()+b'\0'+string(b'TransferHero')+bytes((n*71)&255 for n in range(32000))
    def barrier(s,sid):
        send(s,0x9003,add(sid));expect(s,0x900a,identity()+b'\0','ordered duplicate registration barrier')
    def populate(regression=False):
        main,second,third=sockets
        send(main,0x9003,add(ids[0]));expect(main,0x9008,b'\x01'+identity(),'fresh primary requests load')
        routes=identity()+b'\x02'+b''.join(struct.pack('<IHB',0x0100007f,4000+sid,sid) for sid in ids[1:])
        if regression:
            send(third,0x90c6,identity())
            barrier(third,ids[2])
            send(main,0x9011,routes)
            expect(main,0x9012,routes,'unaccepted CHECKMAIN cannot replace primary or emit RELEASEMAIN')
        send(main,0x9009,enter());expect(main,0x900f,lambda b:b.startswith(identity()),'primary metadata response')
        expect(main,0x9010,position(),'primary route decision request')
        expect(main,0x9057,lambda b:b.startswith(identity()),'fresh social-list response')
        if not regression:
            send(main,0x9011,routes);expect(main,0x9012,routes,'primary plans exact secondary endpoints')
        send(second,0x9003,add(ids[1]));send(third,0x9003,add(ids[2]))
        expect(main,0x9004,identity(),'all accepted connections trigger primary summary')
        send(main,0x9005,identity()+struct.pack('<BBIIIIBBBi',0,10,100,90,80,70,0,0,0,0))
        for s in sockets:
            expect(s,0x9006,lambda b:b.startswith(identity()),'secondary composite delivered')
            send(s,0x9007,identity())
        for s in sockets:expect(s,0x90c5,position(),'only hydrated connections receive CHECKMAIN')
        send(main,0x90c6,identity());expect(main,0x900b,identity()+bytes([0,3,*ids]),'initial primary confirms once')
    def round_(main):
        send(main,0x90ca,position()+b'\0')
        for s in sockets:expect(s,0x90c5,position(),'original movement connection check requests a round')
    def retire(old):
        expect(old,0x900a,identity()+b'\x01','in-flight source invalidated during failed transfer')
        for s in sockets:expect(s,0x900e,identity()+b'\0\0','failed transfer closes each connection')
    try:
        for sid in ids:
            s=socket.create_connection(('127.0.0.1',port),timeout=8);sockets.append(s)
            send(s,0x999a,struct.pack('<H',0x0400|sid));expect(s,0x999b,b'\0'*5,'synthetic typed Map registered')
            expect(s,0x9140,lambda b:len(b)==5051 and 1<=b[0]<=12 and b[1:]==b'\x21'+b'\0'*5049,'exact empty rank replay')
            expect(s,0x915b,b'\0'*4,'exact empty tournament replay')
        main,second,third=sockets
        populate(regression=True)
        send(second,0x9009,enter());barrier(second,ids[1])
        round_(main)
        send(third,0x90c6,identity()+b'\0');barrier(third,ids[2])
        send(second,0x90c6,identity());expect(main,0x9017,position(),'expected target initiates release from original primary')
        send(third,0x90c6,identity());barrier(third,ids[2])
        send(second,0x9009,enter());barrier(second,ids[1])
        send(third,0x9018,release());barrier(third,ids[2])
        send(main,0x9018,b'\x02'+release()[1:]);barrier(main,ids[0])
        send(main,0x9018,release());expect(second,0x9008,release(),'only expected source forwards the exact 32KB opaque state once')
        send(main,0x9018,release());barrier(main,ids[0])
        send(third,0x9009,enter());barrier(third,ids[2])
        send(second,0x9009,enter()+b'\0');barrier(second,ids[1])
        send(second,0x9009,enter());expect(second,0x9013,position(),'only exact target load advances to server-list confirmation')
        send(second,0x9009,enter());barrier(second,ids[1])
        send(second,0x9014,identity()+bytes([2,ids[0],ids[2]]))
        for s in sockets:expect(s,0x90c5,position(),'post-transfer connection set receives confirmation request')
        send(main,0x90c6,identity());barrier(main,ids[0])
        send(second,0x90c6,identity());expect(second,0x900b,identity()+bytes([0,3,*ids]),'new primary completes exactly one source connection verdict')
        send(main,0x9018,release());barrier(main,ids[0])
        round_(second);send(third,0x90c6,identity());expect(second,0x9017,position(),'next handoff uses the new primary as source')
        send(second,0x9018,release());expect(third,0x9008,release(),'stalled target receives source state')
        retire(second)
        check(True,'five-second target-load deadline retires incomplete transfer')
        cid+=1;uid+=1;populate()
        round_(main);send(second,0x90c6,identity());expect(main,0x9017,position(),'failure fixture release requested')
        send(main,0x9018,release());expect(second,0x9008,release(),'failure fixture target receives source state')
        send(second,0x9009,enter(5));expect(second,0x900b,identity()+b'\x05\0','target failure returns original failure verdict')
        retire(main)
        cid+=1;uid+=1;populate()
        # Reconcile drops the source from cons into dead_cons before takeover.
        # Losing it must still retire the pending handoff immediately.
        send(second,0x90c1,identity()+bytes([1,ids[2]]))
        for s in (second,third):expect(s,0x90c5,position(),'new cell excludes previous primary from live connection set')
        send(second,0x90c6,identity());expect(main,0x9017,position(),'removed source remains pinned during release')
        main.close()
        for s in (second,third):expect(s,0x900e,identity()+b'\0\0','source TCP loss retires handoff even after source left live connection set')
        return {'status':'passed','checks':checks,'scope':'Actual World daemon with independent synthetic Map peers; expected source/target/phase, exact opaque forwarding, duplicate and malformed replies, target failure/deadline and dropped-source disconnect. Native Map graph codec and PostgreSQL ownership transfer remain pending.'}
    finally:
        for s in reversed(sockets):s.close()
