#pragma once

// CashItemSaleRegistry — in-memory store of "active" cash-shop item
// sale events. Mirrors legacy `m_mapTCashItemSale` (TWorldSvr
// SSHandler.cpp:342) keyed by the 32-bit campaign index. Each entry
// carries the variable-length list of (item_id, sale_value) pairs
// the client renders in the cash-shop UI.
//
// Wire shape matches MW_CASHITEMSALE_REQ (SSSender.cpp:3304):
//   DWORD dw_index, WORD value, WORD count, N x (WORD id, BYTE sale)
//
// The same payload is replayed to every joining map peer (W6-33's
// extension to OnRelaysvrReq mirrors W6-32 for events).

#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

struct TCashItemSale
{
    std::uint16_t id         = 0;
    std::uint8_t  sale_value = 0;
};

struct TCashItemSaleEvent
{
    std::uint32_t              dw_index = 0;
    std::uint16_t              value    = 0;  // 0 = deactivated
    std::vector<TCashItemSale> items;
};

class CashItemSaleRegistry
{
public:
    CashItemSaleRegistry() = default;
    CashItemSaleRegistry(const CashItemSaleRegistry&) = delete;
    CashItemSaleRegistry& operator=(const CashItemSaleRegistry&) = delete;

    // Insert or replace by `event.dw_index`.
    void Set(TCashItemSaleEvent event);

    // Deactivate an existing entry: clear value and zero every
    // item.sale_value, but keep the row (legacy parity, SSHandler.cpp:
    // 372-385 — the entry stays so the next replay still shows the
    // items with sale_value=0). Returns the deactivated entry by
    // value so the caller can fan it out to peers without holding
    // the registry lock. nullopt if the index didn't exist (legacy
    // SSHandler.cpp:393-397 logs an error and skips the broadcast).
    bool Deactivate(std::uint32_t dw_index, TCashItemSaleEvent& out);

    // Snapshot every entry (legacy SSHandler.cpp:666-668 replay).
    // Order is unordered_map iteration order.
    std::vector<TCashItemSaleEvent> Snapshot() const;

    // Drop the entry under `dw_index`. Idempotent. Tests use this.
    bool Erase(std::uint32_t dw_index);

    std::size_t Size() const;

private:
    mutable std::shared_mutex                                m_lock;
    std::unordered_map<std::uint32_t, TCashItemSaleEvent>    m_events;
};

} // namespace tworldsvr
