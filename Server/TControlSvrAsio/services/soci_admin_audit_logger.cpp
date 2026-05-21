#include "soci_admin_audit_logger.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <string>

namespace tcontrolsvr {

SociAdminAuditLogger::SociAdminAuditLogger(fourstory::db::SessionPool& pool)
    : m_pool(pool)
    , m_log(spdlog::get("audit"))
{
    if (!m_log)
        m_log = spdlog::stdout_color_mt("audit");
}

void SociAdminAuditLogger::InsertDb(const std::string& operator_id,
                                     std::uint8_t       authority,
                                     const std::string& remote_ip,
                                     const std::string& action,
                                     const std::string& target,
                                     const std::string& detail)
{
    const int auth = authority;
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;
        if (detail.empty() && target.empty())
        {
            sql << "INSERT INTO TOP_AUDIT_LOG "
                   "(szOperatorID, bAuthority, szRemoteIP, szAction) "
                   "VALUES (:op, :auth, :ip, :act)",
                soci::use(operator_id, "op"),
                soci::use(auth,        "auth"),
                soci::use(remote_ip,   "ip"),
                soci::use(action,      "act");
        }
        else
        {
            sql << "INSERT INTO TOP_AUDIT_LOG "
                   "(szOperatorID, bAuthority, szRemoteIP, szAction, "
                   "szTarget, szDetail) "
                   "VALUES (:op, :auth, :ip, :act, :tgt, :det)",
                soci::use(operator_id, "op"),
                soci::use(auth,        "auth"),
                soci::use(remote_ip,   "ip"),
                soci::use(action,      "act"),
                soci::use(target,      "tgt"),
                soci::use(detail,      "det");
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("soci_admin_audit_logger: DB insert failed: {}", ex.what());
    }
}

void SociAdminAuditLogger::LogRaw(const std::string& operator_id,
                                   std::uint8_t       authority,
                                   const std::string& remote_ip,
                                   const std::string& action,
                                   const std::string& target,
                                   const std::string& detail)
{
    m_log->info("audit op='{}' auth={} ip={} action={} target='{}' detail='{}'",
        operator_id, authority, remote_ip, action, target, detail);
    InsertDb(operator_id, authority, remote_ip, action, target, detail);
}

void SociAdminAuditLogger::LogKick(const std::string& operator_id,
                                    const std::string& target_user,
                                    AdminOutcome outcome)
{
    const char* res = (outcome == AdminOutcome::Success) ? "ok"
                    : (outcome == AdminOutcome::Denied)  ? "denied" : "failed";
    m_log->info("audit op='{}' action=kick target='{}' outcome={}",
        operator_id, target_user, res);
    InsertDb(operator_id, 0, {}, "CT_USERKICKOUT", target_user,
             std::string("outcome=") + res);
}

void SociAdminAuditLogger::LogMove(const std::string& operator_id,
                                    const std::string& target_user,
                                    std::uint8_t world,
                                    std::uint16_t map_id)
{
    m_log->info("audit op='{}' action=move target='{}' world={} map={}",
        operator_id, target_user, world, map_id);
    InsertDb(operator_id, 0, {}, "CT_USERMOVE", target_user,
             "world=" + std::to_string(world) + " map=" + std::to_string(map_id));
}

void SociAdminAuditLogger::LogTeleportTo(const std::string& operator_id,
                                          const std::string& mover_user,
                                          const std::string& target_user)
{
    m_log->info("audit op='{}' action=teleport mover='{}' target='{}'",
        operator_id, mover_user, target_user);
    InsertDb(operator_id, 0, {}, "CT_USERPOSITION", mover_user,
             "target=" + target_user);
}

void SociAdminAuditLogger::LogBan(const std::string& operator_id,
                                   const std::string& target_user,
                                   std::uint32_t duration_days,
                                   std::uint8_t permanent,
                                   const std::string& reason,
                                   AdminOutcome outcome)
{
    const char* res = (outcome == AdminOutcome::Success) ? "ok"
                    : (outcome == AdminOutcome::Denied)  ? "denied" : "failed";
    m_log->info("audit op='{}' action=ban target='{}' days={} perm={} "
                "reason='{}' outcome={}",
        operator_id, target_user, duration_days, permanent, reason, res);
    InsertDb(operator_id, 0, {}, "CT_USERPROTECTED", target_user,
             "days=" + std::to_string(duration_days) +
             " perm=" + std::to_string(permanent) +
             " reason=" + reason +
             " outcome=" + res);
}

void SociAdminAuditLogger::LogChatBan(const std::string& operator_id,
                                       const std::string& target_user,
                                       std::uint16_t minutes,
                                       const std::string& reason)
{
    m_log->info("audit op='{}' action=chatban target='{}' minutes={} reason='{}'",
        operator_id, target_user, minutes, reason);
    InsertDb(operator_id, 0, {}, "CT_CHATBAN", target_user,
             "minutes=" + std::to_string(minutes) + " reason=" + reason);
}

void SociAdminAuditLogger::LogAnnouncement(const std::string& operator_id,
                                            std::uint32_t world_filter,
                                            const std::string& message)
{
    m_log->info("audit op='{}' action=announce world={} msg='{}'",
        operator_id, world_filter, message);
    InsertDb(operator_id, 0, {}, "CT_ANNOUNCEMENT", {},
             "world=" + std::to_string(world_filter) + " msg=" + message);
}

void SociAdminAuditLogger::LogCharMsg(const std::string& operator_id,
                                       const std::string& target_user,
                                       const std::string& message)
{
    m_log->info("audit op='{}' action=charmsg target='{}' msg='{}'",
        operator_id, target_user, message);
    InsertDb(operator_id, 0, {}, "CT_CHARMSG", target_user, "msg=" + message);
}

void SociAdminAuditLogger::LogAdminAction(const std::string& operator_id,
                                           const std::string& action_kind,
                                           const std::string& target)
{
    m_log->info("audit op='{}' action={} target='{}'",
        operator_id, action_kind, target);
    InsertDb(operator_id, 0, {}, action_kind, target, {});
}

void SociAdminAuditLogger::LogAuthorityDenied(const std::string& operator_id,
                                               std::uint8_t actual_authority,
                                               const std::string& requested_action)
{
    m_log->warn("audit op='{}' action=authority_denied auth={} requested='{}'",
        operator_id, actual_authority, requested_action);
    InsertDb(operator_id, actual_authority, {}, "authority_denied", {},
             "requested=" + requested_action);
}

} // namespace tcontrolsvr
