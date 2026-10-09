"""Old installed Map -> current native Map, with actual persisted wire graphs.

The enclosing runner owns the disposable DB. Only its synthetic selector and
single-cell routing view are reset between cases; production publication is always
performed by the real publisher. Historical imports and player receipt bytes are
never rewritten to make an old graph look current.
"""
import json
import socket
import struct
from activate_actor_catalog import activate_actor_catalog
from migrate import digest
from verify_login_wire import frame,read_packet
from verify_graph_reagent_wire import seed_graph_reagent
from verify_inventory_stack_wire import graph_stack_packet
from verify_character_statistics_wire import source_statistics


def verify_actor_transition(conn,options,image,bin_dir,launch,stop,select_runtime,start,enter,
                            connect_request,parse_character,until,registered,freeze_maps):
    checks=[];cases=[];sockets=[]
    old_hash=digest(json.dumps(json.loads(options['previous_manifest'].read_text()),sort_keys=True,separators=(',',':')).encode())
    new_hash=digest(json.dumps(json.loads(options['manifest'].read_text()),sort_keys=True,separators=(',',':')).encode())
    old_run=conn.execute('SELECT id FROM reconstruction.import_runs WHERE manifest_sha256=%s',(old_hash,)).fetchone()[0]
    definition=conn.execute('SELECT pg_get_viewdef(\'route_compat."TSVRCHART"\'::regclass,true)').fetchone()[0]
    def check(ok,label):
        if not ok:raise RuntimeError('Actor transition: '+label)
        checks.append(label)
    def checkpoint(cid):
        return conn.execute('SELECT row_to_json(p) FROM app_world.map_checkpoints p WHERE char_id=%s',(cid,)).fetchone()[0]
    def journal(cid):
        return conn.execute('SELECT row_to_json(t) FROM app_world.map_transfers t WHERE char_id=%s ORDER BY transfer_id',(cid,)).fetchall()
    try:
        for mode in ('logout','target-crash','prepared-source-crash'):
            # Explicit fixture reconstruction of an old deployment, offline.
            conn.execute('UPDATE runtime_control.actor_catalog SET run_id=%s WHERE singleton',(old_run,))
            conn.execute('CREATE OR REPLACE VIEW route_compat."TSVRCHART" AS SELECT release_id,"bGroup",'
                         'CASE WHEN "wMapID"=0 AND "wUnitID"=772 AND "bChannel"=1 THEN 2::smallint ELSE "bServerID" END AS "bServerID",'
                         '"wMapID","wUnitID","bChannel" FROM ('+definition.rstrip().rstrip(';')+') original')
            select_runtime(old_hash,options['previous_image'],'/opt/fourstory/bin/')
            primary_port=launch(1);replica_port=launch(3);launch(0);login_port=launch(2)
            until(lambda:registered(1) and registered(3),'both old installed Maps register')
            conn.execute('UPDATE app_global."TSERVER" SET "wPort"=%s WHERE "bGroupID"=1 AND "bServerID"=2 AND "bType"=4',(replica_port,))
            cid,key=start(login_port,name=('Upgrade'+str(len(cases))).encode(),slot=len(cases))
            conn.execute('UPDATE app_world."TCHARTABLE" SET "wMapID"=0,"fPosX"=4080,"fPosY"=80,"fPosZ"=3584 WHERE "dwCharID"=%s',(cid,))
            seed_graph_reagent(conn,cid,True)
            conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=300000 WHERE "dwCharID"=%s',(cid,))
            primary=socket.create_connection(('127.0.0.1',primary_port),timeout=8);sockets.append(primary)
            primary.sendall(frame(connect_request(706,cid,key),0x5281,1))
            check(read_packet(primary,1)==(0x53be,b'\0\1'),mode+': old channel packet matches source')
            op,body=read_packet(primary,2);check(op==0x5285,mode+': old Map sends full CHARINFO')
            character=parse_character(body)
            descriptor=next(item for bag,item,magic in character['items'] if bag==255 and item[0]==2)
            check(read_packet(primary,3)==(0x5284,b'\1\x7f\0\0\1'+struct.pack('<HB',replica_port,2)),mode+': old ADDCONNECT advertises the actual secondary')
            replica=socket.create_connection(('127.0.0.1',replica_port),timeout=8);sockets.append(replica)
            request=bytearray(connect_request(706,cid,key));struct.pack_into('<H',request,19,replica_port)
            replica.sendall(frame(bytes(request),0x5281,1))
            check(read_packet(primary,4)==(0x5282,b'\0\2\1\2'),mode+': old CONNECT synchronizes both native Maps')
            replica.sendall(frame(b'',0x5288,2));primary.sendall(frame(b'',0x5288,2))
            until(lambda:conn.execute('SELECT phase FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==('ready',),mode+': old primary ready')
            until(lambda:conn.execute('SELECT phase FROM app_world.map_replicas WHERE char_id=%s',(cid,)).fetchone()==('ready',),mode+': old secondary ready')
            # A live old process prevents certification and selector mutation.
            events=conn.execute('SELECT count(*) FROM runtime_control.actor_catalog_activations').fetchone()[0]
            try:activate_actor_catalog(conn,options['manifest'],previous_manifest=options['previous_manifest'])
            except Exception as error:
                check(str(error)=='ACTIVE_MAP_OWNER_BLOCKS_ACTOR_PUBLICATION' or getattr(error,'sqlstate',None)=='55P03',mode+': live old Map blocks publication')
            else:raise RuntimeError('live old Map allowed catalog publication')
            check(conn.execute('SELECT run_id FROM runtime_control.actor_catalog').fetchone()==(old_run,) and
                  conn.execute('SELECT count(*) FROM runtime_control.actor_catalog_activations').fetchone()==(events,),mode+': rejected live publication is atomic')
            if mode=='prepared-source-crash':
                conn.execute("CREATE FUNCTION public.pause_actor_upgrade_target() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.server_id=2 AND NEW.phase='loaded' THEN PERFORM pg_sleep(5); END IF; RETURN NEW; END $$")
                conn.execute('CREATE TRIGGER actor_upgrade_target_delay BEFORE UPDATE ON app_world.map_sessions FOR EACH ROW EXECUTE FUNCTION public.pause_actor_upgrade_target()')
            primary.sendall(frame(struct.pack('<HfffHHBBBBf',0,4100,80,3584,0,91,0,0,0,0,1.0),0x5289,3))
            if mode=='prepared-source-crash':
                until(lambda:conn.execute("SELECT 1 FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep' LIMIT 1").fetchone() is not None,'old target pauses before commit')
                freeze_maps();stop(1,'KILL');stop(3,'KILL')
                conn.execute('DROP TRIGGER actor_upgrade_target_delay ON app_world.map_sessions')
                conn.execute('DROP FUNCTION public.pause_actor_upgrade_target()')
                check(conn.execute('SELECT phase FROM app_world.map_transfers WHERE char_id=%s',(cid,)).fetchone()==('prepared',),'interrupted old target leaves the committed source prepare receipt')
            else:
                check(read_packet(replica,1)==(0x5282,b'\0\2\1\2'),mode+': old movement promotes target with original CONNECT')
                replica.sendall(frame(b'',0x5288,3));primary.sendall(frame(b'',0x5288,4))
                until(lambda:conn.execute('SELECT server_id,authority_epoch,phase FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(2,1,'ready'),mode+': old target commits ready ownership')
                # This writes only the graph. Old normalized child rows deliberately
                # disagree, detecting a fallback that silently reloads stale data.
                graph_stack_packet(conn,replica,cid,descriptor,4,2,3,'split',check)
                graph_stack_packet(conn,replica,cid,descriptor,5,5,3,'move',check)
                if mode=='logout':
                    replica.close();primary.close()
                    until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'old graph logout completes')
                    stop(1);stop(3)
                else:
                    freeze_maps();stop(1,'KILL');stop(3,'KILL')
            primary.close();replica.close();stop(2);stop(0)
            conn.execute('CREATE OR REPLACE VIEW route_compat."TSVRCHART" AS '+definition)
            old_checkpoint=checkpoint(cid);old_journal=journal(cid)
            check(old_journal[0][0]['actor_manifest']==old_hash,mode+': original old manifest is still in the durable journal')
            if mode!='prepared-source-crash':check(old_checkpoint['actor_manifest']==old_hash,mode+': checkpoint still carries original old manifest')
            if mode=='logout':
                try:activate_actor_catalog(conn,options['manifest'])
                except RuntimeError as error:check(str(error)=='ACTOR_GRAPH_COMPATIBILITY_REQUIRED','old stored graph blocks publication without explicit proof')
                else:raise RuntimeError('uncertified old graph allowed publication')
                check(checkpoint(cid)==old_checkpoint and journal(cid)==old_journal,'failed uncertified publication preserves all player receipts')
            published=activate_actor_catalog(conn,options['manifest'],previous_manifest=options['previous_manifest'])
            check(published['changed'] and published['retired_map_owners']>=2,mode+': offline verified publication retires inactive owners')
            check(checkpoint(cid)==old_checkpoint and journal(cid)==old_journal,mode+': publication preserves every player receipt field and original body byte')
            owners=conn.execute('SELECT world_id,server_id,owner_token FROM app_world.map_runtime_owner ORDER BY world_id,server_id').fetchall()
            repeated=activate_actor_catalog(conn,options['manifest'],previous_manifest=options['previous_manifest'])
            check(not repeated['changed'] and not repeated['compatibility_created'] and not repeated['retired_map_owners'] and
                  owners==conn.execute('SELECT world_id,server_id,owner_token FROM app_world.map_runtime_owner ORDER BY world_id,server_id').fetchall(),mode+': exact repeated publication preserves owner tokens and history')
            select_runtime(new_hash,image,bin_dir)
            primary_port=launch(1);launch(3);launch(0);login_port=launch(2)
            until(lambda:registered(1) and registered(3),'both current Maps register after upgrade')
            recovered=checkpoint(cid)
            check(recovered['actor_manifest']==old_hash and recovered['outcome']==('logout' if mode=='logout' else 'recovered'),mode+': ordinary process replacement preserves old graph evidence')
            if mode=='prepared-source-crash':
                check(conn.execute('SELECT phase FROM app_world.map_transfers WHERE char_id=%s',(cid,)).fetchone()==('cancelled',),'replacement cancels prepared source only after recovering its frozen graph')
            # Recovery of a source prepare still validates its pre-transfer
            # contract-3 normalized skill receipt. After that recovery commits
            # contract 2, only the wire graph may supply timers on fresh login.
            conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=0 WHERE "dwCharID"=%s',(cid,))
            _,new_key=start(login_port,cid);client,restored=enter(primary_port,cid,new_key);sockets.append(client)
            check(restored['position']==(4100,80,3584),mode+': source-frozen target position survives upgrade and recovery')
            check(all(240000<t<=300000 for _,_,t in restored['skills']) and len(restored['skills'])==len(character['skills']),mode+': graph skill timers survive despite zeroed stale normalized rows')
            stacks={item[0]:item[6] for bag,item,magic in restored['items'] if bag==255 and item[1]==8401}
            check(stacks==({2:3} if mode=='prepared-source-crash' else {2:2,4:1}),mode+': exact graph inventory is restored instead of stale child rows')
            expected_items=[(b,i,m) for b,i,m in character['items'] if not (b==255 and i[0]==2)]
            check([(b,i,m) for b,i,m in restored['items'] if not (b==255 and i[1]==8401)]==expected_items,mode+': all other item descriptors and options survive')
            check(conn.execute('SELECT actor_manifest,recovery_contract,app_world.map_checkpoint_matches(map_checkpoints) FROM app_world.map_checkpoints WHERE char_id=%s',(cid,)).fetchone()==(new_hash,2,True),mode+': fresh fenced readiness writes a valid current-manifest graph')
            client.sendall(frame(struct.pack('<I',cid),0x5323,3))
            check(read_packet(client,4)==(0x5324,source_statistics(conn,cid)),mode+': restored old graph supports exact new source-derived stat response')
            client.close()
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'upgraded graph final save completes')
            _,new_key=start(login_port,cid);client,again=enter(primary_port,cid,new_key);sockets.append(client)
            check(again['items']==restored['items'] and again['position']==restored['position'],mode+': upgraded graph survives another complete Login and Map lifecycle')
            client.close();until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'second upgraded logout completes')
            for index in (1,3,2,0):stop(index)
            cases.append(dict(mode=mode,status='passed',activation=published))
    finally:
        for client in sockets:client.close()
        conn.execute('CREATE OR REPLACE VIEW route_compat."TSVRCHART" AS '+definition)
    return dict(status='passed',checks=checks,cases=cases,previous_image=options['previous_image'],
                previous_manifest=old_hash,manifest=new_hash,scope='Old installed Map creates logout, ready-target-crash and interrupted-prepared-source states over actual encrypted TCP. Explicit backup-derived additive proof, immutable receipt bytes, normal owner recovery, graph items/timers and new stat response verified. No original client executable.')
