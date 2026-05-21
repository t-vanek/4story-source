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

} // namespace tmapsvr
