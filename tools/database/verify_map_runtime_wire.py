"""Real Login + World + two native Maps, lifecycle/checkpoint recovery over encrypted TCP.

World runs its in-memory cluster registry without a persistence backend in this
isolated test; social/gameplay parity and original-client execution stay pending.
"""
import json
import socket
import struct
import time
import urllib.request
from disposable_environment import command,private_file,LABEL,verify_container
from verify_login_wire import frame,login_body,ack,read_packet,SECRET
from verify_world_secondary_wire import verify_world_secondary
from verify_world_handoff_wire import verify_world_handoff
from verify_map_rejection_wire import verify_map_rejection
from verify_map_replica_wire import verify_map_replica
from verify_map_skill_wire import seed_skill_cast,verify_skill_cast


def verify_map_runtime_daemons(conn,state,repo,private,public,login_conn,map_conn,image,bin_dir,base_name,manifest,routing,actor,execute):
    checks=[];names=[base_name+'-world',base_name+'-map',base_name+'-login',base_name+'-map-second'];started=[]
    def check(ok,label):
        if not ok:raise RuntimeError('Map runtime wire: '+label)
        checks.append(label)
    common='[health]\nport=18916\n[metrics]\nport=0\n[shutdown]\ndrain_ms=25\n[log]\nlevel="info"\n'
    configs={
        names[0]:'[server]\nport=3815\ngroup_id=1\n'+common,
        names[1]:('[server]\nport=5815\n[crypto]\nrc4_secret_hex="'+SECRET.hex()+'"\n[database]\nbackend="postgresql"\n'
                  'connection_string_env="FOURSTORY_RUNTIME_DB"\npool_size=4\nworker_threads=2\n'
                  '[cluster]\ngroup_id=1\nserver_id=1\n'
                  f'[world]\nhost="{names[0]}"\nport=3815\n[native_map]\ncharacter_manifest_sha256="{manifest}"\n'
                  f'routing_manifest_sha256="{routing}"\nactor_manifest_sha256="{actor}"\ncheckpoint_interval_ms=500\n'+common),
        names[2]:('[server]\nport=4816\ncontrol_server_ip="127.0.0.1"\n[database]\nbackend="postgresql"\n'
                  'connection_string_env="FOURSTORY_RUNTIME_DB"\npool_size=4\nworker_threads=2\n'
                  f'[characters]\nmanifest_sha256="{manifest}"\n[routing]\nmanifest_sha256="{routing}"\n'
                  '[security]\ndb_trust_store=false\n'+common)}
    configs[names[3]]=configs[names[1]].replace('server_id=1', 'server_id=2')
    def launch(index):
        name=names[index];config_file=public/(f'runtime-{index}.toml');config_file.write_text(configs[name])
        env=private/f'runtime-{index}.env'
        private_file(env,'FOURSTORY_RUNTIME_DB='+(map_conn if index in (1,3) else login_conn)+'\nASAN_OPTIONS=detect_leaks=1:halt_on_error=1\nUBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1\n')
        binary=('tworldsvr_asio','tmapsvr_asio','tloginsvr_asio','tmapsvr_asio')[index];port=(3815,5815,4816,5815)[index]
        command(['run','-d','--name',name,'--label',LABEL+'='+state['token'],'--network',state['network'],
                 '-p','127.0.0.1::18916','-p',f'127.0.0.1::{port}','-v',str(repo)+':/src:ro,z',
                 '-v',str(public)+':/run/fourstory-test:ro,z','--env-file',str(env),image,bin_dir+binary,
                 '--config','/run/fourstory-test/'+config_file.name]);started.append(name)
        health=command(['port',name,'18916/tcp']).rsplit(':',1)[1];deadline=time.monotonic()+20
        while True:
            try:
                with urllib.request.urlopen('http://127.0.0.1:'+health+'/healthz',timeout=.5) as response:
                    if response.status==200:break
            except (OSError,TimeoutError):pass
            if not json.loads(command(['inspect',name]))[0]['State']['Running'] or time.monotonic()>deadline:
                raise RuntimeError('Runtime startup: '+execute(['podman','logs',name])[-2500:])
            time.sleep(.1)
        return int(command(['port',name,f'{port}/tcp']).rsplit(':',1)[1])
    def until(predicate,label):
        deadline=time.monotonic()+8
        while not predicate():
            if time.monotonic()>deadline:raise RuntimeError('Map runtime wait: '+label)
            time.sleep(.025)
        check(True,label)
    def connect_request(user,cid,key):
        initial=(0x2918*user+key*cid)&0xffffffff;value=initial
        for _ in range(initial%8):value=((value^(initial//8))+0x336c3aebf71a8b08)&((1<<64)-1)
        return struct.pack('<HBIII4sHQ',0x2918,1,user,cid,key,b'\x7f\x00\x00\x01',5815,value)
    login_attempts=0;first_login_at=None
    def start(login_port,cid=None):
        nonlocal login_attempts,first_login_at
        # Respect the production per-IP burst of five and one-token/10s refill.
        # The longer fault matrix must not disable security or retry failures as
        # success. Pace before requesting authentication, keeping every ACK strict.
        if first_login_at is None:first_login_at=time.monotonic()
        if login_attempts>=5:
            time.sleep(max(0,first_login_at+(login_attempts-4)*10.1-time.monotonic()))
        login_attempts+=1
        with socket.create_connection(('127.0.0.1',login_port),timeout=8) as s:
            s.sendall(frame(login_body(b'SyntheticMap706')));result=ack(s);check(result[0]==0,'real Login authenticates test account')
            # Decode the key by original LOGIN_ACK field order; DB independently
            # verifies selection rather than supplying the client its key.
            key=result[3];seq=2
            if cid is None:
                name=b'WireMapHero';body=b'\x01'+struct.pack('<i',len(name))+name+bytes([0,0,0,4,0,0,0,0,0,0,0,0])
                s.sendall(frame(body,0x1990,seq));op,data=read_packet(s,seq);seq+=1
                status,cid=struct.unpack_from('<Bi',data);check(op==0x1991 and status==0,'real Login creates character from backup charts')
            s.sendall(frame(struct.pack('<BBi',1,1,cid),0x1994,seq));op,data=read_packet(s,seq)
            check(op==0x1995 and data==b'\x00\x7f\x00\x00\x01'+struct.pack('<HB',5815,1),'real START selects native Map endpoint')
            check(s.recv(1)==b'','Login closes after committed START')
        return cid,key
    def enter(port,cid,key,ready_delay=0):
        started_at=time.monotonic()
        s=socket.create_connection(('127.0.0.1',port),timeout=8);s.sendall(frame(connect_request(706,cid,key),0x5281,1))
        op,data=read_packet(s,1);check(op==0x53be and data==bytes([0,1]),'World CHARINFO produces original channel notification')
        op,data=read_packet(s,2);check(op==0x5285,'source CHARINFO precedes CONNECT and client map activation')
        character=parse_character(data)
        character['admission_ms']=int((time.monotonic()-started_at)*1000)+20
        op,data=read_packet(s,3);check(op==0x5282 and data==bytes([0,1,1]),'actual World route/CHARDATA/ENTERCHAR/CHECKMAIN completes CONNECT')
        if ready_delay:time.sleep(ready_delay)
        s.sendall(frame(b'',0x5288,2))
        until(lambda:conn.execute('SELECT phase FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==('ready',),'CONREADY commits readiness after client character hydration')
        return s,character
    def countdown(character,skill,loaded,label):
        check(any(i==(skill&65535) and max(0,loaded-character['admission_ms'])<=t<=loaded
                  for i,l,t in character['skills']),label)
    def parse_character(data):
        offset=0
        def read(fmt):
            nonlocal offset
            val=struct.unpack_from('<'+fmt,data,offset);offset+=struct.calcsize('<'+fmt);return val[0] if len(val)==1 else val
        def string():
            nonlocal offset
            n=read('i');assert 0<=n<=len(data)-offset
            val=data[offset:offset+n];offset+=n;return val
        out={'id':read('I')};read('BBB');out['title']=read('H');out['name']=string();out['appearance']=read('14B')
        read('HIIIBB');string();read('I');string()
        values=read('11I');out['money']=values[:3];out['exp']=values[3:6];out['hpmp']=values[6:10]
        read('H');read('I');out['map']=read('H');out['position']=read('fff');read('HHBI');out['points']=read('4H');read('IB')
        out['bags']=[];out['items']=[]
        for _ in range(read('B')):
            bag=read('BHq');out['bags'].append(bag)
            for _ in range(read('B')):
                item=read('BHBBHHBIIBBBqBBBHHBB');options=[read('BH') for _ in range(item[-1])]
                out['items'].append((bag[0],item,options))
        out['skills']=[read('HBI') for _ in range(read('B'))]
        check(read('B')==0,'fresh character has no fabricated maintained effects')
        out['hotkeys']=[]
        for _ in range(read('B')):out['hotkeys'].append((read('B'),[read('BH') for _ in range(12)]))
        check(read('B')==0,'fresh character has no fabricated item cooldowns')
        read('III');string();read('I');check(offset==len(data),'CHARINFO parsed to exact end with original field widths')
        return out
    try:
        map_port=launch(1)
        def gate_closed(label):
            with socket.create_connection(('127.0.0.1',map_port),timeout=3) as rejected:
                try:
                    rejected.sendall(frame(connect_request(706,1,1),0x5281,1))
                    closed=rejected.recv(1)==b''
                except (ConnectionResetError,BrokenPipeError):closed=True
                check(closed,label)
        def registrations():return execute(['podman','logs',names[1]]).count('registration acknowledged')
        gate_closed('native admission is closed before initial World registration')
        world_port=launch(0);login_port=launch(2)
        until(lambda:registrations()>0,'Map receives actual World registration acknowledgment')
        secondary_wire=verify_world_secondary(world_port)
        handoff_wire=verify_world_handoff(world_port)
        cid,key=start(login_port)
        skill_fixture=seed_skill_cast(conn,cid)
        s,first=enter(map_port,cid,key)
        second_map_port=launch(3)
        until(lambda:'registration acknowledged' in execute(['podman','logs',names[3]]), 'second native Map registers with the actual World')
        rejected_secondary=verify_map_rejection(conn,world_port,second_map_port,706,cid,key,connect_request)
        check(first['id']==cid and first['name']==b'WireMapHero' and first['map']==0,'native character identity and fallback location match')
        rows=conn.execute('SELECT "bInvenID","wItemID" FROM app_world."TINVENTABLE" WHERE "dwCharID"=%s ORDER BY "bInvenID"',(cid,)).fetchall()
        check([(b[0],b[1]) for b in first['bags']]==[(b,i&65535) for b,i in rows],'every original starter bag is present on wire')
        item_count=conn.execute('SELECT count(*) FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s',(cid,)).fetchone()[0]
        check(len(first['items'])==item_count and item_count>0,'every actual starter item is present on wire')
        skills=conn.execute('SELECT "wSkillID","bLevel","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s ORDER BY ("wSkillID"::integer & 65535)',(cid,)).fetchall()
        check(first['skills']==[(i&65535,l,t&0xffffffff) for i,l,t in skills],'every learned skill and cooldown matches PostgreSQL')
        check(first['appearance'][4]==3 and first['exp']==(0,30,1),'original neutral aid country and level thresholds match')
        next_sequence,skill_wire=verify_skill_cast(conn,s,cid,first,skill_fixture,until)
        before=conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
        remaining_items=[(bag,item,options) for bag,item,options in first['items'] if not (bag==255 and item[0]==1)]
        x,y,z=first['position'];destination=(x+3,y,z+2)
        s.sendall(frame(struct.pack('<HfffHHBBBBf',0,*destination,0,90,0,0,0,0,1.0),0x5289,next_sequence));time.sleep(.1);s.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,'disconnect saves and releases native claim')
        row=conn.execute('SELECT "fPosX","fPosY","fPosZ" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        check(struct.pack('<fff',*row)==struct.pack('<fff',*destination),'actual movement persists exact float32 bits on disconnect')
        check(before==conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall(),'core save preserves all item fields exactly')
        cooldown_skill=skills[0][0]
        conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=300000 WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,cooldown_skill))
        short_skill=next(i for i,l,t in skills if i!=cooldown_skill)
        conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=150 WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,short_skill))
        cid,newkey=start(login_port,cid);check(newkey!=key,'new Login issues a fresh key')
        s,second=enter(map_port,cid,newkey,ready_delay=.35);check(second['position']==destination and second['items']==remaining_items,'reconnect through same World restores movement and exact inventory after reagent consumption')
        countdown(second,cooldown_skill,300000,
              'synthetic persisted skill cooldown reaches original CHARINFO layout')
        countdown(second,short_skill,150,'short admission cooldown reaches CHARINFO with its current remaining duration')
        ready_skills=dict(conn.execute('SELECT "wSkillID","dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s',(cid,)).fetchall())
        check(0<ready_skills[cooldown_skill]<=299650 and ready_skills[short_skill]==0,
              'delayed CONREADY persists elapsed long cooldown and expired short cooldown before gameplay')
        request=struct.pack('<IBBHHBIIfffB',cid,1,1,0,cooldown_skill&65535,0,0,0,*destination,0)
        for sequence in (3,4):
            s.sendall(frame(request,0x52b4,sequence));op,data=read_packet(s,sequence+1)
            check(op==0x52b5 and data[0]==6 and struct.unpack_from('<I',data,1)[0]==cid,
                  'native daemon rejects restored skill cooldown with source SKILL_SPEEDYUSE'+str(sequence))
        def stored_cooldown():
            return conn.execute('SELECT "dwRemainTick" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,cooldown_skill)).fetchone()[0]&0xffffffff
        until(lambda:0<stored_cooldown()<300000,'periodic fresh-primary checkpoint advances live cooldown without transfer')
        s.close();until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,'second disconnect completes save')
        saved_cooldown=stored_cooldown()
        check(0<saved_cooldown<300000 and conn.execute('SELECT recovery_contract,transfer_body IS NULL,app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(3,True,True),
              'fresh logout commits exact timer receipt without replacing ordinary character hydration')
        cid,cooldown_key=start(login_port,cid);s,resumed=enter(map_port,cid,cooldown_key)
        countdown(resumed,cooldown_skill,saved_cooldown,
              'fresh reconnect restores saved remaining duration without offline decay or lost inventory')
        check(resumed['items']==remaining_items,'fresh cooldown reconnect preserves every inventory descriptor')
        s.sendall(frame(request,0x52b4,3));op,data=read_packet(s,4)
        check(op==0x52b5 and data[0]==6,'fresh reconnected native daemon still enforces the saved cooldown')
        s.close();until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,'fresh cooldown relogin disconnect completes save')
        conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=0 WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,cooldown_skill))
        replica_wire=verify_map_replica(conn,map_port,second_map_port,login_port,cid,start,connect_request,parse_character)
        # World dies while Map and its dirty player stay alive. Delay the final
        # save, restart World immediately, and prove replacement registration
        # waits for teardown rather than admitting a mixed World generation.
        cid,world_key=start(login_port,cid);live,world_before=enter(map_port,cid,world_key)
        world_position=(destination[0]+2,destination[1],destination[2]+1)
        live.sendall(frame(struct.pack('<HfffHHBBBBf',0,*world_position,0,90,0,0,0,0,1.0),0x5289,3));time.sleep(.1)
        owner_before=conn.execute('SELECT owner_token,backend_pid FROM app_world.map_runtime_owner WHERE world_id=1 AND server_id=1').fetchone()
        registered_before=registrations()
        conn.execute("CREATE FUNCTION public.synthetic_world_save_delay() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.outcome='logout' THEN PERFORM pg_sleep(1.5); END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_world_save_delay BEFORE UPDATE ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.synthetic_world_save_delay()')
        command(['kill','--signal','KILL',names[0]])
        check(execute(['podman','wait',names[0]],timeout=10).strip()=='137','actual World is killed while Map remains alive')
        until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]>0,
              'World loss triggers a real in-flight final save')
        command(['start',names[0]])
        gate_closed('native admission remains closed during World-loss final save')
        check(registrations()==registered_before,'replacement registration waits for durable client teardown')
        check(live.recv(1)==b'','World loss closes the ready client');live.close()
        until(lambda:registrations()>registered_before,'Map registers with replacement World after saving clients')
        conn.execute('DROP TRIGGER synthetic_world_save_delay ON app_world.map_checkpoints')
        conn.execute('DROP FUNCTION public.synthetic_world_save_delay()')
        check(conn.execute('SELECT owner_token,backend_pid FROM app_world.map_runtime_owner WHERE world_id=1 AND server_id=1').fetchone()==owner_before,
              'Map process and PostgreSQL ownership survive World restart')
        check(conn.execute('SELECT outcome FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()[0]=='logout' and
              conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,
              'World-loss save atomically releases the native claim with logout receipt')
        cid,world_key=start(login_port,cid);live,world_after=enter(map_port,cid,world_key)
        check(world_after['position']==world_position and world_after['items']==remaining_items,
              'replacement World admission restores final movement and unchanged inventory')
        live.close();until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,
                           'replacement World client disconnect completes')
        # Loss while CONREADY commits must still promote the in-memory phase for
        # teardown, save/release it, and allow a fresh login on the surviving Map.
        conn.execute("CREATE FUNCTION public.synthetic_world_ready_delay() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.phase='ready' THEN PERFORM pg_sleep(1); END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_world_ready_delay BEFORE UPDATE ON app_world.map_sessions FOR EACH ROW EXECUTE FUNCTION public.synthetic_world_ready_delay()')
        cid,world_key=start(login_port,cid);live=socket.create_connection(('127.0.0.1',map_port),timeout=8)
        live.sendall(frame(connect_request(706,cid,world_key),0x5281,1))
        check(read_packet(live,1)==(0x53be,bytes([0,1])) and read_packet(live,2)[0]==0x5285 and read_packet(live,3)==(0x5282,bytes([0,1,1])),
              'World-loss ready-race client reaches admitted phase')
        live.sendall(frame(b'',0x5288,2))
        until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]>0,
              'ready commit is in flight when World link is lost')
        registered_before=registrations();command(['kill','--signal','TERM',names[0]])
        check(execute(['podman','wait',names[0]],timeout=10).strip()=='0','World shuts down during Map ready commit')
        command(['start',names[0]])
        until(lambda:registrations()>registered_before,'Map reconnects after draining interrupted readiness')
        live.close();conn.execute('DROP TRIGGER synthetic_world_ready_delay ON app_world.map_sessions')
        conn.execute('DROP FUNCTION public.synthetic_world_ready_delay()')
        check(conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0 and
              conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=706').fetchone()[0]==0,
              'World loss during ready commit leaves no stranded account')
        # A deterministic database delay puts SIGTERM inside MarkReady's commit.
        # The closed socket must still save/release the committed ready claim.
        conn.execute("CREATE FUNCTION public.synthetic_ready_delay() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.phase='ready' THEN PERFORM pg_sleep(0.5); END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_ready_delay BEFORE UPDATE ON app_world.map_sessions FOR EACH ROW EXECUTE FUNCTION public.synthetic_ready_delay()')
        cid,racing_key=start(login_port,cid)
        closing=socket.create_connection(('127.0.0.1',map_port),timeout=8)
        closing.sendall(frame(connect_request(706,cid,racing_key),0x5281,1))
        check(read_packet(closing,1)==(0x53be,bytes([0,1])) and read_packet(closing,2)[0]==0x5285 and read_packet(closing,3)==(0x5282,bytes([0,1,1])),
              'shutdown-race client reaches admitted state')
        closing.sendall(frame(b'',0x5288,2))
        until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]==1,
              'ready transaction is deterministically in flight')
        command(['kill','--signal','TERM',names[1]])
        check(execute(['podman','wait',names[1]],timeout=15).strip()=='0','SIGTERM during ready commit drains save and exits zero')
        closing.close()
        check(conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,
              'closed-during-ready session leaves no stranded account reservation')
        conn.execute('DROP TRIGGER synthetic_ready_delay ON app_world.map_sessions')
        conn.execute('DROP FUNCTION public.synthetic_ready_delay()')
        command(['start',names[1]])
        health=command(['port',names[1],'18916/tcp']).rsplit(':',1)[1]
        def healthy():
            try:
                with urllib.request.urlopen('http://127.0.0.1:'+health+'/healthz',timeout=.5) as response:return response.status==200
            except (OSError,TimeoutError):return False
        until(healthy,'restarted Map becomes healthy')
        map_port=int(command(['port',names[1],'5815/tcp']).rsplit(':',1)[1])
        conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=300000 WHERE "dwCharID"=%s AND "wSkillID"=%s',(cid,cooldown_skill))
        cid,restarted_key=start(login_port,cid);s,third=enter(map_port,cid,restarted_key)
        check(third['position']==world_position and third['items']==remaining_items,'real Map process restart restores saved character')
        # Verify the actual periodic saver before crashing it inside the next
        # core transaction. PostgreSQL must retain the previous receipt and core.
        checkpoint_position=(destination[0]+4,destination[1],destination[2]+4)
        s.sendall(frame(struct.pack('<HfffHHBBBBf',0,*checkpoint_position,0,90,0,0,0,0,1.0),0x5289,3))
        def core_position():
            return conn.execute('SELECT "fPosX","fPosY","fPosZ" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        until(lambda:struct.pack('<fff',*core_position())==struct.pack('<fff',*checkpoint_position),'periodic checkpoint saves movement while client remains online')
        check(conn.execute("SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s AND phase='ready'",(cid,)).fetchone()[0]==1,
              'periodic checkpoint preserves ready ownership')
        conn.execute("CREATE FUNCTION public.synthetic_checkpoint_delay() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.\"dwUserID\"=706 THEN PERFORM pg_sleep(1); END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_checkpoint_delay BEFORE UPDATE ON app_world."TCHARTABLE" FOR EACH ROW EXECUTE FUNCTION public.synthetic_checkpoint_delay()')
        unsaved=(checkpoint_position[0]+8,checkpoint_position[1],checkpoint_position[2]+8)
        s.sendall(frame(struct.pack('<HfffHHBBBBf',0,*unsaved,0,90,0,0,0,0,1.0),0x5289,4))
        until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]==1,
              'periodic checkpoint is deterministically in flight before crash')
        command(['kill','--signal','KILL',names[1]])
        check(execute(['podman','wait',names[1]],timeout=5).strip()=='137','hard crash kills the real Map process')
        s.close()
        conn.execute('DROP TRIGGER synthetic_checkpoint_delay ON app_world."TCHARTABLE"')
        check(struct.pack('<fff',*core_position())==struct.pack('<fff',*checkpoint_position),'killed transaction cannot advance core without its receipt')
        check(conn.execute('SELECT outcome FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()[0]=='active',
              'crash retains the last active checkpoint receipt')
        crashed_cooldown=stored_cooldown()
        check(0<crashed_cooldown<300000 and conn.execute('SELECT app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(True,),
              'hard crash preserves exact last committed fresh skill receipt and durable timers')
        command(['start',names[1]])
        health=command(['port',names[1],'18916/tcp']).rsplit(':',1)[1]
        until(healthy,'Map restarts after hard crash')
        map_port=int(command(['port',names[1],'5815/tcp']).rsplit(':',1)[1])
        check(conn.execute("SELECT count(*) FROM app_world.map_checkpoints WHERE char_id=%s AND outcome='recovered' AND recovered_at IS NOT NULL",(cid,)).fetchone()[0]==1,
              'replacement owner records checkpoint recovery')
        check(conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=706').fetchone()[0]==0,
              'recovery releases the crashed account for authentication')
        cid,recovered_key=start(login_port,cid);s,fourth=enter(map_port,cid,recovered_key)
        check(fourth['position']==checkpoint_position and fourth['items']==remaining_items,
              'same World reconnect restores last committed checkpoint and all items')
        countdown(fourth,cooldown_skill,crashed_cooldown,
              'replacement process restores last committed fresh cooldown after hard crash')
        # Shutdown must drain both a pending checkpoint and the final save.
        conn.execute('CREATE TRIGGER synthetic_checkpoint_delay BEFORE UPDATE ON app_world."TCHARTABLE" FOR EACH ROW EXECUTE FUNCTION public.synthetic_checkpoint_delay()')
        s.sendall(frame(struct.pack('<HfffHHBBBBf',0,*unsaved,0,90,0,0,0,0,1.0),0x5289,3))
        until(lambda:conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0]==1,
              'checkpoint is in flight at graceful shutdown')
        command(['kill','--signal','TERM',names[1]])
        check(execute(['podman','wait',names[1]],timeout=15).strip()=='0','SIGTERM drains checkpoint and final save')
        s.close()
        conn.execute('DROP TRIGGER synthetic_checkpoint_delay ON app_world."TCHARTABLE"')
        conn.execute('DROP FUNCTION public.synthetic_checkpoint_delay()')
        check(struct.pack('<fff',*core_position())==struct.pack('<fff',*unsaved) and
              conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]==0,
              'checkpoint/shutdown race keeps final position without a stranded claim')
        command(['start',names[1]])
        health=command(['port',names[1],'18916/tcp']).rsplit(':',1)[1]
        until(healthy,'Map restarts after checkpoint shutdown')

        cid,failed_key=start(login_port,cid);failed_client,failed_before=enter(map_port,cid,failed_key)
        saved_before=conn.execute('SELECT core_state FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()[0]
        conn.execute("CREATE FUNCTION public.synthetic_world_save_failure() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.outcome='logout' THEN RAISE EXCEPTION 'synthetic final-save failure'; END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_world_save_failure BEFORE UPDATE ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.synthetic_world_save_failure()')
        registered_before=registrations();command(['kill','--signal','TERM',names[0]])
        check(execute(['podman','wait',names[0]],timeout=10).strip()=='0','World shutdown injects a failed final save')
        command(['start',names[0]])
        until(lambda:registrations()>registered_before,'World reconnect completes with failed account retained')
        check(failed_client.recv(1)==b'','failed persistence still closes the network client');failed_client.close()
        check(conn.execute('SELECT phase FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()[0]=='ready' and
              conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwUserID"=706').fetchone()[0]==1,
              'failed final save preserves durable ownership instead of reporting logout')
        check(conn.execute('SELECT core_state,outcome FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(saved_before,'active'),
              'failed World-loss save rolls back core and receipt atomically')
        with socket.create_connection(('127.0.0.1',map_port),timeout=8) as retry:
            retry.sendall(frame(connect_request(706,cid,failed_key),0x5281,1))
            op,body=read_packet(retry,1)
            check(op==0x5282 and body==bytes([3,0]),'retained failed-save generation refuses duplicate client admission')
        conn.execute('DROP TRIGGER synthetic_world_save_failure ON app_world.map_checkpoints')
        conn.execute('DROP FUNCTION public.synthetic_world_save_failure()')

        pid=conn.execute('SELECT backend_pid FROM app_world.map_runtime_owner WHERE world_id=1 AND server_id=1').fetchone()[0]
        conn.execute('SELECT pg_terminate_backend(%s)',(pid,))
        until(lambda:not json.loads(command(['inspect',names[1]]))[0]['State']['Running'],'control connection loss stops actual Map process')
        check(execute(['podman','wait',names[1]],timeout=5).strip()=='2','owner loss exits nonzero rather than advertising healthy service')
        map_log=execute(['podman','logs',names[1]])
        check(not any(e in map_log for e in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:')),
              'Map log across restart and owner loss has no sanitizer failure')
        for name in reversed(names):
            if name==names[1]:continue
            command(['kill','--signal','TERM',name]);check(execute(['podman','wait',name],timeout=15).strip()=='0','actual daemon exits zero on SIGTERM')
            log=execute(['podman','logs',name]);check(not any(e in log for e in ('ERROR: AddressSanitizer','ERROR: LeakSanitizer','runtime error:')),'daemon log has no sanitizer failure')
        return {'status':'passed','checks':checks,'skill_cast_wire':skill_wire,'world_secondary_wire':secondary_wire,'world_handoff_wire':handoff_wire,'map_rejection_wire':rejected_secondary,'map_replica_wire':replica_wire,'scope':'Actual Login/World/two-Map TCP, ungranted second-Map rejection and granted replica admission with an explicitly synthetic cell partition, World-loss admission/drain/restart, failed final save retention, periodic core checkpoints and SIGKILL/SIGTERM recovery; synthetic account, original backup content; real client and persisted social systems pending'}
    except Exception:
        (private/'runtime-progress.json').write_text(json.dumps({'completed_checks':checks},indent=2))
        (private/'runtime-failure.json').write_text(json.dumps({n:execute(['podman','logs',n]) for n in started},indent=2))
        raise
    finally:
        for name in reversed(started):verify_container(state,name);command(['rm','--force',name])
        for i in range(4):(private/f'runtime-{i}.env').unlink(missing_ok=True)
