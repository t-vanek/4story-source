// Unit test: SpawnAllStatic — the boot-time monster population. Joins
// the spawn chart (where + how many), the map-mon chart (which monster
// per spawn point), and the monster chart (template) into the registry.
// Pure logic, driven against in-memory / inline fakes — no DB, no boot.

#include "services/spawn_manager.h"
#include "services/spawn_chart.h"
#include "services/map_mon_chart.h"
#include "services/monster_chart.h"
#include "services/monster_registry.h"
#include "domain/monster.h"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <unordered_map>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

class FakeSpawnChart final : public tmapsvr::ISpawnChart
{
public:
    std::vector<tmapsvr::SpawnPoint> rows;
    const std::vector<tmapsvr::SpawnPoint>& All() const override { return rows; }
    std::size_t Size() const override { return rows.size(); }
};

class FakeMonsterChart final : public tmapsvr::IMonsterChart
{
public:
    void Add(std::uint16_t id, std::uint8_t level)
    {
        tmapsvr::MonsterTemplate t;
        t.wID = id;
        t.bLevel = level;
        m_rows[id] = t;
    }
    std::optional<tmapsvr::MonsterTemplate>
        Find(std::uint16_t id) const override
    {
        const auto it = m_rows.find(id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }
    std::size_t Size() const override { return m_rows.size(); }
private:
    std::unordered_map<std::uint16_t, tmapsvr::MonsterTemplate> m_rows;
};

tmapsvr::SpawnPoint MakeSpawn(std::uint16_t id, std::uint16_t map,
                              std::uint8_t count, float x, float y, float z)
{
    tmapsvr::SpawnPoint p;
    p.wID    = id;
    p.wMapID = map;
    p.bCount = count;
    p.fPosX  = x;
    p.fPosY  = y;
    p.fPosZ  = z;
    return p;
}

} // namespace

int main()
{
    using namespace tmapsvr;

    FakeSpawnChart spawns;
    spawns.rows.push_back(MakeSpawn(10, 60, 2, 1.f, 2.f, 3.f));  // 2 monsters
    spawns.rows.push_back(MakeSpawn(20, 60, 1, 4.f, 5.f, 6.f));  // 1 monster
    spawns.rows.push_back(MakeSpawn(30, 61, 3, 7.f, 8.f, 9.f));  // no map-mon rows
    spawns.rows.push_back(MakeSpawn(40, 62, 1, 0.f, 0.f, 0.f));  // monster w/o template

    InMemoryMapMonChart map_mon;
    map_mon.Add({ 10, 100, /*ess=*/1, 0, 50 });
    map_mon.Add({ 10, 101, 0, 0, 50 });
    map_mon.Add({ 20, 100, 1, 0, 100 });
    map_mon.Add({ 40, 888, 1, 0, 100 });   // 888 has no template

    FakeMonsterChart monsters;
    monsters.Add(100, /*level=*/5);
    monsters.Add(101, /*level=*/10);
    // 888 deliberately absent.

    InMemoryMonsterRegistry registry;
    std::uint32_t next_id = 1000;

    const std::size_t spawned =
        SpawnAllStatic(spawns, map_mon, monsters, registry, next_id, /*channel=*/0);

    // SP10 → 2, SP20 → 1, SP30 → 0 (no rows), SP40 → 0 (no template) = 3.
    EXPECT(spawned == 3);
    EXPECT(registry.Size() == 3);
    EXPECT(next_id == 1003);                  // 3 ids drawn from 1000

    // All three are on channel 0 / map 60; map 61 + 62 produced nothing.
    EXPECT(registry.ListInMap(0, 60).size() == 3);
    EXPECT(registry.ListInMap(0, 61).empty());
    EXPECT(registry.ListInMap(0, 62).empty());
    EXPECT(registry.ListInMap(1, 60).empty());  // wrong channel

    // Spot-check a realised instance: SP10 slot 0 → monster 100 (lvl 5),
    // hp = 100 * 5 = 500, at the spawn-point position.
    const auto first = registry.Find(1000);
    EXPECT(first.has_value());
    if (first)
    {
        EXPECT(first->wTemplateID == 100);
        EXPECT(first->wSpawnID == 10);
        EXPECT(first->wMapID == 60);
        EXPECT(first->bChannel == 0);
        EXPECT(first->dwHP == 500);
        EXPECT(first->fPosX == 1.f);
        EXPECT(first->fPosZ == 3.f);
    }
    // SP10 slot 1 → monster 101 (lvl 10), hp = 1000.
    const auto second = registry.Find(1001);
    EXPECT(second.has_value());
    if (second)
    {
        EXPECT(second->wTemplateID == 101);
        EXPECT(second->dwHP == 1000);
    }

    if (g_fails == 0)
        std::printf("test_spawn_manager: join + skip-no-rows + skip-no-template "
                    "+ channel/map filter OK (spawned=%zu)\n", spawned);
    return g_fails == 0 ? 0 : 1;
}
