#include "services/fake_cmgift_repository.h"

namespace tworldsvr {

void FakeCmGiftRepository::SeedChartRow(CmGift gift)
{
    std::lock_guard g(m_mtx);
    m_chart.push_back(std::move(gift));
}

void FakeCmGiftRepository::SetCanTake(const std::string& name,
                                      std::uint8_t result)
{
    std::lock_guard g(m_mtx);
    m_can_take[name] = result;
}

void FakeCmGiftRepository::SetNextAddId(std::uint16_t id)
{
    std::lock_guard g(m_mtx);
    m_next_add_id = id;
}

std::vector<CmGift> FakeCmGiftRepository::LoadChart()
{
    std::lock_guard g(m_mtx);
    return m_chart;
}

std::uint8_t FakeCmGiftRepository::CanTake(const std::string& name,
                                           std::uint16_t gift_id)
{
    std::lock_guard g(m_mtx);
    m_cantake_calls.emplace_back(name, gift_id);
    auto it = m_can_take.find(name);
    return it == m_can_take.end() ? kCmGiftSuccess : it->second;
}

std::optional<std::uint16_t>
FakeCmGiftRepository::Add(const CmGift& gift)
{
    std::lock_guard g(m_mtx);
    m_add_calls.push_back(gift);
    return m_next_add_id++;
}

bool FakeCmGiftRepository::Set(const CmGift& gift)
{
    std::lock_guard g(m_mtx);
    m_set_calls.push_back(gift);
    return true;
}

bool FakeCmGiftRepository::Del(std::uint16_t gift_id)
{
    std::lock_guard g(m_mtx);
    m_del_calls.push_back(gift_id);
    return true;
}

std::vector<CmGift> FakeCmGiftRepository::AddCalls() const
{ std::lock_guard g(m_mtx); return m_add_calls; }
std::vector<CmGift> FakeCmGiftRepository::SetCalls() const
{ std::lock_guard g(m_mtx); return m_set_calls; }
std::vector<std::uint16_t> FakeCmGiftRepository::DelCalls() const
{ std::lock_guard g(m_mtx); return m_del_calls; }
std::vector<std::pair<std::string, std::uint16_t>>
FakeCmGiftRepository::CanTakeCalls() const
{ std::lock_guard g(m_mtx); return m_cantake_calls; }

} // namespace tworldsvr
