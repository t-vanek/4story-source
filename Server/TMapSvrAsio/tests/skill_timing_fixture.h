#pragma once
// Synthetic equipped options on disposable characters; source charts are read-only.
void VerifyNativeSkillTiming(soci::session& admin,tmapsvr::PostgreSQLMapService& map,
                            const tmapsvr::MapSessionClaim& c,bool broken) {
    using namespace tmapsvr;
    const auto owner=std::to_string(c.char_id);
    admin<<"INSERT INTO app_world.\"TSKILLTABLE\"(\"bWorldID\",\"dwCharID\",\"wSkillID\",\"bLevel\",\"dwRemainTick\") VALUES(1,"+owner+",31,1,0),(1,"+owner+",102,2,0),(1,"+owner+",108,1,0),(1,"+owner+",736,1,0),(1,"+owner+",1329,1,0) ON CONFLICT DO NOTHING";
    const auto weapon=" WHERE \"dwOwnerID\"="+owner+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
    Check(Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\""+weapon)==1,"native timing fixture has a real starter primary weapon");
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"bMagic1\"=54,\"wValue1\"=100,\"bMagic2\"=55,\"wValue2\"=100,\"bMagic3\"=56,\"wValue3\"=100,\"dwDuraMax\"=100,\"dwDuraCur\"="+std::string(broken?"0":"100")+weapon;
    Check(Number(admin,"SELECT count(*) FROM character_compat.\"TFORMULACHART\" WHERE \"bID\" IN (4,16) AND \"dwinit\"=0 AND \"fRateX\"=0 AND \"fRateY\"=0")==2,"pinned attack formulas have original zero base; weapon values remain authoritative");
    auto info=map.LookupSession(c.user_id,c.key);Check(info&&map.ClaimSession(c,*info).has_value(),"native timing fixture claims primary");
    const auto loaded=map.LoadAuthorized(c);Check(loaded&&loaded->payload->skill_attack_timing.has_value(),"native character hydrates physical ranged and magic timing");
    const auto delay=[&](const char* slots){return Number(admin,"SELECT COALESCE(sum(t.\"dwSpeedInc\"),0) FROM app_world.\"TITEMTABLE\" i JOIN character_compat.\"TITEMCHART\" t ON t.\"wItemID\"=i.\"wItemID\" WHERE i.\"dwOwnerID\"="+owner+" AND i.\"dwStorageID\"=254 AND i.\"bItemID\" IN ("+slots+") AND (i.\"dwDuraMax\"=0 OR i.\"dwDuraCur\"<>0)");};
    const auto& timing=*loaded->payload->skill_attack_timing;
    Check(timing[0].delay==std::max(0LL,delay("0,1"))&&timing[2].delay==timing[0].delay&&timing[1].delay==std::max(0LL,delay("2")),
          "native timing uses correct weapon slots for physical ranged and magic attacks");
    Check(timing[0].rate==(broken?100:88)&&timing[1].rate==timing[0].rate&&timing[2].rate==timing[0].rate,
          broken?"broken weapon contributes neither speed nor speed magic":"source option projection supplies exact twelve percent speed reductions");
    Check(loaded->payload->skill_templates.size()==loaded->payload->skills.size(),"every learned skill has its pinned timing definition");
    bool exact=true;
    for(const auto& t:loaded->payload->skill_templates) {
        const auto id=std::to_string(std::bit_cast<std::int16_t>(t.wID));
        exact&=t.dwLoopDelay==static_cast<std::uint32_t>(Number(admin,"SELECT \"dwLoopDelay\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"="+id));
        exact&=t.wTargetActiveID==static_cast<std::uint16_t>(Number(admin,"SELECT \"wTargetActiveID\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"="+id));
        exact&=t.wPrevActiveID==static_cast<std::uint16_t>(Number(admin,"SELECT \"wPrevActiveID\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"="+id));
        exact&=t.wMapID==static_cast<std::uint16_t>(Number(admin,"SELECT \"wMapID\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"="+id));
        if(t.wID==31)exact&=t.items==SkillItemGate::Allowed;
    }
    Check(exact,"native loop delay normal/loop prerequisites and map restriction including unsigned sentinel match pinned charts");
    const auto physical=std::find_if(loaded->payload->skill_templates.begin(),loaded->payload->skill_templates.end(),[](const auto& t){return t.wID==102;});
    const auto compatible=Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\" i JOIN character_compat.\"TITEMCHART\" t ON t.\"wItemID\"=i.\"wItemID\" WHERE i.\"dwOwnerID\"="+owner+" AND i.\"dwStorageID\"=254 AND t.\"bKind\" BETWEEN 1 AND 32 AND (413 & (1::bigint << (t.\"bKind\"-1)))<>0 AND t.\"bUseItemKind\"=0 AND (i.\"dwDuraMax\"=0 OR i.\"dwDuraCur\"<>0)");
    Check(physical!=loaded->payload->skill_templates.end()&&physical->items==(compatible?SkillItemGate::Allowed:SkillItemGate::Unsuitable),
          "native loop weapon gate follows source mask and powered equipment");
    map.ReleaseSession(c);
}
