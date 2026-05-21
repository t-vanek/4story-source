#pragma once

#include "companion_service.h"

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociCompanionService final : public ICompanionService
{
public:
    explicit SociCompanionService(fourstory::db::SessionPool& pool);

    std::vector<CompanionRow>
        LoadCompanions(std::uint32_t char_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
