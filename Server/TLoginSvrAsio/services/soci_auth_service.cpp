#include "soci_auth_service.h"
#include "../db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>

namespace tloginsvr::services {

namespace {

bool VerifyPassword(const std::string& stored, const std::string& candidate)
{
    // Phase B initial impl: plaintext compare. BCrypt verify lands
    // in Phase C alongside the transparent-upgrade path. Stored
    // password format is documented as plaintext in the legacy
    // TACCOUNT_PW schema; production may already have hashed rows
    // mixed in, which this impl currently rejects (returns false).
    // Marker for future: if stored starts with "$2", route through
    // libcrypt-based verifier instead.
    return stored == candidate;
}

} // namespace

SociAuthService::SociAuthService(db::SessionPool& pool)
    : m_pool(pool)
{
}

AuthResult SociAuthService::Authenticate(const AuthRequest& req)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Step 1 — IP ban check.
        if (!req.client_ip.empty())
        {
            int hit = 0;
            sql << "SELECT COUNT(*) FROM \"IPBLACKLIST_game\" WHERE \"szIP\" = :ip",
                soci::use(req.client_ip), soci::into(hit);
            if (hit > 0)
            {
                spdlog::warn("auth: IP {} on banlist", req.client_ip);
                return AuthResult{ .status = AuthStatus::IpBanned };
            }
        }

        // Step 2 — user lookup. TACCOUNT_PW is the credentials store
        // per legacy CSPLogin (CSHandler.cpp:320). TACCOUNT is the
        // older / parallel table; not used for auth in legacy SP.
        int user_id = 0;
        std::string stored_password;
        soci::indicator pw_ind = soci::i_null;
        soci::statement lookup =
            (sql.prepare <<
                "SELECT \"dwUserID\", \"szPasswd\" FROM \"TACCOUNT_PW\" "
                "WHERE \"szUserID\" = :uid",
                soci::use(req.user_id),
                soci::into(user_id),
                soci::into(stored_password, pw_ind));
        lookup.execute(true);
        if (!lookup.got_data())
        {
            spdlog::info("auth: user '{}' not found", req.user_id);
            return AuthResult{ .status = AuthStatus::NoUser };
        }

        // Step 3 — password check.
        if (pw_ind == soci::i_null || !VerifyPassword(stored_password, req.password))
        {
            spdlog::info("auth: user '{}' (uid={}) wrong password",
                req.user_id, user_id);
            return AuthResult{ .status = AuthStatus::WrongPassword };
        }

        // Step 4 — user-level ban. Match legacy TGetBanReason
        // semantics (TLogin.sql:110-125): row with bEternal=1 OR
        // (startTime + dwDuration_days) in the future blocks login.
        int ban_count = 0;
        sql << "SELECT COUNT(*) FROM \"TUSERPROTECTED\" "
               "WHERE \"dwUserID\" = :uid "
               "  AND (\"bEternal\" = 1 OR "
               "       \"startTime\" + (\"dwDuration\" || ' days')::interval > CURRENT_TIMESTAMP)",
            soci::use(user_id), soci::into(ban_count);
        if (ban_count > 0)
        {
            spdlog::warn("auth: user_id={} is banned ({} active protection rows)",
                user_id, ban_count);
            return AuthResult{
                .status = AuthStatus::Banned,
                .user_id = user_id,
                .ban_reason = std::string("protected"),
            };
        }

        // Step 5 — duplicate session check.
        int existing = 0;
        sql << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :uid",
            soci::use(user_id), soci::into(existing);
        if (existing > 0)
        {
            // Legacy LR_DUPLICATE: lock the existing row + return
            // duplicate. The handler then kicks the previous session
            // via ConnectionRegistry. For now we just delete the
            // stale row so the new session's insert succeeds.
            spdlog::warn("auth: user_id={} duplicate session — clearing stale TCURRENTUSER",
                user_id);
            sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :uid",
                soci::use(user_id);
        }

        // Step 6 — success: insert TCURRENTUSER, capture identity dwKEY.
        int session_key = 0;
        sql << "INSERT INTO \"TCURRENTUSER\" "
               "(\"dwUserID\", \"szLoginIP\") VALUES (:uid, :ip) "
               "RETURNING \"dwKEY\"",
            soci::use(user_id), soci::use(req.client_ip),
            soci::into(session_key);

        // Stamp TACCOUNT_PW.dLastLogin too (CSPLogin SP step).
        sql << "UPDATE \"TACCOUNT_PW\" SET \"dLastLogin\" = CURRENT_TIMESTAMP "
               "WHERE \"dwUserID\" = :uid",
            soci::use(user_id);

        // Insert audit log row (TLog).
        sql << "INSERT INTO \"TLOG\" "
               "(\"dwKEY\", \"dwUserID\") VALUES (:key, :uid)",
            soci::use(session_key), soci::use(user_id);

        spdlog::info("auth: user '{}' (uid={}) success, session_key={}",
            req.user_id, user_id, session_key);

        return AuthResult{
            .status = AuthStatus::Success,
            .user_id = user_id,
            .session_key = static_cast<std::uint32_t>(session_key),
            .create_char_count = 6,
            .in_pc_bang = 0,
            .premium_id = 0,
        };
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth: DB error for '{}': {}", req.user_id, ex.what());
        return AuthResult{ .status = AuthStatus::InternalError };
    }
}

} // namespace tloginsvr::services
