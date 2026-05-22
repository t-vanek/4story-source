#pragma once

// FakeGuildLevelRepository — in-memory IGuildLevelRepository for
// tests and dev runs without a DB. AddRow seeds individual
// levels; LoadAll returns the seed verbatim.

#include "services/guild_level_repository.h"

#include <mutex>

namespace tworldsvr {

class FakeGuildLevelRepository : public IGuildLevelRepository
{
public:
    void AddRow(const TGuildLevelRow& row);

    std::vector<TGuildLevelRow> LoadAll() override;

private:
    mutable std::mutex          m_mtx;
    std::vector<TGuildLevelRow> m_rows;
};

} // namespace tworldsvr
