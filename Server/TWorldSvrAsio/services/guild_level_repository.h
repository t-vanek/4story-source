#pragma once

// IGuildLevelRepository — read-only loader for TGUILDCHART. The
// table is immutable at runtime so the API is just `LoadAll`; the
// returned vector goes straight into GuildLevelCache::LoadFrom.
//
// Concrete impls live alongside the guild repository:
//   SociGuildLevelRepository  — TGUILDCHART via SOCI
//   FakeGuildLevelRepository  — in-memory seeded by AddRow,
//                               used by tests + the no-DB dev path

#include "services/guild_level_cache.h"

namespace tworldsvr {

class IGuildLevelRepository
{
public:
    virtual ~IGuildLevelRepository() = default;
    virtual std::vector<TGuildLevelRow> LoadAll() = 0;
};

} // namespace tworldsvr
