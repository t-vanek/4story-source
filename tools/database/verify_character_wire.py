"""Native character daemon checks with source-derived encrypted client frames."""
import json
import socket
import struct
import time
import urllib.request
from disposable_environment import command, private_file, LABEL
from verify_login_wire import frame, login_body, ack, read_packet, CREDENTIAL


def verify_character_daemon(conn, state, repo, private, public, app_conn, image,
                            binary, name, manifest, execute):
    checks = []
    def check(ok, label):
        if not ok: raise RuntimeError('Character wire verification failed: '+label)
        checks.append(label)
    config = ('[server]\nport=4816\ncontrol_server_ip="127.0.0.1"\n[database]\nbackend="postgresql"\n'
              'connection_string_env="FOURSTORY_TEST_PG_CONNINFO"\npool_size=4\nworker_threads=2\n'
              f'[characters]\nmanifest_sha256="{manifest}"\n'
              '[security]\ndb_trust_store=false\n[health]\nport=18916\n[metrics]\nport=0\n[shutdown]\ndrain_ms=50\n[log]\nlevel="info"\n')
    (public/'characters.toml').write_text(config)
    private_file(private/'server.env', 'FOURSTORY_TEST_PG_CONNINFO='+app_conn+'\nASAN_OPTIONS=detect_leaks=1:halt_on_error=1\nUBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1\n')
    base = ['podman','run','--rm','--network',state['network'],'-v',str(repo)+':/src:ro,z',
            '-v',str(public)+':/run/fourstory-test:ro,z','--env-file',str(private/'server.env'),image,binary]
    (public/'wrong-character-manifest.toml').write_text(config.replace(manifest,'0'*64))
    import subprocess
    bad = subprocess.run(base+['--config','/run/fourstory-test/wrong-character-manifest.toml'], text=True, capture_output=True, timeout=20)
    check(bad.returncode != 0 and 'Native character schema/catalog validation failed' in bad.stdout+bad.stderr,
          'actual daemon refuses wrong character release before accepting clients')
    command(['run','-d','--name',name,'--label',LABEL+'='+state['token'],'--network',state['network'],
             '-p','127.0.0.1::18916','-p','127.0.0.1::4816','-v',str(repo)+':/src:ro,z',
             '-v',str(public)+':/run/fourstory-test:ro,z','--env-file',str(private/'server.env'),image,binary,
             '--config','/run/fourstory-test/characters.toml'])
    def ready():
        port=command(['port',name,'18916/tcp']).rsplit(':',1)[1]
        deadline=time.monotonic()+30
        while True:
            try:
                with urllib.request.urlopen('http://127.0.0.1:'+port+'/healthz',timeout=1) as r:
                    if r.status==200:break
            except (OSError,TimeoutError):pass
            if not json.loads(command(['inspect',name]))[0]['State']['Running'] or time.monotonic()>deadline:
                raise RuntimeError('Character daemon failed readiness')
            time.sleep(.1)
        return int(command(['port',name,'4816/tcp']).rsplit(':',1)[1])
    def login(port):
        s=socket.create_connection(('127.0.0.1',port),timeout=8)
        s.sendall(frame(login_body(b'SyntheticChar304')))
        check(ack(s)[0]==0,'source encrypted login succeeds')
        return s
    port=ready()
    def exchange(s, body, message, sequence, expected):
        s.sendall(frame(body,message=message,sequence=sequence));opcode,payload=read_packet(s,sequence)
        check(opcode==expected,'reply opcode and ordered checksum match original source')
        return payload
    char_name=b'WireHero'
    with login(port) as s:
        check(exchange(s,b'\x01',0x198e,2,0x198f)==bytes(5),'new account has empty persisted character list')
        tail=bytes([0,0,0,4,0,0,0,0,0,0,0,0])
        payload=exchange(s,b'\x01'+struct.pack('<i',len(char_name))+char_name+tail,0x1990,3,0x1991)
        status,cid,length=struct.unpack_from('<Bii',payload)
        check(status==0 and cid>0 and length==len(char_name) and payload[9:9+length]==char_name and
              payload[9+length:]==tail[:11]+bytes([1,1]),'create ACK preserves name, 13 tail bytes, live count 1 and level 1')
        payload=exchange(s,b'\x01',0x198e,4,0x198f)
        check(payload[4]==1 and struct.unpack_from('<i',payload,5)[0]==cid,'list returns created durable identity')
        pos=9;n=struct.unpack_from('<i',payload,pos)[0];pos+=4+n
        check(payload[pos:pos+13]==bytes([1,0,1,0,0,4,0,0,0,0,0,0,0]),'list appearance and start action match source')
        pos+=13+12;helmet,count=payload[pos:pos+2];pos+=2
        check(helmet==0 and count>0 and len(payload)-pos==12*count,'equipment uses original twelve-byte records including custom texture')
        for i in range(count):check(struct.unpack_from('<H',payload,pos+i*12+7)[0]==0,'new source items have explicit empty custom texture')
        wrong=b'wrong';payload=exchange(s,b'\x01'+struct.pack('<i',len(wrong))+wrong+struct.pack('<i',cid),0x1992,5,0x1993)
        check(payload==struct.pack('<Bi',1,cid),'wrong delete password returns exact original DR_INVALIDPASSWD packet')
    # A real process restart, not just reconstructing a service object.
    command(['kill','--signal','TERM',name]);check(execute(['podman','wait',name],timeout=15).strip()=='0','daemon shuts down cleanly with durable character')
    command(['start',name]);port=ready()
    with login(port) as s:
        payload=exchange(s,b'\x01',0x198e,2,0x198f)
        check(payload[4]==1 and struct.unpack_from('<i',payload,5)[0]==cid,'reconnect after process restart reads the same character and items')
        payload=exchange(s,b'\x01'+struct.pack('<i',len(CREDENTIAL))+CREDENTIAL+struct.pack('<i',cid),0x1992,3,0x1993)
        check(payload==struct.pack('<Bi',0,cid),'correct source SHA1 delete credential commits original success packet')
        check(exchange(s,b'\x01',0x198e,4,0x198f)==bytes(5),'deleted character is absent from subsequent list')
    check(conn.execute('SELECT count(*) FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s',(cid,)).fetchone()[0]==0,
          'TCP delete removes persisted child inventory')
    command(['kill','--signal','TERM',name]);check(execute(['podman','wait',name],timeout=15).strip()=='0','final SIGTERM exits zero')
    logs=execute(['podman','logs',name])
    if any(x in logs for x in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:')):
        raise RuntimeError('Character daemon sanitizer failure')
    return dict(status='passed',checks=checks,scope='source-derived protocol peer; original client execution PENDING')
