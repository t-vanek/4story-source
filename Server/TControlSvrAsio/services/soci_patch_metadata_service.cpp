#include "soci_patch_metadata_service.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tcontrolsvr {

SociPatchMetadataService::SociPatchMetadataService(
    fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

void SociPatchMetadataService::UpdatePatch(const PatchUpdateRow& row,
                                           std::uint32_t beta_ver)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        int nret = 0;
        int size = static_cast<int>(row.size);
        int beta = static_cast<int>(beta_ver);
        soci::statement st = (sql.prepare <<
            "{ ? = CALL TUpdateVersion(?, ?, ?, ?) }",
            soci::into(nret),
            soci::use(row.path), soci::use(row.name),
            soci::use(size),     soci::use(beta));
        st.execute(true);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_patch_meta.UpdatePatch('{}/{}') failed: {}",
            row.path, row.name, ex.what());
    }
}

void SociPatchMetadataService::UpdatePrePatch(const PatchUpdateRow& row)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        int nret = 0;
        int size = static_cast<int>(row.size);
        soci::statement st = (sql.prepare <<
            "{ ? = CALL TUpdatePreVersion(?, ?, ?) }",
            soci::into(nret),
            soci::use(row.path), soci::use(row.name), soci::use(size));
        st.execute(true);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_patch_meta.UpdatePrePatch('{}/{}') failed: {}",
            row.path, row.name, ex.what());
    }
}

void SociPatchMetadataService::BetaToVersion(std::uint32_t beta_ver)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        int nret = 0;
        int beta = static_cast<int>(beta_ver);
        soci::statement st = (sql.prepare <<
            "{ ? = CALL TBetaToVersion(?) }",
            soci::into(nret), soci::use(beta));
        st.execute(true);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_patch_meta.BetaToVersion({}) failed: {}",
            beta_ver, ex.what());
    }
}

void SociPatchMetadataService::DeletePreVersion(std::uint32_t beta_ver)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        int nret = 0;
        int beta = static_cast<int>(beta_ver);
        soci::statement st = (sql.prepare <<
            "{ ? = CALL TDeletePreVersion(?) }",
            soci::into(nret), soci::use(beta));
        st.execute(true);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_patch_meta.DeletePreVersion({}) failed: {}",
            beta_ver, ex.what());
    }
}

std::vector<PreVersionRow>
SociPatchMetadataService::ListPreVersions()
{
    std::vector<PreVersionRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwBetaVer\", \"szPath\", \"szName\", \"dwSize\" "
            "FROM \"TPREVERSION\" ORDER BY \"dwBetaVer\"");
        for (const auto& r : rs)
        {
            PreVersionRow row{};
            row.beta_ver = static_cast<std::uint32_t>(r.get<int>(0));
            row.path     = r.get<std::string>(1);
            row.name     = r.get<std::string>(2);
            row.size     = static_cast<std::uint32_t>(r.get<int>(3));
            out.push_back(std::move(row));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_patch_meta.ListPreVersions failed: {}",
            ex.what());
    }
    return out;
}

} // namespace tcontrolsvr
