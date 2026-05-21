#pragma once

// SOCI-backed inventory service. Reads TINVENTABLE rows for one
// char id. Schema validated at boot by
// tmapsvr::db::ValidateInventorySchema.

#include "inventory_service.h"

#include <cstdint>
#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociInventoryService final : public IInventoryService
{
public:
    explicit SociInventoryService(fourstory::db::SessionPool& pool);

    std::vector<InventoryRow>
        LoadInventory(std::uint32_t char_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
