#pragma once

#include "patch_metadata_service.h"

#include <utility>

namespace tcontrolsvr {

class FakePatchMetadataService final : public IPatchMetadataService
{
public:
    struct Call
    {
        std::string  kind;
        std::string  path;
        std::string  name;
        std::uint32_t size = 0;
        std::uint32_t beta = 0;
    };

    void UpdatePatch(const PatchUpdateRow& r, std::uint32_t beta) override
    {
        m_calls.push_back({"update_patch", r.path, r.name, r.size, beta});
    }
    void UpdatePrePatch(const PatchUpdateRow& r) override
    {
        m_calls.push_back({"update_pre_patch", r.path, r.name, r.size, 0});
        m_pre.push_back({m_next_beta++, r.path, r.name, r.size});
    }
    void BetaToVersion(std::uint32_t beta) override
    {
        m_calls.push_back({"beta_to_version", "", "", 0, beta});
        for (auto it = m_pre.begin(); it != m_pre.end(); ++it)
            if (it->beta_ver == beta) { m_pre.erase(it); break; }
    }
    void DeletePreVersion(std::uint32_t beta) override
    {
        m_calls.push_back({"delete_pre_version", "", "", 0, beta});
        for (auto it = m_pre.begin(); it != m_pre.end(); ++it)
            if (it->beta_ver == beta) { m_pre.erase(it); break; }
    }
    std::vector<PreVersionRow> ListPreVersions() override { return m_pre; }

    void Seed(PreVersionRow row) { m_pre.push_back(std::move(row)); }
    const std::vector<Call>& Calls() const { return m_calls; }

private:
    std::uint32_t              m_next_beta = 100;
    std::vector<PreVersionRow> m_pre;
    std::vector<Call>          m_calls;
};

} // namespace tcontrolsvr
