#pragma once

// FakeCmGiftRepository — in-memory ICmGiftRepository for tests.

#include "services/cmgift_repository.h"

#include <mutex>
#include <unordered_map>

namespace tworldsvr {

class FakeCmGiftRepository : public ICmGiftRepository
{
public:
    void SeedChartRow(CmGift gift);
    // CanTake result for a target name (default: CMGIFT_SUCCESS).
    void SetCanTake(const std::string& name, std::uint8_t result);
    void SetNextAddId(std::uint16_t id);

    std::vector<CmGift> LoadChart() override;
    std::uint8_t CanTake(const std::string& target_name,
                         std::uint16_t gift_id) override;
    std::optional<std::uint16_t> Add(const CmGift& gift) override;
    bool Set(const CmGift& gift) override;
    bool Del(std::uint16_t gift_id) override;

    std::vector<CmGift>        AddCalls() const;
    std::vector<CmGift>        SetCalls() const;
    std::vector<std::uint16_t> DelCalls() const;
    std::vector<std::pair<std::string, std::uint16_t>>
        CanTakeCalls() const;

private:
    mutable std::mutex m_mtx;
    std::vector<CmGift> m_chart;
    std::unordered_map<std::string, std::uint8_t> m_can_take;
    std::uint16_t m_next_add_id = 1000;
    std::vector<CmGift>        m_add_calls;
    std::vector<CmGift>        m_set_calls;
    std::vector<std::uint16_t> m_del_calls;
    std::vector<std::pair<std::string, std::uint16_t>> m_cantake_calls;
};

} // namespace tworldsvr
