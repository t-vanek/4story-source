#pragma once

#include "skill_service.h"

#include <cstdint>
#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociSkillService final : public ISkillService
{
public:
    explicit SociSkillService(fourstory::db::SessionPool& pool);

    std::vector<SkillRow>
        LoadSkills(std::uint32_t char_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
