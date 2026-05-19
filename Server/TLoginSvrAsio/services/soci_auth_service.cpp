#include "soci_auth_service.h"
#include "bcrypt_util.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <bcrypt/bcrypt.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <random>

namespace tloginsvr::services {

namespace {

// Wire-level password format the shipped client sends:
//
//   Client/TClient/TClientWnd.cpp:327 hashes the user-entered string
//   through GetSHA1String (TClientGame.cpp:462 — std SHA1 → hex)
//   BEFORE handing it to SendCS_LOGIN_REQ. The legacy server stored
//   that SHA1-hex value directly in TACCOUNT_PW.szPasswd; the modern
//   server stores a BCrypt hash *over* that SHA1-hex value instead.
//
// Migration to bcrypt is a hard cutover — the legacy plaintext /
// SHA1-hex compare path has been removed. Existing deploys must run
// the offline `tloginsvr_bcrypt_migrate` tool once before bringing
// the modern server online; any row in TACCOUNT_PW whose szPasswd
// does not start with a BCrypt prefix ($2a$/$2b$/$2y$) is treated as
// "wrong password" by the auth path below.
//
// $2a$ / $2b$ / $2y$ — BCrypt hash prefixes (Provos & Mazières).

// Returns true on a successful BCrypt check, false on any other
// stored shape (legacy plaintext / SHA1-hex / null) or on hash
// mismatch. The auth path treats `false` as LR_INVALIDPASSWD; a
// non-bcrypt row therefore looks identical to a wrong password and
// is the operator's cue to run the migration tool.
bool CheckPassword(const std::string& stored,
                   const std::string& candidate)
{
    if (stored.empty()) return false;
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

} // namespace

SociAuthService::SociAuthService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

AuthResult SociAuthService::Authenticate(const AuthRequest& req)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Step 1 — IP banlist. Legacy uses two parallel tables:
        //   1) `IPBLACKLIST_game` — exact-match IP list, queried inline
        //      inside TLogin SP (TLogin.sql:80). Operator-managed.
        //   2) `TIPAUTHORITY`     — LIKE-pattern list, queried by
        //      CSPCheckIP → TCheckIP SP. Returns code 6 (LR_NOMATCHED)
        //      which TLogin re-checks via @bIPCheck and early-aborts.
        //      Patterns like `192.168.%` ban entire subnets.
        // Modern collapses the two into a single Step 1 — both tables
        // are queried, either match → IpBanned. TIPAUTHORITY is
        // optional (table may not be deployed); we swallow the
        // "missing table" error like the TPCBANG / TUSERPREMIUM path.
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
                    return AuthResult{ .status = AuthStatus::IpBanned };
                }
            }
            catch (const std::exception& ex)
            {
                // TIPAUTHORITY missing or shape mismatch — skip the
                // pattern check and let the rest of auth proceed.
                spdlog::debug("auth: TIPAUTHORITY lookup skipped: {}", ex.what());
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
                    "SELECT \"dwUserID\", \"szPasswd\" FROM \"TACCOUNT_PW\" "
                    "WHERE \"szUserID\" = :uid",
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

        // Step 3 — password check.
        if (pw_ind == soci::i_null)
        {
            spdlog::info("auth: user '{}' (uid={}) wrong password (null hash)",
                req.user_id, user_id);
            return AuthResult{ .status = AuthStatus::WrongPassword };
        }
        if (!CheckPassword(stored_password, req.password))
        {
            spdlog::info("auth: user '{}' (uid={}) wrong password",
                req.user_id, user_id);
            return AuthResult{ .status = AuthStatus::WrongPassword };
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
            return AuthResult{
                .status = AuthStatus::Duplicate,
                .user_id = user_id,
            };
        }

        // Step 5b — 2FA new-device challenge.
        //
        // Lookup the user's email + 2FA toggle. If 2FA is on AND this
        // IP isn't on the trusted list, defer the login: do NOT insert
        // TCURRENTUSER/TLOG yet (the session has no dwKEY until the
        // CS_SECURITYCONFIRM_ACK comes back). Returning SecurityRequired
        // tells the handler to issue a code, mail it, and send
        // CS_SECURITYCONFIRM_REQ to the client.
        //
        // Three release valves prevent locking users out:
        //   * email row missing  → log info, fall through to normal login
        //   * 2FA flag = 0       → fall through (per-user opt-in)
        //   * client_ip empty    → fall through (test paths)
        if (!req.client_ip.empty())
        {
            // Inline the email + trusted-IP check on the same session
            // we already hold — calling the public LookupEmail / etc.
            // would re-acquire a lease and deadlock under pool_size=1.
            std::string email_row;
            int tfa = 0;
            soci::indicator email_ind = soci::i_null;
            bool email_present = false;
            try
            {
                soci::statement st = (sql.prepare <<
                    "SELECT \"szEmail\", \"bTwoFactorEnabled\" FROM \"TUSEREMAIL\" "
                    "WHERE \"dwUserID\" = :u",
                    soci::use(user_id),
                    soci::into(email_row, email_ind),
                    soci::into(tfa));
                st.execute(true);
                email_present = st.got_data() && email_ind != soci::i_null;
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("auth: TUSEREMAIL lookup uid={} skipped: {}",
                    user_id, ex.what());
            }

            if (email_present && tfa != 0)
            {
                int trusted_hits = 0;
                try
                {
                    sql << "SELECT COUNT(*) FROM \"TUSERTRUSTEDIP\" "
                           "WHERE \"dwUserID\" = :u AND \"szIP\" = :ip",
                        soci::use(user_id), soci::use(req.client_ip),
                        soci::into(trusted_hits);
                }
                catch (const std::exception& ex)
                {
                    spdlog::debug("auth: TUSERTRUSTEDIP lookup uid={} skipped: {}",
                        user_id, ex.what());
                }
                if (trusted_hits == 0)
                {
                    spdlog::info("auth: user_id={} new device {} — 2FA challenge",
                        user_id, req.client_ip);
                    return AuthResult{
                        .status  = AuthStatus::SecurityRequired,
                        .user_id = user_id,
                    };
                }
            }
        }

        // Step 6 — success: insert TCURRENTUSER, capture identity dwKEY.
        //
        // Race-immune INSERT. Legacy's TLogin SP runs steps 5-9 in
        // one atomic transaction; our 8-statement port has a window
        // between the duplicate check above and this INSERT where a
        // second concurrent login for the same user could slip in
        // and both INSERTs would succeed, leaving two TCURRENTUSER
        // rows for one user.
        //
        // We close the window by switching from a plain INSERT to
        // `INSERT ... SELECT ... WHERE NOT EXISTS`. The SELECT runs
        // inside the INSERT statement and atomically blocks the row
        // when another concurrent INSERT races us. RETURNING /
        // OUTPUT yields zero rows when the WHERE-NOT-EXISTS clause
        // suppresses the insert; we detect that as "lost the race"
        // and fall back to the duplicate path.
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
              "RETURNING \"dwKEY\"";
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
                spdlog::debug("auth: TCURRENTUSER.dwSiteCode update skipped: {}",
                    ex.what());
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
                spdlog::debug("auth: TCURRENTUSER.bChanneling update skipped: {}",
                    ex.what());
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
                spdlog::debug("auth: USERIPLOG insert skipped: {}",
                    ex.what());
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
        // Cap at 0..6 — legacy column is tinyint; if it ever drifts out
        // of range, clamp rather than send a nonsense byte on the wire.
        const std::uint8_t cap = static_cast<std::uint8_t>(
            std::clamp(slots_remaining, 0, 255));
        if (agreement_ok != 1)
        {
            spdlog::info("auth: user_id={} needs agreement (bAgreement != 1)",
                user_id);
            return AuthResult{
                .status = AuthStatus::AgreementNeeded,
                .user_id = user_id,
                .session_key = static_cast<std::uint32_t>(session_key),
                .create_char_count = cap,
            };
        }

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
            // TPCBANG missing or shape doesn't match — keep going at 0.
            spdlog::debug("auth: TPCBANG lookup skipped: {}", ex.what());
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
            spdlog::debug("auth: TUSERPREMIUM lookup skipped: {}", ex.what());
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
            spdlog::debug("auth: TUSERINFOTABLE.dwLastCharID lookup skipped: {}",
                ex.what());
        }

        spdlog::info("auth: user '{}' (uid={}) success, session_key={} "
                     "pc_bang={} premium={} last_char={}",
            req.user_id, user_id, session_key, in_pc_bang, premium_id, last_char_id);

        return AuthResult{
            .status = AuthStatus::Success,
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
        spdlog::error("auth: DB error for '{}': {}", req.user_id, ex.what());
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
                    user_id, ex.what());
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
            user_id, ex.what());
    }
}

namespace {
std::string UpperAscii(std::string s)
{
    for (auto& c : s) if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    return s;
}
} // namespace

bool SociAuthService::VerifySecurityCode(std::int32_t user_id,
                                        const std::string& code)
{
    if (user_id == 0 || code.empty()) return false;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        std::string stored;
        int enabled = 0, tries = 0;
        soci::indicator s_ind = soci::i_null;
        bool got = false;
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"strSecurityCode\", \"bEnabled\", \"bTries\" "
                "FROM \"TSECURECODE\" WHERE \"dwUserID\" = :u",
                soci::use(user_id),
                soci::into(stored, s_ind),
                soci::into(enabled),
                soci::into(tries));
            st.execute(true);
            got = st.got_data();
        }
        if (!got || s_ind == soci::i_null || stored.empty())
        {
            spdlog::info("auth.VerifySecurityCode uid={} → no row / empty", user_id);
            return false;
        }
        if (enabled == 0)
        {
            spdlog::info("auth.VerifySecurityCode uid={} → bEnabled=0 (disabled)", user_id);
            return false;
        }
        const bool ok = UpperAscii(code) == UpperAscii(stored);
        if (ok)
        {
            // Clear the code on success — matches legacy
            // pUser->m_strCode.Empty(). Reset tries too so the next
            // issuance starts clean.
            sql << "UPDATE \"TSECURECODE\" "
                   "SET \"strSecurityCode\" = '', \"bTries\" = 0, \"bEnabled\" = 0 "
                   "WHERE \"dwUserID\" = :u",
                soci::use(user_id);
        }
        else
        {
            sql << "UPDATE \"TSECURECODE\" SET \"bTries\" = \"bTries\" + 1 "
                   "WHERE \"dwUserID\" = :u",
                soci::use(user_id);
        }
        spdlog::info("auth.VerifySecurityCode uid={} → {}", user_id, ok);
        return ok;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.VerifySecurityCode uid={} DB error: {}",
            user_id, ex.what());
        return false;
    }
}

std::string SociAuthService::IssueSecurityCode(std::int32_t user_id)
{
    if (user_id == 0) return {};
    // 6-char alphanumeric, A-Z+0-9 — matches legacy szPool format.
    constexpr char kPool[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    constexpr int kLen = 6;
    thread_local std::mt19937_64 rng{ std::random_device{}() };
    std::string code(kLen, ' ');
    for (int i = 0; i < kLen; ++i)
        code[i] = kPool[rng() % (sizeof(kPool) - 1)];

    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Upsert pattern — TSECURECODE has dwUserID as effective PK.
        // Try UPDATE first; if no row matched, INSERT.
        int exists = 0;
        sql << "SELECT COUNT(*) FROM \"TSECURECODE\" WHERE \"dwUserID\" = :u",
            soci::use(user_id), soci::into(exists);
        if (exists > 0)
        {
            sql << "UPDATE \"TSECURECODE\" "
                   "SET \"strSecurityCode\" = :c, \"bEnabled\" = 1, "
                   "    \"bTries\" = 0, \"iLockTick\" = 0 "
                   "WHERE \"dwUserID\" = :u",
                soci::use(code), soci::use(user_id);
        }
        else
        {
            sql << "INSERT INTO \"TSECURECODE\" "
                   "(\"dwUserID\", \"strSecurityCode\", \"bEnabled\", "
                   " \"bTries\", \"iLockTick\") VALUES (:u, :c, 1, 0, 0)",
                soci::use(user_id), soci::use(code);
        }
        spdlog::info("auth.IssueSecurityCode uid={} code='{}'", user_id, code);
        return code;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.IssueSecurityCode uid={} DB error: {}",
            user_id, ex.what());
        return {};
    }
}

std::optional<IAuthService::EmailRecord>
SociAuthService::LookupEmail(std::int32_t user_id)
{
    if (user_id == 0) return std::nullopt;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        std::string email;
        int tfa = 0;
        soci::indicator email_ind = soci::i_null;
        bool got = false;
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"szEmail\", \"bTwoFactorEnabled\" FROM \"TUSEREMAIL\" "
                "WHERE \"dwUserID\" = :u",
                soci::use(user_id),
                soci::into(email, email_ind),
                soci::into(tfa));
            st.execute(true);
            got = st.got_data();
        }
        if (!got || email_ind == soci::i_null) return std::nullopt;
        return EmailRecord{
            .email = std::move(email),
            .two_factor_enabled = tfa != 0,
        };
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("auth.LookupEmail uid={} skipped: {}", user_id, ex.what());
        return std::nullopt;
    }
}

bool SociAuthService::IsTrustedIp(std::int32_t user_id,
                                  const std::string& client_ip)
{
    if (user_id == 0 || client_ip.empty()) return false;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        int hits = 0;
        sql << "SELECT COUNT(*) FROM \"TUSERTRUSTEDIP\" "
               "WHERE \"dwUserID\" = :u AND \"szIP\" = :ip",
            soci::use(user_id), soci::use(client_ip), soci::into(hits);
        return hits > 0;
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("auth.IsTrustedIp uid={} ip={} skipped: {}",
            user_id, client_ip, ex.what());
        return false;
    }
}

void SociAuthService::AddTrustedIp(std::int32_t user_id,
                                  const std::string& client_ip)
{
    if (user_id == 0 || client_ip.empty()) return;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Composite PK (dwUserID, szIP) — INSERT may throw on dup;
        // swallow because the desired state (entry exists) is reached.
        try
        {
            sql << "INSERT INTO \"TUSERTRUSTEDIP\" (\"dwUserID\", \"szIP\") "
                   "VALUES (:u, :ip)",
                soci::use(user_id), soci::use(client_ip);
            spdlog::info("auth.AddTrustedIp uid={} ip={} → whitelisted",
                user_id, client_ip);
        }
        catch (const std::exception&)
        {
            // already there — fine.
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.AddTrustedIp uid={} DB error: {}", user_id, ex.what());
    }
}

std::uint32_t SociAuthService::CompleteSecurityLogin(std::int32_t user_id,
                                                    const std::string& client_ip)
{
    if (user_id == 0) return 0;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        const bool is_mssql = (m_pool.GetBackend() == fourstory::db::Backend::Odbc);
        int session_key = 0;
        // Full NOT-NULL column set matches the legacy MSSQL schema —
        // see Authenticate() above for the rationale.
        const char* insert_sql = is_mssql
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
        sql << insert_sql,
            soci::use(user_id), soci::use(client_ip),
            soci::into(session_key);
        sql << "UPDATE \"TACCOUNT_PW\" SET \"dLastLogin\" = CURRENT_TIMESTAMP "
               "WHERE \"dwUserID\" = :uid",
            soci::use(user_id);
        sql << "INSERT INTO \"TLOG\" "
               "(\"dwKEY\", \"dwUserID\", \"dwCharID\", "
               " \"bGroupID\", \"bChannel\", "
               " \"timeLOGIN\", \"timeLOGOUT\") "
               "VALUES (:key, :uid, 0, 0, 0, "
               "        CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            soci::use(session_key), soci::use(user_id);
        spdlog::info("auth.CompleteSecurityLogin uid={} ip={} session_key={}",
            user_id, client_ip, session_key);
        return static_cast<std::uint32_t>(session_key);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.CompleteSecurityLogin uid={} DB error: {}",
            user_id, ex.what());
        return 0;
    }
}

std::uint32_t SociAuthService::LookupLastCharId(std::int32_t user_id)
{
    if (user_id == 0) return 0;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
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
            return static_cast<std::uint32_t>(last);
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("auth.LookupLastCharId uid={} skipped: {}", user_id, ex.what());
    }
    return 0;
}

AuthResult SociAuthService::AuthenticateTest(const std::string& client_ip)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
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

        spdlog::info("auth.AuthenticateTest: test_user_id={} session_key={}",
            test_user_id, session_key);
        return AuthResult{
            .status = AuthStatus::Success,
            .user_id = test_user_id,
            .session_key = static_cast<std::uint32_t>(session_key),
            .create_char_count = 6,
        };
    }
    catch (const std::exception& ex)
    {
        spdlog::error("auth.AuthenticateTest DB error: {}", ex.what());
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
            user_id, ex.what());
        return false;
    }
}

} // namespace tloginsvr::services
