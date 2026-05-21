#pragma once

// SOCI-backed player service. Reads TCHARTABLE for a single character
// id and packages the row as a CharSnapshot. Schema is validated at
// boot by tmapsvr::db::ValidateCharSchema.

#include "player_service.h"

#include <cstdint>
#include <optional>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociPlayerService final : public IPlayerService
{
public:
    explicit SociPlayerService(fourstory::db::SessionPool& pool);

    std::optional<CharSnapshot>
        LoadChar(std::uint32_t char_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
