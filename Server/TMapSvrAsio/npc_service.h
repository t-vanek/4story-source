#pragma once

// INpcService — NPC shop and dialogue data.
//
// NPCs are loaded once at boot from TNPCCHART and TNPCITEMCHART.
// This interface provides per-NPC shop items for:
//   * CS_NPCITEMLIST_ACK — shop item listing
//   * CS_ITEMBUY_ACK    — purchase validation
//
// F6 Part 2 scope: shop listing + item buy. Portal, skill, and
// monster NPCs are PENDING (require TNPCPOTALCHART / TSKILLCHART).
//
// Source:
//   CSHandler.cpp:6192 — OnCS_NPCITEMLIST_REQ
//   CSHandler.cpp:6247 — OnCS_ITEMBUY_REQ
//   CSSender.cpp:2756  — SendCS_NPCITEMLIST_ACK

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

// NPC type constants (legacy TNPC_* enum)
namespace NpcType {
    constexpr std::uint8_t Item    = 1;   // TNPC_ITEM — sells items
    constexpr std::uint8_t Portal  = 2;   // TNPC_PORTAL — teleport destinations
    constexpr std::uint8_t Revival = 3;   // revival point
    constexpr std::uint8_t Skill   = 4;   // TNPC_SKILL_MASTER
    constexpr std::uint8_t PvPoint = 5;   // PvP point exchange
}

// Buy-result codes (legacy ITEMBUY_* enum)
namespace ItemBuyResult {
    constexpr std::uint8_t Success      = 0;
    constexpr std::uint8_t Dealing      = 1;
    constexpr std::uint8_t NotFound     = 2;
    constexpr std::uint8_t NeedMoney    = 3;
    constexpr std::uint8_t CantPush     = 4;  // inventory full
    constexpr std::uint8_t InvalidPos   = 5;
    constexpr std::uint8_t NpcCallError = 6;
}

struct NpcShopItem
{
    std::uint16_t item_id = 0;
    std::uint32_t price   = 0;   // gold price
};

struct NpcData
{
    std::uint16_t npc_id       = 0;
    std::uint8_t  type         = NpcType::Item;
    std::uint8_t  discount_rate = 0;  // 0 = no discount
    std::string   name;
    std::vector<NpcShopItem> shop_items;
};

class INpcService
{
public:
    virtual ~INpcService() = default;

    // Returns nullptr if npc_id is unknown.
    virtual const NpcData* GetNpc(std::uint16_t npc_id) const = 0;

    // Find the NpcShopItem index in npc's shop by item_id.
    // Returns nullopt if NPC doesn't exist or item not in shop.
    virtual std::optional<NpcShopItem>
        FindShopItem(std::uint16_t npc_id, std::uint16_t item_id) const = 0;
};

// ---------------------------------------------------------------------------
// FakeNpcService — in-memory for unit tests + dev smoke runs
// ---------------------------------------------------------------------------

class FakeNpcService : public INpcService
{
public:
    void AddNpc(NpcData npc)
    {
        const auto id = npc.npc_id;
        m_npcs[id] = std::move(npc);
    }

    const NpcData* GetNpc(std::uint16_t npc_id) const override
    {
        auto it = m_npcs.find(npc_id);
        return it != m_npcs.end() ? &it->second : nullptr;
    }

    std::optional<NpcShopItem>
    FindShopItem(std::uint16_t npc_id, std::uint16_t item_id) const override
    {
        const auto* npc = GetNpc(npc_id);
        if (!npc) return std::nullopt;
        for (const auto& s : npc->shop_items)
            if (s.item_id == item_id) return s;
        return std::nullopt;
    }

private:
    std::unordered_map<std::uint16_t, NpcData> m_npcs;
};

// ---------------------------------------------------------------------------
// HardcodedNpcService — minimal stub so dev runs have some NPCs
// ---------------------------------------------------------------------------
//
// Exposes two NPCs:
//   NPC 1 (General Merchant): sells HP potion (item_id=1, 100g)
//                                and MP potion (item_id=2, 80g)
//   NPC 2 (Healer / Revival): revival point, no shop items

class HardcodedNpcService : public INpcService
{
public:
    HardcodedNpcService()
    {
        {
            NpcData shop{};
            shop.npc_id = 1;
            shop.type   = NpcType::Item;
            shop.name   = "General Merchant";
            shop.shop_items = {
                { 1, 100u },   // HP Potion
                { 2,  80u },   // MP Potion
                { 3, 500u },   // Medium HP Potion
            };
            m_svc.AddNpc(std::move(shop));
        }
        {
            NpcData healer{};
            healer.npc_id = 2;
            healer.type   = NpcType::Revival;
            healer.name   = "Healer";
            m_svc.AddNpc(std::move(healer));
        }
    }

    const NpcData* GetNpc(std::uint16_t id) const override
    {
        return m_svc.GetNpc(id);
    }

    std::optional<NpcShopItem>
    FindShopItem(std::uint16_t npc_id, std::uint16_t item_id) const override
    {
        return m_svc.FindShopItem(npc_id, item_id);
    }

private:
    FakeNpcService m_svc;
};

} // namespace tmapsvr
