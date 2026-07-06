#pragma once

// ICashSaleRepository — write interface for the cash-shop sale
// persistence step (W6-37). One method mirroring the legacy
// `TCashItemSale` stored procedure (Server/TWorldSvr/DBAccess.h:2286):
// persist one item's bSaleValue into the cash-shop chart. The TGAME
// wrapper SP forwards cross-database into the global cash DB
// (`EXEC TGLOBAL_GSP.dbo.TCashItemSale` — UPDATE TCASHSHOPITEMCHART;
// @wID = 0 updates every row), so the modern impl calls the wrapper
// on the world pool exactly like legacy `DEFINE_QUERY(&m_db, …)` did.
//
// The legacy flow persists only after EVERY map server confirmed the
// campaign broadcast (per-peer m_bCashSale barrier in
// OnMW_CASHITEMSALE_ACK, SSHandler.cpp:10559); the handler owns that
// barrier + the per-item first-failure break, the repo stays a
// single-item primitive.

#include <cstdint>

namespace tworldsvr {

class ICashSaleRepository
{
public:
    virtual ~ICashSaleRepository() = default;

    // EXEC TCashItemSale(item_id, sale_value). item_id 0 means
    // "every chart row" (handled inside the SP). Returns false on a
    // DB failure. Called from the MW_CASHITEMSALE_ACK handler via
    // CoOffloadIf so SOCI never blocks the io_context.
    virtual bool PersistSaleValue(std::uint16_t item_id,
                                  std::uint8_t  sale_value) = 0;
};

} // namespace tworldsvr
