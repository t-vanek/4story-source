"""Actual native Login START with backup routing and independently encoded frames."""
import json
import socket
import struct
import time
import urllib.request
from disposable_environment import command, private_file, LABEL
from verify_login_wire import frame, login_body, ack, read_packet


def verify_routing_daemon(conn,state,repo,private,public,app_conn,image,binary,name,manifest,routing,execute):
    checks=[]
    def check(ok,label):
        if not ok:raise RuntimeError('Routing wire verification failed: '+label)
        checks.append(label)
    config=('[server]\nport=4816\ncontrol_server_ip="127.0.0.1"\n[database]\nbackend="postgresql"\n'
            'connection_string_env="FOURSTORY_TEST_PG_CONNINFO"\npool_size=4\nworker_threads=2\n'
            f'[characters]\nmanifest_sha256="{manifest}"\n[routing]\nmanifest_sha256="{routing}"\n'
            '[security]\ndb_trust_store=false\n[health]\nport=18916\n[metrics]\nport=0\n[shutdown]\ndrain_ms=50\n[log]\nlevel="info"\n')
    (public/'routing.toml').write_text(config)
    private_file(private/'server.env','FOURSTORY_TEST_PG_CONNINFO='+app_conn+'\nASAN_OPTIONS=detect_leaks=1:halt_on_error=1\nUBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1\n')
    import subprocess
    (public/'wrong-routing.toml').write_text(config.replace(routing,'0'*64))
    bad=subprocess.run(['podman','run','--rm','--network',state['network'],'-v',str(repo)+':/src:ro,z',
                        '-v',str(public)+':/run/fourstory-test:ro,z','--env-file',str(private/'server.env'),image,binary,
                        '--config','/run/fourstory-test/wrong-routing.toml'],text=True,capture_output=True,timeout=20)
    check(bad.returncode!=0 and 'Native routing schema/catalog validation failed' in bad.stdout+bad.stderr,
          'actual daemon refuses wrong routing release before accepting clients')
    command(['run','-d','--name',name,'--label',LABEL+'='+state['token'],'--network',state['network'],
             '-p','127.0.0.1::18916','-p','127.0.0.1::4816','-v',str(repo)+':/src:ro,z',
             '-v',str(public)+':/run/fourstory-test:ro,z','--env-file',str(private/'server.env'),image,binary,
             '--config','/run/fourstory-test/routing.toml'])
    def ready():
        health=command(['port',name,'18916/tcp']).rsplit(':',1)[1];deadline=time.monotonic()+30
        while True:
            try:
                with urllib.request.urlopen('http://127.0.0.1:'+health+'/healthz',timeout=1) as r:
                    if r.status==200:break
            except (OSError,TimeoutError):pass
            if not json.loads(command(['inspect',name]))[0]['State']['Running'] or time.monotonic()>deadline:
                raise RuntimeError('Routing daemon failed readiness')
            time.sleep(.1)
        return int(command(['port',name,'4816/tcp']).rsplit(':',1)[1])
    def login(port,user):
        s=socket.create_connection(('127.0.0.1',port),timeout=8)
        s.sendall(frame(login_body(user)));result=ack(s)
        check(result[0]==0,'encrypted login succeeds')
        return s
    def exchange(s,body,message,seq,expected):
        s.sendall(frame(body,message=message,sequence=seq));op,data=read_packet(s,seq)
        check(op==expected,'source reply opcode, sequence and checksum match')
        return data
    port=ready()
    with login(port,b'SyntheticRoute505') as s:
        char_name=b'WireRouteHero';tail=bytes([0,0,0,4,0,0,0,0,0,0,0,0])
        data=exchange(s,b'\x01'+struct.pack('<i',len(char_name))+char_name+tail,0x1990,2,0x1991)
        status,cid=struct.unpack_from('<Bi',data);check(status==0 and cid>0,'native TCP character creation succeeds')
        bad=exchange(s,struct.pack('<BBi',9,2,cid),0x1994,3,0x1995)
        check(bad==b'\x02'+bytes(7),'missing group returns exact eight-byte SR_NOGROUP ACK')
        # A second packet is already buffered when success is queued. It must not
        # create another character after the terminal START response.
        s.sendall(frame(struct.pack('<BBi',1,2,cid),message=0x1994,sequence=4)+
                  frame(b'\x01'+struct.pack('<i',10)+b'IllegalNew'+tail,message=0x1990,sequence=5))
        op,data=read_packet(s,4)
        check(op==0x1995 and data==b'\x00\xc0\x00\x02\x15'+struct.pack('<HB',5816,2),
              'START ACK preserves IPv4 octets, little-endian port and owner server byte')
        check(s.recv(1)==b'','success ACK is fully flushed before original Login connection closes')
    row=conn.execute('SELECT h.char_id,h.channel,h.server_id,s."dwCharID",s."bGroupID",s."bChannel",l."timeLOGOUT"=l."timeLOGIN" '
                     'FROM app_global.map_handoff h JOIN app_global."TCURRENTUSER" s ON s."dwKEY"=h.session_key '
                     'JOIN app_global."TLOG" l ON l."dwKEY"=h.session_key WHERE h.user_id=505').fetchone()
    check(row==(cid,2,2,cid,1,2,True),'actual disconnect preserves committed pending handoff without logout')
    check(conn.execute('SELECT count(*) FROM app_world."TCHARTABLE" WHERE "dwUserID"=505').fetchone()[0]==1,
          'queued post-START create was suppressed')
    command(['kill','--signal','TERM',name]);check(execute(['podman','wait',name],timeout=15).strip()=='0','first SIGTERM exits zero')
    command(['start',name]);port=ready()
    check(conn.execute('SELECT count(*) FROM app_global.map_handoff WHERE user_id=505').fetchone()[0]==1,
          'real Login process restart preserves unexpired Map handoff')
    conn.execute("UPDATE app_global.map_handoff SET expires_at=clock_timestamp()-interval '1 second' WHERE user_id=505")
    with login(port,b'SyntheticRoute505') as s:
        data=exchange(s,b'\x01',0x198e,2,0x198f)
        check(data[4]==1 and struct.unpack_from('<i',data,5)[0]==cid,'expired unclaimed handoff reauthenticates with same persisted character')
    for suffix in (b'',b'\x00\x00\x00\x00\x00',b'\x00'*7):
        with login(port,b'SyntheticRoute506') as s:
            s.sendall(frame(suffix,message=0x1994,sequence=2));check(s.recv(1)==b'','non-six-byte START closes without fabricated request fields')
        deadline=time.monotonic()+3
        while conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=506').fetchone()[0]:
            if time.monotonic()>deadline:raise RuntimeError('Login close cleanup timeout')
            time.sleep(.03)
    command(['kill','--signal','TERM',name]);check(execute(['podman','wait',name],timeout=15).strip()=='0','final SIGTERM exits zero')
    log=execute(['podman','logs',name])
    check(not any(x in log for x in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:')),'daemon log contains no sanitizer diagnostics')
    return dict(status='passed',checks=checks,scope='original-source protocol peer; native Map acceptance and original client PENDING')
