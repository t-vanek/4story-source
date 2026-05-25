#include "soci_player_service.h"

#include "services/char_entities.h"
#include "db/queries.h"

#include "fourstory/db/orm/db_context.h"
#include "fourstory/mapper/mapper.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

using fourstory::db::orm::DbContext;
using fourstory::mapper::Adapt;

SociPlayerService::SociPlayerService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::optional<CharSnapshot>
SociPlayerService::LoadChar(std::uint32_t char_id)
{
    try
    {
        // ORM read: Repository<CharRow>::FindById runs the SELECT (with
        // the legacy `bDelete = 0` filter baked into SelectByIdSql) and
        // maps the row → CharRow; the Automapper then narrows it into the
        // domain CharSnapshot. nullopt = no row / lookup error.
        DbContext ctx(m_pool);
        auto row = ctx.Set<CharRow>().FindById(static_cast<int>(char_id));
        if (!row)
            return std::nullopt;
        return Adapt<CharSnapshot>(*row);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_player_service: LoadChar({}) threw: {}",
            char_id, ex.what());
        return std::nullopt;
    }
}

void SociPlayerService::SaveChar(const CharSnapshot& s)
{
    // Cast all narrow integer fields to int so SOCI ODBC
    // doesn't complain about tinyint / smallint mismatches.
    const int cid         = static_cast<int>(s.dwCharID);
    const int level       = s.bLevel;
    const int exp         = static_cast<int>(s.dwEXP);
    const int hp          = static_cast<int>(s.dwHP);
    const int mp          = static_cast<int>(s.dwMP);
    const int gold        = static_cast<int>(s.dwGold);
    const int silver      = static_cast<int>(s.dwSilver);
    const int cooper      = static_cast<int>(s.dwCooper);
    const int sp          = s.wSkillPoint;
    const int region      = static_cast<int>(s.dwRegion);
    const int guild_leave = s.bGuildLeave;
    const int guild_leave_time = static_cast<int>(s.dwGuildLeaveTime);
    const int map         = s.wMapID;
    const int spawn       = s.wSpawnID;
    const int last_spawn  = s.wLastSpawnID;
    const int last_dest   = static_cast<int>(s.dwLastDestination);
    const int tempted_mon = s.wTemptedMon;
    const int aftermath   = s.bAftermath;
    const int start_act   = s.bStartAct;
    const int hair        = s.bHair;
    const int face        = s.bFace;
    const int body        = s.bBody;
    const int pants       = s.bPants;
    const int hand        = s.bHand;
    const int foot        = s.bFoot;
    const int helmet_hide = s.bHelmetHide;
    const int sex         = s.bSex;
    const int real_sex    = s.bRealSex;
    const int country     = s.bCountry;
    const int ori_country = s.bOriCountry;
    const int class_      = s.bClass;
    const int race        = s.bRace;
    const int dir         = s.wDIR;
    const int stat_level  = s.bStatLevel;
    const int stat_point  = s.bStatPoint;
    const int stat_exp    = static_cast<int>(s.dwStatExp);
    const double px       = s.fPosX;
    const double py       = s.fPosY;
    const double pz       = s.fPosZ;

    try
    {
        // Save is a targeted named-param UPDATE (queries::SaveCharTchart)
        // that doesn't fit generic full-row CRUD — run it on the session
        // the DbContext leases, mirroring the guild repo's write split.
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();

        sql << queries::SaveCharTchart,
            soci::use(s.szNAME,    "name"),
            soci::use(hair,        "hair"),
            soci::use(face,        "face"),
            soci::use(body,        "body"),
            soci::use(pants,       "pants"),
            soci::use(hand,        "hand"),
            soci::use(foot,        "foot"),
            soci::use(helmet_hide, "helmet_hide"),
            soci::use(sex,         "sex"),
            soci::use(real_sex,    "real_sex"),
            soci::use(country,     "country"),
            soci::use(ori_country, "ori_country"),
            soci::use(class_,      "class_"),
            soci::use(race,        "race"),
            soci::use(level,       "level"),
            soci::use(exp,         "exp"),
            soci::use(hp,          "hp"),
            soci::use(mp,          "mp"),
            soci::use(gold,        "gold"),
            soci::use(silver,      "silver"),
            soci::use(cooper,      "cooper"),
            soci::use(sp,          "sp"),
            soci::use(region,      "region"),
            soci::use(guild_leave, "guild_leave"),
            soci::use(guild_leave_time, "guild_leave_time"),
            soci::use(map,         "map"),
            soci::use(spawn,       "spawn"),
            soci::use(last_spawn,  "last_spawn"),
            soci::use(last_dest,   "last_dest"),
            soci::use(tempted_mon, "tempted_mon"),
            soci::use(aftermath,   "aftermath"),
            soci::use(start_act,   "start_act"),
            soci::use(px,          "px"),
            soci::use(py,          "py"),
            soci::use(pz,          "pz"),
            soci::use(dir,         "dir"),
            soci::use(stat_level,  "stat_level"),
            soci::use(stat_point,  "stat_point"),
            soci::use(stat_exp,    "stat_exp"),
            soci::use(cid,         "cid");

        // TALLCHARTABLE: sync bLevel, dwEXP, dLogoutDate, dwPlayTime.
        // Wrapped in try-catch so a missing cross-DB link (TALLCHARTABLE
        // may be in a different DB on sharded deployments) degrades
        // gracefully — TCHARTABLE save above already succeeded.
        try
        {
            sql << queries::SaveCharTallchart,
                soci::use(level, "level"),
                soci::use(exp,   "exp"),
                soci::use(cid,   "cid");
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("soci_player_service: SaveChar char={} "
                         "TALLCHARTABLE update skipped: {}",
                s.dwCharID, ex.what());
        }

        spdlog::info("soci_player_service: SaveChar char={} name='{}' "
                     "lvl={} exp={} hp={}/{} pos=({:.1f},{:.1f},{:.1f})",
            s.dwCharID, s.szNAME, s.bLevel, s.dwEXP,
            s.dwHP, s.dwMP, s.fPosX, s.fPosY, s.fPosZ);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_player_service: SaveChar char={} threw: {}",
            s.dwCharID, ex.what());
    }
}

} // namespace tmapsvr
