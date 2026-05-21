#include "soci_player_service.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

namespace {

// Helper: ODBC sometimes hands back tinyint columns as int32 even
// when bound to a smaller integer. Forcing the SELECT into int32
// receivers and narrowing on copy avoids the SOCI "type mismatch"
// throws we'd otherwise hit on certain SQL Server drivers.
template <class T>
T NarrowCast(std::int32_t v) { return static_cast<T>(v); }

} // namespace

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

        // SELECT the columns the F8 wire encoding needs. Column names
        // and order match legacy CTBLChar (SSHandler.cpp:3329). The
        // receivers are int32 / std::string / double; we narrow on
        // copy below. Float positions go through `double` because
        // ODBC's REAL → float bind path is fragile on some drivers.
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

        sql <<
            "SELECT dwCharID, szNAME, bStartAct, bRealSex, bClass, bLevel, "
            "  bRace, bCountry, bOriCountry, bSex, bHair, bFace, bBody, "
            "  bPants, bHand, bFoot, bHelmetHide, dwGold, dwSilver, dwCooper, "
            "  dwEXP, dwHP, dwMP, wSkillPoint, dwRegion, bGuildLeave, "
            "  dwGuildLeaveTime, wMapID, wSpawnID, wLastSpawnID, "
            "  dwLastDestination, wTemptedMon, bAftermath, fPosX, fPosY, "
            "  fPosZ, wDIR, bStatLevel, bStatPoint, dwStatExp "
            "FROM TCHARTABLE WHERE dwCharID = :cid AND bDelete = 0",
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
        s.dwCharID         = static_cast<std::uint32_t>(row_char_id);
        s.szNAME           = (name_ind == soci::i_ok) ? row_name : std::string{};
        s.bStartAct        = NarrowCast<std::uint8_t> (row_start_act);
        s.bRealSex         = NarrowCast<std::uint8_t> (row_real_sex);
        s.bClass           = NarrowCast<std::uint8_t> (row_class);
        s.bLevel           = NarrowCast<std::uint8_t> (row_level);
        s.bRace            = NarrowCast<std::uint8_t> (row_race);
        s.bCountry         = NarrowCast<std::uint8_t> (row_country);
        s.bOriCountry      = NarrowCast<std::uint8_t> (row_ori_country);
        s.bSex             = NarrowCast<std::uint8_t> (row_sex);
        s.bHair            = NarrowCast<std::uint8_t> (row_hair);
        s.bFace            = NarrowCast<std::uint8_t> (row_face);
        s.bBody            = NarrowCast<std::uint8_t> (row_body);
        s.bPants           = NarrowCast<std::uint8_t> (row_pants);
        s.bHand            = NarrowCast<std::uint8_t> (row_hand);
        s.bFoot            = NarrowCast<std::uint8_t> (row_foot);
        s.bHelmetHide      = NarrowCast<std::uint8_t> (row_helmet_hide);
        s.dwGold           = static_cast<std::uint32_t>(row_gold);
        s.dwSilver         = static_cast<std::uint32_t>(row_silver);
        s.dwCooper         = static_cast<std::uint32_t>(row_cooper);
        s.dwEXP            = static_cast<std::uint32_t>(row_exp);
        s.dwHP             = static_cast<std::uint32_t>(row_hp);
        s.dwMP             = static_cast<std::uint32_t>(row_mp);
        s.wSkillPoint      = NarrowCast<std::uint16_t>(row_skill_point);
        s.dwRegion         = static_cast<std::uint32_t>(row_region);
        s.bGuildLeave      = NarrowCast<std::uint8_t> (row_guild_leave);
        s.dwGuildLeaveTime = static_cast<std::uint32_t>(row_guild_leave_time);
        s.wMapID           = NarrowCast<std::uint16_t>(row_map);
        s.wSpawnID         = NarrowCast<std::uint16_t>(row_spawn);
        s.wLastSpawnID     = NarrowCast<std::uint16_t>(row_last_spawn);
        s.dwLastDestination= static_cast<std::uint32_t>(row_last_dest);
        s.wTemptedMon      = NarrowCast<std::uint16_t>(row_tempted_mon);
        s.bAftermath       = NarrowCast<std::uint8_t> (row_aftermath);
        s.fPosX            = static_cast<float>(row_pos_x);
        s.fPosY            = static_cast<float>(row_pos_y);
        s.fPosZ            = static_cast<float>(row_pos_z);
        s.wDIR             = NarrowCast<std::uint16_t>(row_dir);
        s.bStatLevel       = NarrowCast<std::uint8_t> (row_stat_level);
        s.bStatPoint       = NarrowCast<std::uint8_t> (row_stat_point);
        s.dwStatExp        = static_cast<std::uint32_t>(row_stat_exp);
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
