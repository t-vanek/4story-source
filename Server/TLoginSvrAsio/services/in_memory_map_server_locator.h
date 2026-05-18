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

    // Seed methods for the lobby list calls. Status defaults to
    // GroupStatus::Normal; callers that want to test the busy/full
    // paths can pass an explicit value.
    void AddGroup(GroupInfo info);
    void AddChannel(std::uint8_t group_id, ChannelInfo info);

    std::optional<MapEndpoint> Lookup(
        std::int32_t user_id,
        std::uint8_t group_id,
        std::uint8_t channel,
        std::int32_t char_id) override;

    std::vector<GroupInfo>   ListGroups(std::int32_t user_id) override;
    std::vector<ChannelInfo> ListChannels(std::uint8_t group_id) override;

private:
    mutable std::mutex                            m_mtx;
    std::unordered_map<std::uint8_t, MapEndpoint> m_by_group;
    std::vector<GroupInfo>                        m_groups;
    std::unordered_map<std::uint8_t, std::vector<ChannelInfo>> m_channels;
};

} // namespace tloginsvr::services
