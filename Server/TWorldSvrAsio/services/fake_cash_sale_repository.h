#pragma once

// FakeCashSaleRepository — in-memory ICashSaleRepository for tests.
// Every PersistSaleValue attempt is recorded in call order; ids
// seeded via FailOn() return false, driving the handler's
// first-failure break.

#include "services/cash_sale_repository.h"

#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tworldsvr {

class FakeCashSaleRepository : public ICashSaleRepository
{
public:
    // Make PersistSaleValue(item_id, …) fail from now on.
    void FailOn(std::uint16_t item_id);

    bool PersistSaleValue(std::uint16_t item_id,
                          std::uint8_t  sale_value) override;

    // Snapshot of every attempted persist, in call order.
    std::vector<std::pair<std::uint16_t, std::uint8_t>> Calls() const;

private:
    mutable std::mutex                  m_mtx;
    std::unordered_set<std::uint16_t>   m_fail_ids;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> m_calls;
};

} // namespace tworldsvr
