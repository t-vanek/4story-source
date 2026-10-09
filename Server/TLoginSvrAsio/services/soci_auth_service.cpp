// SociAuthService implementation — SOCI-backed IAuthService against
// TGLOBAL.
//
// One method per IAuthService entry point. Each acquires a session
// from the pool (with the bounded acquire_timeout — exhaustion throws
// AcquireTimeout, which the dispatch wrapper turns into a clean
// per-connection close), runs the relevant query, releases the lease
// on scope exit.
//
// Authenticate() is the centerpiece — mirrors legacy `TLogin` SP:
//   1. Check exact IPBLACKLIST_game → LR_IPBLOCK.
//   2. SELECT TACCOUNT_PW row by szUserID → LR_NOUSER / LR_INVALIDPASSWD.
//   3. BCrypt verify the stored szPasswd (legacy plaintext/SHA1 rows
//      are rejected — run tloginsvr_bcrypt_migrate once before cutover).
//   4. Check TCheckIP patterns after credentials, then TUSERPROTECTED
//      for eternal or currently active account bans.
//   5. Check TCURRENTUSER.bLocked → LR_DUPLICATE for prior live session.
//   6. INSERT TCURRENTUSER row, UPDATE TACCOUNT_PW.dLastLogin, INSERT
//      TLOG audit row.
//   7. Populate AuthResult with TUSERINFOTABLE agreement, TPCBANG,
//      TUSERPREMIUM, dwLastCharID.
//
// Email verification uses modern connection-bound challenges. Confirmation
// authorizes a bounded LOGIN retry; all session writes stay in Authenticate().
//
// Legacy parity: Server/TLoginSvr/CTLoginSvrModule's CSPLogin /
// TLogin / CSPCheckPasswd / CSPGetUserAgreement / CSPSetUserAgreement
// stored-proc calls.

#include "soci_auth_service.h"
#include "postgresql_login_owner.h"
#include "bcrypt_util.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <bcrypt/bcrypt.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <random>

namespace tloginsvr::services {

namespace {

// Normal source client path hashes the entered password before SendCS_LOGIN_REQ
// (TClientWnd.cpp:327 / TClientGame.cpp:462). Bcrypt covers that wire credential.
// The pinned backup contains no TACCOUNT_PW credential corpus, so this is not
// evidence that every historical/direct-login deployment used the same bytes.
// Historical import and code-page conversion require a separate verified contract.
// Non-bcrypt stored rows and embedded-NUL candidates are rejected.
bool CheckPassword(const std::string& stored,
                   const std::string& candidate)
{
    if (stored.empty() || candidate.find('\0') != std::string::npos) return false;
    if (!bcrypt_util::IsBcrypt(stored))
    {
        // Pre-migration row — log loudly so operators notice. We
        // explicitly do NOT fall back to a strcmp compare; that
        // codepath was the whole point of the cutover.
        spdlog::warn("auth: stored password not bcrypt-shaped — run "
                     "tloginsvr_bcrypt_migrate before serving traffic");
        return false;
    }
    // libbcrypt: bcrypt_checkpw returns 0 on a match, -1 on miss / err.
    // An invalid hash also returns -1; logged upstream as wrong password.
    return ::bcrypt_checkpw(candidate.c_str(), stored.c_str()) == 0;
}

std::string Hex(const unsigned char* data, std::size_t size)
{
    constexpr char alphabet[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result += alphabet[data[i] >> 4]; result += alphabet[data[i] & 15];
    }
    return result;
}

std::string Digest(const std::string& value, const EVP_MD* algorithm)
{
    unsigned char bytes[EVP_MAX_MD_SIZE]; unsigned int length = 0;
    if (EVP_Digest(value.data(), value.size(), bytes, &length, algorithm, nullptr) != 1)
        throw std::runtime_error("Credential digest unavailable");
    return Hex(bytes, length);
}

std::string CodeDigest(const std::string& token, std::string code)
{
    for (char& c : code) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return Digest(token + code, EVP_sha256());
}

SecurityChallenge MakeChallenge(const std::string& email)
{
    unsigned char token[32];
    if (RAND_bytes(token, sizeof(token)) != 1) throw std::runtime_error("Random generation failed");
    SecurityChallenge result{Hex(token, sizeof(token)), {}, email};
    constexpr char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (result.code.size() < 6) {
        unsigned char b = 0;
        if (RAND_bytes(&b, 1) != 1) throw std::runtime_error("Random generation failed");
        if (b < 252) result.code += alphabet[b % 36];
    }
    return result;
}

bool LockAccount(soci::session& sql, std::int32_t user_id)
{
    int locked = 0;
    sql << "SELECT \"dwUserID\" FROM \"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",
        soci::use(user_id), soci::into(locked);
    return sql.got_data();
}

} // namespace

SociAuthService::SociAuthService(fourstory::db::SessionPool& pool, std::string owner_token)
    : m_pool(pool), m_owner_token(std::move(owner_token))
{
}

AuthResult SociAuthService::Authenticate(const AuthRequest& req)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        auto tx = BeginLoginTransaction(m_pool, sql, m_owner_token);
        // Exact IP bans are checked before credentials, as in backed-up TLogin.
        // TIPAUTHORITY pattern restrictions are checked later at the procedure's
        // bIPCheck gate. The original C++ wrapper has an additional earlier
        // CSPCheckIP gate; that ordering difference is recorded in source evidence.
        if (!req.client_ip.empty())
        {
            int hit = 0;
            sql << "SELECT COUNT(*) FROM \"IPBLACKLIST_game\" WHERE \"szIP\" = :ip",
                soci::use(req.client_ip), soci::into(hit);
            if (hit > 0)
            {
                spdlog::warn("auth: IP {} on banlist (IPBLACKLIST_game)",
                    req.client_ip);
                return AuthResult{ .status = AuthStatus::IpBanned };
            }

        }

        // Step 2 — user lookup. Scope the statement so its cursor closes
        // before the next query — ODBC/MSSQL doesn't allow a second
        // statement on the same connection while a prior result set is
        // open ("SQL state 24000 — invalid cursor state").
        int user_id = 0;
        std::string stored_password;
        soci::indicator pw_ind = soci::i_null;
        bool got_row = false;
        {
            soci::statement lookup =
                (sql.prepare <<
                    (tx ? "SELECT \"dwUserID\", \"szPasswd\" FROM \"TACCOUNT_PW\" "
                          "WHERE rtrim(\"szUserID\") = rtrim(:uid) FOR UPDATE"
                        : "SELECT \"dwUserID\", \"szPasswd\" FROM \"TACCOUNT_PW\" "
                          "WHERE \"szUserID\" = :uid"),
                    soci::use(req.user_id),
                    soci::into(user_id),
                    soci::into(stored_password, pw_ind));
            lookup.execute(true);
            got_row = lookup.got_data();
        }
        if (!got_row)
        {
            spdlog::info("auth: user '{}' not found", req.user_id);
            return AuthResult{ .status = AuthStatus::NoUser };
        }

        // Only a confirmed challenge bound to this connection/account can select
        // the source 0x2918 form-password retry adapter. Ordinary login continues
        // to require the exact stored wire credential. No IP-based bypass.
        const bool security_retry = !req.security_retry_token.empty();
        std::string candidate = req.password;
        if (security_retry)
        {
            if (!tx || req.client_version != 0x2918 || req.site_code_present)
                return AuthResult{.status = AuthStatus::InternalError};
            int found = 0;
            sql << "SELECT 1 FROM app_global.login_security_challenge WHERE token=:t "
                   "AND owner_token=:o AND user_id=:u AND client_ip=:ip AND client_version=:v "
                   "AND expires_at>clock_timestamp() AND verified_at IS NOT NULL "
                   "AND verified_at + interval '30 seconds'>clock_timestamp() FOR UPDATE",
                soci::use(req.security_retry_token), soci::use(m_owner_token), soci::use(user_id),
                soci::use(req.client_ip), soci::use(req.client_version), soci::into(found);
            if (!sql.got_data() || found != 1) return AuthResult{.status = AuthStatus::InternalError};
            if (candidate.empty() || candidate.size() > 64 || candidate.find('\0') != std::string::npos)
                return AuthResult{.status = AuthStatus::WrongPassword};
            candidate = Digest(candidate, EVP_sha1());
        }

        // Step 3 — password check.
        if (pw_ind == soci::i_null)
        {
            spdlog::info("auth: user '{}' (uid={}) wrong password (null hash)",
                req.user_id, user_id);
            return AuthResult{ .status = AuthStatus::WrongPassword };
        }
        if (!CheckPassword(stored_password, candidate))
        {
            spdlog::info("auth: user '{}' (uid={}) wrong password",
                req.user_id, user_id);
            return AuthResult{ .status = AuthStatus::WrongPassword };
        }

        // Backed-up TLogin checks the TCheckIP restriction only after a
        // valid credential. Preserve that procedure's error precedence.
        if (!req.client_ip.empty())
        {
            try
            {
                // `:ip LIKE "szIP"` — the stored row is the pattern,
                // the incoming IP is the haystack. Matches legacy
                // TCheckIP SP semantics exactly. Legacy also has a
                // commented-out `AND bAuthority = 1` whitelist
                // exception (TCheckIP.sql:6-8) — dead in legacy too,
                // so we mirror just the block path.
                int pattern_hit = 0;
                sql << "SELECT COUNT(*) FROM \"TIPAUTHORITY\" "
                       "WHERE :ip LIKE \"szIP\"",
                    soci::use(req.client_ip), soci::into(pattern_hit);
                if (pattern_hit > 0)
                {
                    spdlog::warn("auth: IP {} on banlist (TIPAUTHORITY pattern)",
                        req.client_ip);
                    return AuthResult{ .status = AuthStatus::IpRestricted };
                }
            }
            catch (const std::exception& ex)
            {
                if (tx) throw;
                // TIPAUTHORITY missing or shape mismatch — skip the
                // pattern check and let the rest of auth proceed.
                spdlog::debug("auth: TIPAUTHORITY lookup skipped: {}", "database operation failed");
            }
        }

        // Step 4 — user-level ban. Match the legacy TLogin SP semantics:
        // two separate SELECTs, one for an active duration window
        // (startTime + dwDuration days >= now), one for the eternal flag.
        // The SP runs them in that order and both map to LR_IPBLOCK on
        // the wire (return 7). Our AuthStatus split keeps Banned (user)
        // vs IpBanned (network) distinguishable in logs; the handler
        // collapses both onto the LR_IPBLOCK value the legacy client
        // expects.
        const bool is_mssql = (m_pool.GetBackend() == fourstory::db::Backend::Odbc);
        const char* ban_dur_sql = is_mssql
            ? "SELECT COUNT(*) FROM \"TUSERPROTECTED\" "
              "WHERE \"dwUserID\" = :uid "
              "  AND DATEADD(day, \"dwDuration\", \"startTime\") >= CURRENT_TIMESTAMP"
            : "SELECT COUNT(*) FROM \"TUSERPROTECTED\" "
              "WHERE \"dwUserID\" = :uid "
              "  AND \"startTime\" + (\"dwDuration\" || ' days')::interval >= CURRENT_TIMESTAMP";
        int ban_dur = 0;
        sql << ban_dur_sql, soci::use(user_id), soci::into(ban_dur);
        if (ban_dur > 0)
        {
            spdlog::warn("auth: user_id={} is banned ({} active duration row(s))",
                user_id, ban_dur);
            return AuthResult{
                .status = AuthStatus::Banned,
                .user_id = user_id,
                .ban_reason = std::string("temporary"),
            };
        }
        int ban_eternal = 0;
        sql << "SELECT COUNT(*) FROM \"TUSERPROTECTED\" "
               "WHERE \"dwUserID\" = :uid AND \"bEternal\" = 1",
            soci::use(user_id), soci::into(ban_eternal);
        if (ban_eternal > 0)
        {
            spdlog::warn("auth: user_id={} is banned (eternal)", user_id);
            return AuthResult{
                .status = AuthStatus::Banned,
                .user_id = user_id,
                .ban_reason = std::string("eternal"),
            };
        }

        if (security_retry)
            sql << "DELETE FROM app_global.login_security_challenge WHERE token=:t",
                soci::use(req.security_retry_token);

        // 2FA is an explicit modern account opt-in. The source carries no usable
        // hardware identity; an IP whitelist is not proof of a verified device.
        // Unsupported direct-login/other-version profiles fail closed here.
        if (!security_retry)
        {
            std::string email;
            int enabled = 0;
            soci::indicator email_ind = soci::i_null;
            bool found = false;
            {
                soci::statement st = (sql.prepare <<
                    "SELECT \"szEmail\", \"bTwoFactorEnabled\" FROM \"TUSEREMAIL\" WHERE \"dwUserID\"=:u",
                    soci::use(user_id), soci::into(email, email_ind), soci::into(enabled));
                st.execute(true); found = st.got_data();
            }
            if (found && enabled != 0)
            {
                if (!tx || email_ind == soci::i_null || email.empty() || req.client_ip.empty() ||
                    req.client_version != 0x2918 || req.site_code_present)
                    return AuthResult{.status = AuthStatus::InternalError};
                sql << "DELETE FROM app_global.login_security_challenge WHERE user_id=:u "
                       "AND (expires_at<=clock_timestamp() OR owner_token<>:o)",
                    soci::use(user_id), soci::use(m_owner_token);
                int active = 0;
                sql << "SELECT count(*) FROM app_global.login_security_challenge WHERE user_id=:u",
                    soci::use(user_id), soci::into(active);
                if (active >= 3) return AuthResult{.status = AuthStatus::InternalError};
                int issued = 0;
                sql << "INSERT INTO app_global.login_security_rate AS r(user_id,window_start,issued_count) "
                       "VALUES (:u,clock_timestamp(),1) ON CONFLICT(user_id) DO UPDATE SET "
                       "window_start=CASE WHEN r.window_start+interval '5 minutes'<=clock_timestamp() THEN clock_timestamp() ELSE r.window_start END, "
                       "issued_count=CASE WHEN r.window_start+interval '5 minutes'<=clock_timestamp() THEN 1 ELSE r.issued_count+1 END "
                       "WHERE r.window_start+interval '5 minutes'<=clock_timestamp() OR r.issued_count<3 RETURNING issued_count",
                    soci::use(user_id), soci::into(issued);
                if (!sql.got_data()) return AuthResult{.status = AuthStatus::InternalError};
                auto challenge = MakeChallenge(email);
                const auto digest = CodeDigest(challenge.token, challenge.code);
                sql << "INSERT INTO app_global.login_security_challenge "
                       "(token,owner_token,user_id,client_ip,client_version,code_digest) VALUES (:t,:o,:u,:ip,:v,:d)",
                    soci::use(challenge.token), soci::use(m_owner_token), soci::use(user_id),
                    soci::use(req.client_ip), soci::use(req.client_version), soci::use(digest);
                tx->commit();
                return AuthResult{.status = AuthStatus::SecurityRequired, .user_id = user_id,
                                  .security_challenge = std::move(challenge)};
            }
        }

        if (m_pool.GetBackend() == fourstory::db::Backend::PostgreSQL)
            ExpirePendingHandoff(sql, user_id);

        // Step 5 — duplicate session. Match legacy TLogin SP:
        //   UPDATE TCURRENTUSER SET bLocked = 1 WHERE dwUserID = :uid
        //   RETURN 3 (LR_DUPLICATE)
        // The handler then kicks the previously connected peer via
        // IConnectionRegistry; the kick triggers SessionTerminator which
        // removes the row. The lock-then-return pattern (rather than
        // DELETE-now) keeps the old session's audit context intact while
        // the kick is in flight and avoids a race where a fast retry
        // races the DELETE.
        int existing = 0;
        sql << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :uid",
            soci::use(user_id), soci::into(existing);
        if (existing > 0)
        {
            sql << "UPDATE \"TCURRENTUSER\" SET \"bLocked\" = 1 "
                   "WHERE \"dwUserID\" = :uid",
                soci::use(user_id);
            spdlog::warn("auth: user_id={} duplicate session — flagged for kick",
                user_id);
            if (tx) tx->commit();
            return AuthResult{
                .status = AuthStatus::Duplicate,
                .user_id = user_id,
            };
        }

        // Step 6 — success: insert TCURRENTUSER, capture identity dwKEY.
        //
        // PostgreSQL account-row locking serializes competing logins. The
        // unique user constraint and ON CONFLICT are a second boundary against
        // writers outside this service. WHERE NOT EXISTS alone is not atomic.
        // Legacy ODBC retains its UPDLOCK/HOLDLOCK insert. Native session,
        // audit and last-login writes commit together, as in backed-up TLogin.
        int session_key = 0;
        soci::indicator key_ind = soci::i_null;
        // Real MSSQL TCURRENTUSER has 5 extra NOT-NULL/no-default
        // columns (dwCharID, bGroupID, bChannel, wPort, bLocked) that
        // PG dev fixture defines with DEFAULT 0. The short INSERT
        // worked on PG but null-violated on the legacy MSSQL schema —
        // explicit zeros for all of them keeps both backends happy.
        const char* insert_user_sql = is_mssql
            ? "INSERT INTO \"TCURRENTUSER\" "
              "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
              " \"wPort\", \"bLocked\", \"szLoginIP\") "
              "OUTPUT INSERTED.\"dwKEY\" "
              "SELECT :uid, 0, 0, 0, 0, 0, :ip "
              "WHERE NOT EXISTS ("
              "  SELECT 1 FROM \"TCURRENTUSER\" WITH (UPDLOCK, HOLDLOCK) "
              "  WHERE \"dwUserID\" = :uid2)"
            : "INSERT INTO \"TCURRENTUSER\" "
              "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
              " \"wPort\", \"bLocked\", \"szLoginIP\") "
              "SELECT :uid, 0, 0, 0, 0, 0, :ip "
              "WHERE NOT EXISTS ("
              "  SELECT 1 FROM \"TCURRENTUSER\" "
              "  WHERE \"dwUserID\" = :uid2) "
              "ON CONFLICT (\"dwUserID\") DO NOTHING RETURNING \"dwKEY\"";
        {
            soci::statement st = (sql.prepare << insert_user_sql,
                soci::use(user_id, "uid"),
                soci::use(req.client_ip, "ip"),
                soci::use(user_id, "uid2"),
                soci::into(session_key, key_ind));
            st.execute(true);
            if (!st.got_data() || key_ind == soci::i_null)
            {
                // Lost the race — someone else inserted between our
                // duplicate-check and this insert. Flag the existing
                // row for kick and report Duplicate, same as the
                // step-5 branch above.
                sql << "UPDATE \"TCURRENTUSER\" SET \"bLocked\" = 1 "
                       "WHERE \"dwUserID\" = :uid",
                    soci::use(user_id);
                spdlog::warn("auth: user_id={} lost duplicate-race during "
                             "INSERT — flagged existing row for kick",
                    user_id);
                if (tx) tx->commit();
                return AuthResult{
                    .status  = AuthStatus::Duplicate,
                    .user_id = user_id,
                };
            }
        }

        // JP/TW site-code persistence. The shipped client appends
        // a DWORD dwSiteCode when MODIFY_DIRECTLOGIN is set
        // (TNationOption::SetNation enables it for JP + TW); legacy
        // server's CSPLoginJP only reads the low byte as
        // `bChanneling`, but we keep the full DWORD on the row so
        // ops can trace the brokering partner end-to-end. Both
        // updates are fire-and-forget — schema without these
        // columns logs + continues so non-JP/TW builds keep
        // working.
        if (req.site_code_present && req.site_code != 0)
        {
            try
            {
                sql << "UPDATE \"TCURRENTUSER\" SET \"dwSiteCode\" = :s "
                       "WHERE \"dwKEY\" = :k",
                    soci::use(static_cast<int>(req.site_code)),
                    soci::use(session_key);
            }
            catch (const std::exception& ex)
            {
                if (tx) throw;
                spdlog::debug("auth: TCURRENTUSER.dwSiteCode update skipped: {}",
                    "database operation failed");
            }
            // Legacy bChanneling = low byte projection. Kept as a
            // separate column so the legacy CSPLoginJP SP signature
            // round-trips. Catches independently from dwSiteCode so
            // either column can exist alone.
            try
            {
                sql << "UPDATE \"TCURRENTUSER\" SET \"bChanneling\" = :c "
                       "WHERE \"dwKEY\" = :k",
                    soci::use(static_cast<int>(req.channeling())),
                    soci::use(session_key);
            }
            catch (const std::exception& ex)
            {
                if (tx) throw;
                spdlog::debug("auth: TCURRENTUSER.bChanneling update skipped: {}",
                    "database operation failed");
            }
        }

        // Stamp TACCOUNT_PW.dLastLogin (TLogin SP final UPDATE).
        sql << "UPDATE \"TACCOUNT_PW\" SET \"dLastLogin\" = CURRENT_TIMESTAMP "
               "WHERE \"dwUserID\" = :uid",
            soci::use(user_id);

        // Insert audit log row (TLOG). TLOG.dwKEY is NOT identity in the
        // legacy schema — it carries the TCURRENTUSER.dwKEY value so the
        // session and the audit row share a key.
        // Real MSSQL TLOG has 5 extra NOT-NULL/no-default columns
        // (dwCharID, bGroupID, bChannel, timeLOGIN, timeLOGOUT) —
        // PG dev fixture defaults all of them. Insert full shape so
        // both backends accept the audit row.
        sql << "INSERT INTO \"TLOG\" "
               "(\"dwKEY\", \"dwUserID\", \"dwCharID\", "
               " \"bGroupID\", \"bChannel\", "
               " \"timeLOGIN\", \"timeLOGOUT\") "
               "VALUES (:key, :uid, 0, 0, 0, "
               "        CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            soci::use(session_key), soci::use(user_id);

        // USERIPLOG audit row (legacy TLogin SP line 160). Per-login
        // IP + username + timestamp. Survives session termination, so
        // post-mortem "what IP did user X log in from on date D"
        // queries work. TLOG.szLoginIP doesn't exist in legacy schema —
        // USERIPLOG is the canonical "historical login → IP" table.
        // Optional: swallow missing-table errors so deployments that
        // don't want the audit can drop the DDL.
        if (!req.client_ip.empty())
        {
            try
            {
                sql << "INSERT INTO \"USERIPLOG\" "
                       "(\"IP\", \"Username\", \"Date_time\") "
                       "VALUES (:ip, :u, CURRENT_TIMESTAMP)",
                    soci::use(req.client_ip),
                    soci::use(req.user_id);
            }
            catch (const std::exception& ex)
            {
                if (tx) throw;
                spdlog::debug("auth: USERIPLOG insert skipped: {}",
                    "database operation failed");
            }
        }

        // Step 7 — terms-of-service / first-login agreement check.
        // Legacy TLogin SP final block: if TUSERINFOTABLE has no row for
        // (dwUserID, bAgreement=1), RETURN 8 (LR_NEEDAGREEMENT). The
        // session is still considered logged in — the client routes to
        // the agreement screen and ACKs back via CS_AGREEMENT_REQ.
        // Pull bAgreement + bCanCreateCharCount in one SELECT so the
        // AgreementNeeded return path also gets the real slot count.
        // The legacy TLogin SP returns m_bCreateCnt as an OUT param
        // populated from the same column. Falls back to 6 when the
        // row doesn't exist (first-time login, no TUSERINFOTABLE
        // entry yet — handler path will upsert on agreement).
        int agreement_ok = 0;
        int slots_remaining = 6;
        soci::indicator agree_ind = soci::i_null;
        soci::indicator slots_ind = soci::i_null;
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"bAgreement\", \"bCanCreateCharCount\" "
                "FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = :uid",
                soci::use(user_id),
                soci::into(agreement_ok, agree_ind),
                soci::into(slots_remaining, slots_ind));
            st.execute(true);
            if (!st.got_data())
            {
                agreement_ok = 0;     // no row → treat as un-agreed
                slots_remaining = 6;  // default per legacy CHARSLOT_MAX
            }
            else
            {
                if (agree_ind == soci::i_null) agreement_ok = 0;
                if (slots_ind == soci::i_null) slots_remaining = 6;
            }
        }
        // Bound to the legacy tinyint range; if it ever drifts out
        // of range, clamp rather than send a nonsense byte on the wire.
        const std::uint8_t cap = static_cast<std::uint8_t>(
            std::clamp(slots_remaining, 0, 255));
        // PC-Bang + premium-tier lookups. The legacy TLogin SP populates
        // these from TPCBANG (IP-range whitelist) and TUSERPREMIUM
        // (active subscription). Both tables are optional in the modern
        // schema: missing tables → fall back to 0, matching the legacy
        // behavior when the operator hasn't deployed those side tables.
        std::uint8_t in_pc_bang = 0;
        try
        {
            int hits = 0;
            sql << "SELECT COUNT(*) FROM \"TPCBANG\" "
                   "WHERE \"szIP\" = :ip OR :ip LIKE \"szIPRange\"",
                soci::use(req.client_ip), soci::use(req.client_ip),
                soci::into(hits);
            if (hits > 0) in_pc_bang = 1;
        }
        catch (const std::exception& ex)
        {
            if (tx) throw;
            // TPCBANG missing or shape doesn't match — keep going at 0.
            spdlog::debug("auth: TPCBANG lookup skipped: {}", "database operation failed");
        }

        std::uint32_t premium_id = 0;
        try
        {
            int prem = 0;
            const char* prem_sql = is_mssql
                ? "SELECT TOP 1 \"dwPremiumID\" FROM \"TUSERPREMIUM\" "
                  "WHERE \"dwUserID\" = :u AND \"dtExpire\" >= CURRENT_TIMESTAMP "
                  "ORDER BY \"dwPremiumID\" DESC"
                : "SELECT \"dwPremiumID\" FROM \"TUSERPREMIUM\" "
                  "WHERE \"dwUserID\" = :u AND \"dtExpire\" >= CURRENT_TIMESTAMP "
                  "ORDER BY \"dwPremiumID\" DESC LIMIT 1";
            soci::statement st = (sql.prepare << prem_sql,
                soci::use(user_id),
                soci::into(prem));
            st.execute(true);
            if (st.got_data()) premium_id = static_cast<std::uint32_t>(prem);
        }
        catch (const std::exception& ex)
        {
            if (tx) throw;
            spdlog::debug("auth: TUSERPREMIUM lookup skipped: {}", "database operation failed");
        }

        // Last-played char (TLogin SP returns it as the 7th OUT
        // param). Modern schema parks it on TUSERINFOTABLE.dwLastCharID;
        // missing column or NULL row → 0, matching the "no last char"
        // path the client UI already handles.
        std::uint32_t last_char_id = 0;
        try
        {
            int last = 0;
            soci::indicator ind = soci::i_null;
            soci::statement st = (sql.prepare <<
                "SELECT \"dwLastCharID\" FROM \"TUSERINFOTABLE\" "
                "WHERE \"dwUserID\" = :u",
                soci::use(user_id),
                soci::into(last, ind));
            st.execute(true);
            if (st.got_data() && ind != soci::i_null && last > 0)
                last_char_id = static_cast<std::uint32_t>(last);
        }
        catch (const std::exception& ex)
        {
            if (tx) throw;
            spdlog::debug("auth: TUSERINFOTABLE.dwLastCharID lookup skipped: {}",
                "database operation failed");
        }

        if (tx) tx->commit();
        spdlog::info("auth: user '{}' (uid={}) authenticated, agreement={} pc_bang={} premium={} last_char={}",
            req.user_id, user_id, agreement_ok, in_pc_bang, premium_id, last_char_id);

        return AuthResult{
            .status = agreement_ok == 1 ? AuthStatus::Success : AuthStatus::AgreementNeeded,
            .user_id = user_id,
            .session_key = static_cast<std::uint32_t>(session_key),
            .create_char_count = cap,
            .in_pc_bang = in_pc_bang,
            .premium_id = premium_id,
            .last_char_id = last_char_id,
        };
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth: DB error for '{}': {}", req.user_id, "database operation failed");
        return AuthResult{ .status = AuthStatus::InternalError };
    }
}

void SociAuthService::SetAgreement(std::int32_t user_id)
{
    if (user_id == 0) return;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        if (m_pool.GetBackend() == fourstory::db::Backend::PostgreSQL)
        {
            auto tx = BeginLoginTransaction(m_pool, sql, m_owner_token);
            sql << "INSERT INTO \"TUSERINFOTABLE\" (\"dwUserID\",\"bCanCreateCharCount\",\"bAgreement\") "
                   "VALUES (:u,6,1) ON CONFLICT (\"dwUserID\") DO UPDATE SET \"bAgreement\"=1",
                soci::use(user_id);
            tx->commit();
            return;
        }
        // Real schema: agreement lives in TUSERINFOTABLE.bAgreement, NOT
        // TACCOUNT_PW.bCheck (which is a separate, legacy flag). The row
        // may not exist for older accounts that never touched the
        // agreement flow, so existence-check first to decide INSERT vs
        // UPDATE. (SOCI 4's session has no portable get_affected_rows;
        // doing the check up front is simpler than juggling statement
        // objects per-dialect.)
        int exists = 0;
        sql << "SELECT COUNT(*) FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = :uid",
            soci::use(user_id), soci::into(exists);
        if (exists > 0)
        {
            sql << "UPDATE \"TUSERINFOTABLE\" SET \"bAgreement\" = 1 "
                   "WHERE \"dwUserID\" = :uid",
                soci::use(user_id);
        }
        else
        {
            try
            {
                sql << "INSERT INTO \"TUSERINFOTABLE\" "
                       "(\"dwUserID\", \"bCanCreateCharCount\", \"bAgreement\") "
                       "VALUES (:uid, 6, 1)",
                    soci::use(user_id);
            }
            catch (const std::exception& ex)
            {
                // Duplicate-key race with a concurrent SetAgreement —
                // the row exists now, retry the UPDATE.
                spdlog::debug("auth.SetAgreement uid={} insert raced: {} — retry UPDATE",
                    user_id, "database operation failed");
                sql << "UPDATE \"TUSERINFOTABLE\" SET \"bAgreement\" = 1 "
                       "WHERE \"dwUserID\" = :uid",
                    soci::use(user_id);
            }
        }
        spdlog::info("auth.SetAgreement uid={}", user_id);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.SetAgreement uid={} DB error: {}",
            user_id, "database operation failed");
        if (m_pool.GetBackend() == fourstory::db::Backend::PostgreSQL)
            throw std::runtime_error("Agreement persistence failed");
    }
}

SecurityCodeResult SociAuthService::VerifySecurityCode(const std::string& token,
                                                       const std::string& client_ip,
                                                       const std::string& code)
{
    if (m_pool.GetBackend() != fourstory::db::Backend::PostgreSQL || token.empty())
        return SecurityCodeResult::Unavailable;
    try {
        auto lease = m_pool.Acquire(); auto& sql = *lease;
        auto tx = BeginLoginTransaction(m_pool, sql, m_owner_token);
        std::string expected; int attempts = 0;
        sql << "SELECT code_digest,attempts FROM app_global.login_security_challenge "
               "WHERE token=:t AND owner_token=:o AND client_ip=:ip AND verified_at IS NULL "
               "AND expires_at>clock_timestamp() AND attempts<5 FOR UPDATE",
            soci::use(token), soci::use(m_owner_token), soci::use(client_ip),
            soci::into(expected), soci::into(attempts);
        if (!sql.got_data()) return SecurityCodeResult::Unavailable;
        const auto digest = CodeDigest(token, code);
        const bool correct = code.size() == 6 && expected.size() == digest.size() &&
            CRYPTO_memcmp(expected.data(), digest.data(), digest.size()) == 0;
        if (correct)
            sql << "UPDATE app_global.login_security_challenge SET verified_at=clock_timestamp() WHERE token=:t",
                soci::use(token);
        else
            sql << "UPDATE app_global.login_security_challenge SET attempts=attempts+1 WHERE token=:t",
                soci::use(token);
        tx->commit();
        if (correct) return SecurityCodeResult::Correct;
        return attempts + 1 < 5 ? SecurityCodeResult::Incorrect : SecurityCodeResult::Unavailable;
    } catch (...) {
        spdlog::error("auth: security verification failed");
        return SecurityCodeResult::Unavailable;
    }
}

void SociAuthService::CancelSecurityChallenge(const std::string& token)
{
    if (m_pool.GetBackend() != fourstory::db::Backend::PostgreSQL || token.empty()) return;
    try {
        auto lease = m_pool.Acquire(); auto& sql = *lease;
        auto tx = BeginLoginTransaction(m_pool, sql, m_owner_token);
        sql << "DELETE FROM app_global.login_security_challenge WHERE token=:t AND owner_token=:o",
            soci::use(token), soci::use(m_owner_token);
        tx->commit();
    } catch (...) { spdlog::error("auth: security challenge cleanup failed"); }
}

AuthResult SociAuthService::AuthenticateTest(const std::string& client_ip)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        auto tx = BeginLoginTransaction(m_pool, sql, m_owner_token);
        const bool is_mssql = (m_pool.GetBackend() == fourstory::db::Backend::Odbc);
        // Pick a random dwUserID from TTESTLOGINUSER. NEWID() / RAND()
        // each have caveats — NEWID() is per-row randomization, exactly
        // what legacy CSPTestLogin uses. PG equivalent is random().
        const char* pick_sql = is_mssql
            ? "SELECT TOP 1 \"dwuserid\" FROM \"TTESTLOGINUSER\" "
              "ORDER BY NEWID()"
            : "SELECT \"dwuserid\" FROM \"TTESTLOGINUSER\" "
              "ORDER BY random() LIMIT 1";

        int test_user_id = 0;
        bool got = false;
        {
            soci::statement st = (sql.prepare << pick_sql,
                soci::into(test_user_id));
            st.execute(true);
            got = st.got_data();
        }
        if (!got || test_user_id == 0)
        {
            spdlog::warn("auth.AuthenticateTest: TTESTLOGINUSER pool is empty");
            return AuthResult{ .status = AuthStatus::InternalError };
        }

        // Insert TCURRENTUSER + TLOG just like a real login so the
        // session has a dwKEY and the disconnect cleanup path works.
        if (tx && !LockAccount(sql, test_user_id)) throw std::runtime_error("Account unavailable");
        int session_key = 0;
        // Same full NOT-NULL column shape as the normal Authenticate
        // path — see notes there.
        const char* insert_user_sql = is_mssql
            ? "INSERT INTO \"TCURRENTUSER\" "
              "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
              " \"wPort\", \"bLocked\", \"szLoginIP\") "
              "OUTPUT INSERTED.\"dwKEY\" "
              "VALUES (:uid, 0, 0, 0, 0, 0, :ip)"
            : "INSERT INTO \"TCURRENTUSER\" "
              "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
              " \"wPort\", \"bLocked\", \"szLoginIP\") "
              "VALUES (:uid, 0, 0, 0, 0, 0, :ip) "
              "RETURNING \"dwKEY\"";
        sql << insert_user_sql,
            soci::use(test_user_id), soci::use(client_ip),
            soci::into(session_key);
        sql << "INSERT INTO \"TLOG\" "
               "(\"dwKEY\", \"dwUserID\", \"dwCharID\", "
               " \"bGroupID\", \"bChannel\", "
               " \"timeLOGIN\", \"timeLOGOUT\") "
               "VALUES (:k, :u, 0, 0, 0, "
               "        CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            soci::use(session_key), soci::use(test_user_id);

        spdlog::info("auth.AuthenticateTest: test_user_id={}", test_user_id);
        if (tx) tx->commit();
        return AuthResult{
            .status = AuthStatus::Success,
            .user_id = test_user_id,
            .session_key = static_cast<std::uint32_t>(session_key),
            .create_char_count = 6,
        };
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.AuthenticateTest DB error: {}", "database operation failed");
        return AuthResult{ .status = AuthStatus::InternalError };
    }
}

bool SociAuthService::VerifyPassword(std::int32_t user_id,
                                     const std::string& password)
{
    if (user_id == 0) return false;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        std::string stored;
        soci::indicator pw_ind = soci::i_null;
        bool got_row = false;
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"szPasswd\" FROM \"TACCOUNT_PW\" WHERE \"dwUserID\" = :uid",
                soci::use(user_id),
                soci::into(stored, pw_ind));
            st.execute(true);
            got_row = st.got_data();
        }
        if (!got_row || pw_ind == soci::i_null) return false;
        const bool matched = CheckPassword(stored, password);
        spdlog::debug("auth.VerifyPassword uid={} → {}", user_id, matched);
        return matched;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.VerifyPassword uid={} DB error: {}",
            user_id, "database operation failed");
        return false;
    }
}

} // namespace tloginsvr::services
