#include "services/soci_rps_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <exception>

namespace tworldsvr {

namespace {

std::tm ToLocalTm(std::int64_t unix_secs)
{
    std::time_t t = static_cast<std::time_t>(unix_secs);
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

std::int64_t FromLocalTm(std::tm tm_val)
{
    tm_val.tm_isdst = -1;
    return static_cast<std::int64_t>(std::mktime(&tm_val));
}

} // namespace

std::vector<TRpsGame> SociRpsRepository::LoadGames()
{
    std::vector<TRpsGame> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bType\", \"bWinCount\", \"bProb_Win\", "
            "\"bProb_Draw\", \"bProb_Lose\", \"wWinKeep\", "
            "\"wWinPeriod\" FROM \"TRPSGAMECHART\"");
        for (const auto& row : rs)
        {
            TRpsGame g{};
            g.type       = static_cast<std::uint8_t>(row.get<int>(0));
            g.win_count  = static_cast<std::uint8_t>(row.get<int>(1));
            g.win_prob   = static_cast<std::uint8_t>(row.get<int>(2));
            g.draw_prob  = static_cast<std::uint8_t>(row.get<int>(3));
            g.lose_prob  = static_cast<std::uint8_t>(row.get<int>(4));
            g.win_keep   = static_cast<std::uint16_t>(row.get<int>(5));
            g.win_period = static_cast<std::uint16_t>(row.get<int>(6));
            out.push_back(g);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociRpsRepository::LoadGames failed: {}",
            ex.what());
        out.clear();
    }
    return out;
}

std::vector<RpsWinDateRow> SociRpsRepository::LoadWinDates()
{
    std::vector<RpsWinDateRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bType\", \"bWinCount\", \"dWinDate\" "
            "FROM \"TRPSGAMERECORDTABLE\"");
        for (const auto& row : rs)
        {
            RpsWinDateRow r{};
            r.type      = static_cast<std::uint8_t>(row.get<int>(0));
            r.win_count = static_cast<std::uint8_t>(row.get<int>(1));
            if (row.get_indicator(2) == soci::i_ok)
                r.date_unix = FromLocalTm(row.get<std::tm>(2));
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociRpsRepository::LoadWinDates failed: {}",
            ex.what());
        out.clear();
    }
    return out;
}

bool SociRpsRepository::Record(bool insert, std::uint32_t char_id,
                               std::uint8_t type,
                               std::uint8_t win_count,
                               std::int64_t date_unix)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int rec = insert ? 1 : 0;
        const int cid = static_cast<int>(char_id);
        const int ty = type, wc = win_count;
        std::tm when = ToLocalTm(date_unix);
        sql << "EXEC dbo.TRPSGameRecord :r, :c, :t, :w, :d",
            soci::use(rec), soci::use(cid), soci::use(ty),
            soci::use(wc), soci::use(when);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociRpsRepository::Record({},{},{},{}) failed: "
                      "{}", insert, char_id, type, win_count, ex.what());
        return false;
    }
}

} // namespace tworldsvr
