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

#include <cstdint>
#include <optional>

namespace tmapsvr {

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
};

} // namespace tmapsvr
