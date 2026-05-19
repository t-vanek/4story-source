#pragma once

// SOCI-backed ISessionTerminator. Mirrors the legacy TLogout SP:
//   * DELETE FROM TCURRENTUSER WHERE dwUserID = :uid
//   * UPDATE TLOG SET timeLOGOUT = CURRENT_TIMESTAMP WHERE dwKEY = :key
//
// Map-handoff exception: when reason == MapHandoff the TCURRENTUSER
// row is left intact so the Map server can validate the handoff dwKEY
// against it. The audit-log timestamp still updates so ops can see the
// transition from Login → Map happened.

#include "session_terminator.h"

namespace fourstory::db { class SessionPool; }

namespace tloginsvr::services {

class SociSessionTerminator : public ISessionTerminator
{
public:
    explicit SociSessionTerminator(fourstory::db::SessionPool& pool);

    void Terminate(std::int32_t  user_id,
                   std::uint32_t session_key,
                   TerminationReason reason,
                   std::int32_t  char_id = 0) override;

    // Bulk-clear every TCURRENTUSER row. Mirrors legacy
    // CSPClearLoginUser, which CTLoginSvrModule::OnEnter calls right
    // after InitNetwork: if the previous process crashed with live
    // sessions the rows are still there and the next login would hit
    // LR_DUPLICATE forever. Returning the row-count so the caller can
    // log it (operators want to know when recovery picks something up).
    int ClearStaleSessions();

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tloginsvr::services
