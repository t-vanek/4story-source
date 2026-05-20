#pragma once

// In-memory IAdminAuditLogger for tests. Stores each invocation
// verbatim so test assertions can verify the audit pipeline emits
// the right call shape for every handler. Production callers should
// not link against this.

#include "admin_audit_logger.h"

#include <vector>

namespace tcontrolsvr {

class FakeAdminAuditLogger final : public IAdminAuditLogger
{
public:
    struct Record
    {
        std::string  kind;
        std::string  op;
        std::string  target;
        std::string  message;
        std::uint32_t param_a = 0;
        std::uint32_t param_b = 0;
        AdminOutcome outcome = AdminOutcome::Success;
    };

    void LogKick(const std::string& op, const std::string& target,
                 AdminOutcome out) override
    {
        m_recs.push_back({"kick", op, target, "", 0, 0, out});
    }
    void LogMove(const std::string& op, const std::string& target,
                 std::uint8_t world, std::uint16_t map_id) override
    {
        m_recs.push_back({"move", op, target, "", world, map_id});
    }
    void LogTeleportTo(const std::string& op, const std::string& mover,
                       const std::string& target) override
    {
        m_recs.push_back({"teleport_to", op, target, mover});
    }
    void LogBan(const std::string& op, const std::string& target,
                std::uint32_t duration, std::uint8_t permanent,
                const std::string& reason, AdminOutcome out) override
    {
        m_recs.push_back({"ban", op, target, reason,
                          duration, permanent, out});
    }
    void LogChatBan(const std::string& op, const std::string& target,
                    std::uint16_t minutes,
                    const std::string& reason) override
    {
        m_recs.push_back({"chat_ban", op, target, reason, minutes});
    }
    void LogAnnouncement(const std::string& op,
                         std::uint32_t world_filter,
                         const std::string& message) override
    {
        m_recs.push_back({"announcement", op, "", message, world_filter});
    }
    void LogCharMsg(const std::string& op, const std::string& target,
                    const std::string& message) override
    {
        m_recs.push_back({"char_msg", op, target, message});
    }
    void LogAdminAction(const std::string& op,
                        const std::string& kind,
                        const std::string& target) override
    {
        m_recs.push_back({kind, op, target, "", 0, 0, AdminOutcome::Success});
    }
    void LogAuthorityDenied(const std::string& op,
                            std::uint8_t actual,
                            const std::string& action) override
    {
        m_recs.push_back({"authority_denied", op, "", action,
                          actual, 0, AdminOutcome::Denied});
    }

    const std::vector<Record>& Records() const { return m_recs; }
    void Clear() { m_recs.clear(); }

private:
    std::vector<Record> m_recs;
};

} // namespace tcontrolsvr
