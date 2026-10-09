"""Actual two-Map native admission with an explicitly synthetic cell partition.

Only the disposable compatibility VIEW is temporarily overridden. Imported rows
and original backups are unchanged; its exact definition is restored in finally.
The synthetic split is not claimed to be topology recovered from the backup.
"""
import select
import socket
import struct
import time
from verify_login_wire import frame, read_packet
from verify_graph_reagent_wire import seed_graph_reagent, graph_reagent_cast
from verify_inventory_stack_wire import graph_stack_packet
from verify_character_statistics_wire import source_statistics
from verify_equipment_wire import descriptor


def verify_map_replica(conn, primary_port, replica_port, login_port, cid, start, connect_request, parse_character, ammunition=False):
    checks = []
    sockets = []
    reagent_id=None;expected_reagent=None;reagent_descriptor=None
    current_character=None
    def check(ok, label):
        if not ok:
            raise RuntimeError('Map replica wire: ' + label)
        checks.append(label)
    def until(predicate, label):
        deadline = time.monotonic()+8
        while not predicate():
            if time.monotonic() >= deadline:
                raise RuntimeError('Map replica wait: ' + label)
            time.sleep(.025)
        check(True, label)
    def no_packet(s, label):
        readable, _, _ = select.select([s], [], [], .15)
        check(not readable, label)
    definition = conn.execute('SELECT pg_get_viewdef(\'route_compat."TSVRCHART"\'::regclass,true)').fetchone()[0]
    before = conn.execute('SELECT "wMapID","fPosX","fPosY","fPosZ" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s', (cid,)).fetchone()
    old_weapon=None
    if ammunition:
        from psycopg import sql
        row=conn.execute('SELECT * FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=2',(cid,))
        old_weapon=dict(zip((c.name for c in row.description),row.fetchone()))
        conn.execute('UPDATE app_world."TITEMTABLE" SET "wItemID"=701,"dwDuraMax"=100,"dwDuraCur"=100 WHERE "dlID"=%s',(old_weapon['dlID'],))
        conn.execute('INSERT INTO app_world."TSKILLTABLE" VALUES(1,%s,32,1,0) ON CONFLICT DO NOTHING',(cid,))
    old_port = conn.execute('SELECT "wPort" FROM app_global."TSERVER" WHERE "bGroupID"=1 AND "bServerID"=2 AND "bType"=4').fetchone()[0]
    items = conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"', (cid,)).fetchall()
    def enter():
        nonlocal reagent_descriptor,current_character
        _, key = start(login_port, cid)
        primary = socket.create_connection(('127.0.0.1', primary_port), timeout=8);sockets.append(primary)
        primary.sendall(frame(connect_request(706, cid, key), 0x5281, 1))
        op, body = read_packet(primary, 1)
        check(op == 0x53be and body == b'\0\1', 'primary receives source channel notification')
        op, body = read_packet(primary, 2)
        check(op == 0x5285, 'source primary CHARINFO precedes ADDCONNECT and CONNECT')
        character = parse_character(body)
        current_character=character
        if expected_reagent is not None:
            rows=[item for bag,item,options in character['items'] if bag==255 and item[0]==2]
            check((len(rows)==1 and rows[0][6]==expected_reagent) if expected_reagent else not rows,
                  'graph relogin restores exact consumed count or deletion despite stale item rows')
            if rows:reagent_descriptor=rows[0]
        op, body = read_packet(primary, 3)
        check(op == 0x5284 and body == b'\1\x7f\0\0\1'+struct.pack('<HB', replica_port, 2), 'exact ADDCONNECT carries the authorized second Map endpoint')
        grant = conn.execute('SELECT phase,connection_id FROM app_world.map_replicas WHERE char_id=%s AND target_server=2', (cid,)).fetchone()
        check(grant == ('granted', None), 'one-use grant commits before endpoint is sent to client')
        replica = socket.create_connection(('127.0.0.1', replica_port), timeout=8);sockets.append(replica)
        request = bytearray(connect_request(706, cid, key));struct.pack_into('<H', request, 19, replica_port)
        replica.sendall(frame(bytes(request), 0x5281, 1))
        op, body = read_packet(primary, 4)
        check(op == 0x5282 and body == b'\0\2\1\2', 'primary CONNECT lists both actual Map connections after composite synchronization')
        check(conn.execute('SELECT phase FROM app_world.map_replicas WHERE char_id=%s', (cid,)).fetchone() == ('loaded',), 'World ENTERCHAR hydrates replica without durable character reload')
        replica.sendall(frame(b'', 0x5288, 2))
        until(lambda: conn.execute('SELECT phase FROM app_world.map_replicas WHERE char_id=%s', (cid,)).fetchone() == ('ready',), 'replica accepts CONREADY without a separate CONRESULT')
        no_packet(replica, 'replica does not invent CONNECT or CHARINFO acknowledgments')
        check(conn.execute('SELECT phase FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone() == ('loaded',), 'replica readiness does not promote or checkpoint the primary')
        primary.sendall(frame(b'', 0x5288, 2))
        until(lambda: conn.execute('SELECT phase FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone() == ('ready',), 'CONREADY commits the already hydrated primary')
        no_packet(primary, 'CONREADY does not reset the client with duplicate CHARINFO')
        check(character['position'][0] >= 4080 and len(character['items']) > 0, 'primary retains its live position and complete original starter inventory')
        check(conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s AND server_id=1 AND phase=\'ready\'', (cid,)).fetchone()[0] == 1, 'two client sockets retain exactly one mutable primary')
        return primary, replica, key
    try:
        conn.execute('UPDATE app_global."TSERVER" SET "wPort"=%s WHERE "bGroupID"=1 AND "bServerID"=2 AND "bType"=4', (replica_port,))
        conn.execute('CREATE OR REPLACE VIEW route_compat."TSVRCHART" AS SELECT release_id,"bGroup",'
                     'CASE WHEN "wMapID"=0 AND "wUnitID"=772 AND "bChannel"=1 THEN 2::smallint ELSE "bServerID" END AS "bServerID",'
                     '"wMapID","wUnitID","bChannel" FROM ('+definition.rstrip().rstrip(';')+') original')
        conn.execute('UPDATE app_world."TCHARTABLE" SET "wMapID"=0,"fPosX"=4080,"fPosY"=80,"fPosZ"=3584 WHERE "dwCharID"=%s', (cid,))
        primary, replica, key = enter()
        replica.sendall(frame(struct.pack('<HfffHHBBBBf', 0, 4083, 80, 3584, 0, 90, 0, 0, 0, 0, 1.0), 0x5289, 3))
        no_packet(primary, 'replica movement does not broadcast a duplicate primary actor')
        primary.sendall(frame(struct.pack('<HfffHHBBBBf', 0, 4081, 80, 3584, 0, 90, 0, 0, 0, 0, 1.0), 0x5289, 3))
        time.sleep(.1);primary.close()
        until(lambda: conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone()[0] == 0, 'primary disconnect saves and closes both durable roles')
        check(replica.recv(1) == b'', 'World retires the actual replica client after primary close')
        check(conn.execute('SELECT count(*) FROM app_world.map_replicas WHERE char_id=%s', (cid,)).fetchone()[0] == 0, 'primary final save cascades the replica claim')
        check(conn.execute('SELECT "fPosX" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s', (cid,)).fetchone()[0] == 4081, 'replica position cannot overwrite primary core at teardown')
        primary, replica, key = enter()
        replica.close()
        check(primary.recv(1) == b'', 'accepted secondary disconnect follows source close-all through World')
        until(lambda: conn.execute('SELECT count(*) FROM app_global."TCURRENTUSER" WHERE "dwKEY"=%s', (key,)).fetchone()[0] == 0, 'secondary-triggered close completes primary save and account release')
        check(conn.execute('SELECT count(*) FROM app_world.map_replicas WHERE char_id=%s', (cid,)).fetchone()[0] == 0, 'secondary-triggered close leaves no stranded replica')
        # A third lifecycle crosses the synthetic unit boundary in both
        # directions using only original encrypted client MOVE/CONREADY packets.
        reagent_id=seed_graph_reagent(conn,cid,ammunition);expected_reagent=9 if ammunition else 3
        if ammunition:conn.execute('UPDATE app_world."TITEMTABLE" SET "bCount"=9 WHERE "dlID"=%s',(reagent_id,))
        conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=CASE WHEN "wSkillID"=%s THEN 0 ELSE 300000 END WHERE "dwCharID"=%s', (32 if ammunition else 1623,cid))
        primary, replica, key = enter()
        primary.sendall(frame(struct.pack('<HfffHHBBBBf', 0, 4100, 80, 3584, 0, 91, 0, 0, 0, 0, 1.0), 0x5289, 3))
        try:
            op, body = read_packet(replica, 1)
        except (OSError, TimeoutError, RuntimeError) as error:
            raise RuntimeError('Map primary transfer: target CONNECT missing after original client MOVE crossed the cell boundary') from error
        check(op == 0x5282 and body == b'\0\2\1\2', 'crossing cell boundary promotes the actual second Map and sends original CONNECT')
        until(lambda: conn.execute('SELECT server_id,authority_epoch,phase FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone() == (2,1,'loaded'), 'target ownership epoch commits before client CONREADY')
        receipt = conn.execute("SELECT source_epoch,target_epoch,phase,octet_length(body) FROM app_world.map_transfers WHERE char_id=%s ORDER BY transfer_id DESC LIMIT 1", (cid,)).fetchone()
        check(receipt[:3] == (0,1,'consumed') and receipt[3] > 200, 'actual native transfer retains complete original wire graph receipt')
        replica.sendall(frame(b'', 0x5288, 3));primary.sendall(frame(b'', 0x5288, 4))
        until(lambda: conn.execute('SELECT server_id,authority_epoch,phase FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone() == (2,1,'ready'), 'promoted native target enters gameplay after client confirmation')
        no_packet(replica, 'promotion does not reload client CHARINFO or invent protocol fields')
        no_packet(primary, 'retained source accepts repeated CONREADY as a replica')
        graph_reagent_cast(conn,replica,cid,reagent_id,reagent_descriptor,4,2,9 if ammunition else 3,False,check,delay=True,ammunition=ammunition)
        expected_reagent=5 if ammunition else 2
        no_packet(primary,'former primary receives no private inventory response from its successor')
        graph_stack_packet(conn,replica,cid,reagent_descriptor,5,5,expected_reagent,'split',check)
        graph_stack_packet(conn,replica,cid,reagent_descriptor,6,8,expected_reagent,'move',check)
        no_packet(primary,'graph split and move remain private to their current owner')
        replica.sendall(frame(struct.pack('<HfffHHBBBBf', 0, 4080, 80, 3584, 0, 92, 0, 0, 0, 0, 1.0), 0x5289, 7))
        op, body = read_packet(primary, 5)
        check(op == 0x5282 and body == b'\0\2\1\2', 'return crossing promotes the same original socket with source CONNECT')
        until(lambda: conn.execute('SELECT server_id,authority_epoch,phase FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone() == (1,2,'loaded'), 'round trip increments authority despite reusing original process and connection')
        primary.sendall(frame(b'', 0x5288, 5));replica.sendall(frame(b'', 0x5288, 8))
        until(lambda: conn.execute('SELECT server_id,authority_epoch,phase FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone() == (1,2,'ready'), 'returned primary becomes ready without replacing either client connection')
        no_packet(primary, 'return handoff keeps existing client character state')
        no_packet(replica, 'returned secondary remains connected without duplicate admission')
        graph_stack_packet(conn,primary,cid,reagent_descriptor,6,6,expected_reagent,'merge',check)
        no_packet(replica,'returned primary merges the transferred split without a private ACK on the replica')
        graph_reagent_cast(conn,primary,cid,reagent_id,reagent_descriptor,7,9,5 if ammunition else 2,True,check,ammunition=ammunition)
        expected_reagent=1
        primary.sendall(frame(struct.pack('<I',cid),0x5323,8))
        check(read_packet(primary,12)==(0x5324,source_statistics(conn,cid)),
              'complete stat sheet survives both ownership transfers and graph inventory/cooldown commits')
        replica.sendall(frame(struct.pack('<I',cid),0x5323,9))
        no_packet(replica,'former primary cannot serve a stale stat sheet after demotion')
        # Exercise equipment while contract-2 graph is authoritative. The stale
        # normalized item rows remain unchanged; expected stats use the original
        # chart calculation with the requested independent equipment placement.
        equipped={i[0]:(i,m) for bag,i,m in current_character['items'] if bag==254}
        head=equipped[3]
        initial_equipped=[r[0] for r in conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s AND "dwStorageID"=254',(cid,))]
        graph_rows=conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()
        ss=13
        for cs,remove in ((9,True),(10,False)):
            sb,db=(254,255) if remove else (255,254)
            primary.sendall(frame(bytes([sb,3,db,3,255]),0x52a8,cs))
            if remove:equipped.pop(3)
            else:equipped[3]=head
            expected_rows=[i for i in initial_equipped if i['bItemID']!=3] if remove else initial_equipped
            hp,mp=conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
            expected=[(0x52ac,bytes([sb,3])),(0x52ab,bytes([db])+descriptor(*head)),
                      (0x52ad,struct.pack('<IB',cid,len(equipped))+b''.join(descriptor(*equipped[k]) for k in sorted(equipped))),
                      (0x52a9,b'\0'),(0x5324,source_statistics(conn,cid,expected_rows)),
                      (0x52a2,struct.pack('<IBIIII',cid,1,current_character['hpmp'][0],hp,current_character['hpmp'][2],mp)),(0x52a9,b'\0')]
            for message in expected:
                check(read_packet(primary,ss)==message,'graph equipment exact source packet '+hex(message[0]));ss+=1
            check(conn.execute('SELECT state_contract,changed_items FROM app_world.equipment_operations WHERE char_id=%s ORDER BY operation_id DESC LIMIT 1',(cid,)).fetchone()==(2,1),
                  'graph equipment commits one identity diff under contract two')
            check(conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"',(cid,)).fetchall()==graph_rows,
                  'graph equipment never overwrites stale normalized item rows')
        no_packet(replica,'equipment item and stat replies stay on the current primary connection')
        primary.close()
        until(lambda: conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone()[0] == 0, 'transferred primary performs final native save and releases account')
        check(replica.recv(1) == b'', 'final close after round trip retires the retained replica')
        check(conn.execute('SELECT "fPosX","wDIR" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s', (cid,)).fetchone() == (4080,92), 'round trip preserves the latest live position and direction through final save')
        saved_graph = conn.execute('SELECT transfer_body,transfer_hash FROM app_world.map_checkpoints WHERE char_id=%s', (cid,)).fetchone()
        check(saved_graph[0] is not None and saved_graph[1], 'final transfer logout checkpoints the complete graph with a verified digest')
        # Durable skill rows deliberately disagree: the saved live graph must
        # remain authoritative for a character that already transferred.
        conn.execute('UPDATE app_world."TSKILLTABLE" SET "dwRemainTick"=0 WHERE "dwCharID"=%s', (cid,))
        primary, replica, key = enter()
        skill = conn.execute('SELECT "wSkillID" FROM app_world."TSKILLTABLE" WHERE "dwCharID"=%s ORDER BY "wSkillID" LIMIT 1', (cid,)).fetchone()[0]
        request = struct.pack('<IBBHHBIIfffB',cid,1,1,0,skill & 65535,0,0,0,4080,80,3584,0)
        primary.sendall(frame(request,0x52b4,3));op,body = read_packet(primary,5)
        check(op == 0x52b5 and body[0] == 6, 'relogin restores transferred runtime cooldown despite stale durable skill rows')
        graph_reagent_cast(conn,primary,cid,reagent_id,reagent_descriptor,4,6,1,False,check,ammunition=ammunition)
        expected_reagent=0
        missing=struct.pack('<IBBHHfffB',cid,1,1,0,32 if ammunition else 1623,0,0,0,0)
        if ammunition:
            time.sleep(1.6);missing=missing[:-1]+b'\x01'+struct.pack('<IBB',cid+100000,2,1)
        primary.sendall(frame(missing,0x5372,5));op,body=read_packet(primary,9)
        check(op==0x5373 and body[0]==9,'exhausted graph reagent rejects a repeated loop without reloading stale row')
        check(conn.execute('SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id=%s AND item_id=%s',(cid,reagent_id)).fetchone()==(3,),
              'depleted graph retries cannot append a fourth consumption')
        primary.close()
        until(lambda: conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone()[0] == 0, 'restored graph can complete a fresh Login lifecycle')
        check(replica.recv(1) == b'', 'restored lifecycle closes its secondary through actual World')
        primary, replica, key = enter()
        conn.execute("CREATE FUNCTION public.delay_native_transfer() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN IF NEW.server_id=2 AND NEW.phase='loaded' AND NEW.authority_epoch>0 THEN PERFORM pg_sleep(.8); END IF; RETURN NEW; END $$")
        conn.execute('CREATE TRIGGER synthetic_native_transfer_delay BEFORE UPDATE ON app_world.map_sessions FOR EACH ROW EXECUTE FUNCTION public.delay_native_transfer()')
        try:
            primary.sendall(frame(struct.pack('<HfffHHBBBBf', 0, 4100, 80, 3584, 0, 93, 0, 0, 0, 0, 1.0), 0x5289, 3))
            until(lambda: conn.execute("SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() AND wait_event='PgSleep'").fetchone()[0] > 0, 'actual target transfer pauses inside its ownership transaction')
            replica.close()
            until(lambda: conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s', (cid,)).fetchone()[0] == 0, 'client close during transfer commit drains promoted ownership before teardown')
            check(read_packet(primary,5) == (0x5283,b''), 'World invalidates original source through the source INVALIDCHAR packet during failed transfer')
            check(primary.recv(1) == b'', 'World closes retained source after target disconnect during transfer')
            check(conn.execute('SELECT "fPosX","wDIR" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s', (cid,)).fetchone() == (4100,93), 'commit-close race preserves frozen source movement without a stale source overwrite')
            check(conn.execute("SELECT outcome,recovery_contract,transfer_body IS NOT NULL FROM app_world.map_checkpoints WHERE char_id=%s", (cid,)).fetchone() == ('logout',2,True), 'commit-close race leaves a complete recoverable graph and confirmed logout')
        finally:
            conn.execute('DROP TRIGGER synthetic_native_transfer_delay ON app_world.map_sessions')
            conn.execute('DROP FUNCTION public.delay_native_transfer()')
        # Explicitly remove only this synthetic operational graph before the
        # outer harness restores its synthetic position/core test fixture.
        conn.execute('UPDATE app_world.map_checkpoints SET recovery_contract=1,transfer_body=NULL,transfer_hash=NULL,character_manifest=NULL,routing_manifest=NULL,actor_manifest=NULL WHERE char_id=%s', (cid,))

        conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dlID"=%s',(cid,reagent_id))
        check(items == conn.execute('SELECT row_to_json(i)::text FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s ORDER BY "dlID"', (cid,)).fetchall(), 'two-Map lifecycle preserves all original item fields')
    finally:
        for s in sockets:s.close()
        if reagent_id is not None:
            conn.execute('DELETE FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dlID"=%s',(cid,reagent_id))
        if old_weapon is not None:
            conn.execute(sql.SQL('UPDATE app_world."TITEMTABLE" SET {} WHERE "dlID"=%s').format(
                sql.SQL(',').join(sql.SQL('{}=%s').format(sql.Identifier(c)) for c in old_weapon)),[*old_weapon.values(),old_weapon['dlID']])
        conn.execute('UPDATE app_global."TSERVER" SET "wPort"=%s WHERE "bGroupID"=1 AND "bServerID"=2 AND "bType"=4', (old_port,))
        conn.execute('CREATE OR REPLACE VIEW route_compat."TSVRCHART" AS '+definition)
        conn.execute('UPDATE app_world."TCHARTABLE" SET "wMapID"=%s,"fPosX"=%s,"fPosY"=%s,"fPosZ"=%s WHERE "dwCharID"=%s', (*before, cid))
    check(conn.execute('SELECT pg_get_viewdef(\'route_compat."TSVRCHART"\'::regclass,true)').fetchone()[0] == definition, 'exact original routing view restored after synthetic partition test')
    return {'status':'passed','checks':checks,'scope':'Actual Login/World/two native Maps, source-sized encrypted client packets, explicitly synthetic single-cell partition; original historical rows untouched. Includes primary transfer round trip; full AOI, secondary gameplay and original-client verification remain pending.'}
