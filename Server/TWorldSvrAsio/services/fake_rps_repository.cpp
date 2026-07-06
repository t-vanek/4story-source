#include "services/fake_rps_repository.h"

namespace tworldsvr {

void FakeRpsRepository::SeedGame(TRpsGame game)
{
    std::lock_guard g(m_mtx);
    m_games.push_back(std::move(game));
}

void FakeRpsRepository::SeedWinDate(RpsWinDateRow row)
{
    std::lock_guard g(m_mtx);
    m_dates.push_back(row);
}

std::vector<TRpsGame> FakeRpsRepository::LoadGames()
{
    std::lock_guard g(m_mtx);
    return m_games;
}

std::vector<RpsWinDateRow> FakeRpsRepository::LoadWinDates()
{
    std::lock_guard g(m_mtx);
    return m_dates;
}

bool FakeRpsRepository::Record(bool insert, std::uint32_t char_id,
                               std::uint8_t type,
                               std::uint8_t win_count,
                               std::int64_t date_unix)
{
    std::lock_guard g(m_mtx);
    m_records.emplace_back(insert, char_id, type, win_count, date_unix);
    return true;
}

std::vector<FakeRpsRepository::RecordCall>
FakeRpsRepository::RecordCalls() const
{
    std::lock_guard g(m_mtx);
    return m_records;
}

} // namespace tworldsvr
