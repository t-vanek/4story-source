#pragma once

// SOCI-backed IMapServerLocator. Replaces the in-memory map-by-group
// lookup with a real query against TSERVER + TIPADDR (legacy schema).
//
// Lookup query:
//   SELECT s.bMachineID, s.wPort, s.bServerID, i.szIPAddr
//   FROM   TSERVER s
//   JOIN   TIPADDR i ON i.bMachineID = s.bMachineID AND i.bActive = 1
//   WHERE  s.bGroupID = :group AND s.bType = 4    -- SVRGRP_MAPSVR
//   ORDER  BY s.bServerID
//   LIMIT  1
//
// Channel + char_id are accepted but not yet consulted — the legacy
// server didn't shard maps per-channel and the BR override is a
// gameplay concern outside this interface's contract.

#include "map_server_locator.h"

namespace tloginsvr::db { class SessionPool; }

namespace tloginsvr::services {

class SociMapServerLocator : public IMapServerLocator
{
public:
    // `global_pool` (TGLOBAL) — TSERVER, TIPADDR, TGROUP, TCHANNEL,
    // TCURRENTUSER (live count).
    // `world_pool` (TGAME, optional) — TBRPLAYERTABLE / TBOWPLAYERTABLE
    // for shard-override routing on CS_START_REQ. If null, the shard
    // check is skipped (Lookup uses the default group-server target).
    explicit SociMapServerLocator(db::SessionPool& global_pool,
                                  db::SessionPool* world_pool = nullptr);

    std::optional<MapEndpoint> Lookup(
        std::int32_t  user_id,
        std::uint8_t  group_id,
        std::uint8_t  channel,
        std::int32_t  char_id) override;

    std::vector<GroupInfo>   ListGroups(std::int32_t user_id) override;
    std::vector<ChannelInfo> ListChannels(std::uint8_t group_id) override;

private:
    db::SessionPool& m_pool;
    db::SessionPool* m_world; // optional — null skips shard checks
};

} // namespace tloginsvr::services
