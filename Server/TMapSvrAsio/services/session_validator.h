#pragma once

// IMapSessionValidator — the token-of-trust check the map server runs
// on CS_CONNECT_REQ. Legacy CTMapSvrModule::OnCS_CONNECT_REQ
// (CSHandler.cpp:249) called the SP `TCheckMapChar(dwUserID, dwCharID,
// dwKEY)` against TGLOBAL_RAGEZONE.TCURRENTUSER. The SP is commented
// out in the shipped source ("/*DEFINE_QUERY(&m_db, CSPCheckMapChar)*/")
// — production deploys rely on the lock-free TCURRENTUSER row that
// TLoginSvr writes on CS_START_REQ as the cross-server handshake.
//
// Modern path: inline parameterized query against TCURRENTUSER (no SP)
// — `SELECT 1 FROM TCURRENTUSER WHERE dwUserID=:u AND dwCharID=:c AND
// dwKEY=:k`. Returns true iff the row exists. SOCI impl arrives in
// F2; F1 wires the interface + a fake that accepts every key.

#include <cstdint>

namespace tmapsvr {

struct MapSessionLookup
{
    std::uint32_t user_id  = 0;
    std::uint32_t char_id  = 0;
    std::uint32_t dw_key   = 0;
    std::uint8_t  channel  = 0;
};

class IMapSessionValidator
{
public:
    virtual ~IMapSessionValidator() = default;

    // Returns true iff `lookup` matches a live TCURRENTUSER row that
    // was minted by TLoginSvr on the player's CS_START_REQ. False on
    // any of: row absent, dwKEY mismatch, DB error (treated as deny —
    // legacy convention).
    virtual bool Validate(const MapSessionLookup& lookup) = 0;
};

} // namespace tmapsvr
