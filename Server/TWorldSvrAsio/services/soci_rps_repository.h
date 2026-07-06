#pragma once

// SociRpsRepository — IRpsRepository against TRPSGAMECHART /
// TRPSGAMERECORDTABLE / the TRPSGameRecord SP on the world pool.

#include "services/rps_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociRpsRepository : public IRpsRepository
{
public:
    explicit SociRpsRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<TRpsGame> LoadGames() override;
    std::vector<RpsWinDateRow> LoadWinDates() override;
    bool Record(bool insert, std::uint32_t char_id, std::uint8_t type,
                std::uint8_t win_count, std::int64_t date_unix) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
