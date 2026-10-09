#include "services/spawn_manager.h"

#include "services/map_mon_chart.h"
#include "services/mon_attr_chart.h"
#include "services/monster_chart.h"
#include "services/monster_registry.h"
#include "services/spawn_chart.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace tmapsvr {

std::size_t SpawnAllStatic(const ISpawnChart&   spawns,
                           const IMapMonChart&  map_mon,
                           const IMonsterChart& monsters,
                           const IMonAttrChart& attrs,
                           IMonsterRegistry&    registry,
                           std::uint32_t&       next_instance_id,
                           std::uint8_t         channel)
{
    std::size_t spawned = 0, skipped_no_entry = 0, skipped_no_tmpl = 0;
    std::size_t skipped_no_attr = 0, skipped_zero_hp = 0;

    for (const auto& p : spawns.All())
    {
        const auto& entries = map_mon.ForSpawn(p.wID);
        if (entries.empty())
        {
            ++skipped_no_entry;
            continue;
        }

        // bCount slots per spawn point. Distribute deterministically
        // across the candidate entries (round-robin) — the prob-weighted
        // / essential / leader selection is an AI/regen refinement; the
        // static population just needs the monsters to exist in view.
        const int count = p.bCount > 0 ? p.bCount : 1;
        for (int slot = 0; slot < count; ++slot)
        {
            const auto& e = entries[static_cast<std::size_t>(slot) % entries.size()];

            const auto tmpl = monsters.Find(e.wMonID);
            if (!tmpl)
            {
                ++skipped_no_tmpl;
                continue;
            }

            // Legacy TMap.cpp rejects an essential spawn without attributes;
            // TAICmdRegen.cpp returns FALSE for the corresponding regen case.
            // Preserve that refusal without the legacy whole-spawn early exit.
            // Zero HP is quarantined as unusable live content, never invented.
            const auto attr = attrs.Find(e.wMonID, tmpl->bLevel);
            if (!attr)
            {
                ++skipped_no_attr;
                continue;
            }
            if (attr->dwMaxHP == 0)
            {
                ++skipped_zero_hp;
                continue;
            }

            MonsterInstance m;
            m.dwInstanceID = next_instance_id++;
            m.wTemplateID  = e.wMonID;
            m.wSpawnID     = p.wID;
            m.wMapID       = p.wMapID;
            m.bChannel     = channel;
            m.fPosX        = p.fPosX;
            m.fPosY        = p.fPosY;
            m.fPosZ        = p.fPosZ;
            m.dwMaxHP  = attr->dwMaxHP;
            m.wAP      = attr->wAP;
            m.wMinWAP  = attr->wMinWAP;
            m.wMaxWAP  = attr->wMaxWAP;
            m.wDP      = attr->wDP;
            m.wMDP     = attr->wMDP;
            m.dwHP = m.dwMaxHP;   // spawns at full health

            registry.Insert(m);
            ++spawned;
        }
    }

    spdlog::info("spawn_manager: spawned {} monster(s) on channel {} "
                 "({} spawn point(s) had no TMAPMONCHART rows, {} skipped "
                 "for missing TMONSTERCHART template, {} missing attribute "
                 "rows, {} zero-HP rows quarantined)",
        spawned, channel, skipped_no_entry, skipped_no_tmpl,
        skipped_no_attr, skipped_zero_hp);

    return spawned;
}

} // namespace tmapsvr
