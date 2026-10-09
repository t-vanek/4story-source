#pragma once
#include <string>

// Native PostgreSQL routing requires the authenticated StartAuthorized path and
// a pinned backup catalog. Lookup remains available for the comparison backend.

#include "map_server_locator.h"

namespace fourstory::db { class SessionPool; }

namespace tloginsvr::services {

class SociMapServerLocator : public IMapServerLocator
{
public:
    // `global_pool` (TGLOBAL) — TSERVER, TIPADDR, TGROUP, TCHANNEL,
    // TCURRENTUSER (live count).
    // `world_pool` (TGAME, optional) — TBRPLAYERTABLE / TBOWPLAYERTABLE
    // for shard-override routing on CS_START_REQ. If null, the shard
    // check is skipped (Lookup uses the default group-server target).
    explicit SociMapServerLocator(fourstory::db::SessionPool& global_pool,
                                  fourstory::db::SessionPool* world_pool = nullptr,
                                  std::string owner_token = {},
                                  std::string routing_manifest = {});

    std::optional<MapEndpoint> Lookup(
        std::int32_t  user_id,
        std::uint8_t  group_id,
        std::uint8_t  channel,
        std::int32_t  char_id) override;

    StartResponse StartAuthorized(const StartRequest& request) override;

    std::vector<GroupInfo>   ListGroups(std::int32_t user_id) override;
    std::vector<ChannelInfo> ListChannels(std::uint8_t group_id) override;

private:
    StartResponse StartNative(const StartRequest& request);
    std::string m_routing_manifest;
    fourstory::db::SessionPool& m_pool;
    std::string m_owner_token;
    fourstory::db::SessionPool* m_world; // optional — null skips shard checks
};

} // namespace tloginsvr::services
