#include "in_memory_map_server_locator.h"

#include <utility>

namespace tloginsvr::services {

void InMemoryMapServerLocator::AddMapServer(std::uint8_t group_id,
                                            MapEndpoint endpoint)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_by_group[group_id] = std::move(endpoint);
}

std::optional<MapEndpoint>
InMemoryMapServerLocator::Lookup(std::int32_t /*user_id*/,
                                 std::uint8_t group_id,
                                 std::uint8_t /*channel*/,
                                 std::int32_t /*char_id*/)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_group.find(group_id); it != m_by_group.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void InMemoryMapServerLocator::AddGroup(GroupInfo info)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_groups.push_back(std::move(info));
}

void InMemoryMapServerLocator::AddChannel(std::uint8_t group_id, ChannelInfo info)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_channels[group_id].push_back(std::move(info));
}

std::vector<GroupInfo>
InMemoryMapServerLocator::ListGroups(std::int32_t /*user_id*/)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_groups;
}

std::vector<ChannelInfo>
InMemoryMapServerLocator::ListChannels(std::uint8_t group_id)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_channels.find(group_id); it != m_channels.end())
        return it->second;
    return {};
}

} // namespace tloginsvr::services
