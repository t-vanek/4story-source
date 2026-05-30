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

            MonsterInstance m;
            m.dwInstanceID = next_instance_id++;
            m.wTemplateID  = e.wMonID;
            m.wSpawnID     = p.wID;
            m.wMapID       = p.wMapID;
            m.bChannel     = channel;
            m.fPosX        = p.fPosX;
            m.fPosY        = p.fPosY;
            m.fPosZ        = p.fPosZ;
            // Real spawn HP from TMONATTRCHART (monster id + level). A
            // monster with no stat row falls back to a level-scaled
            // placeholder so it still shows a sane, non-zero bar
            // (0 = dead/reaped).
            const std::uint8_t lvl = std::max<std::uint8_t>(1, tmpl->bLevel);
            if (const auto attr = attrs.Find(e.wMonID, tmpl->bLevel);
                attr && attr->dwMaxHP > 0)
                m.dwHP = attr->dwMaxHP;
            else
                m.dwHP = 100u * static_cast<std::uint32_t>(lvl);

            registry.Insert(m);
            ++spawned;
        }
    }

    spdlog::info("spawn_manager: spawned {} monster(s) on channel {} "
                 "({} spawn point(s) had no TMAPMONCHART rows, {} skipped "
                 "for missing TMONSTERCHART template)",
        spawned, channel, skipped_no_entry, skipped_no_tmpl);

    return spawned;
}

} // namespace tmapsvr
