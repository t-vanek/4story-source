#include "services/soci_cash_sale_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>

namespace tworldsvr {

bool SociCashSaleRepository::PersistSaleValue(std::uint16_t item_id,
                                              std::uint8_t  sale_value)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int id  = static_cast<int>(item_id);
        const int val = static_cast<int>(sale_value);
        sql << "EXEC dbo.TCashItemSale :id, :val",
            soci::use(id), soci::use(val);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociCashSaleRepository::PersistSaleValue({},{}) "
                      "failed: {}", item_id, sale_value, ex.what());
        return false;
    }
}

} // namespace tworldsvr
