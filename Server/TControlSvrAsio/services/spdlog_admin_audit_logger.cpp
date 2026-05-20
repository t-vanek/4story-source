#include "spdlog_admin_audit_logger.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

namespace {

const char* OutcomeStr(AdminOutcome o)
{
    switch (o)
    {
    case AdminOutcome::Success: return "Success";
    case AdminOutcome::Failed:  return "Failed";
    case AdminOutcome::Denied:  return "Denied";
    }
    return "Unknown";
}

std::shared_ptr<spdlog::logger> EnsureAuditLogger()
{
    auto lg = spdlog::get("audit");
    if (lg) return lg;
    // Share the default sinks (whichever stderr / file sinks main has
    // configured for the rest of the server). Operators can register
    // a structured "audit" logger before constructing this service to
    // peel off into a Seq / Loki / TLOG_AUDIT pipeline.
    auto def = spdlog::default_logger();
    if (!def) return nullptr;
    lg = std::make_shared<spdlog::logger>("audit",
        def->sinks().begin(), def->sinks().end());
    lg->set_level(def->level());
    spdlog::register_logger(lg);
    return lg;
}

} // namespace

SpdlogAdminAuditLogger::SpdlogAdminAuditLogger()
    : m_log(EnsureAuditLogger())
{
}

void SpdlogAdminAuditLogger::LogKick(const std::string& op,
                                     const std::string& target,
                                     AdminOutcome out)
{
    if (m_log) m_log->info("event=kick op='{}' target='{}' outcome={}",
        op, target, OutcomeStr(out));
}

void SpdlogAdminAuditLogger::LogMove(const std::string& op,
                                     const std::string& target,
                                     std::uint8_t world,
                                     std::uint16_t map_id)
{
    if (m_log) m_log->info("event=move op='{}' target='{}' world={} map={}",
        op, target, world, map_id);
}

void SpdlogAdminAuditLogger::LogTeleportTo(const std::string& op,
                                           const std::string& mover,
                                           const std::string& target)
{
    if (m_log) m_log->info("event=teleport_to op='{}' user='{}' target='{}'",
        op, mover, target);
}

void SpdlogAdminAuditLogger::LogBan(const std::string& op,
                                    const std::string& target,
                                    std::uint32_t duration,
                                    std::uint8_t permanent,
                                    const std::string& reason,
                                    AdminOutcome out)
{
    if (m_log) m_log->info("event=ban op='{}' target='{}' duration={} "
                           "permanent={} reason='{}' outcome={}",
        op, target, duration, permanent, reason, OutcomeStr(out));
}

void SpdlogAdminAuditLogger::LogChatBan(const std::string& op,
                                        const std::string& target,
                                        std::uint16_t minutes,
                                        const std::string& reason)
{
    if (m_log) m_log->info("event=chat_ban op='{}' target='{}' minutes={} "
                           "reason='{}'", op, target, minutes, reason);
}

void SpdlogAdminAuditLogger::LogAnnouncement(const std::string& op,
                                             std::uint32_t world_filter,
                                             const std::string& message)
{
    if (m_log) m_log->info("event=announcement op='{}' world={} message='{}'",
        op, world_filter, message);
}

void SpdlogAdminAuditLogger::LogCharMsg(const std::string& op,
                                        const std::string& target,
                                        const std::string& message)
{
    if (m_log) m_log->info("event=char_msg op='{}' target='{}' message='{}'",
        op, target, message);
}

void SpdlogAdminAuditLogger::LogAdminAction(const std::string& op,
                                            const std::string& kind,
                                            const std::string& target)
{
    if (m_log) m_log->info("event={} op='{}' target='{}'", kind, op, target);
}

void SpdlogAdminAuditLogger::LogAuthorityDenied(const std::string& op,
                                                std::uint8_t actual,
                                                const std::string& action)
{
    if (m_log) m_log->warn("event=authority_denied op='{}' authority={} "
                           "requested='{}'", op, actual, action);
}

} // namespace tcontrolsvr
