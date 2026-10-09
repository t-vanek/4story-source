"""Independent SS peer fixture against an actual World daemon.

These are synthetic Map registrations and an in-memory character. Native Map
replica authorization/persistence and primary ownership transfer are not implied.
Opcodes: ProtocolBase.h (MW=0x9001, RW=0x9999), MWProtocol.h/CTProtocol.h.
"""
import socket
import struct


def verify_world_secondary(port):
    checks=[];sockets=[]
    cid=0x7fff0001;key=0x23456789;uid=0x7fff0011
    def check(ok,label):
        if not ok:raise RuntimeError('World secondary wire: '+label)
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
            if not part:raise RuntimeError('World secondary wire: unexpected EOF')
            out+=part
        return out
    def send(s,opcode,body):s.sendall(struct.pack('<HHI',8+len(body),opcode,checksum(body))+body)
    def expect(s,opcode,body,label):
        while True:
            length,actual,crc=struct.unpack('<HHI',exact(s,8))
            check(length>=8,'valid SS frame length')
            payload=exact(s,length-8);check(checksum(payload)==crc,'independent SS checksum matches')
            # Only the defined registration broadcast may be interleaved.
            if actual==0x911e:
                check(payload==b'\0'*5,'exact registration broadcast');continue
            matches = body(payload) if callable(body) else payload == body
            check(actual==opcode and matches,
                  f'{label} (expected opcode {opcode:#06x}, received {actual:#06x}, {len(payload)} body bytes)');return
    def identity(k=key):return struct.pack('<II',cid,k)
    def add(sid,k=key,user=uid,ip=0x0100007f,port_override=None):
        return identity(k)+struct.pack('<IHI',ip,port_override or 4000+sid,user)
    try:
        for sid in (201,202,203):
            s=socket.create_connection(('127.0.0.1',port),timeout=5);sockets.append(s)
            send(s,0x999a,struct.pack('<H',0x0400|sid))
            expect(s,0x999b,b'\0'*5,'actual World registers synthetic Map')
            # The actual native daemon boots empty social registries and replays
            # both packets after registration. MONTHRANKER has 51 bytes when its
            # three DWORD-length strings are empty: 3 countries x 33 slots.
            # Rank month comes from the World process local clock.
            expect(s,0x9140,lambda p: len(p)==5051 and 1<=p[0]<=12 and
                   p[1:]==b'\x21'+b'\0'*5049,'exact empty monthly-rank registration replay')
            expect(s,0x915b,b'\0'*4,'exact empty tournament registration replay')
        main,second,third=sockets
        send(main,0x9003,add(201))
        expect(main,0x9008,b'\x01'+identity(),'fresh synthetic main receives DB-load request')
        send(second,0x9003,add(202))
        expect(second,0x900a,identity()+b'\0','unplanned secondary is rejected')
        routes=identity()+b'\x02'+b''.join(struct.pack('<IHB',0x0100007f,4000+sid,sid) for sid in (202,203))
        send(main,0x9011,routes)
        expect(main,0x9012,routes,'original valid main survives rejection and plans secondary endpoints')
        for request,bad_key,label in [
            (add(202,k=key+1),key+1,'wrong key rejected'),
            (add(202,user=uid+1),key,'wrong account rejected'),
            (add(202,ip=0x0200007f),key,'wrong endpoint address rejected'),
            (add(202,port_override=4999),key,'wrong endpoint port rejected')]:
            send(second,0x9003,request)
            expect(second,0x900a,identity(bad_key)+b'\0',label)
        send(second,0x9003,add(202))
        send(second,0x9003,add(202))
        expect(second,0x900a,identity()+b'\0','planned secondary accepted once; duplicate rejected')
        send(third,0x9003,add(203))
        expect(main,0x9004,identity(),'all planned connections trigger CHARDATA on original primary')
        send(third,0x9003,add(203))
        expect(third,0x900a,identity()+b'\0','duplicate completion rejected without replacing connection')
        return {'status':'passed','checks':checks,'scope':'Actual World daemon with three independently encoded synthetic Map peers; secondary registration only, native Map replica persistence/primary transfer remain pending'}
    finally:
        for s in reversed(sockets):s.close()
