#pragma once

// SociCashSaleRepository — ICashSaleRepository backed by the legacy
// `TCashItemSale` stored procedure on the world (TGAME) pool. We call
// the SP rather than inlining the UPDATE because the wrapper hops
// databases (TGAME → TGLOBAL_GSP.dbo.TCashItemSale → UPDATE
// TCASHSHOPITEMCHART) and the world server doesn't own a pool into
// the global cash DB — same single-connection shape as legacy
// `DEFINE_QUERY(&m_db, CSPCashItemSale)`.

#include "services/cash_sale_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociCashSaleRepository : public ICashSaleRepository
{
public:
    explicit SociCashSaleRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    bool PersistSaleValue(std::uint16_t item_id,
                          std::uint8_t  sale_value) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
