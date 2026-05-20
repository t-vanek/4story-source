#pragma once

// IUserProtectedService — backs the legacy CSPUserProtectedAdd
// stored procedure (Server/TControlSvr/DBAccess.h):
//
//   { ? = CALL TUserProtectedAdd(?, ?, ?, ?, ?) }
//   OUTPUT  m_nRET        INT
//   INPUT   szUserID      NVARCHAR
//   INPUT   dwDuration    INT          — duration in days
//   INPUT   szReason      NVARCHAR
//   INPUT   bPermanent    TINYINT      — 1 = permanent, 0 = timed
//   INPUT   szOperator    NVARCHAR     — who issued the ban
//
// Returns the SP's nRET as the wire-level bRet used in
// CT_USERPROTECTED_ACK.

#include <cstdint>
#include <string>

namespace tcontrolsvr {

class IUserProtectedService
{
public:
    virtual ~IUserProtectedService() = default;

    virtual std::uint8_t AddBan(const std::string& user_id,
                                std::uint32_t duration_days,
                                const std::string& reason,
                                std::uint8_t permanent,
                                const std::string& operator_id) = 0;
};

} // namespace tcontrolsvr
