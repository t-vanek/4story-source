#pragma once

// IPatchMetadataService — TVERSION + TPREVERSION CRUD via the
// legacy SPs:
//
//   { ? = CALL TUpdateVersion(?, ?, ?, ?) }
//   { ? = CALL TUpdatePreVersion(?, ?, ?) }
//   { ? = CALL TBetaToVersion(?) }
//   { ? = CALL TDeletePreVersion(?) }
//
// Plus a plain SELECT against TPREVERSION for the operator's
// table view.

#include <cstdint>
#include <string>
#include <vector>

namespace tcontrolsvr {

struct PreVersionRow
{
    std::uint32_t  beta_ver = 0;
    std::string    path;
    std::string    name;
    std::uint32_t  size = 0;
};

struct PatchUpdateRow
{
    std::string    path;
    std::string    name;
    std::uint32_t  size = 0;
};

class IPatchMetadataService
{
public:
    virtual ~IPatchMetadataService() = default;

    // CSPUpdatePatch — promote a file into TVERSION. F5 calls it
    // with beta_ver=0 (release promotion) for CT_UPDATEPATCH_REQ.
    virtual void UpdatePatch(const PatchUpdateRow& row,
                             std::uint32_t beta_ver) = 0;

    // CSPUpdatePrePatch — insert into TPREVERSION.
    virtual void UpdatePrePatch(const PatchUpdateRow& row) = 0;

    // CSPBetaToVer — promote one beta row into TVERSION.
    virtual void BetaToVersion(std::uint32_t beta_ver) = 0;

    // CSPDeletePreVersion — drop a beta entry.
    virtual void DeletePreVersion(std::uint32_t beta_ver) = 0;

    // SELECT from TPREVERSION for CT_PREVERSIONTABLE_REQ /
    // CT_PREVERSIONUPDATE_REQ tail reply.
    virtual std::vector<PreVersionRow> ListPreVersions() = 0;
};

} // namespace tcontrolsvr
