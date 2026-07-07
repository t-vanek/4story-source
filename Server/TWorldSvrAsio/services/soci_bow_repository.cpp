#include "services/soci_bow_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <string>

namespace tworldsvr {

namespace {

// BT_BOW (TWorldType.h:238).
constexpr int kBtBow = 5;

std::int64_t NumCol(const soci::row& row, std::size_t i)
{
    if (row.get_indicator(i) != soci::i_ok)
        return 0;
    switch (row.get_properties(i).get_data_type())
    {
    case soci::dt_integer:
        return row.get<int>(i);
    case soci::dt_long_long:
        return row.get<long long>(i);
    case soci::dt_unsigned_long_long:
        return static_cast<std::int64_t>(
            row.get<unsigned long long>(i));
    case soci::dt_double:
        return static_cast<std::int64_t>(row.get<double>(i));
    default:
        return 0;
    }
}

} // namespace

std::optional<BowSettings> SociBowRepository::LoadSettings()
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"wMapID\", \"bMinPlayersCount\", "
            "\"bMaxNationDifference\", \"dwAlarmDur\", "
            "\"dwBuyTimeDur\", \"dwBattleDur\" "
            "FROM \"TBOWSETTINGSCHART\"");
        for (const auto& row : rs)
        {
            BowSettings s;
            s.map_id = static_cast<std::uint16_t>(NumCol(row, 0));
            s.min_players =
                static_cast<std::uint8_t>(NumCol(row, 1));
            s.max_nation_difference =
                static_cast<std::uint8_t>(NumCol(row, 2));
            s.alarm_dur =
                static_cast<std::uint32_t>(NumCol(row, 3));
            s.buy_dur =
                static_cast<std::uint32_t>(NumCol(row, 4));
            s.battle_dur =
                static_cast<std::uint32_t>(NumCol(row, 5));
            return s;                    // legacy takes the first row
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociBowRepository::LoadSettings failed: {}",
            ex.what());
    }
    return std::nullopt;
}

std::vector<std::uint32_t> SociBowRepository::LoadStartTimes()
{
    std::vector<std::uint32_t> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"dwTime\" FROM \"TCUSTOMTIMECHART\" "
            "WHERE \"bType\" = :t", soci::use(kBtBow));
        for (const auto& row : rs)
            out.push_back(static_cast<std::uint32_t>(NumCol(row, 0)));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociBowRepository::LoadStartTimes failed: {}",
            ex.what());
        out.clear();
    }
    return out;
}

void SociBowRepository::AddPlayer(std::uint32_t char_id,
                                  std::uint32_t user_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        // CSPAddBOWPlayer (DBAccess.h:3005).
        *lease << "EXEC dbo.TAddBOWPlayer " +
            std::to_string(static_cast<long long>(char_id)) + ", " +
            std::to_string(static_cast<long long>(user_id));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociBowRepository::AddPlayer(char={}) failed: "
                      "{}", char_id, ex.what());
    }
}

void SociBowRepository::ClearPlayers()
{
    try
    {
        auto lease = m_pool.Acquire();
        // CSPClearBOWPlayers (DBAccess.h:3019).
        *lease << "EXEC dbo.TClearBOWPlayers";
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociBowRepository::ClearPlayers failed: {}",
            ex.what());
    }
}

void SociBowRepository::DeleteSinglePlayer(std::uint32_t user_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        // CSPDeleteSingleBOWPlayer (DBAccess.h:3025).
        *lease << "EXEC dbo.TDeleteSingleBOWPlayer " +
            std::to_string(static_cast<long long>(user_id));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociBowRepository::DeleteSinglePlayer(user={}) "
                      "failed: {}", user_id, ex.what());
    }
}

} // namespace tworldsvr
