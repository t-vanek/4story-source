#include "castle_war_registry.h"

namespace tworldsvr {

namespace {
constexpr std::uint8_t kContryD = 0;
constexpr std::uint8_t kContryC = 1;
constexpr std::uint8_t kContryN = 3;
} // namespace

void CastleWarRegistry::Store(CastleWarInfo report)
{
    std::lock_guard g(m_mtx);
    const std::uint16_t id = report.id;
    m_castles.erase(id);
    m_castles.emplace(id, std::move(report));
}

std::size_t CastleWarRegistry::Size() const
{
    std::lock_guard g(m_mtx);
    return m_castles.size();
}

// Legacy CompareGuildRank (TWorldSvr.cpp:7558): PvP total desc,
// member count desc, establish time asc.
bool CastleWarRegistry::CompareGuildRank(const CastleGuildFacts& a,
                                         const CastleGuildFacts& b)
{
    if (a.pvp_total != b.pvp_total)
        return a.pvp_total > b.pvp_total;
    if (a.member_count != b.member_count)
        return a.member_count > b.member_count;
    return a.establish < b.establish;
}

// Legacy CompareOccupation (TWorldSvr.cpp:7576): walk the per-slot
// ACCEPT history — first the days before the war day (war_day-2 down
// to slot 0), then from slot 5 down to war_day-1. Higher bonus wins;
// an equal non-zero bonus falls back to the castle id (the guild
// keeps the HIGHER-id castle: c1<c2 → g2 wins).
std::uint32_t CastleWarRegistry::CompareOccupation(
    const CastleWarInfo& c1, std::uint32_t g1,
    const CastleWarInfo& c2, std::uint32_t g2, std::uint8_t war_day)
{
    auto it1 = c1.occupy.find(g1);
    auto it2 = c2.occupy.find(g2);
    const bool has1 = it1 != c1.occupy.end();
    const bool has2 = it2 != c2.occupy.end();
    if (!has1 && !has2) return 0;
    if (has1 && !has2) return g1;
    if (!has1 && has2) return g2;

    auto pick = [&](std::uint16_t v1, std::uint16_t v2)
        -> std::uint32_t
    {
        if (v1 > v2) return g1;
        if (v1 < v2) return g2;
        if (v1)                          // equal non-zero → castle id
        {
            if (c1.id < c2.id) return g2;
            if (c1.id > c2.id) return g1;
        }
        return 0;                        // keep scanning
    };

    for (std::uint8_t day = war_day; day > 1; --day)
    {
        const auto r = pick(it1->second[day - 2], it2->second[day - 2]);
        if (r) return r;
        if (it1->second[day - 2] && it1->second[day - 2] ==
            it2->second[day - 2])
            return 0;                    // legacy equal-same-castle 0
    }
    for (std::uint8_t day = 6; day >= war_day && day >= 1; --day)
    {
        const auto r = pick(it1->second[day - 1], it2->second[day - 1]);
        if (r) return r;
        if (it1->second[day - 1] && it1->second[day - 1] ==
            it2->second[day - 1])
            return 0;
    }
    return 0;
}

// Legacy SelectCastleWarGuild (TWorldSvr.cpp:7426). Returns the
// elected guild id via out_top (0 = none); nullptr return means the
// recursion already finalised this castle (legacy case-B NULL).
CastleWarInfo* CastleWarRegistry::SelectWarGuild(
    bool is_def, CastleWarInfo& castle,
    const CastleGuildFactsFn& facts, std::uint8_t war_day,
    std::uint32_t* out_top)
{
    if (is_def)
    {
        castle.def_guild   = 0;
        castle.def_country = kContryN;
    }
    else
        castle.atk_guild = 0;

    // Champion scan over the enable map (ordered — legacy ties fall
    // to iteration order).
    std::uint32_t top_id = 0;
    std::uint32_t top_points = 0;
    CastleGuildFacts top_facts{};
    for (const auto& [gid, points] : castle.enable)
    {
        const CastleGuildFacts f = facts(gid);
        if (!f.found)
            continue;
        if (!is_def && f.country == castle.def_country)
            continue;                        // attacker ≠ def country

        if (top_id == 0 ||
            static_cast<std::uint16_t>(top_points) <
                static_cast<std::uint16_t>(points))
        {
            top_id = gid; top_points = points; top_facts = f;
        }
        else if (static_cast<std::uint16_t>(top_points) ==
                 static_cast<std::uint16_t>(points))
        {
            const auto& cp = castle.country_point;
            if (top_facts.country != f.country &&
                cp[kContryD] > cp[kContryC] && f.country == kContryD)
            {
                top_id = gid; top_points = points; top_facts = f;
            }
            else if (top_facts.country != f.country &&
                     cp[kContryC] > cp[kContryD] &&
                     f.country == kContryC)
            {
                top_id = gid; top_points = points; top_facts = f;
            }
            else
            {
                const auto sel = CompareOccupation(castle, gid,
                    castle, top_id, war_day);
                if (sel == gid)
                {
                    top_id = gid; top_points = points; top_facts = f;
                }
                else if (sel == 0 && CompareGuildRank(f, top_facts))
                {
                    top_id = gid; top_points = points; top_facts = f;
                }
            }
        }
    }

    // Cross-castle conflict loop (legacy: a guild keeps only one
    // castle per role; the weaker claim is re-elected).
    for (auto& [other_id, other] : m_castles)
    {
        if (top_id == 0)
            break;
        auto it_top = castle.enable.find(top_id);
        if (it_top == castle.enable.end())
        {
            // The case-B recursion below already finalised us.
            top_id = 0;
            break;
        }
        const std::uint32_t compare =
            is_def ? other.def_guild : other.atk_guild;
        if (compare != top_id)
            continue;
        auto it_other = other.guild_points.find(compare);
        if (it_other == other.guild_points.end())
            continue;

        if (it_other->second < it_top->second)
        {
            other.enable.erase(compare);
            std::uint32_t ignored = 0;
            SelectWarGuild(is_def, other, facts, war_day, &ignored);
        }
        else if (it_other->second > it_top->second)
        {
            castle.enable.erase(compare);
            SelectWarGuild(is_def, castle, facts, war_day, out_top);
            return nullptr;                  // legacy case-B NULL path
        }
        else
        {
            // Equal points: legacy compares the SAME guild id on both
            // castles, so a non-zero occupation verdict always strips
            // the OTHER castle (the dwSelGuild == itS->first branch
            // is unreachable — quirk kept).
            const auto sel = CompareOccupation(castle, compare,
                other, compare, war_day);
            if (sel != 0)
            {
                other.enable.erase(compare);
                std::uint32_t ignored = 0;
                SelectWarGuild(is_def, other, facts, war_day, &ignored);
            }
        }
    }

    if (top_id != 0)
    {
        if (is_def)
        {
            castle.def_guild   = top_id;
            castle.def_country = top_facts.country;
        }
        else
            castle.atk_guild = top_id;
        castle.enable.erase(top_id);
    }
    *out_top = top_id;
    return &castle;
}

std::vector<CastleWarBroadcastRow>
CastleWarRegistry::Recompute(const CastleGuildFactsFn& facts,
                             std::uint8_t castle_war_day)
{
    std::lock_guard g(m_mtx);

    // Phase 1 — reset scratch (legacy TWorldSvr.cpp:7315).
    for (auto& [id, c] : m_castles)
    {
        c.enable = c.guild_points;
        c.top3[kContryD].clear();
        c.top3[kContryC].clear();
        c.atk_guild = 0;
        c.def_guild = 0;
        c.country_point.fill(0);
    }

    // Phase 2 — country points + per-country top-3 insertion
    // (legacy TWorldSvr.cpp:7326).
    for (auto& [id, c] : m_castles)
    {
        for (const auto& [gid, points] : c.guild_points)
        {
            const CastleGuildFacts f = facts(gid);
            if (!f.found)
                continue;
            if (f.country < kCastleCountryCount)
                c.country_point[f.country] +=
                    static_cast<std::uint16_t>(points);
            if (f.country > kContryC)
                continue;                    // top-3 only for D/C

            auto& board = c.top3[f.country];
            std::uint8_t rank =
                static_cast<std::uint8_t>(board.size() + 1);
            for (const auto& [r, entry] : board)
            {
                const CastleGuildFacts tf = facts(entry.id);
                if (!tf.found || entry.id == gid)
                {
                    rank = 4;
                    break;
                }
                if (entry.point < points)
                    --rank;
                else if (entry.point == points)
                {
                    const auto sel = CompareOccupation(c, gid, c,
                        entry.id, castle_war_day);
                    if (sel == gid)
                        --rank;
                    else if (sel == 0 && CompareGuildRank(f, tf))
                        --rank;
                }
            }
            if (rank < 4)
            {
                CastleTop3Entry fresh{gid, f.name,
                    static_cast<std::uint16_t>(points)};
                board.erase(3);
                if (rank < 3)
                {
                    auto it2 = board.find(2);
                    if (it2 != board.end())
                        board.insert_or_assign(3, it2->second);
                }
                if (rank == 1)
                {
                    auto it1 = board.find(1);
                    if (it1 != board.end())
                    {
                        board.erase(2);
                        board.insert_or_assign(2, it1->second);
                    }
                }
                board.erase(rank);
                board.insert_or_assign(rank, fresh);
            }
        }

        std::uint32_t ignored = 0;
        SelectWarGuild(true, c, facts, castle_war_day, &ignored);
    }

    // Phase 3 — every castle's defender leaves every enable map
    // (legacy TWorldSvr.cpp:7400).
    for (auto& [id, c] : m_castles)
        for (auto& [oid, other] : m_castles)
            other.enable.erase(c.def_guild);

    // Phase 4 — attacker pass.
    for (auto& [id, c] : m_castles)
    {
        std::uint32_t ignored = 0;
        SelectWarGuild(false, c, facts, castle_war_day, &ignored);
    }

    // Phase 5 — emit broadcast rows.
    std::vector<CastleWarBroadcastRow> rows;
    for (const auto& [id, c] : m_castles)
    {
        CastleWarBroadcastRow row{};
        row.info = c;
        const CastleGuildFacts def = facts(c.def_guild);
        const CastleGuildFacts atk = facts(c.atk_guild);
        row.def_id   = def.found ? c.def_guild : 0;
        row.def_name = def.found ? def.name : "";
        row.atk_id   = atk.found ? c.atk_guild : 0;
        row.atk_name = atk.found ? atk.name : "";
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<CastleWarBroadcastRow>
CastleWarRegistry::Snapshot(const CastleGuildFactsFn& facts) const
{
    std::lock_guard g(m_mtx);
    std::vector<CastleWarBroadcastRow> rows;
    for (const auto& [id, c] : m_castles)
    {
        CastleWarBroadcastRow row{};
        row.info = c;
        const CastleGuildFacts def = facts(c.def_guild);
        const CastleGuildFacts atk = facts(c.atk_guild);
        row.def_id   = def.found ? c.def_guild : 0;
        row.def_name = def.found ? def.name : "";
        row.atk_id   = atk.found ? c.atk_guild : 0;
        row.atk_name = atk.found ? atk.name : "";
        rows.push_back(std::move(row));
    }
    return rows;
}

} // namespace tworldsvr
