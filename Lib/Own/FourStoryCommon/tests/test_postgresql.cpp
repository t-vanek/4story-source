#include "fourstory/db/session_pool.h"
#include "fourstory/db/orm/sp_call.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

namespace {
int failures = 0;
void Check(bool value, const char* label)
{
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
    if (!value) ++failures;
}
using fourstory::db::Backend;
using fourstory::db::SessionPool;

void ConnectionOptions()
{
    for (const std::string suffix : {
            " invalid_option=SyntheticSecret", " connect_timeout=0",
            " connect_timeout=-1", " connect_timeout=301", " connect_timeout=1garbage",
            " sslmode=prefer", " sslmode=allow", " password='SyntheticSecret"})
    {
        bool rejected = false;
        try { SessionPool pool(Backend::PostgreSQL, "host=localhost " + suffix, 1); }
        catch (const fourstory::db::ConnectionError& error)
        {
            rejected = true;
            Check(std::string(error.what()).find("SyntheticSecret") == std::string::npos,
                  "invalid connection diagnostics redact values");
        }
        Check(rejected, "invalid or unbounded connection options refused before connection");
    }
}

void Integration(const std::string& options)
{
    using namespace std::chrono_literals;
    SessionPool pool(Backend::PostgreSQL, options, 2, 100ms);
    {
        auto first = pool.Acquire();
        auto second = pool.Acquire();
        std::string database;
        *first << "SELECT current_database()", soci::into(database);
        if (database.rfind("fourstory_runtime_", 0) != 0)
            throw std::runtime_error("Native tests require an isolated fourstory_runtime_* database");
        int first_id = 0, second_id = 0;
        *first << "SELECT pg_backend_pid()", soci::into(first_id);
        *second << "SELECT pg_backend_pid()", soci::into(second_id);
        Check(first_id != second_id, "pool sessions are distinct native PostgreSQL connections");
        int tls = 0;
        *first << "SELECT CASE WHEN ssl THEN 1 ELSE 0 END FROM pg_stat_ssl WHERE pid=pg_backend_pid()", soci::into(tls);
        Check(tls == 1, "native PostgreSQL connection uses TLS with certificate verification");
        const auto start = std::chrono::steady_clock::now();
        bool timed_out = false;
        try { auto exhausted = pool.Acquire(25ms); }
        catch (const fourstory::db::AcquireTimeout&) { timed_out = true; }
        Check(timed_out && std::chrono::steady_clock::now() - start < 1s, "pool exhaustion has a bounded timeout");
        auto moved = std::move(first);
        int value = 0;
        *moved << "SELECT 1", soci::into(value);
        Check(value == 1, "moved lease retains exclusive live session");
    }
    {
        auto lease = pool.Acquire();
        auto& sql = *lease;
        sql << "DELETE FROM synthetic_runtime.transactions";
        const std::string value = "Synthetic Žluťoučký 韓 ' ; -- bound value";
        {
            soci::transaction tx(sql);
            sql << "INSERT INTO synthetic_runtime.transactions(id,value) VALUES (1,:value)", soci::use(value);
            tx.commit();
        }
        std::string actual;
        sql << "SELECT value FROM synthetic_runtime.transactions WHERE id=1", soci::into(actual);
        Check(actual == value, "native prepared binding preserves Unicode and literal punctuation");
        {
            soci::transaction tx(sql);
            sql << "INSERT INTO synthetic_runtime.transactions(id,value) VALUES (2,:value)", soci::use(value);
        }
        int count = 0;
        sql << "SELECT count(*) FROM synthetic_runtime.transactions WHERE id=2", soci::into(count);
        Check(count == 0, "transaction destructor rolls back uncommitted writes");
        bool rejected = false;
        try
        {
            soci::transaction tx(sql);
            sql << "INSERT INTO synthetic_runtime.transactions(id,value) VALUES (3,'synthetic')";
            sql << "INSERT INTO synthetic_runtime.transactions(id,value) VALUES (1,'duplicate')";
            tx.commit();
        }
        catch (const soci::soci_error&) { rejected = true; }
        sql << "SELECT count(*) FROM synthetic_runtime.transactions WHERE id=3", soci::into(count);
        Check(rejected && count == 0, "constraint failure rolls back entire transaction and session is reusable");
        sql << "UPDATE synthetic_runtime.counters SET value=0 WHERE id=1";

        std::ostringstream captured;
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(captured);
        auto logger = std::make_shared<spdlog::logger>("redaction_test", sink);
        logger->set_level(spdlog::level::debug);
        auto previous = spdlog::default_logger();
        spdlog::set_default_logger(logger);
        auto result = fourstory::db::orm::SpCall("SyntheticUnsupportedOperation")
            .In("wire_secret", "SyntheticWireSecret").Execute(sql);
        spdlog::set_default_logger(previous);
        Check(!result.Ok() && captured.str().find("SyntheticWireSecret") == std::string::npos,
              "unsupported T-SQL fails without submitting or logging secret literals");
    }
    std::atomic<int> worker_failures{0};
    auto increment = [&] {
        try
        {
            for (int i = 0; i < 20; ++i)
            {
                auto lease = pool.Acquire();
                soci::transaction tx(*lease);
                *lease << "UPDATE synthetic_runtime.counters SET value=value+1 WHERE id=1";
                tx.commit();
            }
        }
        catch (...) { ++worker_failures; }
    };
    std::thread first(increment), second(increment);
    first.join(); second.join();
    auto lease = pool.Acquire();
    int count = 0;
    *lease << "SELECT value FROM synthetic_runtime.counters WHERE id=1", soci::into(count);
    Check(worker_failures == 0 && count == 40, "concurrent transactional updates do not lose writes");
    bool denied = false;
    try { *lease << "SELECT 1 FROM legacy_game.\"TMONSTERCHART\""; }
    catch (const soci::soci_error&) { denied = true; }
    Check(denied, "mutable test role has no historical snapshot access");
    const char* untrusted = std::getenv("FOURSTORY_TEST_PG_UNTRUSTED_CONNINFO");
    bool bad_ca_rejected = false;
    if (untrusted)
    {
        try { SessionPool rejected(Backend::PostgreSQL, untrusted, 1); }
        catch (const fourstory::db::ConnectionError&) { bad_ca_rejected = true; }
    }
    Check(bad_ca_rejected, "untrusted PostgreSQL TLS certificate is rejected");

    SessionPool disposable(Backend::PostgreSQL, options, 1, 100ms);
    bool disconnect_reported = false;
    {
        auto disconnected = disposable.Acquire();
        try { *disconnected << "SELECT pg_terminate_backend(pg_backend_pid())"; }
        catch (const soci::soci_error&) { disconnect_reported = true; }
    }
    Check(disconnect_reported, "database disconnect is surfaced without retrying an operation");
    SessionPool reconnected(Backend::PostgreSQL, options, 1, 100ms);
    auto fresh = reconnected.Acquire();
    *fresh << "SELECT value FROM synthetic_runtime.counters WHERE id=1", soci::into(count);
    Check(count == 40, "fresh connection preserves committed state after a disconnect");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--options-only") ConnectionOptions();
        else
        {
            const char* options = std::getenv("FOURSTORY_TEST_PG_CONNINFO");
            if (!options || !*options)
            {
                std::puts("SKIP: isolated native PostgreSQL fixture not configured");
                return 77;
            }
            Integration(options);
        }
    }
    catch (const std::exception&)
    {
        std::puts("FAIL: native PostgreSQL test failed (backend detail suppressed)");
        return 1;
    }
    return failures ? 1 : 0;
}
