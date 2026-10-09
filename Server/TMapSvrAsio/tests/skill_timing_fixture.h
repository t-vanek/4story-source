#pragma once
// Synthetic equipped options on disposable characters; source charts are read-only.
void VerifyNativeSkillTiming(soci::session& admin,tmapsvr::PostgreSQLMapService& map,
                            const tmapsvr::MapSessionClaim& c,bool broken) {
    using namespace tmapsvr;
    const auto owner=std::to_string(c.char_id);
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
    map.ReleaseSession(c);
}
