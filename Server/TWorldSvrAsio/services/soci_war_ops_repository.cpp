#include "services/soci_war_ops_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <exception>

namespace tworldsvr {

std::vector<ActiveCharRow>
SociWarOpsRepository::LoadActiveChars(std::int64_t prune_before_unix)
{
    std::vector<ActiveCharRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;

        // Legacy CTBLActiveCharDel — prune stale rows first
        // (__TIMETODB uses local time).
        std::time_t t = static_cast<std::time_t>(prune_before_unix);
        std::tm     before{};
#ifdef _WIN32
        localtime_s(&before, &t);
#else
        localtime_r(&t, &before);
#endif
        sql << "DELETE FROM \"TACTIVECHARTABLE\" WHERE \"dateEnter\" < :d",
            soci::use(before);

        // Legacy CTBLActiveCharTable join.
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT AC.\"dwCharID\", CT.\"bCountry\", AT.\"bCountry\", "
            "CT.\"bLevel\" "
            "FROM \"TACTIVECHARTABLE\" AS AC "
            "INNER JOIN \"TCHARTABLE\" AS CT "
            "ON AC.\"dwCharID\" = CT.\"dwCharID\" "
            "LEFT OUTER JOIN \"TAIDTABLE\" AS AT "
            "ON AC.\"dwCharID\" = AT.\"dwCharID\"");
        for (const auto& row : rs)
        {
            ActiveCharRow r{};
            r.char_id = static_cast<std::uint32_t>(row.get<int>(0));
            r.country = static_cast<std::uint8_t>(row.get<int>(1));
            if (row.get_indicator(2) == soci::i_ok)
            {
                r.has_aid     = true;
                r.aid_country = static_cast<std::uint8_t>(row.get<int>(2));
            }
            r.level = static_cast<std::uint8_t>(row.get<int>(3));
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociWarOpsRepository::LoadActiveChars failed: {}",
            ex.what());
        out.clear();
    }
    return out;
}

bool SociWarOpsRepository::SaveCastleApplicant(std::uint16_t castle,
                                               std::uint32_t char_id,
                                               std::uint8_t  camp)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int c = castle, id = static_cast<int>(char_id), k = camp;
        sql << "EXEC dbo.TSaveCastleApplicant :c, :id, :k",
            soci::use(c), soci::use(id), soci::use(k);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociWarOpsRepository::SaveCastleApplicant"
                      "({},{},{}) failed: {}", castle, char_id, camp,
            ex.what());
        return false;
    }
}

} // namespace tworldsvr
