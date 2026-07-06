#include "services/soci_month_rank_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <string>

namespace tworldsvr {

bool SociMonthRankRepository::InitMonthRank(std::uint8_t month)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int m = static_cast<int>(month);
        sql << "EXEC dbo.TInitMonthRank :m", soci::use(m);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociMonthRankRepository::InitMonthRank({}) "
                      "failed: {}", month, ex.what());
        return false;
    }
}

bool SociMonthRankRepository::SaveRanker(std::uint8_t month,
                                         std::uint8_t rank,
                                         std::uint8_t total_rank,
                                         const MonthRanker& r)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int p_month = month, p_rank = rank, p_mrank = total_rank;
        const int cid = static_cast<int>(r.char_id);
        const int tp  = static_cast<int>(r.total_point);
        const int mp  = static_cast<int>(r.month_point);
        const int mw  = r.month_win,  ml = r.month_lose;
        const int tw  = static_cast<int>(r.total_win);
        const int tl  = static_cast<int>(r.total_lose);
        const int co  = r.country, lv = r.level, kl = r.klass;
        const int ra  = r.race, se = r.sex, ha = r.hair, fa = r.face;
        sql << "EXEC dbo.TSaveMonthRank :m, :r, :mr, :cid, :name, "
               ":tp, :mp, :mw, :ml, :tw, :tl, :co, :lv, :kl, :ra, "
               ":se, :ha, :fa, :say, :guild",
            soci::use(p_month), soci::use(p_rank), soci::use(p_mrank),
            soci::use(cid), soci::use(r.name), soci::use(tp),
            soci::use(mp), soci::use(mw), soci::use(ml), soci::use(tw),
            soci::use(tl), soci::use(co), soci::use(lv), soci::use(kl),
            soci::use(ra), soci::use(se), soci::use(ha), soci::use(fa),
            soci::use(r.say), soci::use(r.guild);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociMonthRankRepository::SaveRanker(m={}, j={}, "
                      "t={}, char={}) failed: {}",
            month, rank, total_rank, r.char_id, ex.what());
        return false;
    }
}

bool SociMonthRankRepository::InitMonthPvPoint(
    std::uint8_t month, std::uint32_t top_total_point,
    std::optional<MonthRanker>& new_top)
{
    new_top.reset();
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        // 16 OUTPUT params + return value — DECLARE/EXEC/SELECT batch
        // (inputs are server-derived integers; embedding them keeps
        // the statement a single ODBC round-trip).
        const std::string q =
            "DECLARE @ret INT, @cid INT, @name NVARCHAR(64), "
            "@tp INT, @mp INT, @mw SMALLINT, @ml SMALLINT, "
            "@tw INT, @tl INT, @co TINYINT, @lv TINYINT, "
            "@kl TINYINT, @ra TINYINT, @se TINYINT, @ha TINYINT, "
            "@fa TINYINT, @gu NVARCHAR(64); "
            "EXEC @ret = dbo.TInitMonthPvPoint " +
            std::to_string(static_cast<int>(month)) + ", " +
            std::to_string(static_cast<long long>(top_total_point)) +
            ", @cid OUTPUT, @name OUTPUT, @tp OUTPUT, @mp OUTPUT, "
            "@mw OUTPUT, @ml OUTPUT, @tw OUTPUT, @tl OUTPUT, "
            "@co OUTPUT, @lv OUTPUT, @kl OUTPUT, @ra OUTPUT, "
            "@se OUTPUT, @ha OUTPUT, @fa OUTPUT, @gu OUTPUT; "
            "SELECT @ret, @cid, @name, @tp, @mp, @mw, @ml, @tw, @tl, "
            "@co, @lv, @kl, @ra, @se, @ha, @fa, @gu;";
        soci::rowset<soci::row> rs = (sql.prepare << q);
        for (const auto& row : rs)
        {
            const int ret = row.get_indicator(0) == soci::i_ok
                                ? row.get<int>(0) : 0;
            if (ret == 0)
                return true;                    // no new top ranker
            auto geti = [&row](std::size_t i) -> int {
                return row.get_indicator(i) == soci::i_ok
                           ? row.get<int>(i) : 0;
            };
            MonthRanker top{};
            top.char_id     = static_cast<std::uint32_t>(geti(1));
            top.name        = row.get_indicator(2) == soci::i_ok
                                  ? row.get<std::string>(2) : "";
            top.total_point = static_cast<std::uint32_t>(geti(3));
            top.month_point = static_cast<std::uint32_t>(geti(4));
            top.month_win   = static_cast<std::uint16_t>(geti(5));
            top.month_lose  = static_cast<std::uint16_t>(geti(6));
            top.total_win   = static_cast<std::uint32_t>(geti(7));
            top.total_lose  = static_cast<std::uint32_t>(geti(8));
            top.country     = static_cast<std::uint8_t>(geti(9));
            top.level       = static_cast<std::uint8_t>(geti(10));
            top.klass       = static_cast<std::uint8_t>(geti(11));
            top.race        = static_cast<std::uint8_t>(geti(12));
            top.sex         = static_cast<std::uint8_t>(geti(13));
            top.hair        = static_cast<std::uint8_t>(geti(14));
            top.face        = static_cast<std::uint8_t>(geti(15));
            top.guild       = row.get_indicator(16) == soci::i_ok
                                  ? row.get<std::string>(16) : "";
            new_top = std::move(top);
            return true;
        }
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociMonthRankRepository::InitMonthPvPoint({},{}) "
                      "failed: {}", month, top_total_point, ex.what());
        return false;
    }
}

} // namespace tworldsvr
