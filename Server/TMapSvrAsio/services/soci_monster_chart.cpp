#include "soci_monster_chart.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

SociMonsterChart::SociMonsterChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_id = 0, row_race = 0, row_class = 0, row_kind = 0,
                 row_level = 0, row_ai = 0, row_range = 0, row_chase = 0,
                 row_roam = 0, row_money_prob = 0, row_min_money = 0,
                 row_max_money = 0, row_item_prob = 0, row_drop_count = 0,
                 row_exp = 0, row_self = 0, row_recall = 0, row_select = 0;
    std::string  row_name;
    soci::indicator name_ind = soci::i_null;

    soci::statement st = (sql.prepare <<
        "SELECT wID, szName, bRace, bClass, wKind, bLevel, bAIType, "
        "  bRange, wChaseRange, bRoamProb, bMoneyProb, dwMinMoney, "
        "  dwMaxMoney, bItemProb, bDropCount, wExp, bIsSelf, "
        "  bRecallType, bCanSelect "
        "FROM TMONSTERCHART",
        soci::into(row_id),       soci::into(row_name, name_ind),
        soci::into(row_race),     soci::into(row_class),
        soci::into(row_kind),     soci::into(row_level),
        soci::into(row_ai),       soci::into(row_range),
        soci::into(row_chase),    soci::into(row_roam),
        soci::into(row_money_prob), soci::into(row_min_money),
        soci::into(row_max_money), soci::into(row_item_prob),
        soci::into(row_drop_count), soci::into(row_exp),
        soci::into(row_self),     soci::into(row_recall),
        soci::into(row_select));
    st.execute(false);

    while (st.fetch())
    {
        MonsterTemplate t;
        t.wID         = static_cast<std::uint16_t>(row_id);
        t.szName      = (name_ind == soci::i_ok) ? row_name : std::string{};
        t.bRace       = static_cast<std::uint8_t> (row_race);
        t.bClass      = static_cast<std::uint8_t> (row_class);
        t.wKind       = static_cast<std::uint16_t>(row_kind);
        t.bLevel      = static_cast<std::uint8_t> (row_level);
        t.bAIType     = static_cast<std::uint8_t> (row_ai);
        t.bRange      = static_cast<std::uint8_t> (row_range);
        t.wChaseRange = static_cast<std::uint16_t>(row_chase);
        t.bRoamProb   = static_cast<std::uint8_t> (row_roam);
        t.bMoneyProb  = static_cast<std::uint8_t> (row_money_prob);
        t.dwMinMoney  = static_cast<std::uint32_t>(row_min_money);
        t.dwMaxMoney  = static_cast<std::uint32_t>(row_max_money);
        t.bItemProb   = static_cast<std::uint8_t> (row_item_prob);
        t.bDropCount  = static_cast<std::uint8_t> (row_drop_count);
        t.wExp        = static_cast<std::uint16_t>(row_exp);
        t.bIsSelf     = static_cast<std::uint8_t> (row_self);
        t.bRecallType = static_cast<std::uint8_t> (row_recall);
        t.bCanSelect  = static_cast<std::uint8_t> (row_select);
        m_rows[t.wID] = std::move(t);
    }

    spdlog::info("soci_monster_chart: loaded {} row(s) from TMONSTERCHART",
        m_rows.size());
}

std::optional<MonsterTemplate>
SociMonsterChart::Find(std::uint16_t template_id) const
{
    const auto it = m_rows.find(template_id);
    if (it == m_rows.end()) return std::nullopt;
    return it->second;
}

} // namespace tmapsvr
