#pragma once
#include "guild_level_repository.h"
#include "fourstory/db/session_pool.h"
#include <string>

namespace tworldsvr {
class PostgreSQLGuildLevelRepository final:public IGuildLevelRepository {
public:
    PostgreSQLGuildLevelRepository(fourstory::db::SessionPool& pool,std::uint8_t world,std::string owner_token);
    std::vector<TGuildLevelRow> LoadAll() override;
private:
    fourstory::db::SessionPool& m_pool;
    int m_world;
    std::string m_token;
};
}
