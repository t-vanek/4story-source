#include "soci_inventory_service.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "inventory_slots.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociInventoryService::SociInventoryService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::vector<InventoryRow>
SociInventoryService::LoadInventory(std::uint32_t char_id)
{
    std::vector<InventoryRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        // Cursored fetch — TINVENTABLE rows for this char. Receivers
        // are int32 / int64; we narrow on copy. dEndTime in legacy
        // schemas is sometimes DATETIME (string) and sometimes BIGINT
        // (raw tick); SOCI BIGINT bind covers both via the driver's
        // implicit conversion.
        std::int32_t row_inven_id = 0, row_item_id = 0, row_eld = 0;
        std::int64_t row_end_time = 0;

        soci::statement st = (sql.prepare << queries::InventoryByCharId,
            soci::use(static_cast<std::int32_t>(char_id), "cid"),
            soci::into(row_inven_id),
            soci::into(row_item_id),
            soci::into(row_end_time),
            soci::into(row_eld));
        st.execute(false);

        while (st.fetch())
        {
            InventoryRow r;
            r.bInvenID = db::Narrow8 (row_inven_id);
            r.wItemID  = db::Narrow16(row_item_id);
            r.dEndTime = row_end_time;
            r.bELD     = db::Narrow8 (row_eld);
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_inventory_service: LoadInventory({}) threw: {} — "
                      "returning empty (legacy m_bLoadCharError path)",
            char_id, ex.what());
        out.clear();
    }
    return out;
}

std::optional<std::uint8_t>
SociInventoryService::AddItem(std::uint32_t char_id, const ItemInstance& it)
{
    // Allocate the lowest free bag slot over the char's current rows, then
    // persist (dEndTime NULL = permanent). A re-read of the rows is the
    // simplest correct slot source — loot/pickup is infrequent, so the
    // extra SELECT is cheap and avoids a stale in-memory cache.
    const auto slot = FindBlankSlot(LoadInventory(char_id));
    if (!slot)
        return std::nullopt;   // bag full → caller replies MIT_FULLINVEN

    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;
        sql << queries::InsertInventoryItem,
            soci::use(static_cast<std::int32_t>(char_id), "cid"),
            soci::use(static_cast<std::int32_t>(*slot),   "slot"),
            soci::use(static_cast<std::int32_t>(it.wItemID), "item"),
            soci::use(static_cast<std::int32_t>(it.bELD),    "eld");
    }
    catch (const std::exception& ex)
    {
        // PK (dwCharID, bInvenID) collision or write failure → treat as
        // "couldn't place" so the corpse keeps the item (no silent loss).
        spdlog::error("soci_inventory_service: AddItem(char={}, item={}, "
                      "slot={}) threw: {}",
            char_id, it.wItemID, static_cast<int>(*slot), ex.what());
        return std::nullopt;
    }
    return slot;
}

} // namespace tmapsvr
