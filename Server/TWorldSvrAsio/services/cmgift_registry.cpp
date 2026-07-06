#include "cmgift_registry.h"

namespace tworldsvr {

void CmGiftRegistry::LoadFrom(const std::vector<CmGift>& rows)
{
    std::lock_guard g(m_mtx);
    m_gifts.clear();
    for (const auto& r : rows)
        m_gifts.emplace(r.gift_id, r);
}

std::optional<CmGift> CmGiftRegistry::Get(std::uint16_t gift_id) const
{
    std::lock_guard g(m_mtx);
    auto it = m_gifts.find(gift_id);
    if (it == m_gifts.end())
        return std::nullopt;
    return it->second;
}

void CmGiftRegistry::Add(CmGift gift)
{
    std::lock_guard g(m_mtx);
    const std::uint16_t id = gift.gift_id;
    m_gifts.emplace(id, std::move(gift));   // insert — keep existing
}

void CmGiftRegistry::Update(CmGift gift)
{
    std::lock_guard g(m_mtx);
    auto it = m_gifts.find(gift.gift_id);
    if (it != m_gifts.end())
        it->second = std::move(gift);
}

bool CmGiftRegistry::Erase(std::uint16_t gift_id)
{
    std::lock_guard g(m_mtx);
    return m_gifts.erase(gift_id) != 0;
}

std::vector<CmGift> CmGiftRegistry::Snapshot() const
{
    std::lock_guard g(m_mtx);
    std::vector<CmGift> out;
    out.reserve(m_gifts.size());
    for (const auto& [id, gift] : m_gifts)
        out.push_back(gift);
    return out;
}

std::size_t CmGiftRegistry::Size() const
{
    std::lock_guard g(m_mtx);
    return m_gifts.size();
}

} // namespace tworldsvr
