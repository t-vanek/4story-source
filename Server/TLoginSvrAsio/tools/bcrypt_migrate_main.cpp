// tloginsvr_bcrypt_migrate — one-shot offline tool.
//
// Walks TACCOUNT_PW and rewrites every non-bcrypt szPasswd as a fresh
// BCrypt hash *over the value that was already stored*. The shipped
// client SHA1-hashes the user's password before sending it on the
// wire, so legacy deploys store the SHA1-hex string; hashing that
// SHA1-hex with BCrypt preserves login compatibility because the
// online auth path calls bcrypt_checkpw(client_sha1_hex, stored_hash).
//
// Usage:
//   tloginsvr_bcrypt_migrate --conn "<ODBC connection string>"
//   tloginsvr_bcrypt_migrate --conn "<...>" --apply
//   tloginsvr_bcrypt_migrate --conn "<...>" --apply --cost 11
//
// Without --apply the tool runs dry: it lists every row that would be
// migrated and prints a summary, but does not UPDATE. With --apply
// each non-bcrypt row is rehashed in its own statement (committed
// per-row so a network blip mid-run leaves the DB in a partially-
// migrated state that is still safe — already-bcrypt rows are
// skipped on the next pass).
//
// Connection string can also be supplied via the
// TLOGINSVR_BCRYPT_MIGRATE_CONN environment variable so operators
// don't have to put a password on the command line.

#include "../services/bcrypt_util.h"

#include <soci/soci.h>
#include <soci/odbc/soci-odbc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace {

struct Args
{
    std::string conn;
    bool        apply = false;
    int         cost  = 10;
    int         batch = 100;   // rows per transaction; 0 = one txn for the whole run
};

void PrintUsage()
{
    std::fputs(
        "tloginsvr_bcrypt_migrate — rehash legacy TACCOUNT_PW rows to bcrypt\n"
        "\n"
        "Usage:\n"
        "  tloginsvr_bcrypt_migrate --conn \"<ODBC connection string>\" [--apply] [--cost N]\n"
        "\n"
        "Options:\n"
        "  --conn <s>   ODBC connection string (or TLOGINSVR_BCRYPT_MIGRATE_CONN env)\n"
        "  --apply      actually UPDATE rows (default is dry-run)\n"
        "  --cost N     bcrypt work factor, 4..15 (default 10)\n"
        "  --batch N    rows committed per transaction, 1..10000 (default 100;\n"
        "               0 = single transaction for the whole run — fastest but\n"
        "               re-running from scratch on crash)\n"
        "  --help       print this help\n",
        stderr);
}

bool ParseArgs(int argc, char** argv, Args& out)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h")
        {
            PrintUsage();
            std::exit(0);
        }
        else if (a == "--apply")
        {
            out.apply = true;
        }
        else if (a == "--conn")
        {
            if (i + 1 >= argc) { std::fputs("--conn needs a value\n", stderr); return false; }
            out.conn = argv[++i];
        }
        else if (a == "--cost")
        {
            if (i + 1 >= argc) { std::fputs("--cost needs a value\n", stderr); return false; }
            out.cost = std::atoi(argv[++i]);
            if (out.cost < 4 || out.cost > 15)
            {
                std::fputs("--cost must be in [4, 15]\n", stderr);
                return false;
            }
        }
        else if (a == "--batch")
        {
            if (i + 1 >= argc) { std::fputs("--batch needs a value\n", stderr); return false; }
            out.batch = std::atoi(argv[++i]);
            if (out.batch < 0 || out.batch > 10000)
            {
                std::fputs("--batch must be in [0, 10000]\n", stderr);
                return false;
            }
        }
        else
        {
            std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
            return false;
        }
    }
    if (out.conn.empty())
    {
        const char* env = std::getenv("TLOGINSVR_BCRYPT_MIGRATE_CONN");
        if (env && env[0] != '\0') out.conn = env;
    }
    if (out.conn.empty())
    {
        std::fputs("missing --conn (or TLOGINSVR_BCRYPT_MIGRATE_CONN)\n", stderr);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    Args args;
    if (!ParseArgs(argc, argv, args))
    {
        PrintUsage();
        return 2;
    }

    std::printf("tloginsvr_bcrypt_migrate\n");
    std::printf("  mode  = %s\n", args.apply ? "APPLY (rows will be rewritten)" : "dry-run");
    std::printf("  cost  = %d\n", args.cost);
    std::printf("  batch = %d\n", args.batch);

    soci::session sql;
    try
    {
        sql.open(soci::odbc, args.conn);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "failed to connect: %s\n", ex.what());
        return 1;
    }

    // Stream the rowset into memory first so the UPDATE loop doesn't
    // collide with an open cursor on the same connection (some ODBC
    // drivers reject that with SQLSTATE 24000).
    struct Row { int uid; std::string passwd; soci::indicator pw_ind; };
    std::vector<Row> rows;
    try
    {
        soci::rowset<soci::row> rs =
            (sql.prepare << "SELECT \"dwUserID\", \"szPasswd\" FROM \"TACCOUNT_PW\"");
        for (const auto& r : rs)
        {
            Row row{};
            row.uid    = r.get<int>(0);
            row.pw_ind = r.get_indicator(1);
            if (row.pw_ind != soci::i_null) row.passwd = r.get<std::string>(1);
            rows.push_back(std::move(row));
        }
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "SELECT failed: %s\n", ex.what());
        return 1;
    }

    std::size_t total = rows.size();
    std::size_t already_bcrypt = 0;
    std::size_t null_rows = 0;
    std::size_t empty_rows = 0;
    std::size_t to_migrate = 0;
    std::size_t migrated = 0;
    std::size_t failed = 0;

    using namespace tloginsvr::services;

    // Batched-transaction loop. We commit every `args.batch` UPDATEs so
    // a crash mid-run loses at most one batch (re-runnable safely — the
    // tool skips already-bcrypt rows on the next pass). batch=0 = wrap
    // the entire run in one transaction (fastest, but everything rolls
    // back on any failure).
    std::unique_ptr<soci::transaction> txn;
    std::size_t batch_pending = 0;
    auto open_txn = [&]() {
        if (!args.apply) return;
        if (!txn) txn = std::make_unique<soci::transaction>(sql);
    };
    auto commit_txn = [&](const char* reason) {
        if (!args.apply || !txn) return;
        try
        {
            txn->commit();
            std::printf("  [commit] %s — %zu update(s) flushed\n", reason, batch_pending);
        }
        catch (const std::exception& ex)
        {
            std::fprintf(stderr, "  [commit] %s — FAILED: %s\n", reason, ex.what());
            // The transaction destructor will roll back. We surface the
            // failure to the caller via the exit code; subsequent
            // updates can still proceed in a fresh transaction.
            ++failed;
        }
        txn.reset();
        batch_pending = 0;
    };

    if (args.apply && args.batch == 0)
        open_txn();   // single-transaction mode: open once, commit at end

    for (const auto& row : rows)
    {
        if (row.pw_ind == soci::i_null)
        {
            ++null_rows;
            continue;
        }
        if (row.passwd.empty())
        {
            ++empty_rows;
            continue;
        }
        if (bcrypt_util::IsBcrypt(row.passwd))
        {
            ++already_bcrypt;
            continue;
        }

        ++to_migrate;

        if (!args.apply)
        {
            std::printf("  [dry-run] would rehash uid=%d (stored length=%zu)\n",
                        row.uid, row.passwd.size());
            continue;
        }

        const auto fresh = bcrypt_util::MakeBcryptHash(row.passwd, args.cost);
        if (fresh.empty())
        {
            ++failed;
            std::fprintf(stderr, "  uid=%d — bcrypt_hashpw failed, skipping\n", row.uid);
            continue;
        }

        if (args.batch > 0) open_txn();

        try
        {
            sql << "UPDATE \"TACCOUNT_PW\" SET \"szPasswd\" = :h "
                   "WHERE \"dwUserID\" = :uid",
                soci::use(fresh), soci::use(row.uid);
            ++migrated;
            ++batch_pending;
            std::printf("  uid=%d → bcrypt'd\n", row.uid);
        }
        catch (const std::exception& ex)
        {
            ++failed;
            std::fprintf(stderr, "  uid=%d — UPDATE failed: %s\n", row.uid, ex.what());
        }

        if (args.batch > 0 && batch_pending >= static_cast<std::size_t>(args.batch))
            commit_txn("batch full");
    }

    // Final flush — either the leftover batch, or (for batch=0) the
    // single transaction covering the whole run.
    if (args.apply) commit_txn("final");

    std::printf("\nSummary:\n");
    std::printf("  total rows         : %zu\n", total);
    std::printf("  already bcrypt     : %zu\n", already_bcrypt);
    std::printf("  null szPasswd      : %zu\n", null_rows);
    std::printf("  empty szPasswd     : %zu\n", empty_rows);
    if (args.apply)
    {
        std::printf("  migrated this run  : %zu\n", migrated);
        std::printf("  failed this run    : %zu\n", failed);
        if (failed > 0) return 1;
    }
    else
    {
        std::printf("  to migrate         : %zu (re-run with --apply)\n", to_migrate);
    }
    return 0;
}
