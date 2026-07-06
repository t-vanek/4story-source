#include "services/soci_lucky_event_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>

namespace tworldsvr {

namespace {

// TGetItemName(5 in ids, 5 out names) — one DECLARE/EXEC/SELECT.
void ResolveItemNames(soci::session& sql, LuckyEvent& e)
{
    std::string q = "DECLARE @n1 NVARCHAR(64), @n2 NVARCHAR(64), "
                    "@n3 NVARCHAR(64), @n4 NVARCHAR(64), "
                    "@n5 NVARCHAR(64); "
                    "EXEC dbo.TGetItemName ";
    for (int i = 0; i < 5; ++i)
        q += std::to_string(static_cast<int>(e.item_id[i])) + ", ";
    q += "@n1 OUTPUT, @n2 OUTPUT, @n3 OUTPUT, @n4 OUTPUT, @n5 OUTPUT; "
         "SELECT @n1, @n2, @n3, @n4, @n5;";
    soci::rowset<soci::row> rs = (sql.prepare << q);
    for (const auto& row : rs)
    {
        for (int i = 0; i < 5; ++i)
            e.item_name[i] =
                row.get_indicator(i) == soci::i_ok
                    ? row.get<std::string>(static_cast<std::size_t>(i))
                    : "";
        break;
    }
}

} // namespace

std::vector<LuckyEvent>
SociLuckyEventRepository::List(std::uint8_t day)
{
    std::vector<LuckyEvent> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int d = day;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"wID\", \"bDay\", \"bHour\", \"bMinute\", "
            "\"wItemID1\", \"wItemID2\", \"wItemID3\", \"wItemID4\", "
            "\"wItemID5\", \"bCount\", \"szPresent\", \"szAnnounce\", "
            "\"szTitle\", \"szMessage\" "
            "FROM \"TEVENTQUARTERCHART\" WHERE \"bDay\" = :d",
            soci::use(d));
        for (const auto& row : rs)
        {
            LuckyEvent e{};
            e.id     = static_cast<std::uint16_t>(row.get<int>(0));
            e.day    = static_cast<std::uint8_t>(row.get<int>(1));
            e.hour   = static_cast<std::uint8_t>(row.get<int>(2));
            e.minute = static_cast<std::uint8_t>(row.get<int>(3));
            for (int i = 0; i < 5; ++i)
                e.item_id[i] = static_cast<std::uint16_t>(
                    row.get<int>(static_cast<std::size_t>(4 + i)));
            e.count = static_cast<std::uint8_t>(row.get<int>(9));
            auto str = [&row](std::size_t i) {
                return row.get_indicator(i) == soci::i_ok
                           ? row.get<std::string>(i) : std::string{};
            };
            e.present  = str(10);
            e.announce = str(11);
            e.title    = str(12);
            e.message  = str(13);
            out.push_back(std::move(e));
        }
        for (auto& e : out)
            ResolveItemNames(sql, e);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociLuckyEventRepository::List({}) failed: {}",
            day, ex.what());
        out.clear();
    }
    return out;
}

std::optional<LuckyEventUpdateResult>
SociLuckyEventRepository::Update(std::uint8_t type,
                                 const LuckyEvent& event)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        // 1 RET + 16 IN + 6 OUT, PARAM_ENTRY order
        // (DBAccess.h:2616).
        std::string q =
            "DECLARE @ret INT, @oid SMALLINT, "
            "@n1 NVARCHAR(64), @n2 NVARCHAR(64), @n3 NVARCHAR(64), "
            "@n4 NVARCHAR(64), @n5 NVARCHAR(64); "
            "EXEC @ret = dbo.TEventQuarterUpdate " +
            std::to_string(static_cast<int>(type)) + ", " +
            std::to_string(static_cast<int>(event.id)) + ", " +
            std::to_string(static_cast<int>(event.day)) + ", " +
            std::to_string(static_cast<int>(event.hour)) + ", " +
            std::to_string(static_cast<int>(event.minute)) + ", ";
        for (int i = 0; i < 5; ++i)
            q += std::to_string(static_cast<int>(event.item_id[i])) +
                 ", ";
        q += std::to_string(static_cast<int>(event.count)) +
             ", :present, :announce, :title, :message, "
             "@oid OUTPUT, @n1 OUTPUT, @n2 OUTPUT, @n3 OUTPUT, "
             "@n4 OUTPUT, @n5 OUTPUT; "
             "SELECT @ret, @oid, @n1, @n2, @n3, @n4, @n5;";

        LuckyEventUpdateResult res{};
        res.event = event;
        soci::rowset<soci::row> rs = (sql.prepare << q,
            soci::use(event.present), soci::use(event.announce),
            soci::use(event.title), soci::use(event.message));
        for (const auto& row : rs)
        {
            res.ret = row.get_indicator(0) == soci::i_ok
                          ? static_cast<std::uint8_t>(row.get<int>(0))
                          : 1;
            if (type == kEkAdd &&
                row.get_indicator(1) == soci::i_ok)
                res.event.id =
                    static_cast<std::uint16_t>(row.get<int>(1));
            for (int i = 0; i < 5; ++i)
                res.event.item_name[i] =
                    row.get_indicator(static_cast<std::size_t>(2 + i))
                            == soci::i_ok
                        ? row.get<std::string>(
                              static_cast<std::size_t>(2 + i))
                        : "";
            return res;
        }
        return res;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociLuckyEventRepository::Update(type={}, "
                      "id={}) failed: {}", type, event.id, ex.what());
        return std::nullopt;
    }
}

} // namespace tworldsvr
