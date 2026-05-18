#include "in_memory_char_service.h"

#include <algorithm>
#include <utility>

namespace tloginsvr::services {

namespace {

// Legacy CheckCharName: a-z A-Z 0-9 only. Used by both Create here
// and the eventual SOCI impl. Same validation rules as
// Server/TLoginSvr/CSHandler.cpp:1024.
bool IsValidCharName(const std::string& name)
{
    if (name.size() < 3 || name.size() > 16) return false;
    for (char c : name)
    {
        const bool ok = (c >= 'a' && c <= 'z')
                     || (c >= 'A' && c <= 'Z')
                     || (c >= '0' && c <= '9');
        if (!ok) return false;
    }
    return true;
}

constexpr std::uint8_t kMaxCharsPerUser = 6;  // legacy CHARSLOT_MAX

} // namespace

void InMemoryCharService::AddCharacter(std::int32_t user_id,
                                       std::uint8_t group_id,
                                       CharacterInfo info)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (info.char_id == 0)
    {
        info.char_id = m_next_char_id++;
    }
    else
    {
        m_next_char_id = std::max(m_next_char_id, info.char_id + 1);
    }
    m_name_to_char_id[info.name] = info.char_id;
    m_chars[MakeKey(user_id, group_id)].push_back(std::move(info));
}

std::vector<CharacterInfo>
InMemoryCharService::List(std::int32_t user_id, std::uint8_t group_id)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_chars.find(MakeKey(user_id, group_id)); it != m_chars.end())
    {
        auto copy = it->second;
        std::sort(copy.begin(), copy.end(),
            [](const CharacterInfo& a, const CharacterInfo& b) {
                return a.slot < b.slot;
            });
        return copy;
    }
    return {};
}

CharacterCreateResponse
InMemoryCharService::Create(const CharacterCreateRequest& req)
{
    // Name validation first — same charset / length as legacy.
    if (!IsValidCharName(req.name))
    {
        return CharacterCreateResponse{
            .status = CreateCharResult::OverChar,
        };
    }

    std::lock_guard<std::mutex> lock(m_mtx);

    if (m_name_to_char_id.contains(req.name))
    {
        return CharacterCreateResponse{
            .status = CreateCharResult::DuplicateName,
        };
    }

    auto& slot_list = m_chars[MakeKey(req.user_id, req.group_id)];
    if (slot_list.size() >= kMaxCharsPerUser)
    {
        return CharacterCreateResponse{
            .status = CreateCharResult::OverChar,
        };
    }
    if (std::any_of(slot_list.begin(), slot_list.end(),
            [&](const auto& c) { return c.slot == req.slot; }))
    {
        return CharacterCreateResponse{
            .status = CreateCharResult::InvalidSlot,
        };
    }

    const std::int32_t new_id = m_next_char_id++;
    CharacterInfo info{
        .char_id      = new_id,
        .name         = req.name,
        .start_act    = 0,
        .slot         = req.slot,
        .level        = 1,
        .char_class   = req.char_class,
        .race         = req.race,
        .country      = req.country,
        .sex          = req.sex,
        .hair         = req.hair,
        .face         = req.face,
        .body         = req.body,
        .pants        = req.pants,
        .hand         = req.hand,
        .foot         = req.foot,
        .region       = 0,
        .fame         = 0,
        .fame_color   = 0,
        .helmet_hide  = 0,
    };
    slot_list.push_back(info);
    m_name_to_char_id[req.name] = new_id;

    return CharacterCreateResponse{
        .status = CreateCharResult::Success,
        .char_id = new_id,
        .remaining_slots = static_cast<std::uint8_t>(
            kMaxCharsPerUser - slot_list.size()),
        .starting_level = info.level,
    };
}

DeleteCharResult
InMemoryCharService::Delete(std::int32_t user_id,
                            std::uint8_t group_id,
                            std::int32_t char_id,
                            const std::string& /*password*/)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_chars.find(MakeKey(user_id, group_id));
    if (it == m_chars.end()) return DeleteCharResult::Failed;

    auto& list = it->second;
    auto target = std::find_if(list.begin(), list.end(),
        [char_id](const auto& c) { return c.char_id == char_id; });
    if (target == list.end()) return DeleteCharResult::Failed;

    m_name_to_char_id.erase(target->name);
    list.erase(target);
    return DeleteCharResult::Success;
}

} // namespace tloginsvr::services
