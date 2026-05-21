#include "soci_inventory_service.h"

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

        soci::statement st = (sql.prepare <<
            "SELECT bInvenID, wItemID, dEndTime, bELD "
            "FROM TINVENTABLE WHERE dwCharID = :cid",
            soci::use(static_cast<std::int32_t>(char_id), "cid"),
            soci::into(row_inven_id),
            soci::into(row_item_id),
            soci::into(row_end_time),
            soci::into(row_eld));
        st.execute(false);

        while (st.fetch())
        {
            InventoryRow r;
            r.bInvenID = static_cast<std::uint8_t> (row_inven_id);
            r.wItemID  = static_cast<std::uint16_t>(row_item_id);
            r.dEndTime = row_end_time;
            r.bELD     = static_cast<std::uint8_t> (row_eld);
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

} // namespace tmapsvr
