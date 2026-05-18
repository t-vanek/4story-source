#pragma once

// FakeSessionTerminator — TEST-ONLY ISessionTerminator. Doesn't touch
// a database; instead keeps a counter + a vector of TerminationRecord
// entries so tests can assert "the terminator was called with these
// arguments".
//
// **Not for production.** Production uses SociSessionTerminator
// which DELETEs from TCURRENTUSER and UPDATEs TLOG.timeLOGOUT on
// real disconnect, or preserves TCURRENTUSER on MapHandoff so the
// Map server can validate dwKEY on the reconnect.

#include "session_terminator.h"

#include <mutex>
#include <vector>

namespace tloginsvr::services {

struct TerminationRecord
{
    std::int32_t      user_id;
    std::uint32_t     session_key;
    TerminationReason reason;
};

class FakeSessionTerminator : public ISessionTerminator
{
public:
    void Terminate(std::int32_t user_id,
                   std::uint32_t session_key,
                   TerminationReason reason) override;

    // Test introspection — returns a snapshot copy.
    std::vector<TerminationRecord> History() const;
    std::size_t                    Count() const;

private:
    mutable std::mutex             m_mtx;
    std::vector<TerminationRecord> m_history;
};

} // namespace tloginsvr::services
