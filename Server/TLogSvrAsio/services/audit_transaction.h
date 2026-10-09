#pragma once
#include <utility>

namespace tlogsvr {
// No event identity exists in the original UDP record. A failed COMMIT can
// mean the insert is already durable, so it must never enter the retry queue.
enum class AuditWriteOutcome { Committed, Retryable, Unknown };

template<class Session, class Insert>
AuditWriteOutcome RunAuditTransaction(Session& sql, Insert&& insert) {
    const auto discard = [&] { try { sql.close(); } catch (...) {} };
    try { sql.begin(); }
    catch (...) { discard(); return AuditWriteOutcome::Retryable; }
    try { std::forward<Insert>(insert)(); }
    catch (...) {
        try { sql.rollback(); return AuditWriteOutcome::Retryable; }
        catch (...) { discard(); return AuditWriteOutcome::Unknown; }
    }
    try { sql.commit(); return AuditWriteOutcome::Committed; }
    catch (...) {
        // A subsequent ROLLBACK cannot prove whether this COMMIT succeeded.
        // Close the lease's connection before returning it to the pool.
        discard();
        return AuditWriteOutcome::Unknown;
    }
}
} // namespace tlogsvr
