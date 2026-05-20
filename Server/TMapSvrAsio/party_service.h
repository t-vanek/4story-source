#pragma once

// IPartyService — in-memory party management.
//
// In the legacy system, party state is owned by TWorldSvr — MapSvr
// forwards party requests via MW_PARTY* messages. For standalone mode
// (no WorldSvr), LocalPartyService provides a self-contained party
// registry.
//
// Party lifecycle:
//   CS_PARTYADD_REQ → leader invites member by name
//   CS_PARTYJOIN_REQ → invited member accepts
//   CS_PARTYLEAVE_REQ → member leaves
//
// Source:
//   CSHandler.cpp:3419 — OnCS_PARTYADD_REQ
//   CSHandler.cpp:3451 — OnCS_PARTYJOIN_REQ

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

struct PartyMemberInfo
{
    std::uint32_t char_id   = 0;
    std::string   name;
    std::uint8_t  level     = 0;
    std::uint32_t hp        = 0;
    std::uint32_t max_hp    = 0;
    std::uint32_t mp        = 0;
    std::uint32_t max_mp    = 0;
    std::uint8_t  race      = 0;
    std::uint8_t  sex       = 0;
    std::uint8_t  face      = 0;
    std::uint8_t  hair      = 0;
    std::uint8_t  char_class= 0;
    std::uint8_t  country   = 0;
};

struct PartyInfo
{
    std::uint16_t                  party_id   = 0;
    std::uint32_t                  chief_id   = 0;  // leader
    std::uint8_t                   obtain_type= 0;  // loot distribution
    std::vector<PartyMemberInfo>   members;
};

// Pending invite — waiting for target to accept
struct PartyInvite
{
    std::uint32_t inviter_id   = 0;
    std::uint16_t party_id     = 0;  // 0 = not yet formed
    std::uint8_t  obtain_type  = 0;
};

class IPartyService
{
public:
    virtual ~IPartyService() = default;

    // Create a new party with leader as first member.
    // Returns the new party_id.
    virtual std::uint16_t CreateParty(const PartyMemberInfo& leader,
                                      std::uint8_t           obtain_type) = 0;

    // Add member to existing party.
    virtual bool AddMember(std::uint16_t          party_id,
                           const PartyMemberInfo& member) = 0;

    // Remove member. If last member, dissolves the party.
    virtual void RemoveMember(std::uint16_t party_id,
                              std::uint32_t char_id) = 0;

    // Get party info (nullptr if not found).
    virtual const PartyInfo* GetParty(std::uint16_t party_id) const = 0;

    // Get party for a specific char_id (nullopt if not in a party).
    virtual std::optional<std::uint16_t>
        GetCharParty(std::uint32_t char_id) const = 0;

    // Store pending invite (inviter → target_name mapping).
    virtual void StorePendingInvite(const std::string& target_name,
                                    PartyInvite        invite) = 0;

    // Take pending invite for target_name (removes it after retrieval).
    virtual std::optional<PartyInvite>
        TakePendingInvite(const std::string& target_name) = 0;
};

class LocalPartyService : public IPartyService
{
public:
    std::uint16_t CreateParty(const PartyMemberInfo& leader,
                              std::uint8_t           obtain_type) override
    {
        const std::uint16_t id = ++m_next_id;
        PartyInfo party{};
        party.party_id    = id;
        party.chief_id    = leader.char_id;
        party.obtain_type = obtain_type;
        party.members.push_back(leader);
        m_parties[id] = std::move(party);
        m_char_party[leader.char_id] = id;
        return id;
    }

    bool AddMember(std::uint16_t          party_id,
                   const PartyMemberInfo& member) override
    {
        auto it = m_parties.find(party_id);
        if (it == m_parties.end()) return false;
        it->second.members.push_back(member);
        m_char_party[member.char_id] = party_id;
        return true;
    }

    void RemoveMember(std::uint16_t party_id,
                      std::uint32_t char_id) override
    {
        auto pit = m_parties.find(party_id);
        if (pit == m_parties.end()) return;
        auto& members = pit->second.members;
        members.erase(
            std::remove_if(members.begin(), members.end(),
                [char_id](const PartyMemberInfo& m) {
                    return m.char_id == char_id;
                }),
            members.end());
        m_char_party.erase(char_id);
        if (members.empty())
            m_parties.erase(pit);
    }

    const PartyInfo* GetParty(std::uint16_t party_id) const override
    {
        auto it = m_parties.find(party_id);
        return it != m_parties.end() ? &it->second : nullptr;
    }

    std::optional<std::uint16_t>
    GetCharParty(std::uint32_t char_id) const override
    {
        auto it = m_char_party.find(char_id);
        if (it == m_char_party.end()) return std::nullopt;
        return it->second;
    }

    void StorePendingInvite(const std::string& target_name,
                            PartyInvite        invite) override
    {
        m_pending_invites[target_name] = std::move(invite);
    }

    std::optional<PartyInvite>
    TakePendingInvite(const std::string& target_name) override
    {
        auto it = m_pending_invites.find(target_name);
        if (it == m_pending_invites.end()) return std::nullopt;
        auto invite = std::move(it->second);
        m_pending_invites.erase(it);
        return invite;
    }

private:
    std::uint16_t                                         m_next_id = 0;
    std::unordered_map<std::uint16_t, PartyInfo>          m_parties;
    std::unordered_map<std::uint32_t, std::uint16_t>      m_char_party;
    std::unordered_map<std::string,   PartyInvite>        m_pending_invites;
};

} // namespace tmapsvr
