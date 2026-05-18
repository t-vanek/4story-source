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

    // Resolve an endpoint for (`group_id`, `channel`, `char_id`).
    // The triplet matches the legacy CS_START_REQ wire format —
    // most impls only consult group_id, but the full signature is
    // future-proof for per-channel sharding or BR override paths.
    // Returns nullopt if no Map server is available.
    virtual std::optional<MapEndpoint> Lookup(
        std::uint8_t  group_id,
        std::uint8_t  channel,
        std::int32_t  char_id) = 0;
};

} // namespace tloginsvr::services
