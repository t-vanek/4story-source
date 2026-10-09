"""Independent original-source stat oracle and actual native Map TCP checks.

Arithmetic transcribed from TObjBase.cpp:1409-2238, TItem.cpp:321-462,
TMapSvr.cpp:6833 and CSSender.cpp:3241; no modern calculator/encoder is called.
All chart reads use the verified backup releases. Changes below affect only the
owned synthetic character's mutable rows while it is disconnected.
"""
from collections import defaultdict
import math
import select
import struct
from verify_login_wire import frame, read_packet


def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def source_statistics(conn,cid):
    def rows(schema,table,where='',args=()):
        return [r[0] for r in conn.execute(f'SELECT row_to_json(t) FROM {schema}."{table}" t '+where,args)]
    ch=rows('app_world','TCHARTABLE','WHERE "dwCharID"=%s',(cid,))[0]
    race=rows('character_compat','TRACECHART','WHERE "bRaceID"=%s',(ch['bRace'],))[0]
    klass=rows('character_compat','TCLASSCHART','WHERE "bClassID"=%s',(ch['bClass'],))[0]
    formula={r['bID']:r for r in rows('character_compat','TFORMULACHART')}
    primary_names=['wSTR','wDEX','wCON','wINT','wWIS','wMEN']
    unscaled=[1+race[n]+klass[n] for n in primary_names]
    growth=f32(formula[34]['fRateX']);magic=defaultdict(int);attrs=defaultdict(int);delay=[0,0,0]
    grades={r['bLevel']:r['bGrade'] for r in rows('actor_compat','TITEMGRADECHART')}
    attributes={r['wID']&65535:r for r in rows('actor_compat','TITEMATTRCHART')}
    magic_chart={r['bMagic']:r for r in rows('actor_compat','TITEMMAGICCHART')}
    for item in rows('app_world','TITEMTABLE','WHERE "dwOwnerID"=%s AND "dwStorageID"=254',(cid,)):
        if item['dwDuraMax'] and not item['dwDuraCur']:continue
        template=rows('character_compat','TITEMCHART','WHERE "wItemID"=%s',(item['wItemID'],))[0]
        for i in range(1,7):
            mid=item[f'bMagic{i}'];value=item[f'wValue{i}']&0xffffffff
            if not mid or not value:continue
            m=magic_chart[mid];rev=m['bRvType']
            revision=f32(template[['fRevision','fMRevision','fAtRate','fMAtRate'][rev-1]]) if rev else 1.
            magic[mid]+=max((int(f32(f32(revision*value)*(m['wMaxValue']&65535)))&65535)//100,1)
        a=attributes.get(((template['wAttrID']&65535)+grades.get(item['bLevel'],0)+item['bGem'])&65535,attributes[min(attributes)])
        typ=template['bType']
        if typ==1:
            for col,key in [('wMinAP','minp'),('wMaxAP','maxp'),('wMinMAP','minm'),('wMaxMAP','maxm')]:attrs[key]+=a[col]&65535
        if typ==4:attrs['minl']+=a['wMinAP']&65535;attrs['maxl']+=a['wMaxAP']&65535
        if typ!=6:attrs['dp']+=a['wDP']&65535;attrs['mdp']+=a['wMDP']&65535
        if item['bItemID'] in (0,1):delay[0]+=template['dwSpeedInc'];delay[2]+=template['dwSpeedInc']
        if item['bItemID']==2:delay[1]+=template['dwSpeedInc']
    passive=[]
    for skill in rows('app_world','TSKILLTABLE','WHERE "dwCharID"=%s',(cid,)):
        template=rows('character_compat','TSKILLCHART','WHERE "wID"=%s',(skill['wSkillID'],))[0]
        data=rows('character_compat','TSKILLDATA','WHERE "wSkillID"=%s',(skill['wSkillID'],))
        if not any(d['bAction']==1 for d in data):continue
        for d in data:
            if d['bAction'] not in (1,4) or d['bType']!=1:continue
            value=d['wValue']&65535;inc=d['wValueInc']&65535;rank=skill['bLevel']
            if d['bCalc']==1:value+=(rank-1)*inc
            elif d['bCalc']==2:value=int(value*math.pow(growth,template['bLevel']+(rank-1)*template['bNextLevel'] if rank else 0)/100)
            elif d['bCalc']==3:value-=(rank-1)*inc
            passive.append((d['bExec'],d['bInc'],value))
    def delta(base,typ):
        total=0
        for target,op,value in passive:
            if target!=typ:continue
            if op==1:total+=value
            elif op==2:total-=value
            elif op==3:total+=base*value-base
            elif op==4:total+=base//max(1,value)-base
            elif op==5:total+=int(base*value/100.)-base
        return total
    def primary(i,floor=0):
        value=f32(max(floor,unscaled[i]&65535)*math.pow(growth,ch['bLevel']-1))
        if ch['bLevel']>=10:value=f32(value-f32(f32(value*f32(min(ch['bAftermath'],100)*.3))/100))
        value=f32(value+magic[i+1]);return f32(value+delta(int(value),i+1))
    def modified(base,typ):return max(0,base+delta(base,typ))
    def second(fid,i=None,floor=False):
        f=formula[fid];x=f32(f['fRateX']);y=f32(f['fRateY'])
        if fid in (12,23):return f['dwinit']+int(math.pow(y,ch['bLevel'])*x)
        if floor:return int(f32(primary(i,int(y))*x))
        if fid in (7,18):return f['dwinit']+int(f32(unscaled[i]*x))
        if fid==17:return int(f32(f32(unscaled[5]*x)-y))
        if fid==24:return f['dwinit']
        return f['dwinit']+int(f32(primary(i)*x))
    def attack(fid,i,typ,lo,hi,mi,ma):
        b=second(fid,i)+magic[typ]
        low=modified(b+attrs[lo]+magic[mi],typ);high=modified(b+attrs[hi]+magic[ma],typ)
        return min(low,high),high
    minp,maxp=attack(1,0,7,'minp','maxp',61,62)
    minl,maxl=attack(3,1,9,'minl','maxl',63,64)
    minm,maxm=attack(13,3,17,'minm','maxm',65,66)
    for i in range(3):
        f=formula[16 if i==2 else 4];st=4 if i==2 else 1
        delay[i]=max(0,delay[i]+int(min(f['dwinit'],max(f32(f32(f['fRateY'])-f32(unscaled[st]*f32(f['fRateX']))),0))))
    rates=[(modified(100,t)*(100-min(magic[t],100))&0xffffffff)//100 for t in (54,55,56)]
    # Literal source/client layout, not the modern serializer.
    fields=[cid,*[int(primary(i))&65535 for i in range(6)],minp,maxp,modified(second(12)+attrs['dp']+magic[8],8),minl,maxl,
            *delay,*rates,modified(second(5,1)+magic[11],11)&65535,modified(second(6,1,True)+magic[12],12)&65535,
            modified(second(7,1)+magic[13],13)&255,minm,maxm,modified(second(23)+attrs['mdp']+magic[16],16),
            modified(second(28,4)+magic[86],86)&65535,modified(second(29,4,True)+magic[87],87)&65535,
            modified(second(24)+magic[19],19)&255,modified(second(17)+magic[20],20)&255,modified(second(18,4)+magic[21],21)&255,
            ch['wSkillPoint']&65535,ch['bAftermath']]
    return struct.pack('<I6H11I2HB3I2H3BHB',*fields)


def verify_character_statistics(conn,cid,start,enter,login_port,map_port,until):
    checks=[];client=None
    def check(ok,label):
        if not ok:raise RuntimeError('Character statistics: '+label)
        checks.append(label)
    baseline=conn.execute('SELECT "bLevel","bAftermath" FROM app_world."TCHARTABLE" WHERE "dwCharID"=%s',(cid,)).fetchone()
    weapon=conn.execute('SELECT "dlID","bLevel","bGem","dwDuraMax","dwDuraCur" FROM app_world."TITEMTABLE" WHERE "dwOwnerID"=%s AND "dwStorageID"=254 AND "bItemID"=0',(cid,)).fetchone()
    try:
        for case in ('source starter','grade and aftermath','broken weapon'):
            if case!='source starter':
                conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=20,"bAftermath"=95 WHERE "dwCharID"=%s',(cid,))
                conn.execute('UPDATE app_world."TITEMTABLE" SET "bLevel"=7,"bGem"=2,"dwDuraMax"=100,"dwDuraCur"=%s WHERE "dlID"=%s',(0 if case=='broken weapon' else 100,weapon[0]))
            expected=source_statistics(conn,cid)
            _,key=start(login_port,cid);client,char=enter(map_port,cid,key)
            client.sendall(frame(struct.pack('<I',cid),0x5323,3))
            actual=read_packet(client,4)
            check(actual==(0x5324,expected),case+': all 87 bytes match independent original-source calculation')
            before=conn.execute('SELECT count(*) FROM app_world.inventory_stack_changes WHERE char_id=%s',(cid,)).fetchone()
            client.sendall(frame(struct.pack('<I',0xffffffff),0x5323,4))
            check(not select.select([client],[],[],.15)[0],case+': absent target produces no fabricated success')
            client.sendall(frame(struct.pack('<I',cid),0x5323,5))
            check(read_packet(client,5)==(0x5324,expected),case+': repeated inspect is stable after missing remote target')
            check(conn.execute('SELECT count(*) FROM app_world.inventory_stack_changes WHERE char_id=%s',(cid,)).fetchone()==before,case+': read-only inspect creates no item transaction')
            client.sendall(frame(struct.pack('<IB',cid,0),0x5323,6))
            check(client.recv(1)==b'',case+': trailing forged request bytes close the client')
            client.close();client=None
            until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'stat test disconnect saves and releases ownership')
    finally:
        if client is not None:client.close()
        until(lambda:conn.execute('SELECT count(*) FROM app_world.map_sessions WHERE char_id=%s',(cid,)).fetchone()==(0,),'stat fixture is offline before restoring mutable rows')
        conn.execute('UPDATE app_world."TCHARTABLE" SET "bLevel"=%s,"bAftermath"=%s WHERE "dwCharID"=%s',(*baseline,cid))
        conn.execute('UPDATE app_world."TITEMTABLE" SET "bLevel"=%s,"bGem"=%s,"dwDuraMax"=%s,"dwDuraCur"=%s WHERE "dlID"=%s',(*weapon[1:],weapon[0]))
    return {'status':'passed','checks':checks,'source':'TObjBase/TItem original code + pinned backup charts; synthetic TCP, real client unavailable'}
