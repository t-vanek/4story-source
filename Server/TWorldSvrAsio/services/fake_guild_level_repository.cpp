#include "services/fake_guild_level_repository.h"

namespace tworldsvr {

void FakeGuildLevelRepository::AddRow(const TGuildLevelRow& row)
{
    std::lock_guard lock(m_mtx);
    m_rows.push_back(row);
}

std::vector<TGuildLevelRow> FakeGuildLevelRepository::LoadAll()
{
    std::lock_guard lock(m_mtx);
    return m_rows;
}

} // namespace tworldsvr
