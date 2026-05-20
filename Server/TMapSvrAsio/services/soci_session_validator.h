#pragma once

// SOCI-backed IMapSessionValidator. Reads TGLOBAL_RAGEZONE.TCURRENTUSER
// to verify the (dwUserID, dwCharID, dwKEY) tuple a client presents on
// CS_CONNECT_REQ matches a row that TLoginSvr wrote during CS_START_REQ.
//
// Wire-faithful to legacy CSHandler.cpp:305-313 — the commented-out
// SP `CSPCheckMapChar` had the same semantic; modern path inlines the
// query so the SP is no longer required on the deployed DB.

#include "session_validator.h"

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociMapSessionValidator final : public IMapSessionValidator
{
public:
    explicit SociMapSessionValidator(fourstory::db::SessionPool& pool);

    // Returns true iff a TCURRENTUSER row matches lookup.{user_id,
    // char_id, dw_key}. DB errors are logged and treated as deny
    // (legacy parity — `bRet = TRUE` branch when the SP call fails).
    bool Validate(const MapSessionLookup& lookup) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
