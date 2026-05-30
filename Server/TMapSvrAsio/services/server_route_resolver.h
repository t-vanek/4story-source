#pragma once

// IServerRouteResolver — resolves cluster server ids to their live
// network address. Backs OnMW_ROUTELIST_REQ: TWorld hands the map a set
// of server ids and the map answers MW_ROUTE_ACK with each id's ip/port.
//
// Legacy did this through the DB-batch process: OnMW_ROUTELIST_REQ
// forwarded DM_ROUTE_REQ, and the batch ran the CSPRoute query
// (SSHandler.cpp:6534 — filter by group id + SVRGRP_MAPSVR + server id,
// return ip/port) before DM_ROUTE_ACK looped the addresses back. The
// modern map owns its DB in-process, so that collapses to a single
// resolver call; the production SOCI implementation against the cluster
// server-registry table is a follow-up, hence the interface + fake here
// (same interface/fake/soci staging the other map services use).

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

// One resolved server endpoint. Structurally the same tuple the wire
// carries (DWORD ip, WORD port, BYTE server_id), kept distinct from
// client_senders::ConnectRoute so the world-routing and client-relay
// paths don't couple.
struct ServerRoute
{
    std::uint32_t ip_addr   = 0;
    std::uint16_t port      = 0;
    std::uint8_t  server_id = 0;
};

class IServerRouteResolver
{
public:
    virtual ~IServerRouteResolver() = default;

    // Resolve the given server ids (within the map's group) to their
    // endpoints. Unknown ids are omitted from the result — the legacy
    // CSPRoute query simply returned no row for them.
    virtual std::vector<ServerRoute> Resolve(
        std::uint8_t                     group_id,
        const std::vector<std::uint8_t>& server_ids) = 0;
};

// Header-only in-memory resolver for tests + dev runs without the
// cluster server-registry table. Pre-load with Add().
class FakeServerRouteResolver final : public IServerRouteResolver
{
public:
    void Add(std::uint8_t server_id, std::uint32_t ip_addr, std::uint16_t port)
    {
        m_rows[server_id] = ServerRoute{ ip_addr, port, server_id };
    }

    std::vector<ServerRoute> Resolve(
        std::uint8_t /*group_id*/,
        const std::vector<std::uint8_t>& server_ids) override
    {
        std::vector<ServerRoute> out;
        out.reserve(server_ids.size());
        for (const auto id : server_ids)
        {
            const auto it = m_rows.find(id);
            if (it != m_rows.end())
                out.push_back(it->second);
        }
        return out;
    }

private:
    std::unordered_map<std::uint8_t, ServerRoute> m_rows;
};

} // namespace tmapsvr
