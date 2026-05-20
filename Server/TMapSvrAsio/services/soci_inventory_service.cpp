// SociInventoryService — loads ItemInstance rows from TITEMTABLE.
//
// SELECT column order follows DBAccess.h::CTBLItem (35 columns).
// All BYTE/WORD/DWORD columns are bound as `int` (SOCI signed-int
// compatibility), then narrowed to the target type. __int64 (dlID)
// is bound as `long long`. TIMESTAMP_STRUCT (dEndTime) is bound as
// `std::tm` and converted to a Unix timestamp (int64_t seconds).

#include "services/soci_inventory_service.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <cstdint>

namespace tmapsvr {

namespace {

// Convert std::tm to Unix seconds. std::mktime interprets in local
// time — acceptable for game timestamps (precision, not epoch anchor).
std::int64_t TmToUnix(const std::tm& t)
{
    std::tm copy = t;
    return static_cast<std::int64_t>(std::mktime(&copy));
}

} // anonymous namespace

SociInventoryService::SociInventoryService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{}

void SociInventoryService::LoadCharItems(std::uint32_t char_id,
                                         std::vector<ItemInstance> items)
{
    m_cache.LoadCharItems(char_id, std::move(items));
}

void SociInventoryService::LoadCharItemsFromDb(std::uint32_t char_id)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    // Bind variables — SOCI requires int (not uint8/uint16) for most backends
    long long dl_id     = 0;
    int bitem_id        = 0;
    int witem_id        = 0;
    int blevel          = 0;
    int bcount          = 0;
    int bglevel         = 0;
    int dura_max        = 0;
    int dura_cur        = 0;
    int refine_cur      = 0;
    std::tm  end_time{};
    int grade_effect    = 0;
    int bm[6]           = {};
    int wv[6]           = {};
    int ext[6]          = {};
    int bgem            = 0;
    int wmogg           = 0;

    const int owner_id   = static_cast<int>(char_id);
    const int owner_type = 1;  // OT_PC

    std::vector<ItemInstance> loaded;

    try
    {
        // CTBLItem SELECT order from DBAccess.h (simplified — skipping
        // bStorageType/bStorageID/bOwnerType/bOwnerID as filters only).
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dlID\","
            "  \"bItemID\", \"wItemID\", \"bLevel\", \"bCount\","
            "  \"bGLevel\", \"dwDuraMax\", \"dwDuraCur\", \"bRefineCur\","
            "  \"dEndTime\", \"bGradeEffect\","
            "  \"bMagic1\", \"bMagic2\", \"bMagic3\","
            "  \"bMagic4\", \"bMagic5\", \"bMagic6\","
            "  \"wValue1\", \"wValue2\", \"wValue3\","
            "  \"wValue4\", \"wValue5\", \"wValue6\","
            "  \"dwTime1\","  // = dwExtValue[0] = ELD
            "  \"bGem\", \"wMoggItemID\""
            " FROM \"TITEMTABLE\""
            " WHERE \"dwOwnerID\" = :oid AND \"bOwnerType\" = :ot"
            "   AND \"bStorageType\" <> 2",
            soci::use(owner_id, "oid"),
            soci::use(owner_type, "ot"));

        for (const auto& row : rs)
        {
            ItemInstance item{};
            item.dl_id      = static_cast<std::uint64_t>(row.get<long long>(0));
            item.inven_id   = static_cast<std::uint8_t> (row.get<int>(1));
            item.item_id    = static_cast<std::uint16_t>(row.get<int>(2));
            item.level      = static_cast<std::uint8_t> (row.get<int>(3));
            item.count      = static_cast<std::uint8_t> (row.get<int>(4));
            item.g_level    = static_cast<std::uint8_t> (row.get<int>(5));
            item.dura_max   = static_cast<std::uint32_t>(row.get<int>(6));
            item.dura_cur   = static_cast<std::uint32_t>(row.get<int>(7));
            item.refine_cur = static_cast<std::uint8_t> (row.get<int>(8));

            // TIMESTAMP_STRUCT → std::tm → Unix seconds
            try
            {
                const auto& tm_val = row.get<std::tm>(9);
                item.end_time = TmToUnix(tm_val);
            }
            catch (...) { item.end_time = 0; }

            item.g_effect = static_cast<std::uint8_t>(row.get<int>(10));

            // Magic attributes (up to 6 pairs)
            for (int i = 0; i < 6; ++i)
            {
                const auto magic_id  = static_cast<std::uint8_t>(row.get<int>(11 + i));
                const auto magic_val = static_cast<std::uint16_t>(row.get<int>(17 + i));
                if (magic_id != 0)
                    item.magic.push_back({ magic_id, magic_val });
            }

            // ELD = dwExtValue[0] = dwTime1 (column 23)
            item.eld = static_cast<std::uint8_t>(row.get<int>(23));

            item.gem    = static_cast<std::uint8_t> (row.get<int>(24));
            // wMoggItemID col 25 — not stored in ItemInstance F5 stub

            // Inventory type from item_id range (stub — real classification
            // needs TITEMCHART.bType lookup, F5b):
            item.inven_type = InvenType::Main;  // assume main inventory

            loaded.push_back(std::move(item));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("soci_inventory: TITEMTABLE load char={} failed: {}",
            char_id, ex.what());
    }

    spdlog::debug("soci_inventory: loaded {} items for char_id={}",
        loaded.size(), char_id);
    m_cache.LoadCharItems(char_id, std::move(loaded));
}

void SociInventoryService::UnloadCharItems(std::uint32_t char_id)
{
    m_cache.UnloadCharItems(char_id);
}

std::vector<const ItemInstance*>
SociInventoryService::GetItems(std::uint32_t char_id,
                                std::uint8_t  inven_type) const
{
    return m_cache.GetItems(char_id, inven_type);
}

const ItemInstance*
SociInventoryService::FindItem(std::uint32_t char_id,
                                std::uint8_t  inven_type,
                                std::uint8_t  inven_id) const
{
    return m_cache.FindItem(char_id, inven_type, inven_id);
}

bool SociInventoryService::MoveItem(std::uint32_t char_id,
                                     std::uint8_t  src_type,
                                     std::uint8_t  src_slot,
                                     std::uint8_t  dst_type,
                                     std::uint8_t  dst_slot,
                                     std::uint8_t  count)
{
    // Write-back to DB is PENDING F5 Part 3
    return m_cache.MoveItem(char_id, src_type, src_slot, dst_type, dst_slot, count);
}

std::optional<ItemInstance>
SociInventoryService::RemoveItem(std::uint32_t char_id,
                                  std::uint8_t  inven_type,
                                  std::uint8_t  inven_id)
{
    // Write-back PENDING F5 Part 3
    return m_cache.RemoveItem(char_id, inven_type, inven_id);
}

bool SociInventoryService::AddItem(std::uint32_t char_id, ItemInstance item)
{
    // Write-back PENDING F5 Part 3
    return m_cache.AddItem(char_id, std::move(item));
}

} // namespace tmapsvr
