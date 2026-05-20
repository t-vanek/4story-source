// SociPlayerService — loads CharSnapshot from TCHARTABLE.
//
// Column order matches DBAccess.h::CTBLChar::Open() write path so a
// future reviewer can diff the SOCI into() list against the legacy
// SELECT column list without hunting through the source tree.
//
// SOCI type mapping:
//   BYTE (uint8_t)  → int   (SOCI doesn't bind uint8; narrow after fetch)
//   WORD (uint16_t) → int
//   DWORD (uint32_t)→ int
//   FLOAT           → double (SOCI maps SQL FLOAT/REAL to double)
//   CString/TCHAR[] → std::string
//
// NULL handling: the PRIMARY KEY lookup (dwID + dwUserID) either
// returns exactly one row or zero rows. execute(true) returns false
// when zero rows are fetched; any DB error is caught and treated as
// nullopt (same as the legacy `query->Open()` failure path).

#include "services/soci_player_service.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociPlayerService::SociPlayerService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::optional<legacy::CharSnapshot>
SociPlayerService::LoadChar(std::uint32_t char_id,
                            std::uint32_t user_id,
                            std::uint32_t dw_key)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    // SOCI bind variables — int for all integer columns (avoids
    // unsigned/signed mismatch warnings on ODBC/PostgreSQL backends).
    std::string name;
    int start_act = 0, real_sex = 0, char_class = 0, level = 0;
    int race = 0, country = 0, ori_country = 0, sex = 0;
    int hair = 0, face = 0, body = 0, pants = 0, hand = 0;
    int foot = 0, helmet_hide = 0;
    int cooper = 0, silver = 0, gold = 0;
    int exp_val = 0, hp = 0, mp = 0, skill_point = 0;
    int region = 0, guild_leave = 0;
    int guild_leave_time_i = 0;
    int tempted_mon = 0, aftermath = 0;
    int map_id = 0, spawn_id = 0, last_spawn_id = 0;
    int last_dest_i = 0;
    double pos_x = 0.0, pos_y = 0.0, pos_z = 0.0;
    int dir = 0, stat_level = 0, stat_point = 0, stat_exp_i = 0;

    const int cid = static_cast<int>(char_id);
    const int uid = static_cast<int>(user_id);

    try
    {
        // Column order: DBAccess.h::CTBLChar SELECT list (szNAME … dwStatExp).
        soci::statement st = (sql.prepare <<
            "SELECT \"szNAME\","
            "  \"bStartAct\", \"bRealSex\", \"bClass\", \"bLevel\","
            "  \"bRace\", \"bCountry\", \"bOriCountry\","
            "  \"bSex\", \"bHair\", \"bFace\","
            "  \"bBody\", \"bPants\", \"bHand\", \"bFoot\", \"bHelmetHide\","
            "  \"dwCooper\", \"dwSilver\", \"dwGold\","
            "  \"dwEXP\", \"dwHP\", \"dwMP\", \"wSkillPoint\","
            "  \"dwRegion\", \"bGuildLeave\", \"dwGuildLeaveTime\","
            "  \"wMapID\", \"wSpawnID\", \"wLastSpawnID\","
            "  \"dwLastDestination\", \"wTemptedMon\", \"bAftermath\","
            "  \"fPosX\", \"fPosY\", \"fPosZ\", \"wDIR\","
            "  \"bStatLevel\", \"bStatPoint\", \"dwStatExp\""
            " FROM \"TCHARTABLE\""
            " WHERE \"dwID\" = :cid AND \"dwUserID\" = :uid",
            soci::into(name),
            soci::into(start_act),  soci::into(real_sex),
            soci::into(char_class), soci::into(level),
            soci::into(race),       soci::into(country),
            soci::into(ori_country),
            soci::into(sex),        soci::into(hair),    soci::into(face),
            soci::into(body),       soci::into(pants),   soci::into(hand),
            soci::into(foot),       soci::into(helmet_hide),
            soci::into(cooper),     soci::into(silver),  soci::into(gold),
            soci::into(exp_val),    soci::into(hp),      soci::into(mp),
            soci::into(skill_point),
            soci::into(region),     soci::into(guild_leave),
            soci::into(guild_leave_time_i),
            soci::into(map_id),     soci::into(spawn_id),
            soci::into(last_spawn_id),
            soci::into(last_dest_i),
            soci::into(tempted_mon),soci::into(aftermath),
            soci::into(pos_x),      soci::into(pos_y),   soci::into(pos_z),
            soci::into(dir),
            soci::into(stat_level), soci::into(stat_point),
            soci::into(stat_exp_i),
            soci::use(cid, "cid"),
            soci::use(uid, "uid"));

        const bool got = st.execute(true);
        if (!got)
        {
            spdlog::info("player_service: no TCHARTABLE row "
                         "char_id={} user_id={}", char_id, user_id);
            return std::nullopt;
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("player_service: TCHARTABLE query failed "
                     "char_id={} user_id={}: {}",
                     char_id, user_id, ex.what());
        return std::nullopt;
    }

    // Assemble CharSnapshot from fetched columns.
    legacy::CharSnapshot snap{};
    snap.char_id = char_id;
    snap.user_id = user_id;
    snap.dw_key  = dw_key;   // caller supplies — not stored in TCHARTABLE
    snap.name    = std::move(name);

    snap.level       = static_cast<std::uint8_t>(level);
    snap.exp         = static_cast<std::uint32_t>(exp_val);
    snap.hp          = static_cast<std::uint32_t>(hp);
    snap.mp          = static_cast<std::uint32_t>(mp);
    snap.skill_point = static_cast<std::uint16_t>(skill_point);
    snap.stat_level  = static_cast<std::uint8_t>(stat_level);
    snap.stat_point  = static_cast<std::uint8_t>(stat_point);
    snap.stat_exp    = static_cast<std::uint32_t>(stat_exp_i);

    auto& a         = snap.appearance;
    a.race          = static_cast<std::uint8_t>(race);
    a.sex           = static_cast<std::uint8_t>(sex);
    a.real_sex      = static_cast<std::uint8_t>(real_sex);
    a.char_class    = static_cast<std::uint8_t>(char_class);
    a.hair          = static_cast<std::uint8_t>(hair);
    a.face          = static_cast<std::uint8_t>(face);
    a.body          = static_cast<std::uint8_t>(body);
    a.pants         = static_cast<std::uint8_t>(pants);
    a.hand          = static_cast<std::uint8_t>(hand);
    a.foot          = static_cast<std::uint8_t>(foot);
    a.helmet_hide   = static_cast<std::uint8_t>(helmet_hide);
    a.country       = static_cast<std::uint8_t>(country);
    a.ori_country   = static_cast<std::uint8_t>(ori_country);
    a.start_act     = static_cast<std::uint8_t>(start_act);

    auto& p              = snap.position;
    p.map_id             = static_cast<std::uint16_t>(map_id);
    p.spawn_id           = static_cast<std::uint16_t>(spawn_id);
    p.last_spawn_id      = static_cast<std::uint16_t>(last_spawn_id);
    p.last_destination   = static_cast<std::uint32_t>(last_dest_i);
    p.region             = static_cast<std::uint32_t>(region);
    p.pos_x              = static_cast<float>(pos_x);
    p.pos_y              = static_cast<float>(pos_y);
    p.pos_z              = static_cast<float>(pos_z);
    p.dir                = static_cast<std::uint16_t>(dir);

    snap.gold             = static_cast<std::uint32_t>(gold);
    snap.silver           = static_cast<std::uint32_t>(silver);
    snap.copper           = static_cast<std::uint32_t>(cooper);
    snap.guild_leave      = static_cast<std::uint8_t>(guild_leave);
    snap.guild_leave_time = static_cast<std::uint32_t>(guild_leave_time_i);
    snap.tempted_mon      = static_cast<std::uint16_t>(tempted_mon);
    snap.aftermath        = static_cast<std::uint8_t>(aftermath);

    // F5: load inventory from TITEMTABLE WHERE dwOwnerID = char_id
    // snap.inventory = ...;

    return snap;
}

} // namespace tmapsvr
