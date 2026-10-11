#include "soci_patch_metadata_service.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>
#include <bit>

namespace tcontrolsvr {

SociPatchMetadataService::SociPatchMetadataService(
    fourstory::db::SessionPool& pool, std::shared_ptr<native_control::Owner> owner)
    : m_owner(std::move(owner)), m_pool(pool)
{
}

void SociPatchMetadataService::UpdatePatch(const PatchUpdateRow& row,
                                           std::uint32_t beta_ver)
{
    if(native_control::Enabled(m_pool)) {
        if(m_commit_uncertain)throw std::runtime_error("Patch commit outcome unknown; restart required");
        if(row.path.size()>260||row.name.size()>260||row.path.find('\0')!=std::string::npos||row.name.find('\0')!=std::string::npos)
            throw std::runtime_error("Patch path exceeds original parameter contract");
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=native_control::Begin(sql,m_owner);
        int contract=0;sql << "SELECT contract FROM app_patch.runtime_contract WHERE singleton",soci::into(contract);
        if(!sql.got_data()||contract!=1)throw std::runtime_error("Patch metadata has not been initialized");
        // TUpdateVersion allocates MAX(dwVersion)+1, not dwBetaVer. Serialize
        // allocation and the source path/name upsert; no retry after COMMIT.
        sql << "LOCK TABLE app_patch.\"TVERSION\" IN SHARE ROW EXCLUSIVE MODE";
        int version=0,exists=0;
        sql << "SELECT COALESCE(MAX(\"dwVersion\")+1,1) FROM app_patch.\"TVERSION\"",soci::into(version);
        const auto path=native_control::Hex(row.path),name=native_control::Hex(row.name);
        const int size=std::bit_cast<std::int32_t>(row.size),beta=std::bit_cast<std::int32_t>(beta_ver);
        sql << "SELECT count(*)::integer FROM app_patch.\"TVERSION\" WHERE rtrim(\"szPath\") COLLATE app_global.account_name_ci="
               "rtrim(convert_from(decode(:path,'hex'),'WIN1252')) COLLATE app_global.account_name_ci "
               "AND rtrim(\"szName\") COLLATE app_global.account_name_ci=rtrim(convert_from(decode(:name,'hex'),'WIN1252')) COLLATE app_global.account_name_ci",
            soci::use(path,"path"),soci::use(name,"name"),soci::into(exists);
        if(exists) {
            // Source UPDATE keeps the existing spelling and original bytes.
            sql << "UPDATE app_patch.\"TVERSION\" SET \"dwVersion\"=:version,\"dwSize\"=:size,\"dwBetaVer\"=:beta "
                   "WHERE rtrim(\"szPath\") COLLATE app_global.account_name_ci=rtrim(convert_from(decode(:path,'hex'),'WIN1252')) COLLATE app_global.account_name_ci "
                   "AND rtrim(\"szName\") COLLATE app_global.account_name_ci=rtrim(convert_from(decode(:name,'hex'),'WIN1252')) COLLATE app_global.account_name_ci",
                soci::use(version,"version"),soci::use(size,"size"),soci::use(beta,"beta"),soci::use(path,"path"),soci::use(name,"name");
        } else {
            sql << "INSERT INTO app_patch.\"TVERSION\"(\"dwVersion\",\"szPath\",\"szName\",path_bytes,name_bytes,\"dwSize\",\"dwBetaVer\") "
                   "VALUES(:version,convert_from(decode(:path,'hex'),'WIN1252'),convert_from(decode(:name,'hex'),'WIN1252'),decode(:path,'hex'),decode(:name,'hex'),:size,:beta)",
                soci::use(version,"version"),soci::use(path,"path"),soci::use(name,"name"),soci::use(size,"size"),soci::use(beta,"beta");
        }
        native_control::Commit(*tx,m_commit_uncertain,m_owner);return;
    }
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
    if(native_control::Enabled(m_pool))throw std::runtime_error("TUpdatePreVersion definition is absent from both authoritative backups");
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
    if(native_control::Enabled(m_pool))throw std::runtime_error("TBetaToVersion definition is absent from both authoritative backups");
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
    if(native_control::Enabled(m_pool))throw std::runtime_error("TDeletePreVersion definition is absent from both authoritative backups");
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
    if(native_control::Enabled(m_pool)) {
        auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);
        soci::rowset<soci::row> rows=((*lease).prepare <<
            "SELECT \"dwBetaVer\",encode(path_bytes,'hex'),encode(name_bytes,'hex'),\"dwSize\" FROM app_patch.\"TPREVERSION\" ORDER BY \"dwBetaVer\"");
        for(const auto& r:rows)out.push_back({static_cast<std::uint32_t>(r.get<int>(0)),native_control::Bytes(r.get<std::string>(1)),
            native_control::Bytes(r.get<std::string>(2)),static_cast<std::uint32_t>(r.get<int>(3))});
        tx->commit();return out;
    }
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
