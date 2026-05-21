#pragma once

// IAdminAuditLogger — operational audit sink for GM actions issued
// through TControlSvr. Mirrors the legacy `LogSmartly("TCONTROL: %s
// kicked %s", …)` calls that every admin handler in
// Server/TControlSvr/Handler.cpp emits before forwarding the action
// to the peer servers.
//
// One method per common shape so the call sites stay readable —
// `LogAdminAction` is the generic fallback for handlers that don't
// fit the kick / move / ban / chat-ban families.
//
// The default implementation (SpdlogAdminAuditLogger) writes
// records to the shared "audit" spdlog logger, the same channel
// IAuditLogger uses. Operators can attach a structured sink (Seq,
// Loki, TLOG_AUDIT table) to that logger without changing call
// sites. The shared-audit decision matches
// `_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md` §7 question 5
// (TLOG_AUDIT shared, extended `audit_kind` enum).

#include <cstdint>
#include <string>

namespace tcontrolsvr {

enum class AdminOutcome
{
    Success,
    Failed,        // forwarded to a peer that's offline / no longer registered
    Denied,        // CheckAuthority gate rejected
};

class IAdminAuditLogger
{
public:
    virtual ~IAdminAuditLogger() = default;

    // CT_USERKICKOUT_REQ — operator kicks an in-game user (forwarded
    // to every MapSvr).
    virtual void LogKick(const std::string& operator_id,
                         const std::string& target_user,
                         AdminOutcome outcome) = 0;

    // CT_USERMOVE_REQ — teleport user(s) to a position. One audit
    // record per target.
    virtual void LogMove(const std::string& operator_id,
                         const std::string& target_user,
                         std::uint8_t world,
                         std::uint16_t map_id) = 0;

    // CT_USERPOSITION_REQ — move user to another user's coords.
    virtual void LogTeleportTo(const std::string& operator_id,
                               const std::string& mover_user,
                               const std::string& target_user) = 0;

    // CT_USERPROTECTED_REQ — apply a TUserProtectedAdd ban.
    virtual void LogBan(const std::string& operator_id,
                        const std::string& target_user,
                        std::uint32_t duration_days,
                        std::uint8_t permanent,
                        const std::string& reason,
                        AdminOutcome outcome) = 0;

    // CT_CHATBAN_REQ — temporary mute.
    virtual void LogChatBan(const std::string& operator_id,
                            const std::string& target_user,
                            std::uint16_t minutes,
                            const std::string& reason) = 0;

    // CT_ANNOUNCEMENT_REQ — server-wide or world-scoped announcement.
    virtual void LogAnnouncement(const std::string& operator_id,
                                 std::uint32_t world_filter,
                                 const std::string& message) = 0;

    // CT_CHARMSG_REQ — DM to a specific character.
    virtual void LogCharMsg(const std::string& operator_id,
                            const std::string& target_user,
                            const std::string& message) = 0;

    // Generic fallback for handlers without a dedicated method
    // (CASTLEINFO / ITEMFIND / MONACTION / …). `action_kind` is a
    // short identifier like "castle_enable" or "item_find".
    virtual void LogAdminAction(const std::string& operator_id,
                                const std::string& action_kind,
                                const std::string& target) = 0;

    // Authority-denied attempt. Records the requested action +
    // operator so abuse patterns are visible in the audit stream.
    virtual void LogAuthorityDenied(const std::string& operator_id,
                                    std::uint8_t actual_authority,
                                    const std::string& requested_action) = 0;

    // Generic structured record with full session context (authority,
    // remote IP). Called by handlers that have access to the
    // OperatorSession; provides richer DB rows than the specialized
    // methods above. Default impl is a no-op — SociAdminAuditLogger
    // overrides it to persist to TOP_AUDIT_LOG.
    virtual void LogRaw(const std::string& /*operator_id*/,
                        std::uint8_t       /*authority*/,
                        const std::string& /*remote_ip*/,
                        const std::string& /*action*/,
                        const std::string& /*target*/,
                        const std::string& /*detail*/ = {}) {}
};

} // namespace tcontrolsvr
