#pragma once

// IMapServerLocator — resolves "where do I send this character?" for
// CS_START_REQ.
//
// Legacy server queries TSERVER + TIPADDR (joined on bMachineID) for
// rows where bGroupID matches the requested world AND bType == 4
// (SVRGRP_MAPSVR). Returns the first active row, or the special BR
// (Battle Royale) shard ID 50 for characters registered in
// TBRPLAYERTABLE — see CSHandler.cpp:1387-1398.
//
// Phase A.5 keeps the BR override out of the interface — that's a
// gameplay concern that the eventual SociMapServerLocator handles
// internally via a separate JOIN. The interface here just gives the
// "regular" lookup; BR routing is a service-impl detail.

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace tloginsvr::services {

struct MapEndpoint
{
    // IPv4 octets in network byte order — so the wire bytes match
    // what the legacy server sends (inet_addr() return value, sent
    // as DWORD straight into the packet buffer on a little-endian
    // host, which happens to lay down the bytes in network order).
    std::array<std::uint8_t, 4> ipv4 = {0, 0, 0, 0};
    std::uint16_t               port = 0;
    std::uint8_t                server_id = 0;
};

class IMapServerLocator
{
public:
    virtual ~IMapServerLocator() = default;

    // Resolve an endpoint for (`user_id`, `group_id`, `channel`, `char_id`).
    // The tuple matches the legacy CS_START_REQ wire format + the
    // session-level user_id needed by the BR/BOW override checks.
    //
    // Behavior reference (legacy CSHandler.cpp:1312-1430):
    //   1. Find the "natural" server_id for the character (channel/map
    //      based). Current impl uses "first map server in group" — a
    //      future SociMapServerLocator revision can plug in the
    //      character-specific routing once we wire CSPFindServerID's
    //      logic against the schema's map placement columns.
    //   2. If (user_id, char_id) ∈ TBOWPLAYERTABLE → override
    //      server_id to BOW_SERVER_ID (30).
    //   3. If (user_id, char_id) ∈ TBRPLAYERTABLE → override
    //      server_id AND channel to BR_SERVER_ID (50).
    //
    // Returns nullopt if no Map server is available for the chosen
    // server_id (covers SR_NOSERVER / SR_NOGROUP at the wire level).
    virtual std::optional<MapEndpoint> Lookup(
        std::int32_t  user_id,
        std::uint8_t  group_id,
        std::uint8_t  channel,
        std::int32_t  char_id) = 0;
};

} // namespace tloginsvr::services
