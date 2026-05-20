#pragma once

// IOperatorAuthService — backs the CSPOPLogin stored procedure that
// CT_OPLOGIN_REQ / CT_STLOGIN_REQ resolve against. The SP takes a
// (szID, szPW) pair and returns an authority byte in [0..6]; 0 means
// "auth failed", any non-zero value is the operator's MANAGER_CLASS
// (see TControlType.h MANAGER_ALL / CONTROL / USER / SERVICE /
// GMLEVEL1..3). Authority 1 (= MANAGER_ALL) is restricted to
// 127.0.0.1 connections per the legacy gate in OnCT_OPLOGIN_REQ.
//
// Production impl: SOCI proc call against TGLOBAL_RAGEZONE.TOPLogin
// (added in F2 with the rest of the service inventory). F1 ships
// FakeOperatorAuthService only so the wire path can be tested in
// isolation.

#include <cstdint>
#include <string>

namespace tcontrolsvr {

struct OperatorAuthResult
{
    bool          ok        = false;   // SP returned non-zero authority
    std::uint8_t  authority = 0;       // 1..6 mapping MANAGER_CLASS
};

class IOperatorAuthService
{
public:
    virtual ~IOperatorAuthService() = default;

    // Synchronous in F1 — the fake is in-memory; the SOCI impl in F2
    // will run on a worker via fourstory::db::SessionPool::run_on_pool,
    // exposed as `awaitable<OperatorAuthResult>`. Keeping this sync
    // for now so the dispatch path stays straightforward to test.
    virtual OperatorAuthResult Authenticate(const std::string& user_id,
                                            const std::string& password) = 0;
};

} // namespace tcontrolsvr
