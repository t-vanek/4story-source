#include "postgresql_char_service.h"
#include "postgresql_login_owner.h"
#include "bcrypt_util.h"
#include <bcrypt/bcrypt.h>
#include <soci/soci.h>
#ifdef FOURSTORY_HAS_POSTGRESQL
#include <soci/postgresql/soci-postgresql.h>
#endif
#include <spdlog/spdlog.h>
#include <algorithm>
#include <stdexcept>

namespace tloginsvr::services {
namespace {
constexpr auto kNow = "date_trunc('minute', timezone('UTC', CURRENT_TIMESTAMP) + interval '30 seconds')";
bool ValidName(const std::string& name) {
    return name.size() >= 3 && name.size() <= 16 && std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    });
}
// Only retry an aborted serializable snapshot/deadlock. No external effects occur
// before commit; connection loss/unknown commit outcome MUST NOT be replayed.
bool Retryable(const soci::soci_error& error) {
#ifdef FOURSTORY_HAS_POSTGRESQL
    const auto* pg = dynamic_cast<const soci::postgresql_soci_error*>(&error);
    return pg && (pg->sqlstate() == "40001" || pg->sqlstate() == "40P01");
#else
    (void)error; return false;
#endif
}
}

PostgreSQLCharService::PostgreSQLCharService(fourstory::db::SessionPool& pool,
    std::string owner, std::string manifest)
    : m_pool(pool), m_owner(std::move(owner)), m_manifest(std::move(manifest)) {
    if (pool.GetBackend() != fourstory::db::Backend::PostgreSQL || m_owner.empty() ||
        m_manifest.size() != 64 || m_manifest.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Native characters require owned PostgreSQL and an explicit verified manifest");
    try {
        auto lease = m_pool.Acquire(); auto& sql = *lease;
        soci::transaction tx(sql);
        sql << "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY";
        CheckRelease(sql);
        int invalid = 0;
        sql << "SELECT count(*) FROM app_global.\"TALLCHARTABLE\" WHERE \"szName\" IS NULL", soci::into(invalid);
        if (invalid) throw std::runtime_error("Unreconciled character directory");
        soci::rowset<soci::row> rows = (sql.prepare << "SELECT \"bID\",\"bLevel\" FROM character_compat.\"TVETERANCHART\" ORDER BY \"bID\"");
        for (const auto& r : rows) {
            const auto level = static_cast<std::uint8_t>(r.get<int>(1));
            switch (r.get<int>(0)) { case 1:m_veterans.first=level;break; case 2:m_veterans.second=level;break; case 3:m_veterans.third=level;break; default:break; }
        }
        tx.commit();
    } catch (...) { throw std::runtime_error("Native character schema/catalog validation failed"); }
}
void PostgreSQLCharService::CheckRelease(soci::session& sql) const {
    int n=0;
    sql << "SELECT count(*) FROM character_compat.catalog_release WHERE manifest_sha256=:m AND status='verified'",
        soci::use(m_manifest,"m"), soci::into(n);
    if (n!=1) throw std::runtime_error("Character reference release changed or unavailable");
}
bool PostgreSQLCharService::HasWorld(soci::session& sql, int world) const {
    int n=0;
    sql << "SELECT count(*) FROM app_world.worlds w JOIN app_global.\"TGROUP\" g ON g.\"bGroupID\"=w.group_id "
           "WHERE w.group_id=:w AND g.\"bType\"=0", soci::use(world,"w"), soci::into(n);
    return n==1;
}
bool PostgreSQLCharService::LockSession(soci::session& sql, int user, std::uint32_t key) const {
    if (!key || user<=0) return false;
    int n=0; const long long wire_key=key;
    // Same lock ordering as native Authenticate; this protects limits across worlds.
    sql << "UPDATE app_global.\"TACCOUNT_PW\" SET \"dLastLogin\"=\"dLastLogin\" WHERE \"dwUserID\"=:u RETURNING 1", soci::use(user,"u"), soci::into(n);
    if (!sql.got_data()) return false;
    n=0;
    sql << "SELECT 1 FROM app_global.\"TCURRENTUSER\" s JOIN app_global.\"TUSERINFOTABLE\" i USING (\"dwUserID\") "
           "WHERE s.\"dwUserID\"=:u AND s.\"dwKEY\"=:k AND s.\"bLocked\"=0 AND s.\"dwCharID\"=0 AND i.\"bAgreement\"=1 FOR UPDATE OF s",
        soci::use(user,"u"), soci::use(wire_key,"k"), soci::into(n);
    return sql.got_data() && n==1;
}

CharacterCreateResponse PostgreSQLCharService::Create(const CharacterCreateRequest& req) {
    if (!ValidName(req.name) || req.char_class>=6 || req.race>=3 || req.sex>1)
        return {.status=CreateCharResult::Protected};
    if (req.slot>=6) return {.status=CreateCharResult::InvalidSlot};
    for (int attempt=0; attempt<3; ++attempt) {
        try { return CreateOnce(req); }
        catch (const soci::soci_error& error) {
            if (attempt<2 && Retryable(error)) continue;
#ifdef FOURSTORY_HAS_POSTGRESQL
            if (const auto* pg=dynamic_cast<const soci::postgresql_soci_error*>(&error)) {
                if (pg->sqlstate()=="23505") {
                    const std::string diagnostic=pg->what();
                    if (diagnostic.find("char_name_ci")!=std::string::npos)
                        return {.status=CreateCharResult::DuplicateName};
                    if (diagnostic.find("char_live_slot")!=std::string::npos)
                        return {.status=CreateCharResult::InvalidSlot};
                }
                spdlog::error("native char creation SQLSTATE={}",pg->sqlstate());
            }
#endif
        }
        catch (...) {}
        break;
    }
    spdlog::error("native char creation failed; transaction rolled back or commit outcome unavailable");
    return {.status=CreateCharResult::Internal};
}

CharacterCreateResponse PostgreSQLCharService::CreateOnce(const CharacterCreateRequest& req) {
    auto lease=m_pool.Acquire(); auto& sql=*lease;
    auto tx=BeginLoginTransaction(m_pool, sql, m_owner, true);
    CheckRelease(sql);
    const int world=req.group_id, uid=req.user_id, slot=req.slot, cls=req.char_class, race=req.race;
    if (!LockSession(sql,uid,req.session_key)) return {.status=CreateCharResult::Internal};
    if (!HasWorld(sql,world)) return {.status=CreateCharResult::NoGroup};
    int n=0;
    sql << "SELECT count(*) FROM app_global.\"TALLCHARTABLE\" WHERE \"dwUserID\"=:u AND \"bDelete\"=0",
        soci::use(uid,"u"), soci::into(n);
    if(n>=255) return {.status=CreateCharResult::Internal}; // backed-up BYTE output cannot represent a larger live count
    const int create_count=n+1;
    sql << "SELECT count(*) FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwUserID\"=:u AND \"bSlot\"=:s AND \"bDelete\"=0",
        soci::use(world,"w"), soci::use(uid,"u"), soci::use(slot,"s"), soci::into(n);
    if(n) return {.status=CreateCharResult::InvalidSlot};
    // Native supported names are ASCII. C collation lower() implements their
    // source case-insensitive equality; arbitrary Unicode equivalence is not claimed.
    sql << R"SQL(SELECT count(*) FROM (
        SELECT "szName" AS name FROM app_global."TALLCHARTABLE"
        UNION ALL SELECT "szName" FROM character_compat."TNPCCHART"
        UNION ALL SELECT "szName" FROM character_compat."TMONSTERCHART"
        UNION ALL SELECT "szName" FROM app_global."TRESERVEDNAME" WHERE "dwUserID" IS DISTINCT FROM :u
        ) names WHERE lower(rtrim(name) COLLATE "C")=lower(:name COLLATE "C"))SQL",
        soci::use(uid,"u"), soci::use(req.name,"name"), soci::into(n);
    if(n) return {.status=CreateCharResult::DuplicateName};
    sql << "SELECT count(*) FROM app_global.\"TKEEPINGNAME\" WHERE lower(:name COLLATE \"C\") LIKE lower(\"szName\" COLLATE \"C\")",
        soci::use(req.name,"name"), soci::into(n);
    if(n) return {.status=CreateCharResult::DuplicateName};
    int country=req.country, original=4, level=1, exp=1, skill_points=0, spawn=15003;
    // Backup TOP 1 has no ordering. Refuse contradictory country evidence instead
    // of depending on a database planner's row order.
    int countries=0;
    sql << "SELECT count(DISTINCT \"bOriCountry\"),COALESCE(min(\"bOriCountry\"),4) FROM app_world.\"TCHARTABLE\" "
           "WHERE \"bWorldID\"=:w AND \"dwUserID\"=:u AND \"bDelete\"=0 AND \"bOriCountry\"<2",
        soci::use(world,"w"), soci::use(uid,"u"), soci::into(countries), soci::into(original);
    if(countries>1) return {.status=CreateCharResult::Internal};
    if(req.level_option) {
        const int option=req.level_option;
        sql << "SELECT v.\"bLevel\",l.\"dwEXP\" FROM character_compat.\"TVETERANCHART\" v "
               "JOIN character_compat.\"TLEVELCHART\" l ON l.\"bLevel\"=v.\"bLevel\"-1 WHERE v.\"bID\"=:o",
            soci::use(option,"o"), soci::into(level), soci::into(exp);
        if(!sql.got_data()) return {.status=CreateCharResult::Protected};
        country=original; skill_points=200; spawn=country==0?15001:(country==1?15002:15003);
    } else if(country!=4) return {.status=CreateCharResult::Protected};
    int hp=0,mp=0;
    sql << "SELECT 7*(2+c.\"wCON\"+r.\"wCON\")+1,9*(2+c.\"wMEN\"+r.\"wMEN\")+1 "
           "FROM character_compat.\"TCLASSCHART\" c CROSS JOIN character_compat.\"TRACECHART\" r "
           "WHERE c.\"bClassID\"=:c AND r.\"bRaceID\"=:r", soci::use(cls,"c"), soci::use(race,"r"), soci::into(hp), soci::into(mp);
    if(!sql.got_data()) throw std::runtime_error("Missing starter attributes");
    soci::values v;
    v.set("w",world);v.set("u",uid);v.set("s",slot);v.set("name",req.name);v.set("c",cls);v.set("r",race);
    v.set("country",country);v.set("ori",original);v.set("sex",int(req.sex));v.set("hair",int(req.hair));v.set("face",int(req.face));
    v.set("body",int(req.body));v.set("pants",int(req.pants));v.set("hand",int(req.hand));v.set("foot",int(req.foot));
    v.set("level",level);v.set("exp",exp);v.set("hp",hp);v.set("mp",mp);v.set("points",skill_points);v.set("spawn",spawn);
    int id=0;
    sql << R"SQL(INSERT INTO app_world."TCHARTABLE"("bWorldID","dwUserID","bSlot","szNAME","bClass","bRace","bCountry",
        "bOriCountry","bSex","bRealSex","bHair","bFace","bBody","bPants","bHand","bFoot","bLevel","dwEXP","dwHP","dwMP",
        "wSkillPoint","dwGold","dwSilver","dwCooper","wMapID","wSpawnID","wTemptedMon","bAftermath","fPosX","fPosY","fPosZ","wDIR")
        VALUES(:w,:u,:s,:name,:c,:r,:country,:ori,:sex,0,:hair,:face,:body,:pants,:hand,:foot,:level,:exp,:hp,:mp,:points,
        0,0,0,2010,:spawn,0,0,3664.405,86.16578,557.2542,762) RETURNING "dwCharID")SQL", soci::use(v), soci::into(id);
    int directory_id=0;
    sql << "INSERT INTO app_global.\"TALLCHARTABLE\"(\"bWorldID\",\"dwUserID\",\"dwCharID\",\"szName\") "
           "VALUES(:w,:u,:id,:name) ON CONFLICT DO NOTHING RETURNING \"dwCharID\"",
        soci::use(world,"w"),soci::use(uid,"u"),soci::use(id,"id"),soci::use(req.name,"name"),soci::into(directory_id);
    if(!sql.got_data()) return {.status=CreateCharResult::DuplicateName};
    sql << "INSERT INTO app_world.\"TINVENTABLE\"(\"bWorldID\",\"dwCharID\",\"bInvenID\",\"wItemID\",\"dEndTime\") "
           "VALUES(:w,:id,255,3,'1900-01-01'),(:w,:id,254,2,'1900-01-01')",soci::use(world,"w"),soci::use(id,"id");
    sql << "INSERT INTO app_world.\"TTITLETABLE\" VALUES(:w,:id,0,1)",soci::use(world,"w"),soci::use(id,"id");
    sql << "INSERT INTO app_world.\"TCABINETTABLE\" VALUES(:w,:id,0,1)",soci::use(world,"w"),soci::use(id,"id");
    sql << "INSERT INTO app_world.\"TSKILLTABLE\" SELECT :w,:id,\"wSkillID\",\"bLevel\",0 FROM character_compat.\"TSTARTSKILL\" WHERE \"bClassID\"=:c",
        soci::use(world,"w"),soci::use(id,"id"),soci::use(cls,"c");
    std::string hotkey_columns="\"bInvenID\"";
    for(int i=1;i<=12;++i) hotkey_columns+=",\"bType"+std::to_string(i)+"\",\"wID"+std::to_string(i)+"\"";
    sql << "INSERT INTO app_world.\"THOTKEYTABLE\" SELECT :w,:id,"+hotkey_columns+
           " FROM character_compat.\"TSTARTHOTKEY\" WHERE \"bClassID\"=:c",soci::use(world,"w"),soci::use(id,"id"),soci::use(cls,"c");
    int item_count=0;
    sql << "SELECT count(*) FROM character_compat.\"TSTARTITEMCHART\" WHERE \"bCountry\"=:country AND \"bClass\"=:c",
        soci::use(country,"country"),soci::use(cls,"c"),soci::into(item_count);
    if(!item_count) throw std::runtime_error("Empty starter inventory contract");
    long long item_end=0;
    sql << "UPDATE app_world.worlds SET item_high_water=item_high_water+:n WHERE group_id=:w "
           "AND item_high_water::numeric+:n < (item_world::numeric+1)*72057594037927936 RETURNING item_high_water",
        soci::use(item_count,"n"),soci::use(world,"w"),soci::into(item_end);
    if(!sql.got_data()) throw std::runtime_error("Item identity range exhausted");
    const long long item_begin=item_end-item_count;
    std::string item_columns, item_select;
    for(const auto* prefix:{"bMagic","wValue","dwTime"}) for(int i=1;i<=6;++i) {
        const std::string col="\""+std::string(prefix)+std::to_string(i)+"\"";
        item_columns+=","+col;item_select+=",CASE WHEN s.\"bChartType\"=1 THEN 0 ELSE q."+col+" END";
    }
    sql << R"SQL(INSERT INTO app_world."TITEMTABLE"("bWorldID","dlID","bStorageType","dwStorageID","bOwnerType","dwOwnerID",
        "bItemID","wItemID","bLevel","bCount","bGLevel","dwDuraMax","dwDuraCur","bRefineCur","dEndTime","bGradeEffect")SQL"+
        item_columns+R"SQL(,"bGem","wMoggItemID") SELECT :w,:base+row_number() OVER(ORDER BY s."bInven",s."bSlot"),0,s."bInven",0,:id,s."bSlot",
        CASE WHEN s."bChartType"=1 THEN s."wItemID" ELSE q."wItemID" END,
        CASE WHEN s."bChartType"=1 THEN 0 ELSE q."bLevel" END,s."bCount",
        CASE WHEN s."bChartType"=1 THEN 0 ELSE q."bGLevel" END,
        CASE WHEN s."bChartType"=1 THEN i."dwDuraMax" ELSE q."dwDuraMax" END,
        CASE WHEN s."bChartType"=1 THEN i."dwDuraMax" ELSE q."dwDuraCur" END,
        CASE WHEN s."bChartType"=1 THEN 0 ELSE q."bRefineCur" END,TIMESTAMP '1900-01-01',
        CASE WHEN s."bChartType"=1 THEN 0 ELSE q."bGradeEffect" END)SQL"+item_select+
        R"SQL(,0,0 FROM character_compat."TSTARTITEMCHART" s LEFT JOIN character_compat."TITEMCHART" i ON i."wItemID"=s."wItemID"
        LEFT JOIN character_compat."TQUESTITEMCHART" q ON q."dwID"=s."wItemID" WHERE s."bCountry"=:country AND s."bClass"=:c)SQL",
        soci::use(world,"w"),soci::use(item_begin,"base"),soci::use(id,"id"),soci::use(country,"country"),soci::use(cls,"c");
    // Exact ASCII + CRLF payload/prefixes from the backup's TCreateChar/TSavePost.
    const std::string title="00000013Welcome to 4StoryPW!";
    const std::string message="00000065Welcome to 4StoryPW,\r\n\tif you find some bugs please report them in our Forum.\r\n\r\n\tYour 4StoryPW Team!";
    sql << "INSERT INTO app_world.\"TPOSTTABLE\"(\"bWorldID\",\"dwCharID\",\"szRecvName\",\"bType\",\"bRead\",\"timeRecv\",\"dwSendID\",\"szSender\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\") "
           "VALUES(:w,:id,:name,0,0,"+std::string(kNow)+",0,'Mysterious helper',:title,:message,9999,0,0)",
        soci::use(world,"w"),soci::use(id,"id"),soci::use(req.name,"name"),soci::use(title,"title"),soci::use(message,"message");
    sql << R"SQL(INSERT INTO app_world."TRECALLMONTABLE"("bWorldID","dwOwnerID","wMonID","wPetID","dwATTR","bLevel","dwHP","dwMP","bSkillLevel","wPosX","wPosY","wPosZ","dwTime")
        SELECT :w,:id,s."wMonID",0,m."wSummonAttr"+65536,1,a."dwMaxHP",a."dwMaxMP",1,3664,86,557,0
        FROM character_compat."TSTARTRECALL" s JOIN character_compat."TMONSTERCHART" m ON m."wID"=s."wMonID"
        JOIN character_compat."TMONATTRCHART" a ON a."wID"=m."wSummonAttr" AND a."bLevel"=1
        WHERE s."bClassID"=:c AND s."bCountryID"=:country AND s."wMonID">0)SQL",
        soci::use(world,"w"),soci::use(id,"id"),soci::use(cls,"c"),soci::use(country,"country");
    sql << "UPDATE app_world.\"TCHARTABLE\" SET \"wTemptedMon\"=r.\"wMonID\" FROM app_world.\"TRECALLMONTABLE\" r "
           "WHERE \"dwCharID\"=:id AND \"TCHARTABLE\".\"bWorldID\"=:w AND r.\"dwOwnerID\"=:id AND r.\"bWorldID\"=:w",
        soci::use(id,"id"),soci::use(world,"w");
    sql << "INSERT INTO app_world.\"TPETTABLE\"(\"bWorldID\",\"dwUserID\",\"wPetID\",\"szName\",\"timeUse\") "
           "VALUES(:w,:u,2,replace(:name || '''s Mount','s''s','s'''),'1900-01-01') "
           "ON CONFLICT(\"bWorldID\",\"dwUserID\",\"wPetID\") DO UPDATE SET \"szName\"=EXCLUDED.\"szName\",\"timeUse\"=EXCLUDED.\"timeUse\",\"bEffect\"=NULL",
        soci::use(world,"w"),soci::use(uid,"u"),soci::use(req.name,"name");
    tx->commit();
    return {.status=CreateCharResult::Success,.char_id=id,.create_count=static_cast<std::uint8_t>(create_count),.starting_level=static_cast<std::uint8_t>(level)};
}

std::vector<CharacterInfo> PostgreSQLCharService::List(std::int32_t user, std::uint8_t group) {
    try {
        auto lease=m_pool.Acquire(); auto& sql=*lease;
        soci::transaction tx(sql);sql << "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY";
        sql << "SET LOCAL statement_timeout='5s'";
        CheckRelease(sql);const int world=group;
        if(!HasWorld(sql,world)) return {};
        std::vector<CharacterInfo> out;
        {
            soci::rowset<soci::row> rows=(sql.prepare << R"SQL(SELECT c."dwCharID",c."szNAME",c."bStartAct",c."bSlot",c."bLevel",
                c."bClass",c."bRace",c."bCountry",c."bSex",c."bHair",c."bFace",c."bBody",c."bPants",c."bHand",c."bFoot",
                c."dwRegion",c."bHelmetHide",COALESCE(g.fame,0),COALESCE(g.fame_color,0)
                FROM app_world."TCHARTABLE" c LEFT JOIN app_world.guild_membership g ON g.world_id=c."bWorldID" AND g.char_id=c."dwCharID"
                WHERE c."dwUserID"=:u AND c."bWorldID"=:w AND c."bDelete"=0 ORDER BY c."bSlot")SQL",soci::use(user,"u"),soci::use(world,"w"));
            for(const auto& r:rows) {
                CharacterInfo c;c.char_id=r.get<int>(0);c.name=r.get<std::string>(1);
                std::uint8_t* fields[]={&c.start_act,&c.slot,&c.level,&c.char_class,&c.race,&c.country,&c.sex,&c.hair,&c.face,&c.body,&c.pants,&c.hand,&c.foot};
                for(int i=0;i<13;++i)*fields[i]=static_cast<std::uint8_t>(r.get<int>(i+2));
                c.region=static_cast<std::uint32_t>(r.get<int>(15));c.helmet_hide=static_cast<std::uint8_t>(r.get<int>(16));
                c.fame=static_cast<std::uint32_t>(r.get<int>(17));c.fame_color=static_cast<std::uint32_t>(r.get<int>(18));
                out.push_back(std::move(c));
            }
        }
        for(auto& c:out) {
            soci::rowset<soci::row> items=(sql.prepare << R"SQL(SELECT "bItemID","wItemID","bLevel","bGradeEffect","dwTime3","dwTime6","dwTime4","wMoggItemID"
                FROM app_world."TITEMTABLE" WHERE "bWorldID"=:w AND "dwOwnerID"=:id AND "bOwnerType"=0 AND "bStorageType"=0 AND "dwStorageID"=254 ORDER BY "bItemID")SQL",
                soci::use(world,"w"),soci::use(c.char_id,"id"));
            for(const auto& r:items) c.items.push_back({static_cast<std::uint8_t>(r.get<int>(0)),static_cast<std::uint16_t>(r.get<int>(1)),
                static_cast<std::uint8_t>(r.get<int>(2)),static_cast<std::uint8_t>(r.get<int>(3)),static_cast<std::uint16_t>(r.get<int>(4)),
                static_cast<std::uint16_t>(r.get<int>(5)),static_cast<std::uint8_t>(r.get<int>(6)),static_cast<std::uint16_t>(r.get<int>(7))});
        }
        tx.commit();return out;
    } catch (...) { throw std::runtime_error("Native character list unavailable"); }
}

DeleteCharResult PostgreSQLCharService::DeleteAuthorized(std::int32_t user,std::uint8_t group,std::int32_t character,
    const std::string& password,std::uint32_t key) {
    for(int attempt=0;attempt<3;++attempt) try {
        auto lease=m_pool.Acquire();auto& sql=*lease;
        auto tx=BeginLoginTransaction(m_pool,sql,m_owner,true);CheckRelease(sql);const int world=group;
        if(!LockSession(sql,user,key)) return DeleteCharResult::Internal;
        if(!HasWorld(sql,world)) return DeleteCharResult::NoGroup;
        // Recheck under the account lock; an upstream password check alone can
        // race a credential rotation. Delete wire contains SHA1(form password).
        std::string stored;
        sql << "SELECT \"szPasswd\" FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u",soci::use(user,"u"),soci::into(stored);
        if(!bcrypt_util::IsBcrypt(stored) || password.size()>64 || password.find('\0')!=std::string::npos ||
           bcrypt_checkpw(password.c_str(),stored.c_str())!=0) return DeleteCharResult::InvalidPassword;
        int level=0;
        sql << "SELECT \"bLevel\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwUserID\"=:u AND \"dwCharID\"=:id AND \"bDelete\"=0 FOR UPDATE",
            soci::use(world,"w"),soci::use(user,"u"),soci::use(character,"id"),soci::into(level);
        if(!sql.got_data()) return DeleteCharResult::Failed;
        int guild=0;
        sql << "SELECT count(*) FROM app_world.guild_membership WHERE world_id=:w AND char_id=:id",soci::use(world,"w"),soci::use(character,"id"),soci::into(guild);
        if(guild) return DeleteCharResult::Failed;
        if(level>5) {
            sql << "UPDATE app_world.\"TCHARTABLE\" SET \"bDelete\"=1,\"dDeleteDate\"="+std::string(kNow)+" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:id",
                soci::use(world,"w"),soci::use(character,"id");
            sql << "UPDATE app_global.\"TALLCHARTABLE\" SET \"bDelete\"=1,\"dDeleteDate\"="+std::string(kNow)+" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:id",
                soci::use(world,"w"),soci::use(character,"id");
        } else {
            // Every implemented character child has a composite FK with cascade.
            // Titles are cleaned too (the source leaves orphan titles). Account pets
            // stay, matching the source's account-owned mount semantics.
            sql << "DELETE FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:id",soci::use(world,"w"),soci::use(character,"id");
            sql << "DELETE FROM app_global.\"TALLCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:id",soci::use(world,"w"),soci::use(character,"id");
        }
        tx->commit();return DeleteCharResult::Success;
    } catch(const soci::soci_error& e) { if(attempt<2 && Retryable(e))continue;break; }
      catch(...) { break; }
    spdlog::error("native char deletion failed; transaction rolled back or commit outcome unavailable");
    return DeleteCharResult::Internal;
}
}
