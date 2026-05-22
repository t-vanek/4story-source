#include "services/soci_guild_level_repository.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

namespace tworldsvr {

std::vector<TGuildLevelRow> SociGuildLevelRepository::LoadAll()
{
    std::vector<TGuildLevelRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bLevel\", \"dwEXP\", \"bMaxCnt\", \"bMinCnt\", "
            "\"bCabinetCnt\", \"bTacticsCnt\", \"bBattleSetCnt\", "
            "\"bGuardCnt\", \"bRoyalGuardCnt\", \"bTurretCnt\", "
            "\"bPeer1\", \"bPeer2\", \"bPeer3\", \"bPeer4\", \"bPeer5\" "
            "FROM \"TGUILDCHART\"");
        for (const auto& r : rs)
        {
            TGuildLevelRow row;
            row.level             = static_cast<std::uint8_t>(r.get<int>("bLevel"));
            row.exp               = static_cast<std::uint32_t>(r.get<int>("dwEXP"));
            row.max_count         = static_cast<std::uint8_t>(r.get<int>("bMaxCnt"));
            row.min_count         = static_cast<std::uint8_t>(r.get<int>("bMinCnt"));
            row.cabinet_count     = static_cast<std::uint8_t>(r.get<int>("bCabinetCnt"));
            row.tactics_count     = static_cast<std::uint8_t>(r.get<int>("bTacticsCnt"));
            row.battle_set_count  = static_cast<std::uint8_t>(r.get<int>("bBattleSetCnt"));
            row.guard_count       = static_cast<std::uint8_t>(r.get<int>("bGuardCnt"));
            row.royal_guard_count = static_cast<std::uint8_t>(r.get<int>("bRoyalGuardCnt"));
            row.turret_count      = static_cast<std::uint8_t>(r.get<int>("bTurretCnt"));
            row.peer_slots[0]     = static_cast<std::uint8_t>(r.get<int>("bPeer1"));
            row.peer_slots[1]     = static_cast<std::uint8_t>(r.get<int>("bPeer2"));
            row.peer_slots[2]     = static_cast<std::uint8_t>(r.get<int>("bPeer3"));
            row.peer_slots[3]     = static_cast<std::uint8_t>(r.get<int>("bPeer4"));
            row.peer_slots[4]     = static_cast<std::uint8_t>(r.get<int>("bPeer5"));
            out.push_back(std::move(row));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildLevelRepository::LoadAll failed: {}",
            ex.what());
        return {};
    }
    spdlog::info("SociGuildLevelRepository: loaded {} TGUILDCHART row(s)",
        out.size());
    return out;
}

} // namespace tworldsvr
