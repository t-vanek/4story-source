#pragma once

// SOCI-backed INpcService. Loads the entire TNPCCHART into memory
// once at boot (NPCs are static world fixtures, not per-request
// data) and serves FindNpc out of an unordered_map keyed by wID.
//
// Construction reads the table; if the connection fails the
// constructor throws so main() can fall back to an empty registry
// (legacy treated missing TNPCCHART as "no NPCs in world").

#include "npc_service.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociNpcService final : public INpcService
{
public:
    explicit SociNpcService(fourstory::db::SessionPool& pool);

    std::optional<NpcRow>
        FindNpc(std::uint16_t npc_id) const override;

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint16_t, NpcRow> m_rows;
};

} // namespace tmapsvr
