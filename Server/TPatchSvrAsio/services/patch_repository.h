#pragma once

// PatchRepository — SOCI-backed reads of TVERSION / TPREVERSION /
// TINTERFACECHART, plus min-beta SP. Mirrors the four DEF_QUERY
// templates from legacy Server/TPatchSvr/DBAccess.h.

#include "fourstory/db/session_pool.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tpatchsvr {

struct PatchFile
{
    std::uint32_t version    = 0;
    std::uint32_t beta_ver   = 0;
    std::string   path;
    std::string   name;
    std::uint32_t size       = 0;
};

class PatchRepository
{
public:
    explicit PatchRepository(fourstory::db::SessionPool& pool);

    // CTBLVersion equivalent — patches >= `from_version`.
    std::vector<PatchFile> ListPatchesSince(std::uint32_t from_version);

    // CTBLPreVersion equivalent — pre-release patches >= beta_version.
    std::vector<PatchFile> ListPrePatchesSince(std::uint32_t beta_version);

    // CTBLInterface equivalent — UI/interface patch files for a
    // particular option set. Optional table: returns empty if the
    // schema doesn't have TINTERFACECHART deployed.
    std::vector<PatchFile> ListInterfaceFiles(std::uint8_t option);

    // CSPMinBetaVer (legacy SP) → TMinBetaVer in the deployed DB.
    // Returns the minimum beta version a client must run; 0 = no
    // constraint / SP missing.
    std::uint32_t MinBetaVersion();

    // CSPPreComplete equivalent — mark a beta version as complete.
    // No-op return; logs DB errors.
    void MarkPreVersionComplete(std::uint32_t beta_version);

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tpatchsvr
