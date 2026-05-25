#pragma once

// Character DB-row entity + EntityMapping<> for TCHARTABLE.
//
// CharRow is the persistence shape (DB-natural int32/double/string types);
// the domain CharSnapshot (domain/character.h) is reached via the
// Automapper profile in mapper_profiles.h, which narrows int32→uint8/16/32
// and double→float through fourstory::mapper::Convert:
//
//   TCHARTABLE row ── CharRow ─(Adapt)→ CharSnapshot
//
// Read path: FromRow + SelectAllSql/SelectByIdSql back
// Repository<CharRow>::FindById, replacing ~40 lines of hand-written
// soci::into + db::Narrow* copies. The save path stays a targeted named-
// param UPDATE (it doesn't fit generic full-row CRUD), routed through
// DbContext::Session() — same split as TWorldSvrAsio's guild repo.

#include "fourstory/db/orm/entity_mapping.h"

#include <soci/soci.h>

#include <cstdint>
#include <string>

namespace tmapsvr {

// One TCHARTABLE row — the columns queries::CharByCharId selects.
struct CharRow
{
    std::int32_t char_id          = 0;
    std::string  name;
    std::int32_t start_act        = 0;
    std::int32_t real_sex         = 0;
    std::int32_t klass            = 0;
    std::int32_t level            = 1;
    std::int32_t race             = 0;
    std::int32_t country          = 0;
    std::int32_t ori_country      = 0;
    std::int32_t sex              = 0;
    std::int32_t hair             = 0;
    std::int32_t face             = 0;
    std::int32_t body             = 0;
    std::int32_t pants            = 0;
    std::int32_t hand             = 0;
    std::int32_t foot             = 0;
    std::int32_t helmet_hide      = 0;
    std::int32_t gold             = 0;
    std::int32_t silver           = 0;
    std::int32_t cooper           = 0;
    std::int32_t exp              = 0;
    std::int32_t hp               = 1;
    std::int32_t mp               = 1;
    std::int32_t skill_point      = 0;
    std::int32_t region           = 0;
    std::int32_t guild_leave      = 0;
    std::int32_t guild_leave_time = 0;
    std::int32_t map              = 0;
    std::int32_t spawn            = 0;
    std::int32_t last_spawn       = 0;
    std::int32_t last_dest        = 0;
    std::int32_t tempted_mon      = 0;
    std::int32_t aftermath        = 0;
    double       pos_x            = 0.0;
    double       pos_y            = 0.0;
    double       pos_z            = 0.0;
    std::int32_t dir              = 0;
    std::int32_t stat_level       = 0;
    std::int32_t stat_point       = 0;
    std::int32_t stat_exp         = 0;
};

} // namespace tmapsvr

namespace fourstory::db::orm {

template<>
struct EntityMapping<tmapsvr::CharRow>
{
    using T = tmapsvr::CharRow;

    static constexpr const char* Table    = "TCHARTABLE";
    static constexpr const char* PkColumn = "dwCharID";

    static T FromRow(const soci::row& r)
    {
        T c;
        c.char_id          = r.get<int>("dwCharID");
        // szNAME is the one nullable column the legacy load guarded with
        // an indicator; the by-name default overload reproduces that.
        c.name             = r.get<std::string>("szNAME", std::string{});
        c.start_act        = r.get<int>("bStartAct");
        c.real_sex         = r.get<int>("bRealSex");
        c.klass            = r.get<int>("bClass");
        c.level            = r.get<int>("bLevel");
        c.race             = r.get<int>("bRace");
        c.country          = r.get<int>("bCountry");
        c.ori_country      = r.get<int>("bOriCountry");
        c.sex              = r.get<int>("bSex");
        c.hair             = r.get<int>("bHair");
        c.face             = r.get<int>("bFace");
        c.body             = r.get<int>("bBody");
        c.pants            = r.get<int>("bPants");
        c.hand             = r.get<int>("bHand");
        c.foot             = r.get<int>("bFoot");
        c.helmet_hide      = r.get<int>("bHelmetHide");
        c.gold             = r.get<int>("dwGold");
        c.silver           = r.get<int>("dwSilver");
        c.cooper           = r.get<int>("dwCooper");
        c.exp              = r.get<int>("dwEXP");
        c.hp               = r.get<int>("dwHP");
        c.mp               = r.get<int>("dwMP");
        c.skill_point      = r.get<int>("wSkillPoint");
        c.region           = r.get<int>("dwRegion");
        c.guild_leave      = r.get<int>("bGuildLeave");
        c.guild_leave_time = r.get<int>("dwGuildLeaveTime");
        c.map              = r.get<int>("wMapID");
        c.spawn            = r.get<int>("wSpawnID");
        c.last_spawn       = r.get<int>("wLastSpawnID");
        c.last_dest        = r.get<int>("dwLastDestination");
        c.tempted_mon      = r.get<int>("wTemptedMon");
        c.aftermath        = r.get<int>("bAftermath");
        // Positions bind through double — ODBC's REAL → float path is
        // fragile on some drivers (legacy note preserved).
        c.pos_x            = r.get<double>("fPosX");
        c.pos_y            = r.get<double>("fPosY");
        c.pos_z            = r.get<double>("fPosZ");
        c.dir              = r.get<int>("wDIR");
        c.stat_level       = r.get<int>("bStatLevel");
        c.stat_point       = r.get<int>("bStatPoint");
        c.stat_exp         = r.get<int>("dwStatExp");
        return c;
    }

    static std::string SelectAllSql()
    {
        return "SELECT dwCharID, szNAME, bStartAct, bRealSex, bClass, bLevel, "
               "  bRace, bCountry, bOriCountry, bSex, bHair, bFace, bBody, "
               "  bPants, bHand, bFoot, bHelmetHide, dwGold, dwSilver, dwCooper, "
               "  dwEXP, dwHP, dwMP, wSkillPoint, dwRegion, bGuildLeave, "
               "  dwGuildLeaveTime, wMapID, wSpawnID, wLastSpawnID, "
               "  dwLastDestination, wTemptedMon, bAftermath, fPosX, fPosY, "
               "  fPosZ, wDIR, bStatLevel, bStatPoint, dwStatExp "
               "FROM TCHARTABLE";
    }
    // Preserves the legacy load filter (active, non-deleted char).
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE dwCharID = :pk AND bDelete = 0";
    }
    static std::string DeleteSql()
    {
        return "DELETE FROM TCHARTABLE WHERE dwCharID = :pk";
    }

    // Write path is a targeted named-param UPDATE in SociPlayerService
    // (queries::SaveCharTchart) — not generic full-row CRUD.
    static std::string InsertSql() { return ""; }
    static std::string UpdateSql() { return ""; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static int  GetPk(const T& c) { return c.char_id; }
};

} // namespace fourstory::db::orm
