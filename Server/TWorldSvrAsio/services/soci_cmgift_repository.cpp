#include "services/soci_cmgift_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>

namespace tworldsvr {

std::vector<CmGift> SociCmGiftRepository::LoadChart()
{
    std::vector<CmGift> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"wGiftID\", \"bGiftType\", \"dwValue\", \"bCount\", "
            "\"bTakeType\", \"bMaxTakeCount\", \"bToolOnly\", "
            "\"wErrGiftID\", \"szTitle\", \"szMsg\" "
            "FROM \"TCMGIFTCHART\"");
        for (const auto& row : rs)
        {
            CmGift g{};
            g.gift_id        = static_cast<std::uint16_t>(row.get<int>(0));
            g.gift_type      = static_cast<std::uint8_t>(row.get<int>(1));
            g.value          = static_cast<std::uint32_t>(
                                   row.get<long long>(2));
            g.count          = static_cast<std::uint8_t>(row.get<int>(3));
            g.take_type      = static_cast<std::uint8_t>(row.get<int>(4));
            g.max_take_count = static_cast<std::uint8_t>(row.get<int>(5));
            g.tool_only      = static_cast<std::uint8_t>(row.get<int>(6));
            g.err_gift_id    = static_cast<std::uint16_t>(row.get<int>(7));
            g.title = row.get_indicator(8) == soci::i_ok
                          ? row.get<std::string>(8) : "";
            g.msg   = row.get_indicator(9) == soci::i_ok
                          ? row.get<std::string>(9) : "";
            out.push_back(std::move(g));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociCmGiftRepository::LoadChart failed: {}",
            ex.what());
        out.clear();
    }
    return out;
}

std::uint8_t SociCmGiftRepository::CanTake(const std::string& target,
                                           std::uint16_t gift_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const std::string q =
            "DECLARE @ret INT; "
            "EXEC @ret = dbo.TCMGiftCanTake :n, " +
            std::to_string(static_cast<int>(gift_id)) + "; "
            "SELECT @ret;";
        int ret = kCmGiftFail;
        soci::indicator ind = soci::i_null;
        sql << q, soci::use(target), soci::into(ret, ind);
        if (ind != soci::i_ok)
            return kCmGiftFail;
        return static_cast<std::uint8_t>(ret);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociCmGiftRepository::CanTake('{}',{}) failed: "
                      "{}", target, gift_id, ex.what());
        return kCmGiftFail;                  // legacy m_bRET init
    }
}

std::optional<std::uint16_t> SociCmGiftRepository::Add(const CmGift& g)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const std::string q =
            "DECLARE @id SMALLINT; "
            "EXEC dbo.TCMGiftAdd @id OUTPUT, " +
            std::to_string(static_cast<int>(g.gift_type)) + ", " +
            std::to_string(static_cast<long long>(g.value)) + ", " +
            std::to_string(static_cast<int>(g.count)) + ", " +
            std::to_string(static_cast<int>(g.take_type)) + ", " +
            std::to_string(static_cast<int>(g.max_take_count)) + ", " +
            std::to_string(static_cast<int>(g.tool_only)) + ", " +
            std::to_string(static_cast<int>(g.err_gift_id)) +
            ", :t, :m; "
            "SELECT @id;";
        int new_id = 0;
        soci::indicator ind = soci::i_null;
        sql << q, soci::use(g.title), soci::use(g.msg),
            soci::into(new_id, ind);
        if (ind != soci::i_ok)
            return std::nullopt;
        return static_cast<std::uint16_t>(new_id);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociCmGiftRepository::Add failed: {}", ex.what());
        return std::nullopt;
    }
}

bool SociCmGiftRepository::Set(const CmGift& g)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int id = g.gift_id, ty = g.gift_type, cnt = g.count,
                  tk = g.take_type, mx = g.max_take_count,
                  to = g.tool_only, err = g.err_gift_id;
        const long long val = g.value;
        sql << "EXEC dbo.TCMGiftSet :id, :ty, :val, :cnt, :tk, :mx, "
               ":to, :err, :title, :msg",
            soci::use(id), soci::use(ty), soci::use(val),
            soci::use(cnt), soci::use(tk), soci::use(mx),
            soci::use(to), soci::use(err), soci::use(g.title),
            soci::use(g.msg);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociCmGiftRepository::Set({}) failed: {}",
            g.gift_id, ex.what());
        return false;
    }
}

bool SociCmGiftRepository::Del(std::uint16_t gift_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int id = gift_id;
        sql << "EXEC dbo.TCMGiftDel :id", soci::use(id);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociCmGiftRepository::Del({}) failed: {}",
            gift_id, ex.what());
        return false;
    }
}

} // namespace tworldsvr
