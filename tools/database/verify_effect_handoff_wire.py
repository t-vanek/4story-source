"""Original posture/cancellation packets over an actual two-Map round trip.

Called with two admitted sockets in the disposable routing partition. Expected
packets come from legacy CSSender/CSHandler/TObjBase, not modern serializers.
"""
import struct
from verify_login_wire import frame, read_packet
from verify_equipment_wire import descriptor
from verify_character_statistics_wire import source_statistics


def verify_effect_handoff(conn,primary,replica,cid,character,check,until,no_packet):
    peers=[primary,replica];cs=[3,3];ss=[5,1]
    def send(peer,op,body):
        peers[peer].sendall(frame(body,op,cs[peer]));cs[peer]+=1
    def expect(peer,op,body,label):
        check(read_packet(peers[peer],ss[peer])==(op,body),label);ss[peer]+=1
    equipped={i[0]:(i,m) for bag,i,m in character['items'] if bag==254}
    shield=equipped[1]
    kind=conn.execute('SELECT "bKind" FROM character_compat."TITEMCHART" WHERE "wItemID"=%s',(shield[0][1],)).fetchone()
    check(character['effects']==[] and kind==(12,),'handoff fixture uses the backup starter shield and no active effect')
    original=[r[0] for r in conn.execute('SELECT row_to_json(i) FROM app_world."TITEMTABLE" i WHERE "dwOwnerID"=%s AND "dwStorageID"=254',(cid,))]
    def stat(peer,effects,rows=None):
        expect(peer,0x5324,source_statistics(conn,cid,rows,effects=[(i,1) for i in effects]),'handoff posture statistics match original formulas')
    for remove in (True,False):
        sb,sp,db,dp=(254,1,255,12) if remove else (255,12,254,1)
        send(0,0x52a8,bytes([sb,sp,db,dp,1]))
        if remove:equipped.pop(1)
        else:equipped[1]=shield
        moved=(tuple([dp,*shield[0][1:]]),shield[1])
        expect(0,0x52ac,bytes([sb,sp]),'handoff posture item deletion follows source order')
        expect(0,0x52ab,bytes([db])+descriptor(*moved),'handoff posture item addition retains source descriptor')
        if not remove:
            level,country=conn.execute('SELECT "bLevel","bCountry" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
            values=[cid,cid,1,1,0,0,0,0,1,0,0,1,1,level,0,0,0,0,1,0,country,3,131,1,0,1,*character['position'],*character['position'],0]
            expect(0,0x52a1,struct.pack('<IIBBIBIIBIBBHB4I4BHBHB6fB',*values),'handoff creates original permanent Defence Stance')
        expect(0,0x52ad,struct.pack('<IB',cid,len(equipped))+b''.join(descriptor(*equipped[k]) for k in sorted(equipped)),'handoff EQUIP includes complete original equipment')
        expect(0,0x52a9,b'\0','handoff first MOVEITEM success')
        stat(0,[] if remove else [131],[i for i in original if i['bItemID']!=1] if remove else original)
        hp,mp=conn.execute('SELECT "dwHP","dwMP" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
        expect(0,0x52a2,struct.pack('<IBIIII',cid,1,character['hpmp'][0],hp,character['hpmp'][2],mp),'handoff equipment preserves HPMP')
        expect(0,0x52a9,b'\0','handoff final MOVEITEM success')
    effects=[[131,1,0,1,cid,1,cid,4]]
    cancel=struct.pack('<IBIIBHHB',cid,1,0,cid,1,131,65535,255)
    ack=struct.pack('<IBH',cid,1,131)
    count=conn.execute('SELECT count(*) FROM app_world.maintained_effect_operations WHERE char_id=%s',(cid,)).fetchone()[0]
    send(1,0x52b6,cancel);expect(1,0x52b7,ack,'replica answers original SayToAll cancellation without primary mutation')
    no_packet(primary,'replica cancellation has no broadcast on primary socket')
    check(conn.execute('SELECT count(*) FROM app_world.maintained_effect_operations WHERE char_id=%s',(cid,)).fetchone()==(count,),'replica ACK creates no database write')
    send(0,0x5323,struct.pack('<I',cid));stat(0,[131])
    def transfer(source,target,x,epoch):
        send(source,0x5289,struct.pack('<HfffHHBBBBf',0,x,80,3584,0,92,0,0,0,0,1.0))
        expect(target,0x5282,b'\0\2\1\2','posture handoff sends original CONNECT on promoted socket')
        until(lambda:conn.execute('SELECT server_id,authority_epoch,phase FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(target+1,epoch,'loaded'),'posture target commits exact new owner epoch')
        send(target,0x5288,b'');send(source,0x5288,b'')
        until(lambda:conn.execute('SELECT phase FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==('ready',),'posture target reaches ready without duplicate CHARINFO')
    transfer(0,1,4100,1)
    send(1,0x5323,struct.pack('<I',cid));stat(1,[131])
    send(0,0x52b6,cancel);expect(0,0x52b7,ack,'former primary cancellation receives replica-only ACK')
    check(conn.execute('SELECT count(*) FROM app_world.maintained_effect_operations WHERE char_id=%s',(cid,)).fetchone()==(count,),'demoted owner cannot write maintained effects')
    send(1,0x52b6,cancel);expect(1,0x52b7,ack,'promoted owner cancels the transferred posture');stat(1,[])
    receipt=conn.execute('SELECT server_id,authority_epoch,state_contract,request,removed,before_effects,after_effects,before_graph_hash<>after_graph_hash FROM app_world.maintained_effect_operations WHERE char_id=%s ORDER BY operation_id DESC LIMIT 1',(cid,)).fetchone()
    check(receipt==(2,1,2,cancel,1,effects,[],True),'actual target atomically commits graph cancellation under its new epoch')
    check(conn.execute('SELECT app_world.map_maintain_state(1::smallint,%s)',(cid,)).fetchone()==([],),'graph cancellation leaves normalized effect rows untouched')
    no_packet(primary,'target cancellation sends no duplicate response on retained replica')
    transfer(1,0,4080,2)
    send(0,0x5323,struct.pack('<I',cid));stat(0,[])
    send(0,0x52b6,cancel);expect(0,0x52b7,ack,'returning owner confirms absent effect without recreating it')
    no_packet(primary,'absent-effect ACK has no extra STAT after return handoff')
    no_packet(replica,'returning primary leaves retained replica silent')
    primary.close()
    until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'posture round trip completes fenced logout')
    check(replica.recv(1)==b'','posture round trip closes retained replica through World')
