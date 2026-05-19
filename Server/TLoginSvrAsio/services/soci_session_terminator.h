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

    // Clear pre-handoff TCURRENTUSER rows (`WHERE dwCharID = 0`).
    // Mirrors legacy CSPClearLoginUser / TClearLoginCurrentUser SP,
    // which CTLoginSvrModule::OnEnter calls right after InitNetwork:
    // if the previous process crashed with sessions stuck on the
    // group/char-select screen, the rows are still there and the next
    // login would hit LR_DUPLICATE forever. Returns the row-count so
    // the caller can log it.
    //
    // Sessions already handed off to a map server (dwCharID != 0) are
    // preserved — wiping them would desync global session state from
    // the world server's in-memory state and silently break the
    // cross-instance duplicate-kick path.
    int ClearStaleSessions();

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tloginsvr::services
