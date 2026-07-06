#pragma once

// FakeRpsRepository — in-memory IRpsRepository for tests.

#include "services/rps_repository.h"

#include <mutex>
#include <tuple>

namespace tworldsvr {

class FakeRpsRepository : public IRpsRepository
{
public:
    void SeedGame(TRpsGame game);
    void SeedWinDate(RpsWinDateRow row);

    std::vector<TRpsGame> LoadGames() override;
    std::vector<RpsWinDateRow> LoadWinDates() override;
    bool Record(bool insert, std::uint32_t char_id, std::uint8_t type,
                std::uint8_t win_count, std::int64_t date_unix) override;

    // (insert, char_id, type, win_count, date)
    using RecordCall =
        std::tuple<bool, std::uint32_t, std::uint8_t, std::uint8_t,
                   std::int64_t>;
    std::vector<RecordCall> RecordCalls() const;

private:
    mutable std::mutex          m_mtx;
    std::vector<TRpsGame>       m_games;
    std::vector<RpsWinDateRow>  m_dates;
    std::vector<RecordCall>     m_records;
};

} // namespace tworldsvr
