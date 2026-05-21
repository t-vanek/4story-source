#include "soci_player_service.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

SociPlayerService::SociPlayerService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::optional<CharSnapshot>
SociPlayerService::LoadChar(std::uint32_t char_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        // Receivers are int32 / std::string / double; we narrow on
        // copy below via db::Narrow* helpers. Float positions go
        // through `double` because ODBC's REAL → float bind path is
        // fragile on some drivers. Column order matches
        // queries::CharByCharId exactly.
        std::int32_t row_char_id = 0;
        std::string  row_name;
        std::int32_t row_start_act = 0, row_real_sex = 0, row_class = 0,
                     row_level = 1, row_race = 0, row_country = 0,
                     row_ori_country = 0, row_sex = 0,
                     row_hair = 0, row_face = 0, row_body = 0,
                     row_pants = 0, row_hand = 0, row_foot = 0,
                     row_helmet_hide = 0;
        std::int32_t row_gold = 0, row_silver = 0, row_cooper = 0;
        std::int32_t row_exp = 0, row_hp = 1, row_mp = 1;
        std::int32_t row_skill_point = 0, row_region = 0;
        std::int32_t row_guild_leave = 0, row_guild_leave_time = 0;
        std::int32_t row_map = 0, row_spawn = 0, row_last_spawn = 0,
                     row_last_dest = 0, row_tempted_mon = 0,
                     row_aftermath = 0;
        double       row_pos_x = 0.0, row_pos_y = 0.0, row_pos_z = 0.0;
        std::int32_t row_dir = 0;
        std::int32_t row_stat_level = 0, row_stat_point = 0, row_stat_exp = 0;

        soci::indicator name_ind = soci::i_null;

        sql << queries::CharByCharId,
            soci::use(static_cast<std::int32_t>(char_id), "cid"),
            soci::into(row_char_id),
            soci::into(row_name, name_ind),
            soci::into(row_start_act), soci::into(row_real_sex),
            soci::into(row_class),     soci::into(row_level),
            soci::into(row_race),      soci::into(row_country),
            soci::into(row_ori_country),
            soci::into(row_sex),       soci::into(row_hair),
            soci::into(row_face),      soci::into(row_body),
            soci::into(row_pants),     soci::into(row_hand),
            soci::into(row_foot),      soci::into(row_helmet_hide),
            soci::into(row_gold),      soci::into(row_silver),
            soci::into(row_cooper),    soci::into(row_exp),
            soci::into(row_hp),        soci::into(row_mp),
            soci::into(row_skill_point), soci::into(row_region),
            soci::into(row_guild_leave),
            soci::into(row_guild_leave_time),
            soci::into(row_map),       soci::into(row_spawn),
            soci::into(row_last_spawn),soci::into(row_last_dest),
            soci::into(row_tempted_mon),
            soci::into(row_aftermath),
            soci::into(row_pos_x),     soci::into(row_pos_y),
            soci::into(row_pos_z),     soci::into(row_dir),
            soci::into(row_stat_level), soci::into(row_stat_point),
            soci::into(row_stat_exp);

        if (!sql.got_data())
            return std::nullopt;

        CharSnapshot s;
        s.dwCharID         = db::Narrow32(row_char_id);
        s.szNAME           = db::SafeString(row_name, name_ind);
        s.bStartAct        = db::Narrow8 (row_start_act);
        s.bRealSex         = db::Narrow8 (row_real_sex);
        s.bClass           = db::Narrow8 (row_class);
        s.bLevel           = db::Narrow8 (row_level);
        s.bRace            = db::Narrow8 (row_race);
        s.bCountry         = db::Narrow8 (row_country);
        s.bOriCountry      = db::Narrow8 (row_ori_country);
        s.bSex             = db::Narrow8 (row_sex);
        s.bHair            = db::Narrow8 (row_hair);
        s.bFace            = db::Narrow8 (row_face);
        s.bBody            = db::Narrow8 (row_body);
        s.bPants           = db::Narrow8 (row_pants);
        s.bHand            = db::Narrow8 (row_hand);
        s.bFoot            = db::Narrow8 (row_foot);
        s.bHelmetHide      = db::Narrow8 (row_helmet_hide);
        s.dwGold           = db::Narrow32(row_gold);
        s.dwSilver         = db::Narrow32(row_silver);
        s.dwCooper         = db::Narrow32(row_cooper);
        s.dwEXP            = db::Narrow32(row_exp);
        s.dwHP             = db::Narrow32(row_hp);
        s.dwMP             = db::Narrow32(row_mp);
        s.wSkillPoint      = db::Narrow16(row_skill_point);
        s.dwRegion         = db::Narrow32(row_region);
        s.bGuildLeave      = db::Narrow8 (row_guild_leave);
        s.dwGuildLeaveTime = db::Narrow32(row_guild_leave_time);
        s.wMapID           = db::Narrow16(row_map);
        s.wSpawnID         = db::Narrow16(row_spawn);
        s.wLastSpawnID     = db::Narrow16(row_last_spawn);
        s.dwLastDestination= db::Narrow32(row_last_dest);
        s.wTemptedMon      = db::Narrow16(row_tempted_mon);
        s.bAftermath       = db::Narrow8 (row_aftermath);
        s.fPosX            = db::NarrowF(row_pos_x);
        s.fPosY            = db::NarrowF(row_pos_y);
        s.fPosZ            = db::NarrowF(row_pos_z);
        s.wDIR             = db::Narrow16(row_dir);
        s.bStatLevel       = db::Narrow8 (row_stat_level);
        s.bStatPoint       = db::Narrow8 (row_stat_point);
        s.dwStatExp        = db::Narrow32(row_stat_exp);
        return s;
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
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

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
