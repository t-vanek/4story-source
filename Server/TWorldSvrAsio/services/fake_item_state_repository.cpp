#include "services/fake_item_state_repository.h"

namespace tworldsvr {

void FakeItemStateRepository::FailOn(std::uint16_t item_id)
{
    std::lock_guard g(m_mtx);
    m_fail_ids.insert(item_id);
}

bool FakeItemStateRepository::ChangeState(std::uint16_t item_id,
                                          std::uint8_t  init_state)
{
    std::lock_guard g(m_mtx);
    m_calls.push_back({item_id, init_state});
    return m_fail_ids.count(item_id) == 0;
}

void FakeItemStateRepository::AddRow(std::uint16_t item_id,
                                     std::uint8_t init_state,
                                     std::string name)
{
    std::lock_guard g(m_mtx);
    m_rows.push_back({item_id, init_state, std::move(name)});
}

std::vector<ItemFindRow>
FakeItemStateRepository::FindItems(std::uint16_t item_id,
                                   const std::string& name_pattern)
{
    // LIKE emulation: leading/trailing '%' → suffix/prefix/substring.
    auto like = [](const std::string& text, std::string pat) -> bool
    {
        if (pat.empty()) return false;
        const bool lead  = pat.front() == '%';
        const bool trail = pat.size() > 1 && pat.back() == '%';
        if (lead)  pat.erase(pat.begin());
        else if (pat.front() == '%') pat.clear();
        if (trail && !pat.empty()) pat.pop_back();
        if (pat.empty()) return lead;   // "%" or "%%" matches all
        const auto pos = text.find(pat);
        if (pos == std::string::npos) return false;
        if (!lead && pos != 0) return false;
        if (!trail && pos + pat.size() != text.size()) return false;
        return true;
    };
    std::lock_guard g(m_mtx);
    std::vector<ItemFindRow> out;
    for (const auto& r : m_rows)
        if (r.item_id == item_id || like(r.name, name_pattern))
            out.push_back(r);
    return out;
}

std::vector<ItemStateChange> FakeItemStateRepository::Calls() const
{
    std::lock_guard g(m_mtx);
    return m_calls;
}

} // namespace tworldsvr
