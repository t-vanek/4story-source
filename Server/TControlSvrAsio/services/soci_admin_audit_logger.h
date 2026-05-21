#pragma once

// SociAdminAuditLogger — dual-sink audit logger.
// Writes every GM action to TOP_AUDIT_LOG in TGLOBAL_RAGEZONE AND
// mirrors it to the "audit" spdlog channel (same as SpdlogAdminAuditLogger).
// Replaces SpdlogAdminAuditLogger in main() when a DB pool is configured.

#include "admin_audit_logger.h"
#include "fourstory/db/session_pool.h"

#include <memory>

namespace spdlog { class logger; }

namespace tcontrolsvr {

class SociAdminAuditLogger final : public IAdminAuditLogger
{
public:
    explicit SociAdminAuditLogger(fourstory::db::SessionPool& pool);

    void LogKick(const std::string& operator_id,
                 const std::string& target_user,
                 AdminOutcome outcome) override;
    void LogMove(const std::string& operator_id,
                 const std::string& target_user,
                 std::uint8_t world,
                 std::uint16_t map_id) override;
    void LogTeleportTo(const std::string& operator_id,
                       const std::string& mover_user,
                       const std::string& target_user) override;
    void LogBan(const std::string& operator_id,
                const std::string& target_user,
                std::uint32_t duration_days,
                std::uint8_t permanent,
                const std::string& reason,
                AdminOutcome outcome) override;
    void LogChatBan(const std::string& operator_id,
                    const std::string& target_user,
                    std::uint16_t minutes,
                    const std::string& reason) override;
    void LogAnnouncement(const std::string& operator_id,
                         std::uint32_t world_filter,
                         const std::string& message) override;
    void LogCharMsg(const std::string& operator_id,
                    const std::string& target_user,
                    const std::string& message) override;
    void LogAdminAction(const std::string& operator_id,
                        const std::string& action_kind,
                        const std::string& target) override;
    void LogAuthorityDenied(const std::string& operator_id,
                            std::uint8_t actual_authority,
                            const std::string& requested_action) override;

    // Rich variant called by handlers that have full session context.
    // All other Log* methods forward here after filling what they know.
    void LogRaw(const std::string& operator_id,
                std::uint8_t       authority,
                const std::string& remote_ip,
                const std::string& action,
                const std::string& target,
                const std::string& detail = {}) override;

private:
    fourstory::db::SessionPool&       m_pool;
    std::shared_ptr<spdlog::logger>   m_log;

    void InsertDb(const std::string& operator_id,
                  std::uint8_t       authority,
                  const std::string& remote_ip,
                  const std::string& action,
                  const std::string& target,
                  const std::string& detail);
};

} // namespace tcontrolsvr
