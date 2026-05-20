#pragma once

// SOCI-backed IPatchMetadataService — calls the four CSP* SPs
// from Server/TControlSvr/DBAccess.h and selects from TPREVERSION.

#include "patch_metadata_service.h"
#include "fourstory/db/session_pool.h"

namespace tcontrolsvr {

class SociPatchMetadataService final : public IPatchMetadataService
{
public:
    explicit SociPatchMetadataService(fourstory::db::SessionPool& pool);

    void UpdatePatch(const PatchUpdateRow& row,
                     std::uint32_t beta_ver) override;
    void UpdatePrePatch(const PatchUpdateRow& row) override;
    void BetaToVersion(std::uint32_t beta_ver) override;
    void DeletePreVersion(std::uint32_t beta_ver) override;
    std::vector<PreVersionRow> ListPreVersions() override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tcontrolsvr
