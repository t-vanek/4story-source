#pragma once

// Map session validator — looks up the TCURRENTUSER row TLoginSvrAsio
// wrote at login. The handshake handler in handlers.cpp uses it to
// verify the (dwUserID, dwKEY) pair the client sends in
// CS_CONNECT_REQ matches what's actually pending in the session table.
//
// Two implementations:
//   SociMapSessionValidator    production — SOCI query against TUSER
//   FakeMapSessionValidator    tests + dev — in-memory std::map
//
// Both share the IMapSessionValidator interface so the handler doesn't
// know which one it's talking to. The same pattern is used by the
// other Asio servers' service layers.

#include "domain/session.h"
#include "server_route_resolver.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace tmapsvr {
struct CharSnapshot;

class IMapSessionValidator
{
public:
    virtual ~IMapSessionValidator() = default;

    // Look up the session row for (user_id, key). Returns an empty
    // optional when no row matches — handler treats that as "session
    // token rejected". A populated optional means "row found", but
    // the handler still validates the channel / lock fields against
    // the claims the client sent in the packet body.
    virtual std::optional<MapSessionInfo>
        LookupSession(std::uint32_t user_id, std::uint32_t key) = 0;

    // The native backend atomically consumes Login's pending reservation.
    // The connection generation is reserved before offloading the transaction,
    // so disconnect can always release an in-flight claim by the same identity.
    virtual std::optional<MapSessionInfo> ClaimSession(const MapSessionClaim&, const MapSessionInfo& candidate) {
        return candidate;
    }
    virtual std::optional<std::vector<std::uint8_t>> NeighborServers(const MapSessionClaim&,std::uint16_t,float,float) { return std::nullopt; }
    virtual std::optional<std::vector<std::uint8_t>> MovementServers(const MapSessionClaim&,std::uint16_t,float,float) { return std::nullopt; }
    virtual bool OwnsCell(const MapSessionClaim&,std::uint16_t,float,float) { return true; }
    virtual bool AuthorizeReplicas(const MapSessionClaim&,std::uint16_t,float,float,const std::vector<ServerRoute>&) { return false; }
    virtual bool LoadReplica(const MapSessionClaim&,std::uint16_t,float,float) { return false; }
    virtual void MarkReady(const MapSessionClaim&) {}
    virtual void MarkReady(const MapSessionClaim& claim,const CharSnapshot&) { MarkReady(claim); }
    virtual void ReleaseSession(const MapSessionClaim&) {}

};

} // namespace tmapsvr
