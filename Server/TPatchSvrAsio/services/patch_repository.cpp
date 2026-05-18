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
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwVersion\", \"szPath\", \"szName\", \"dwSize\", "
            "       \"dwBetaVer\" "
            "FROM \"TVERSION\" WHERE \"dwVersion\" >= :v "
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
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwBetaVer\", \"szPath\", \"szName\", \"dwSize\" "
            "FROM \"TPREVERSION\" WHERE \"dwBetaVer\" >= :v "
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
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwVersion\", \"szName\", \"dwSize\" "
            "FROM \"TINTERFACECHART\" WHERE \"bOption\" = :o "
            "ORDER BY \"dwVersion\"",
            soci::use(static_cast<int>(option)));
        for (const auto& r : rs)
        {
            PatchFile p{};
            p.version = static_cast<std::uint32_t>(r.get<int>(0));
            p.name    = r.get<std::string>(1);
            p.size    = static_cast<std::uint32_t>(r.get<int>(2));
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
        // The deployed SP is `TMinBetaVer` — returns the minimum
        // dwBetaVer the client must have. Call via T-SQL EXEC because
        // SOCI doesn't have a portable stored-proc wrapper.
        int min_ver = 0;
        sql << "SELECT TOP 1 \"dwBetaVer\" FROM \"TPREVERSION\" "
               "ORDER BY \"dwBetaVer\"",
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
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Legacy SP CSPPreComplete promotes a pre-version row into
        // TVERSION (no equivalent SP in the current DB; we just log
        // for now — operators promote manually via SQL until the SP
        // is deployed).
        spdlog::info("patch_repo.MarkPreVersionComplete beta_ver={}",
            beta_version);
        (void)sql;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("patch_repo.MarkPreVersionComplete({}) error: {}",
            beta_version, ex.what());
    }
}

} // namespace tpatchsvr
