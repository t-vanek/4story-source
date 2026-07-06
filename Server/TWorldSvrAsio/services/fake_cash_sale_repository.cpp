#include "services/fake_cash_sale_repository.h"

namespace tworldsvr {

void FakeCashSaleRepository::FailOn(std::uint16_t item_id)
{
    std::lock_guard g(m_mtx);
    m_fail_ids.insert(item_id);
}

bool FakeCashSaleRepository::PersistSaleValue(std::uint16_t item_id,
                                              std::uint8_t  sale_value)
{
    std::lock_guard g(m_mtx);
    m_calls.emplace_back(item_id, sale_value);
    return m_fail_ids.count(item_id) == 0;
}

std::vector<std::pair<std::uint16_t, std::uint8_t>>
FakeCashSaleRepository::Calls() const
{
    std::lock_guard g(m_mtx);
    return m_calls;
}

} // namespace tworldsvr
