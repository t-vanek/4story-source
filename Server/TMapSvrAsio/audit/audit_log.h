#pragma once

// IAuditLog — central audit emitter. Each EmitXxx call:
//   1. Fills the EventHeader (wall-clock ts, correlation id, size)
//   2. Mirrors the event to spdlog in human-readable form so the
//      .log file always shows the audit trail even without a
//      collector
//   3. Sends the binary POD to the configured ILogPeer (UDP) when
//      one is attached — best-effort; failure is logged at debug
//      and never throws
//
// Correlation ids are uint32 wraparound counters managed by the
// implementation. The dispatch entry point asks for a fresh id and
// threads it through any sub-events (e.g. a CS_CONNECT_REQ that
// triggers an MW_ADDCHAR_ACK reuses the parent's correlation).

#include "event.h"

#include <cstdint>

namespace tmapsvr {
class ILogPeer;
}

namespace tmapsvr::audit {

class IAuditLog
{
public:
    virtual ~IAuditLog() = default;

    // Reserve a fresh correlation id. The dispatch site stamps it
    // into every event it emits for the duration of one handler
    // call. uint32 wraparound is fine — log readers can cluster on
    // adjacent ids without needing global uniqueness.
    virtual std::uint32_t NextCorrelation() = 0;

    virtual void Emit(const LoginAttemptEvent&    ev) = 0;
    virtual void Emit(const CharLoadEvent&        ev) = 0;
    virtual void Emit(const HandlerInvokeEvent&   ev) = 0;
};

// Production implementation. Mirrors to spdlog::info / debug and
// sends to peer when configured. The peer pointer is non-owning;
// main() keeps the lifetime.
class AuditLog final : public IAuditLog
{
public:
    explicit AuditLog(ILogPeer* peer);

    std::uint32_t NextCorrelation() override;

    void Emit(const LoginAttemptEvent&    ev) override;
    void Emit(const CharLoadEvent&        ev) override;
    void Emit(const HandlerInvokeEvent&   ev) override;

private:
    ILogPeer*      m_peer;
    std::uint32_t  m_next_corr = 1;
};

} // namespace tmapsvr::audit
