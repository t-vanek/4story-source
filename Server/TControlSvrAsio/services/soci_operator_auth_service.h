#pragma once

// SOCI-backed IOperatorAuthService — calls the legacy TOPLogin
// stored procedure (Server/TControlSvr/DBAccess.h CSPOPLogin).
//
// Procedure shape:
//   { ? = CALL TOPLogin(?, ?) }
//   OUTPUT @authority INT     — 1..6 on success, 0 / NULL on reject
//   INPUT  @szID      NVARCHAR
//   INPUT  @szPW      NVARCHAR
//
// The SP returns 1 (MANAGER_ALL) for the console operator; the
// loopback gate enforced in the handler (handlers_auth.cpp) blocks
// remote auths from claiming that role.

#include "operator_auth_service.h"
#include "fourstory/db/session_pool.h"

namespace tcontrolsvr {

class SociOperatorAuthService final : public IOperatorAuthService
{
public:
    explicit SociOperatorAuthService(fourstory::db::SessionPool& pool);

    OperatorAuthResult Authenticate(const std::string& user_id,
                                    const std::string& password) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tcontrolsvr
