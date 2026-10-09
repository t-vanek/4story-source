// Real PostgreSQL integration; synthetic accounts only in an owned disposable DB.
#include "services/soci_auth_service.h"
#include "services/postgresql_login_owner.h"
#include "services/soci_map_server_locator.h"
#include "services/soci_session_terminator.h"
#include "services/bcrypt_util.h"
#include "db/schema_validator.h"
#include "fourstory/db/session_pool.h"
#include "fourstory/smtp/spdlog_smtp_client.h"
#include <soci/soci.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>
#include <barrier>
#include <future>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

namespace {
using namespace tloginsvr::services;
using fourstory::db::SessionPool;
using fourstory::db::Backend;
int failures = 0;
void Check(bool ok, const char* label) {
    std::cout << (ok ? "PASS " : "FAIL ") << label << '\n';
    if (!ok) ++failures;
}
int Number(soci::session& sql, const std::string& query) {
    int n = -1; sql << query, soci::into(n); return n;
}

constexpr auto security_password = "4Story-Synthetic-Security!";
constexpr auto security_credential = "6444b65498b4c2fc6c3ed2e16cdf93ea67e34dab";
AuthRequest SecurityRequest(int id) {
    return AuthRequest{"SyntheticSecurity" + std::to_string(id), security_credential, "192.0.2.71", 0x2918};
}
SecurityChallenge Issue(SociAuthService& auth, int id) {
    auto result = auth.Authenticate(SecurityRequest(id));
    if (result.status != AuthStatus::SecurityRequired || !result.security_challenge)
        throw std::runtime_error("Synthetic security challenge missing");
    return *result.security_challenge;
}
AuthRequest Retry(int id, const SecurityChallenge& challenge) {
    auto req = SecurityRequest(id); req.password = security_password;
    req.security_retry_token = challenge.token; return req;
}
void VerifySecurity(SociAuthService& auth, SociSessionTerminator& term, soci::session& admin) {
    auto c = Issue(auth, 120);
    Check(c.code.size() == 6 && c.code.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos &&
          c.token.size() == 64 && Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=120") == 0,
          "six-character CSPRNG challenge has private 256-bit token and creates no session");
    auto unverified = Retry(120, c);
    Check(auth.Authenticate(unverified).status == AuthStatus::InternalError,
          "unconfirmed connection grant cannot select form-password retry");
    Check(auth.VerifySecurityCode(c.token, "192.0.2.72", c.code) == SecurityCodeResult::Unavailable &&
          auth.VerifySecurityCode(std::string(64,'0'), "192.0.2.71", c.code) == SecurityCodeResult::Unavailable,
          "foreign IP and missing challenge cannot confirm a code");
    SecurityCodeResult ca{}, cb{}; std::barrier start(3);
    std::jthread a([&]{start.arrive_and_wait(); ca=auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);});
    std::jthread b([&]{start.arrive_and_wait(); cb=auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);});
    start.arrive_and_wait(); a.join(); b.join();
    Check((ca==SecurityCodeResult::Correct && cb==SecurityCodeResult::Unavailable) ||
          (cb==SecurityCodeResult::Correct && ca==SecurityCodeResult::Unavailable),
          "concurrent code confirmation grants exactly one LOGIN retry");
    Check(Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=120") == 0 &&
          Number(admin,"SELECT count(*) FROM \"TLOG\" WHERE \"dwUserID\"=120") == 0,
          "confirmation creates neither session nor login audit");
    auto plain=SecurityRequest(120); plain.password=security_password;
    Check(auth.Authenticate(plain).status==AuthStatus::WrongPassword,
          "form password is never accepted without the private verified grant");
    auto foreign=Retry(121,c);
    Check(auth.Authenticate(foreign).status==AuthStatus::InternalError,
          "verified grant cannot authenticate a different account");
    auto success=auth.Authenticate(Retry(120,c));
    Check(success.status==AuthStatus::Success && success.user_id==120 && success.session_key && success.create_char_count==6,
          "source form-password retry commits normal full login result");
    Check(auth.Authenticate(Retry(120,c)).status==AuthStatus::InternalError &&
          Number(admin,"SELECT count(*) FROM \"TLOG\" WHERE \"dwUserID\"=120")==1 &&
          Number(admin,"SELECT \"bLocked\" FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=120")==0 &&
          Number(admin,"SELECT count(*) FROM \"TUSERTRUSTEDIP\"")==0,
          "grant replay cannot duplicate audit, lock current session or create trusted-IP bypass");
    term.Terminate(120,success.session_key,TerminationReason::Disconnect);

    c=Issue(auth,121);
    for(int i=0;i<5;++i)
        Check(auth.VerifySecurityCode(c.token,"192.0.2.71","!!!!!!")==
              (i==4?SecurityCodeResult::Unavailable:SecurityCodeResult::Incorrect),
              "incorrect code uses bounded five-attempt budget");
    Check(auth.VerifySecurityCode(c.token,"192.0.2.71",c.code)==SecurityCodeResult::Unavailable,
          "correct code cannot revive an exhausted challenge");
    auth.CancelSecurityChallenge(c.token);

    c=Issue(auth,122);
    admin << "UPDATE login_security_challenge SET issued_at=CURRENT_TIMESTAMP-interval '6 minutes', "
             "expires_at=CURRENT_TIMESTAMP-interval '1 minute' WHERE user_id=122";
    Check(auth.VerifySecurityCode(c.token,"192.0.2.71",c.code)==SecurityCodeResult::Unavailable,
          "expired challenge refuses even a correct code");
    c=Issue(auth,123);
    auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);
    admin << "UPDATE login_security_challenge SET issued_at=clock_timestamp()-interval '2 minutes', "
             "expires_at=clock_timestamp()+interval '2 minutes', verified_at=clock_timestamp()-interval '31 seconds' WHERE user_id=123";
    Check(auth.Authenticate(Retry(123,c)).status==AuthStatus::InternalError,
          "verified LOGIN retry expires after thirty seconds");
    auth.CancelSecurityChallenge(c.token);

    for(int i=0;i<3;++i) { c=Issue(auth,124); auth.CancelSecurityChallenge(c.token); }
    Check(auth.Authenticate(SecurityRequest(124)).status==AuthStatus::InternalError &&
          Number(admin,"SELECT issued_count FROM login_security_rate WHERE user_id=124")==3,
          "three-per-five-minute issuance budget survives cancellation");
    admin << "UPDATE login_security_rate SET window_start=clock_timestamp()-interval '6 minutes' WHERE user_id=124";
    c=Issue(auth,124); auth.CancelSecurityChallenge(c.token);
    Check(Number(admin,"SELECT issued_count FROM login_security_rate WHERE user_id=124")==1,
          "issuance budget resets after its five-minute window");

    c=Issue(auth,125); auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);
    admin << "INSERT INTO \"TUSERPROTECTED\"(\"dwUserID\",\"bBlockType\",\"bEternal\",\"startTime\",\"dwDuration\",\"bBlockReason\",\"szComment\",\"szGMID\",\"sentBanMail\") "
             "VALUES (125,1,1,CURRENT_TIMESTAMP,0,0,'synthetic','test',0)";
    Check(auth.Authenticate(Retry(125,c)).status==AuthStatus::Banned,
          "LOGIN retry rechecks a ban added after code confirmation");
    auth.CancelSecurityChallenge(c.token);
    c=Issue(auth,126); auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);
    const auto changed=bcrypt_util::MakeBcryptHash("changed-wire-credential");
    admin << "UPDATE \"TACCOUNT_PW\" SET \"szPasswd\"=:p WHERE \"dwUserID\"=126",soci::use(changed);
    Check(auth.Authenticate(Retry(126,c)).status==AuthStatus::WrongPassword,
          "LOGIN retry rechecks a password changed after code confirmation");
    auth.CancelSecurityChallenge(c.token);
    c=Issue(auth,127); auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);
    admin << "UPDATE \"TUSERINFOTABLE\" SET \"bAgreement\"=0,\"dwLastCharID\"=731 WHERE \"dwUserID\"=127";
    auto agreement=auth.Authenticate(Retry(127,c));
    Check(agreement.status==AuthStatus::AgreementNeeded && agreement.session_key && agreement.last_char_id==731,
          "LOGIN retry preserves current agreement gate and last-character fields");
    term.Terminate(127,agreement.session_key,TerminationReason::Disconnect);

    c=Issue(auth,128); auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);
    admin << "CREATE FUNCTION app_global.fail_security_audit() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN "
             "IF NEW.\"dwUserID\"=128 THEN RAISE EXCEPTION 'SyntheticSensitiveDiagnostic'; END IF; RETURN NEW; END $$";
    admin << "CREATE TRIGGER security_failure BEFORE INSERT ON \"TLOG\" FOR EACH ROW EXECUTE FUNCTION app_global.fail_security_audit()";
    Check(auth.Authenticate(Retry(128,c)).status==AuthStatus::InternalError &&
          Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=128")==0 &&
          Number(admin,"SELECT count(*) FROM login_security_challenge WHERE user_id=128 AND verified_at IS NOT NULL")==1,
          "audit failure atomically rolls back session and challenge consumption");
    auth.CancelSecurityChallenge(c.token); // Same cleanup used by the production handler.
    admin << "DROP TRIGGER security_failure ON \"TLOG\"";

    c=Issue(auth,129); auth.VerifySecurityCode(c.token,"192.0.2.71",c.code);
    AuthResult ra,rb; std::barrier retry_start(3);
    std::jthread ar([&]{retry_start.arrive_and_wait();ra=auth.Authenticate(Retry(129,c));});
    std::jthread br([&]{retry_start.arrive_and_wait();rb=auth.Authenticate(Retry(129,c));});
    retry_start.arrive_and_wait();ar.join();br.join();
    Check((ra.status==AuthStatus::Success && rb.status==AuthStatus::InternalError) ||
          (rb.status==AuthStatus::Success && ra.status==AuthStatus::InternalError),
          "concurrent confirmed retries create exactly one session");
    term.Terminate(129,ra.session_key?ra.session_key:rb.session_key,TerminationReason::Disconnect);
    auto first=Issue(auth,130), second=Issue(auth,130);
    auth.CancelSecurityChallenge(second.token);
    Check(auth.VerifySecurityCode(first.token,"192.0.2.71",first.code)==SecurityCodeResult::Correct,
          "cancelling one pending connection preserves another account challenge");
    auth.CancelSecurityChallenge(first.token);
    auto direct=SecurityRequest(131); direct.site_code_present=true;
    Check(auth.Authenticate(direct).status==AuthStatus::InternalError &&
          Number(admin,"SELECT count(*) FROM login_security_challenge WHERE user_id=131")==0,
          "unverified direct-login profile cannot enable form-password retry adapter");
    // Leave only future failover fixture challenges, no expired test residue.
    admin << "DELETE FROM login_security_challenge WHERE user_id BETWEEN 120 AND 131";
}

void Verify(const char* app, const char* fixture) {
    SessionPool pool(Backend::PostgreSQL, app, 4);
    SessionPool fixtures(Backend::PostgreSQL, fixture, 1);
    auto lease = fixtures.Acquire(); auto& admin = *lease;
    std::string db;
    admin << "SELECT current_database()", soci::into(db);
    if (db.rfind("fourstory_runtime_", 0) != 0)
        throw std::runtime_error("Requires disposable runtime database");
    tloginsvr::db::ValidateGlobalSchema(pool);
    Check(true, "production startup schema validator accepts migrated app_global");
    admin << "SET search_path=app_global,pg_catalog";
    const std::string credential = "d7c9416b31ba5b02b27fe10c13ad3cbd8d9ad81d";
    const std::string hash = bcrypt_util::MakeBcryptHash(credential);
    if (hash.empty()) throw std::runtime_error("Fixture hash generation failed");
    for (int id = 101; id <= 109; ++id) {
        const auto name = "Synthetic" + std::to_string(id);
        admin << "INSERT INTO \"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES (:u,:n,:p)",
            soci::use(id), soci::use(name), soci::use(hash);
        admin << "INSERT INTO \"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES (:u,1)", soci::use(id);
    }

    const auto security_hash=bcrypt_util::MakeBcryptHash(security_credential);
    for(int id=120;id<=221;++id) {
        if(id>133 && id<206) continue;
        const auto name="SyntheticSecurity"+std::to_string(id);
        const auto email="synthetic"+std::to_string(id)+"@example.invalid";
        admin << "INSERT INTO \"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES (:u,:n,:p)",
            soci::use(id),soci::use(name),soci::use(security_hash);
        admin << "INSERT INTO \"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES (:u,1)",soci::use(id);
        admin << "INSERT INTO \"TUSEREMAIL\" VALUES (:u,:e,1)",soci::use(id),soci::use(email);
    }
    // Preserve this dedicated account for the subsequent actual-daemon wire test.
    admin << "INSERT INTO \"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES (201,'SyntheticWire',:p)", soci::use(hash);
    admin << "INSERT INTO \"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES (201,1)";
    admin << "INSERT INTO \"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES (202,'SyntheticAgreement',:p)", soci::use(hash);
    admin << "INSERT INTO \"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES (202,0)";
    admin << "INSERT INTO \"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES (203,'SyntheticAbandoned',:p)", soci::use(hash);
    admin << "INSERT INTO \"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES (203,1)";
    for (int id : {204,205}) {
        const std::string name = id == 204 ? "SyntheticLive" : "SyntheticDrain";
        admin << "INSERT INTO \"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES (:u,:n,:p)", soci::use(id), soci::use(name), soci::use(hash);
        admin << "INSERT INTO \"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES (:u,1)", soci::use(id);
    }
    admin << "CREATE FUNCTION app_global.delay_synthetic_audit() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN "
             "IF NEW.\"dwUserID\" IN (203,205) THEN PERFORM pg_sleep(1); END IF; RETURN NEW; END $$";
    admin << "CREATE TRIGGER synthetic_delay BEFORE INSERT ON \"TLOG\" FOR EACH ROW EXECUTE FUNCTION app_global.delay_synthetic_audit()";
    auto owner = std::make_unique<PostgreSQLLoginOwner>(app);
    Check(owner->Healthy(), "native Login claims a live dedicated process owner");
    bool second_denied = false;
    try { PostgreSQLLoginOwner second_owner(app); }
    catch (...) { second_denied = true; }
    Check(second_denied && owner->Healthy(), "second live owner is refused without replacing the first");
    SociAuthService auth(pool, owner->Token());
    SociSessionTerminator term(pool, owner->Token());
    auto request = [&](int id) { return AuthRequest{"Synthetic" + std::to_string(id), credential, "192.0.2.71", 0x2918}; };
    auto req = request(101);
    req.password = "incorrect";
    Check(auth.Authenticate(req).status == AuthStatus::WrongPassword, "incorrect wire credential rejected");
    req.password = credential + std::string(1, '\0') + "suffix";
    Check(auth.Authenticate(req).status == AuthStatus::WrongPassword, "embedded NUL cannot alias bcrypt credential");
    admin << "INSERT INTO \"TIPAUTHORITY\" VALUES ('192.0.2.99',0)";
    req = request(101); req.client_ip = "192.0.2.99"; req.password = "incorrect";
    Check(auth.Authenticate(req).status == AuthStatus::WrongPassword, "TCheckIP restriction does not override original credential error precedence");
    req.password = credential;
    Check(auth.Authenticate(req).status == AuthStatus::IpRestricted, "valid credential on restricted IP receives LR_BLOCK category");
    req = request(199);
    Check(auth.Authenticate(req).status == AuthStatus::NoUser, "unknown account rejected");
    req = request(101); req.user_id = "sYNTHETIC101   ";
    auto first = auth.Authenticate(req);
    Check(first.status == AuthStatus::Success && first.user_id == 101 && first.session_key && first.create_char_count == 6,
          "native login accepts ASCII case and trailing-space policy with original ACK fields");
    Check(Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=101") == 1 &&
          Number(admin,"SELECT count(*) FROM \"TLOG\" WHERE \"dwUserID\"=101") == 1 &&
          Number(admin,"SELECT count(*) FROM \"TACCOUNT_PW\" WHERE \"dwUserID\"=101 AND \"dLastLogin\" IS NOT NULL") == 1,
          "successful transaction commits current session, audit and last-login time");
    term.Terminate(101, first.session_key, TerminationReason::Disconnect);
    auto second = auth.Authenticate(request(101));
    Check(second.status == AuthStatus::Success && second.session_key != first.session_key, "relogin gets a fresh session key");
    term.Terminate(101, first.session_key, TerminationReason::Disconnect);
    Check(Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=101") == 1, "delayed logout cannot delete newer session");
    term.Terminate(101, second.session_key, TerminationReason::MapHandoff, 731);
    Check(Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=101") == 1 &&
          Number(admin,"SELECT \"dwLastCharID\" FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\"=101") == 731,
          "handoff preserves current session and persists last character");
    term.Terminate(101, second.session_key, TerminationReason::Disconnect);

    std::barrier start(3);
    AuthResult a, b;
    std::jthread ta([&] { start.arrive_and_wait(); a = auth.Authenticate(request(102)); });
    std::jthread tb([&] { start.arrive_and_wait(); b = auth.Authenticate(request(102)); });
    start.arrive_and_wait(); ta.join(); tb.join();
    Check((a.status == AuthStatus::Success && b.status == AuthStatus::Duplicate) ||
          (b.status == AuthStatus::Success && a.status == AuthStatus::Duplicate), "concurrent login has exactly one success and one duplicate");
    Check(Number(admin,"SELECT count(*) FROM \"TLOG\" WHERE \"dwUserID\"=102") == 1 &&
          Number(admin,"SELECT \"bLocked\" FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=102") == 1,
          "concurrent loser locks existing session without adding audit");
    term.Terminate(102, a.session_key ? a.session_key : b.session_key, TerminationReason::Disconnect);

    admin << "CREATE FUNCTION app_global.fail_synthetic_audit() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN "
             "IF NEW.\"dwUserID\"=103 THEN RAISE EXCEPTION 'SyntheticSensitiveDiagnostic'; END IF; RETURN NEW; END $$";
    admin << "CREATE TRIGGER synthetic_failure BEFORE INSERT ON \"TLOG\" FOR EACH ROW EXECUTE FUNCTION app_global.fail_synthetic_audit()";
    Check(auth.Authenticate(request(103)).status == AuthStatus::InternalError, "injected audit failure refuses login");
    Check(Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=103") == 0 &&
          Number(admin,"SELECT count(*) FROM \"USERIPLOG\" WHERE \"Username\"='Synthetic103'") == 0 &&
          Number(admin,"SELECT count(*) FROM \"TACCOUNT_PW\" WHERE \"dwUserID\"=103 AND \"dLastLogin\" IS NOT NULL") == 0,
          "audit failure rolls back session, IP audit and last-login timestamp");
    admin << "DROP TRIGGER synthetic_failure ON \"TLOG\"";
    auto retry = auth.Authenticate(request(103));
    Check(retry.status == AuthStatus::Success, "pool connection is usable after transaction rollback");
    term.Terminate(103, retry.session_key, TerminationReason::Disconnect);

    admin << "UPDATE \"TUSERINFOTABLE\" SET \"bAgreement\"=0 WHERE \"dwUserID\"=104";
    auto agreement = auth.Authenticate(request(104));
    Check(agreement.status == AuthStatus::AgreementNeeded && agreement.session_key != 0, "agreement gate retains committed session");
    auth.SetAgreement(104); auth.SetAgreement(104);
    Check(Number(admin,"SELECT \"bAgreement\" FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\"=104") == 1, "agreement persistence is idempotent");
    term.Terminate(104, agreement.session_key, TerminationReason::Disconnect);
    admin << "INSERT INTO \"IPBLACKLIST_game\" VALUES ('192.0.2.72')";
    req = request(105); req.client_ip = "192.0.2.72";
    Check(auth.Authenticate(req).status == AuthStatus::IpBanned, "IP blacklist rejects before session creation");
    admin << "INSERT INTO \"TUSERPROTECTED\"(\"dwUserID\",\"bBlockType\",\"bEternal\",\"startTime\",\"dwDuration\",\"bBlockReason\",\"szComment\",\"szGMID\",\"sentBanMail\") "
             "VALUES (105,1,1,CURRENT_TIMESTAMP,0,0,'synthetic','test',0)";
    Check(auth.Authenticate(request(105)).status == AuthStatus::Banned, "eternal account ban rejects login");

    VerifySecurity(auth, term, admin);

    auto lobby = auth.Authenticate(request(107)), world = auth.Authenticate(request(108));
    admin << "UPDATE \"TCURRENTUSER\" SET \"dwCharID\"=900 WHERE \"dwUserID\"=108";
    Check(lobby.session_key && world.session_key && term.ClearStaleSessions() == 1 &&
          Number(admin,"SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=108") == 1,
          "startup cleanup removes lobby sessions and preserves map-owned rows");
    term.Terminate(108, world.session_key, TerminationReason::Disconnect);
    // A control-connection failure must not leave the former process able to
    // write through its otherwise healthy application pool after takeover.
    auto abandoned_challenge = Issue(auth,133);
    auth.VerifySecurityCode(abandoned_challenge.token,"192.0.2.71",abandoned_challenge.code);
    auto before_takeover = auth.Authenticate(request(109));
    Check(before_takeover.status == AuthStatus::Success, "old owner establishes a session before takeover");
    auto held = pool.Acquire();
    auto inflight = BeginLoginTransaction(pool, *held, owner->Token());
    int terminated = 0; const int old_pid = owner->BackendPid();
    admin << "SELECT CASE WHEN pg_terminate_backend(:p) THEN 1 ELSE 0 END", soci::use(old_pid), soci::into(terminated);
    Check(terminated == 1 && !owner->Healthy(), "owner connection loss is detected while ordinary pool remains alive");
    auto replacement = std::async(std::launch::async, [&] { return std::make_unique<PostgreSQLLoginOwner>(app); });
    bool waiting = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (Number(admin, "SELECT count(*) FROM pg_stat_activity WHERE datname=current_database() "
                          "AND wait_event_type='Lock' AND query LIKE 'INSERT INTO app_global.login_runtime_owner%'")) {
            waiting = true; break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Check(waiting, "replacement waits for an in-flight fenced transaction before changing owner");
    inflight->commit(); inflight.reset();
    auto new_owner = replacement.get();
    Check(new_owner->Healthy(), "replacement claims ownership after preceding transaction completes");
    SociAuthService new_auth(pool, new_owner->Token());
    SociSessionTerminator new_term(pool, new_owner->Token());
    Check(new_term.ClearStaleSessions() == 1, "replacement recovers previous owner's abandoned lobby session");
    Check(Number(admin,"SELECT count(*) FROM login_security_challenge WHERE user_id=133")==0 &&
          new_auth.Authenticate(Retry(133,abandoned_challenge)).status==AuthStatus::InternalError,
          "takeover removes old owner challenges and invalidates verified retry grants");
    auto current_challenge=Issue(new_auth,133);
    auth.CancelSecurityChallenge(current_challenge.token);
    Check(auth.VerifySecurityCode(current_challenge.token,"192.0.2.71",current_challenge.code)==SecurityCodeResult::Unavailable &&
          new_auth.VerifySecurityCode(current_challenge.token,"192.0.2.71",current_challenge.code)==SecurityCodeResult::Correct,
          "stale owner cannot cancel or confirm replacement challenge");
    new_auth.CancelSecurityChallenge(current_challenge.token);
    Check(Number(admin,"SELECT issued_count FROM login_security_rate WHERE user_id=133")==2,
          "challenge issuance budget survives process ownership replacement");
    auto new_session = new_auth.Authenticate(request(109));
    Check(new_session.status == AuthStatus::Success, "replacement accepts a new login");
    Check(auth.Authenticate(request(109)).status == AuthStatus::InternalError && term.ClearStaleSessions() == -1,
          "stale process cannot authenticate or sweep current sessions");
    term.Terminate(109, new_session.session_key, TerminationReason::Disconnect);
    Check(Number(admin, "SELECT count(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\"=109") == 1,
          "stale process cannot delete replacement session even with its correct key");
    admin << "INSERT INTO \"TMACHINE\" VALUES (9,'SyntheticHost',0)";
    admin << "INSERT INTO \"TSERVER\" VALUES (9,1,4,9,5815)";
    admin << "INSERT INTO \"TIPADDR\" VALUES (9,'192.0.2.80',1)";
    SociMapServerLocator old_route(pool, nullptr, owner->Token());
    SociMapServerLocator new_route(pool, nullptr, new_owner->Token());
    Check(!old_route.Lookup(109,9,1,0) && Number(admin, "SELECT \"bRouteID\" FROM \"TMACHINE\" WHERE \"bMachineID\"=9") == 0,
          "stale owner cannot mutate routing metadata or session entry state");
    auto endpoint = new_route.Lookup(109,9,1,0);
    Check(!endpoint &&
          Number(admin, "SELECT \"bRouteID\" FROM \"TMACHINE\" WHERE \"bMachineID\"=9") == 0,
          "unkeyed native lookup cannot select an endpoint or mutate routing");
    Check(auth.Authenticate(SecurityRequest(133)).status==AuthStatus::InternalError, "stale process cannot issue a security code");
    bool agreement_denied = false;
    try { auth.SetAgreement(109); } catch (...) { agreement_denied = true; }
    Check(agreement_denied, "stale process cannot persist agreement");
    new_term.Terminate(109, new_session.session_key, TerminationReason::Disconnect);

    {
        auto app_lease = pool.Acquire();
        bool denied=false;
        try { *app_lease << "INSERT INTO \"TACCOUNT_PW\"(\"szUserID\") VALUES ('UnauthorizedSynthetic')"; }
        catch (...) { denied=true; }
        Check(denied, "runtime role cannot provision accounts");
        denied=false;
        try { *app_lease << "SELECT count(*) FROM legacy_game.\"TMONSTERCHART\""; }
        catch (...) { denied=true; }
        Check(denied, "runtime login role cannot read historical game records");
    }
}
}
int main() {
    const auto* conn=std::getenv("FOURSTORY_TEST_PG_CONNINFO");
    const auto* fixture=std::getenv("FOURSTORY_LOGIN_FIXTURE_CONNINFO");
    if (!conn || !fixture) { std::cout << "SKIP: owned PostgreSQL login fixture required\n"; return 77; }
    std::ostringstream logs;
    auto sink=std::make_shared<spdlog::sinks::ostream_sink_mt>(logs);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("native-login-test", sink));
    try {
        Verify(conn, fixture);
        fourstory::smtp::SpdlogSmtpClient smtp;
        Check(!smtp.Send("sensitive@example.invalid", "private-subject", "SensitiveOneTimeCode"), "missing SMTP relay cannot claim delivery");
        Check(logs.str().find("SyntheticSensitiveDiagnostic") == std::string::npos &&
              logs.str().find(security_password) == std::string::npos &&
              logs.str().find(security_credential) == std::string::npos &&
              logs.str().find("SensitiveOneTimeCode") == std::string::npos &&
              logs.str().find("sensitive@example.invalid") == std::string::npos &&
              logs.str().find("d7c9416b31ba5b02b27fe10c13ad3cbd8d9ad81d") == std::string::npos,
              "logs exclude database diagnostics, wire credentials and email contents");
    } catch (...) { std::cerr << "Native login test failed (backend details suppressed)\n" << logs.str(); return 1; }
    return failures ? 1 : 0;
}
