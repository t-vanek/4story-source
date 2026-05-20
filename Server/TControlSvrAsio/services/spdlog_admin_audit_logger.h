#pragma once

// Default IAdminAuditLogger — writes one log record per GM action
// to the shared "audit" spdlog logger (the same channel
// fourstory::audit::SpdlogAuditLogger uses for login / char events).
// Output format mirrors the legacy `TCONTROL: %s ed %s` lines while
// using key=value pairs so structured-log consumers can parse it.

#include "admin_audit_logger.h"

#include <memory>

namespace spdlog { class logger; }

namespace tcontrolsvr {

class SpdlogAdminAuditLogger final : public IAdminAuditLogger
{
public:
    SpdlogAdminAuditLogger();

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

private:
    std::shared_ptr<spdlog::logger> m_log;
};

} // namespace tcontrolsvr
