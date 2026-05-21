#pragma once

// NPC service — read-only registry of static NPC fixtures loaded from
// TNPCCHART. NPCs are part of the world's static content (positioned
// in a map, typed as shop / quest-giver / gate / …), not per-char
// state, so the SOCI impl populates an in-memory map once at boot
// rather than fetching per request.
//
// Used by OnNpcTalkReq (CS_NPCTALK_REQ): client passes wNpcID, server
// looks up the row, runs the country / disguise check (F10+ when the
// player state lands), and replies CS_NPCTALK_ACK with the quest id
// the NPC triggers (quest engine: F12 — for F10 we always reply
// dwQuestID = 0).
//
// Modeled on the legacy CTMapSvrModule::m_mapTNpc + FindTNpc in
// legacy_src/TMapSvrModule.h:82,481.

#include "domain/npc.h"

#include <cstdint>
#include <optional>

namespace tmapsvr {

class INpcService
{
public:
    virtual ~INpcService() = default;

    // Single-NPC lookup keyed by wID. Returns nullopt when the id
    // isn't in the chart — handler treats that as "client asked about
    // a fictional NPC" and drops the request without sending an ack
    // (legacy CSHandler.cpp:3517 returns EC_NOERROR with no reply).
    virtual std::optional<NpcRow>
        FindNpc(std::uint16_t npc_id) const = 0;

    // Total registered NPCs — used by main()'s boot log.
    virtual std::size_t Size() const = 0;
};

} // namespace tmapsvr
