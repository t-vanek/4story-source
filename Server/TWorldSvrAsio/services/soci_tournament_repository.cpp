#include "services/soci_tournament_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>

namespace tworldsvr {

namespace {

// BT_TOURNAMENT (TWorldType.h:235).
constexpr int kBtTournament = 2;

std::string StrCol(const soci::row& row, std::size_t i)
{
    return row.get_indicator(i) == soci::i_ok
        ? row.get<std::string>(i) : std::string{};
}

// Numeric column tolerant of the backend's INT / BIGINT / NUMERIC
// mapping (the legacy `dw*` columns vary across chart tables).
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

template <class T>
T Num(const soci::row& row, std::size_t i)
{
    return static_cast<T>(NumCol(row, i));
}

// The entry-chart column tail shared by TTOURNAMENTCHART and
// TTNMTEVENTTABLE (name, type, class, fee, fee_back, permit item +
// count) starting at column `base`.
void ReadEntryTail(const soci::row& row, std::size_t base,
                   TournamentEntrySeed& e)
{
    e.name           = StrCol(row, base);
    e.type           = Num<std::uint8_t>(row, base + 1);
    e.cls            = Num<std::uint32_t>(row, base + 2);
    e.fee            = Num<std::uint32_t>(row, base + 3);
    e.fee_back       = Num<std::uint32_t>(row, base + 4);
    e.permit_item_id = Num<std::uint16_t>(row, base + 5);
    e.permit_count   = Num<std::uint8_t>(row, base + 6);
}

TnmtRewardRow ReadRewardRow(const soci::row& row)
{
    // Column order mirrors CTBLTournamentReward / CTBLTnmtEventReward
    // (DBAccess.h:1098 / 1220): entry, class, shield, chart, item,
    // count.
    TnmtRewardRow r;
    r.entry_id            = Num<std::uint8_t>(row, 0);
    r.reward.cls          = Num<std::uint32_t>(row, 1);
    r.reward.check_shield = Num<std::uint8_t>(row, 2);
    r.reward.chart_type   = Num<std::uint8_t>(row, 3);
    r.reward.item_id      = Num<std::uint16_t>(row, 4);
    r.reward.count        = Num<std::uint8_t>(row, 5);
    return r;
}

} // namespace

std::optional<TournamentBattleTime>
SociTournamentRepository::LoadBattleTime()
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bWeek\", \"bDay\", \"dwBattleStart\" "
            "FROM \"TBATTLETIMECHART\" WHERE \"bType\" = :t",
            soci::use(kBtTournament));
        for (const auto& row : rs)
        {
            TournamentBattleTime bt;
            bt.week      = Num<std::uint8_t>(row, 0);
            bt.day       = Num<std::uint8_t>(row, 1);
            bt.start_sec = Num<std::uint32_t>(row, 2);
            return bt;
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadBattleTime "
                      "failed: {}", ex.what());
    }
    return std::nullopt;
}

std::optional<std::uint8_t> SociTournamentRepository::LoadMaxLevel()
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "DECLARE @lvl TINYINT; "
            "EXEC dbo.TGetLimitedLevel @lvl OUTPUT; SELECT @lvl;");
        for (const auto& row : rs)
        {
            if (row.get_indicator(0) == soci::i_ok)
                return Num<std::uint8_t>(row, 0);
            break;
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadMaxLevel "
                      "failed: {}", ex.what());
    }
    return std::nullopt;
}

std::optional<TnmtCurrentStep>
SociTournamentRepository::LoadCurrentStep()
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"wID\", \"bGroup\", \"bStep\" "
            "FROM \"TTOURNAMENTSTATUSTABLE\"");
        for (const auto& row : rs)
        {
            TnmtCurrentStep s;
            s.id    = Num<std::uint16_t>(row, 0);
            s.group = Num<std::uint8_t>(row, 1);
            s.step  = Num<std::uint8_t>(row, 2);
            return s;
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadCurrentStep "
                      "failed: {}", ex.what());
    }
    return std::nullopt;
}

std::vector<TnmtScheduleRow>
SociTournamentRepository::LoadMainSchedule()
{
    std::vector<TnmtScheduleRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bGroup\", \"bStep\", \"dwPeriod\" "
            "FROM \"TTOURNAMENTSCHEDULECHART\" "
            "WHERE \"bGroup\" = 0 OR \"bGroup\" IN "
            "(SELECT \"bGroup\" FROM \"TTOURNAMENTCHART\" "
            "WHERE \"bEnable\" = 1)");
        for (const auto& row : rs)
        {
            TnmtScheduleRow r;
            r.group  = Num<std::uint8_t>(row, 0);
            r.step   = Num<std::uint8_t>(row, 1);
            r.period = Num<std::uint32_t>(row, 2);
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadMainSchedule "
                      "failed: {}", ex.what());
        out.clear();
    }
    return out;
}

std::vector<TournamentEntrySeed>
SociTournamentRepository::LoadMainEntries()
{
    std::vector<TournamentEntrySeed> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bGroup\", \"bEntryID\", \"szName\", \"bType\", "
            "\"dwClass\", \"dwFee\", \"dwFeeBack\", \"wItemID\", "
            "\"bItemCount\" "
            "FROM \"TTOURNAMENTCHART\" WHERE \"bEnable\" = 1");
        for (const auto& row : rs)
        {
            TournamentEntrySeed e;
            e.group    = Num<std::uint8_t>(row, 0);
            e.entry_id = Num<std::uint8_t>(row, 1);
            ReadEntryTail(row, 2, e);
            // Boot fixes the main chart's band (TWorldSvr.cpp:1637).
            e.min_level = 0;
            e.max_level = 0xFF;
            out.push_back(std::move(e));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadMainEntries "
                      "failed: {}", ex.what());
        out.clear();
    }
    return out;
}

std::vector<TnmtRewardRow> SociTournamentRepository::LoadMainRewards()
{
    std::vector<TnmtRewardRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bEntryID\", \"dwClass\", \"bCheckShield\", "
            "\"bChartType\", \"wItemID\", \"bItemCount\" "
            "FROM \"TTOURNAMENTREWARDCHART\" WHERE \"bItemCount\" > 0");
        for (const auto& row : rs)
            out.push_back(ReadRewardRow(row));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadMainRewards "
                      "failed: {}", ex.what());
        out.clear();
    }
    return out;
}

std::vector<TnmtEventTimeRow>
SociTournamentRepository::LoadEventTimes()
{
    std::vector<TnmtEventTimeRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"wTournamentID\", \"bWeek\", \"bDay\", "
            "\"dwStart\" FROM \"TTNMTEVENTTIMETABLE\"");
        for (const auto& row : rs)
        {
            TnmtEventTimeRow r;
            r.tour_id        = Num<std::uint16_t>(row, 0);
            r.time.week      = Num<std::uint8_t>(row, 1);
            r.time.day       = Num<std::uint8_t>(row, 2);
            r.time.start_sec = Num<std::uint32_t>(row, 3);
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadEventTimes "
                      "failed: {}", ex.what());
        out.clear();
    }
    return out;
}

std::vector<TnmtEventStepRow>
SociTournamentRepository::LoadEventSchedule(std::uint16_t tour_id)
{
    std::vector<TnmtEventStepRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        const int tid = tour_id;
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bStep\", \"dwPeriod\" "
            "FROM \"TTNMTEVENTSCHEDULETABLE\" "
            "WHERE \"wTournamentID\" = :t", soci::use(tid));
        for (const auto& row : rs)
        {
            TnmtEventStepRow r;
            r.step   = Num<std::uint8_t>(row, 0);
            r.period = Num<std::uint32_t>(row, 1);
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadEventSchedule({}) "
                      "failed: {}", tour_id, ex.what());
        out.clear();
    }
    return out;
}

std::vector<TournamentEntrySeed>
SociTournamentRepository::LoadEventEntries(std::uint16_t tour_id)
{
    std::vector<TournamentEntrySeed> out;
    try
    {
        auto lease = m_pool.Acquire();
        const int tid = tour_id;
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bEntryID\", \"szName\", \"bType\", \"dwClass\", "
            "\"dwFee\", \"dwFeeBack\", \"wItemID\", \"bItemCount\", "
            "\"bMinLevel\", \"bMaxLevel\" "
            "FROM \"TTNMTEVENTTABLE\" WHERE \"wTournamentID\" = :t",
            soci::use(tid));
        for (const auto& row : rs)
        {
            TournamentEntrySeed e;
            e.entry_id = Num<std::uint8_t>(row, 0);
            ReadEntryTail(row, 1, e);
            e.min_level = Num<std::uint8_t>(row, 8);
            e.max_level = Num<std::uint8_t>(row, 9);
            out.push_back(std::move(e));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadEventEntries({}) "
                      "failed: {}", tour_id, ex.what());
        out.clear();
    }
    return out;
}

std::vector<TnmtRewardRow>
SociTournamentRepository::LoadEventRewards(std::uint16_t tour_id)
{
    std::vector<TnmtRewardRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        const int tid = tour_id;
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"bEntryID\", \"dwClass\", \"bCheckShield\", "
            "\"bChartType\", \"wItemID\", \"bItemCount\" "
            "FROM \"TTNMTEVENTREWARDTABLE\" "
            "WHERE \"wTournamentID\" = :t", soci::use(tid));
        for (const auto& row : rs)
            out.push_back(ReadRewardRow(row));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::LoadEventRewards({}) "
                      "failed: {}", tour_id, ex.what());
        out.clear();
    }
    return out;
}

void SociTournamentRepository::SaveEventSchedule(
    std::uint16_t tour_id, const TournamentBattleTime& time,
    const std::vector<TnmtEventStepRow>& steps)
{
    try
    {
        auto lease = m_pool.Acquire();
        // OnDM_TNMTEVENTSCHEDULEADD_REQ (SSHandler.cpp:12698).
        *lease << "EXEC dbo.TTnmtEventTime " +
            std::to_string(static_cast<int>(tour_id)) + ", " +
            std::to_string(static_cast<int>(time.week)) + ", " +
            std::to_string(static_cast<int>(time.day)) + ", " +
            std::to_string(static_cast<long long>(time.start_sec));
        for (const auto& s : steps)
            *lease << "EXEC dbo.TTnmtEventSchedule " +
                std::to_string(static_cast<int>(tour_id)) + ", " +
                std::to_string(static_cast<int>(s.step)) + ", " +
                std::to_string(static_cast<long long>(s.period));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::SaveEventSchedule({}) "
                      "failed: {}", tour_id, ex.what());
    }
}

void SociTournamentRepository::DeleteEvent(std::uint16_t tour_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        *lease << "EXEC dbo.TTnmtEventDel " +
            std::to_string(static_cast<int>(tour_id));
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::DeleteEvent({}) "
                      "failed: {}", tour_id, ex.what());
    }
}

void SociTournamentRepository::SaveEventEntries(
    std::uint16_t tour_id,
    const std::vector<TournamentEntrySeed>& entries)
{
    try
    {
        auto lease = m_pool.Acquire();
        // OnDM_TNMTEVENTENTRYADD_REQ (SSHandler.cpp:12748): the
        // entry-0 + empty-name call clears the event's rows.
        const std::string tid =
            std::to_string(static_cast<int>(tour_id));
        std::string empty{};
        *lease << "EXEC dbo.TTnmtEventEntry " + tid +
            ", 0, :name, 0, 0, 0, 0, 0, 0, 0, 0",
            soci::use(empty);
        for (const auto& e : entries)
        {
            *lease << "EXEC dbo.TTnmtEventEntry " + tid + ", " +
                std::to_string(static_cast<int>(e.entry_id)) +
                ", :name, " +
                std::to_string(static_cast<int>(e.type)) + ", " +
                std::to_string(static_cast<long long>(e.cls)) + ", " +
                std::to_string(static_cast<long long>(e.fee)) + ", " +
                std::to_string(static_cast<long long>(e.fee_back)) +
                ", " +
                std::to_string(static_cast<int>(e.permit_item_id)) +
                ", " +
                std::to_string(static_cast<int>(e.permit_count)) +
                ", " +
                std::to_string(static_cast<int>(e.min_level)) + ", " +
                std::to_string(static_cast<int>(e.max_level)),
                soci::use(e.name);
            for (const auto& r : e.rewards)
                *lease << "EXEC dbo.TTnmtEventReward " + tid + ", " +
                    std::to_string(static_cast<int>(e.entry_id)) +
                    ", " +
                    std::to_string(static_cast<int>(r.chart_type)) +
                    ", " +
                    std::to_string(static_cast<int>(r.item_id)) +
                    ", " +
                    std::to_string(static_cast<int>(r.count)) + ", " +
                    std::to_string(static_cast<long long>(r.cls)) +
                    ", " +
                    std::to_string(static_cast<int>(r.check_shield));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::SaveEventEntries({}) "
                      "failed: {}", tour_id, ex.what());
    }
}

std::vector<TnmtPlayerBrief>
SociTournamentRepository::FindCharInfoByName(const std::string& name)
{
    std::vector<TnmtPlayerBrief> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = (lease->prepare <<
            "SELECT \"dwCharID\", \"szName\", \"bCountry\", "
            "\"bLevel\", \"bClass\" "
            "FROM \"TCHARTABLE\" WHERE \"szName\" = :n",
            soci::use(name));
        for (const auto& row : rs)
        {
            TnmtPlayerBrief b;
            b.char_id = Num<std::uint32_t>(row, 0);
            b.name    = StrCol(row, 1);
            b.country = Num<std::uint8_t>(row, 2);
            b.level   = Num<std::uint8_t>(row, 3);
            b.cls     = Num<std::uint8_t>(row, 4);
            out.push_back(std::move(b));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::FindCharInfoByName "
                      "failed: {}", ex.what());
        out.clear();
    }
    return out;
}

void SociTournamentRepository::ApplyPlayer(const TnmtApplyOp& op)
{
    try
    {
        auto lease = m_pool.Acquire();
        *lease << "EXEC dbo.TTournamentApply " +
            std::to_string(op.add ? 1 : 0) + ", " +
            std::to_string(static_cast<long long>(op.char_id)) + ", " +
            std::to_string(static_cast<int>(op.entry_id)) + ", " +
            std::to_string(static_cast<long long>(op.chief_id)) +
            ", :hwid, " +
            std::to_string(static_cast<long long>(op.ip_addr)),
            soci::use(op.hwid);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::ApplyPlayer(char={}) "
                      "failed: {}", op.char_id, ex.what());
    }
}

void SociTournamentRepository::ClearPersisted()
{
    try
    {
        auto lease = m_pool.Acquire();
        // CSPTournamentClear with bClear=TRUE (SSHandler.cpp:12043).
        *lease << "EXEC dbo.TTournamentClear 1";
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociTournamentRepository::ClearPersisted "
                      "failed: {}", ex.what());
    }
}

} // namespace tworldsvr
