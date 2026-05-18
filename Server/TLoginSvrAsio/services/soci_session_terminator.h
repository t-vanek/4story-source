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

namespace tloginsvr::db { class SessionPool; }

namespace tloginsvr::services {

class SociSessionTerminator : public ISessionTerminator
{
public:
    explicit SociSessionTerminator(db::SessionPool& pool);

    void Terminate(std::int32_t user_id,
                   std::uint32_t session_key,
                   TerminationReason reason) override;

private:
    db::SessionPool& m_pool;
};

} // namespace tloginsvr::services
