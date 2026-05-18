#pragma once

// In-memory IMapServerLocator. Seeded via AddMapServer; production
// dev mode wires one entry pointing at a co-located test Map binary.

#include "map_server_locator.h"

#include <mutex>
#include <unordered_map>

namespace tloginsvr::services {

class InMemoryMapServerLocator : public IMapServerLocator
{
public:
    // Register the Map endpoint that handles `group_id`. Overwrites
    // any previous entry for that group.
    void AddMapServer(std::uint8_t group_id, MapEndpoint endpoint);

    std::optional<MapEndpoint> Lookup(
        std::uint8_t group_id,
        std::uint8_t channel,
        std::int32_t char_id) override;

private:
    mutable std::mutex                            m_mtx;
    std::unordered_map<std::uint8_t, MapEndpoint> m_by_group;
};

} // namespace tloginsvr::services
