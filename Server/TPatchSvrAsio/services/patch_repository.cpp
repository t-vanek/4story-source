#include "patch_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tpatchsvr {

PatchRepository::PatchRepository(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::vector<PatchFile>
PatchRepository::ListPatchesSince(std::uint32_t from_version)
{
    std::vector<PatchFile> out;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Legacy CTBLVersion uses strict greater-than so the client's
        // own current version is NOT echoed back as a "new patch" —
        // mirror that ordering exactly. Off-by-one here means each
        // poll returns one stale row and wastes the file download
        // round-trip on the client.
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwVersion\", \"szPath\", \"szName\", \"dwSize\", "
            "       \"dwBetaVer\" "
            "FROM \"TVERSION\" WHERE \"dwVersion\" > :v "
            "ORDER BY \"dwVersion\"",
            soci::use(static_cast<int>(from_version)));
        for (const auto& r : rs)
        {
            PatchFile p{};
            p.version  = static_cast<std::uint32_t>(r.get<int>(0));
            p.path     = r.get<std::string>(1);
            p.name     = r.get<std::string>(2);
            p.size     = static_cast<std::uint32_t>(r.get<int>(3));
            soci::indicator beta_ind = r.get_indicator(4);
            if (beta_ind != soci::i_null)
                p.beta_ver = static_cast<std::uint32_t>(r.get<int>(4));
            out.push_back(std::move(p));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("patch_repo.ListPatchesSince({}) DB error: {}",
            from_version, ex.what());
    }
    return out;
}

std::vector<PatchFile>
PatchRepository::ListPrePatchesSince(std::uint32_t beta_version)
{
    std::vector<PatchFile> out;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Legacy CTBLPreVersion uses strict greater-than — mirror it.
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwBetaVer\", \"szPath\", \"szName\", \"dwSize\" "
            "FROM \"TPREVERSION\" WHERE \"dwBetaVer\" > :v "
            "ORDER BY \"dwBetaVer\"",
            soci::use(static_cast<int>(beta_version)));
        for (const auto& r : rs)
        {
            PatchFile p{};
            p.beta_ver = static_cast<std::uint32_t>(r.get<int>(0));
            p.path     = r.get<std::string>(1);
            p.name     = r.get<std::string>(2);
            p.size     = static_cast<std::uint32_t>(r.get<int>(3));
            out.push_back(std::move(p));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("patch_repo.ListPrePatchesSince({}) DB error: {}",
            beta_version, ex.what());
    }
    return out;
}

std::vector<PatchFile>
PatchRepository::ListInterfaceFiles(std::uint8_t option)
{
    std::vector<PatchFile> out;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Legacy CTBLInterface queries TUSER_INTERFACE — there is no
        // dwVersion column on the row itself, so the legacy SP grafts
        // on a `(SELECT MAX(dwVersion) FROM TVERSION)` subquery to
        // synthesize a per-row version. TUSER_INTERFACE.dwSize is
        // declared `float` in the deployed schema (not int), so bind
        // it as `double` and narrow on the C++ side. Naming the
        // subquery column `wDV` keeps it stable across both SOCI's
        // positional and column-name binding paths.
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT (SELECT MAX(\"dwVersion\") FROM \"TVERSION\") AS \"wDV\", "
            "       \"szName\", \"dwSize\" "
            "FROM \"TUSER_INTERFACE\" WHERE \"bOption\" = :o",
            soci::use(static_cast<int>(option)));
        for (const auto& r : rs)
        {
            PatchFile p{};
            const auto v_ind = r.get_indicator(0);
            if (v_ind != soci::i_null)
                p.version = static_cast<std::uint32_t>(r.get<int>(0));
            p.name = r.get<std::string>(1);
            p.size = static_cast<std::uint32_t>(r.get<double>(2));
            out.push_back(std::move(p));
        }
    }
    catch (const std::exception& ex)
    {
        // Table is optional in some deploys — keep it quiet.
        spdlog::debug("patch_repo.ListInterfaceFiles({}) skipped: {}",
            static_cast<int>(option), ex.what());
    }
    return out;
}

std::uint32_t PatchRepository::MinBetaVersion()
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Legacy CSPMinBetaVer calls the deployed `TMinBetaVer` SP
        // (operator-configured cutoff, returned via the SP's @dwMinVer
        // OUTPUT parameter). The previous heuristic `MIN(dwBetaVer)`
        // on TPREVERSION returned the lowest existing pre-version,
        // which has nothing to do with the cutoff — fixed in the
        // round-1 audit. The exec wrapper here uses a T-SQL local
        // variable to capture the OUT param and SELECT it back as a
        // single-column rowset SOCI can bind into. Falls back to 0
        // on any error (SP missing, ODBC quirk, etc.) so the wire
        // path stays compatible with deploys that haven't shipped
        // the SP.
        int min_ver = 0;
        sql << "DECLARE @v INT; EXEC \"TMinBetaVer\" @dwMinVer = @v OUTPUT; "
               "SELECT @v",
            soci::into(min_ver);
        return static_cast<std::uint32_t>(min_ver);
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("patch_repo.MinBetaVersion skipped: {}", ex.what());
        return 0;
    }
}

void PatchRepository::MarkPreVersionComplete(std::uint32_t beta_version)
{
    // Legacy SP `TPreCompleteAdd` is *not* deployed in the restored
    // RageZone DB (P-5 in TPATCH_AUDIT). The documented operator
    // procedure is to promote every TPREVERSION row at the given
    // beta into TVERSION and then delete them — we run that sequence
    // inline, wrapped in a single transaction so a crash mid-promote
    // can't leave half-promoted rows behind.
    //
    // dwVersion is assigned by reusing dwBetaVer (matches the legacy
    // operator runbook in TPATCH_AUDIT P-5). Upsert key is
    // (szPath, szName), same shape as the TUpdateVersion SP uses for
    // the single-file path.
    //
    // Backend dispatch: MSSQL gets a single atomic MERGE; PostgreSQL
    // and SQLite fall back to the standard UPDATE-then-INSERT
    // WHERE NOT EXISTS pattern (both wrapped in the same
    // transaction so either way the visible state is all-or-nothing).
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        const int beta = static_cast<int>(beta_version);
        const bool is_mssql =
            (m_pool.GetBackend() == fourstory::db::Backend::Odbc);

        soci::transaction tx(sql);

        if (is_mssql)
        {
            sql <<
                "MERGE INTO \"TVERSION\" AS T "
                "USING (SELECT \"dwBetaVer\", \"szPath\", \"szName\", \"dwSize\" "
                "       FROM \"TPREVERSION\" WHERE \"dwBetaVer\" = :b) AS S "
                "ON  T.\"szPath\" = S.\"szPath\" "
                "AND T.\"szName\" = S.\"szName\" "
                "WHEN MATCHED THEN UPDATE SET "
                "    T.\"dwVersion\"  = S.\"dwBetaVer\", "
                "    T.\"dwSize\"     = S.\"dwSize\", "
                "    T.\"dwBetaVer\"  = S.\"dwBetaVer\" "
                "WHEN NOT MATCHED THEN INSERT "
                "    (\"dwVersion\", \"szPath\", \"szName\", \"dwSize\", \"dwBetaVer\") "
                "    VALUES (S.\"dwBetaVer\", S.\"szPath\", S.\"szName\", "
                "            S.\"dwSize\",    S.\"dwBetaVer\") ;",
                soci::use(beta, "b");
        }
        else
        {
            // UPDATE pass — picks up any rows already in TVERSION at
            // the same (szPath, szName), refreshes their dwVersion /
            // dwSize / dwBetaVer.
            sql <<
                "UPDATE \"TVERSION\" "
                "SET    \"dwVersion\"  = p.\"dwBetaVer\", "
                "       \"dwSize\"     = p.\"dwSize\", "
                "       \"dwBetaVer\"  = p.\"dwBetaVer\" "
                "FROM   \"TPREVERSION\" p "
                "WHERE  \"TVERSION\".\"szPath\" = p.\"szPath\" "
                "AND    \"TVERSION\".\"szName\" = p.\"szName\" "
                "AND    p.\"dwBetaVer\" = :b",
                soci::use(beta, "b");

            // INSERT pass — append the pre-versions that didn't
            // already have a (szPath, szName) row in TVERSION.
            sql <<
                "INSERT INTO \"TVERSION\" "
                "  (\"dwVersion\", \"szPath\", \"szName\", \"dwSize\", \"dwBetaVer\") "
                "SELECT p.\"dwBetaVer\", p.\"szPath\", p.\"szName\", "
                "       p.\"dwSize\",    p.\"dwBetaVer\" "
                "FROM   \"TPREVERSION\" p "
                "WHERE  p.\"dwBetaVer\" = :b "
                "AND    NOT EXISTS ( "
                "    SELECT 1 FROM \"TVERSION\" v "
                "    WHERE v.\"szPath\" = p.\"szPath\" "
                "    AND   v.\"szName\" = p.\"szName\")",
                soci::use(beta, "b");
        }

        // Remove the now-promoted rows so subsequent CT_PREPATCH_REQ
        // calls don't replay them.
        sql << "DELETE FROM \"TPREVERSION\" WHERE \"dwBetaVer\" = :b",
            soci::use(beta, "b");

        tx.commit();

        spdlog::info("patch_repo.MarkPreVersionComplete beta_ver={} "
                     "promoted (backend={})",
            beta_version, is_mssql ? "mssql" : "pg/sqlite");
    }
    catch (const std::exception& ex)
    {
        spdlog::error("patch_repo.MarkPreVersionComplete({}) error: {}",
            beta_version, ex.what());
    }
}

} // namespace tpatchsvr
