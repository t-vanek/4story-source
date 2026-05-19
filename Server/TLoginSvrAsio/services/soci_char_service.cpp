#include "soci_char_service.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <tuple>
#include <vector>

namespace tloginsvr::services {

namespace {

// Same charset / length as legacy CSHandler.cpp:1024 + in-memory impl.
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

constexpr std::uint8_t kMaxCharsPerUser    = 6;  // legacy CHARSLOT_MAX
constexpr std::uint8_t kCountryPeace       = 4;  // NetCode.h: TCONTRY_PEACE
constexpr std::uint8_t kPeaceStartLevel    = 1;  // peace country starts at 1
constexpr std::uint8_t kChoiceStartLevel   = 9;  // NetCode.h: CHOICE_COUNTRY_LEVEL
constexpr std::int32_t kClassCount         = 6;  // NetCode.h: TCLASS_COUNT

// Equipped-inventory selector for CTBLItem (NetCode.h):
//   bStorageType = STORAGE_INVEN = 0
//   dwStorageID  = INVEN_EQUIP   = 0xFE
//   bOwnerType   = TOWNER_CHAR   = 0
constexpr int kStorageInven = 0;
constexpr int kInvenEquip   = 0xFE;
constexpr int kOwnerChar    = 0;

// Starter inventory per class — placeholder mapping. The legacy SP
// CSPCreateChar inserts the gameplay-tuned starter set; we don't have
// its source here. Each tuple: (storage_type, item_kind, item_id, level).
struct StarterItem { std::int16_t storage_type; std::int16_t item_kind; std::int16_t item_id; std::int16_t level; };
const StarterItem* StarterSet(std::uint8_t char_class, std::size_t& out_n)
{
    static const StarterItem warrior[] = { { 1, 1, 1, 1 }, { 1, 5, 10, 1 } };
    static const StarterItem ranger[]  = { { 1, 1, 2, 1 }, { 1, 5, 10, 1 } };
    static const StarterItem archer[]  = { { 1, 1, 3, 1 }, { 1, 5, 10, 1 } };
    static const StarterItem wizard[]  = { { 1, 1, 4, 1 }, { 1, 5, 11, 1 } };
    static const StarterItem priest[]  = { { 1, 1, 4, 1 }, { 1, 5, 11, 1 } };
    static const StarterItem sorcerer[]= { { 1, 1, 4, 1 }, { 1, 5, 11, 1 } };
    switch (char_class)
    {
    case 0: out_n = std::size(warrior);  return warrior;
    case 1: out_n = std::size(ranger);   return ranger;
    case 2: out_n = std::size(archer);   return archer;
    case 3: out_n = std::size(wizard);   return wizard;
    case 4: out_n = std::size(priest);   return priest;
    case 5: out_n = std::size(sorcerer); return sorcerer;
    default: out_n = 0; return nullptr;
    }
}

} // namespace

SociCharService::SociCharService(fourstory::db::SessionPool& global_pool,
                                 fourstory::db::SessionPool& world_pool)
    : m_global(global_pool)
    , m_world(world_pool)
{
    RefreshVeteranChart();
}

void SociCharService::RefreshVeteranChart()
{
    // Best-effort TVETERANCHART load from TGLOBAL. Empty / missing is OK —
    // chars then just get the country-based starting level.
    std::unordered_map<std::uint8_t, std::uint8_t> next;
    try
    {
        auto lease = m_global.Acquire();
        soci::session& sql = *lease;
        int opt = 0, lvl = 0;
        soci::statement st = (sql.prepare <<
            "SELECT \"bID\", \"bLevel\" FROM \"TVETERANCHART\"",
            soci::into(opt), soci::into(lvl));
        st.execute();
        while (st.fetch())
        {
            next[static_cast<std::uint8_t>(opt)] =
                static_cast<std::uint8_t>(lvl);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("char_service: TVETERANCHART unreadable ({}) — "
                     "veteran bonus disabled",
            ex.what());
        return;
    }
    std::lock_guard<std::mutex> lock(m_veteran_mtx);
    m_veteran_levels = std::move(next);
    spdlog::info("char_service: refreshed {} veteran-chart row(s)",
        m_veteran_levels.size());
}

std::vector<CharacterInfo>
SociCharService::List(std::int32_t user_id, std::uint8_t /*group_id*/)
{
    // TCHARTABLE / TITEMTABLE / TGUILDMEMBERTABLE / TGUILDTABLE are
    // per-world (TGAME). The current schema doesn't carry a bWorldID
    // column on TCHARTABLE — each world has its own TGAME DB, so the
    // world pool already pins us to the right world.
    auto lease = m_world.Acquire();
    soci::session& sql = *lease;

    std::vector<CharacterInfo> out;
    try
    {
        // Step 1 — character rows. Scope the rowset to release its
        // cursor before any other query runs on the same connection
        // (MSSQL "SQL state 24000" otherwise).
        {
            soci::rowset<soci::row> rs = (sql.prepare <<
                "SELECT \"dwCharID\", \"szNAME\", \"bSlot\", \"bLevel\","
                "       \"bClass\", \"bRace\", \"bCountry\", \"bSex\","
                "       \"bHair\", \"bFace\", \"bBody\", \"bPants\","
                "       \"bHand\", \"bFoot\", \"bHelmetHide\","
                "       \"dwRegion\", \"bStartAct\" "
                "FROM \"TCHARTABLE\" "
                "WHERE \"dwUserID\" = :uid "
                "  AND \"bDelete\" = 0 "
                "ORDER BY \"bSlot\"",
                soci::use(user_id));
            for (const auto& r : rs)
            {
                CharacterInfo info{};
                info.char_id     = r.get<int>(0);
                info.name        = r.get<std::string>(1);
                info.slot        = static_cast<std::uint8_t>(r.get<int>(2));
                info.level       = static_cast<std::uint8_t>(r.get<int>(3));
                info.char_class  = static_cast<std::uint8_t>(r.get<int>(4));
                info.race        = static_cast<std::uint8_t>(r.get<int>(5));
                info.country     = static_cast<std::uint8_t>(r.get<int>(6));
                info.sex         = static_cast<std::uint8_t>(r.get<int>(7));
                info.hair        = static_cast<std::uint8_t>(r.get<int>(8));
                info.face        = static_cast<std::uint8_t>(r.get<int>(9));
                info.body        = static_cast<std::uint8_t>(r.get<int>(10));
                info.pants       = static_cast<std::uint8_t>(r.get<int>(11));
                info.hand        = static_cast<std::uint8_t>(r.get<int>(12));
                info.foot        = static_cast<std::uint8_t>(r.get<int>(13));
                info.helmet_hide = static_cast<std::uint8_t>(r.get<int>(14));
                info.region      = static_cast<std::uint32_t>(r.get<int>(15));
                info.start_act   = static_cast<std::uint8_t>(r.get<int>(16));
                out.push_back(std::move(info));
            }
        }

        // Step 2 — equipped items + guild fame per char. Two small
        // per-char queries matching legacy CTBLItem + TGetGuildInfo
        // semantics. Done in a separate pass after the char rowset is
        // released so the cursor doesn't conflict.
        for (auto& info : out)
        {
            const int char_id = info.char_id;
            const int storage_type = kStorageInven;
            const int storage_id   = kInvenEquip;
            const int owner_type   = kOwnerChar;

            // Items — column aliasing per legacy DBAccess.h:607-628.
            // dwTime3 → wColor, dwTime4 → bRegGuild. wCustomTex is a
            // separate column in TITEMTABLE (shipped client parses it
            // between wColor and bRegGuild — TNetHandler.cpp:438).
            // The SELECT lists wCustomTex inside an inner try so a
            // schema without the column gracefully falls back to 0.
            try
            {
                bool customtex_supported = true;
                try
                {
                    soci::rowset<soci::row> items = (sql.prepare <<
                        "SELECT \"bItemID\", \"wItemID\", \"bLevel\", "
                        "       \"bGradeEffect\", \"dwTime3\", \"wCustomTex\", "
                        "       \"dwTime4\", \"wMoggItemID\" "
                        "FROM \"TITEMTABLE\" "
                        "WHERE \"dwOwnerID\" = :c "
                        "  AND \"bOwnerType\" = :ot "
                        "  AND \"bStorageType\" = :st "
                        "  AND \"dwStorageID\" = :sid",
                        soci::use(char_id),
                        soci::use(owner_type),
                        soci::use(storage_type),
                        soci::use(storage_id));
                    for (const auto& r : items)
                    {
                        EquipItem it{};
                        it.item_id      = static_cast<std::uint8_t>(r.get<int>(0));
                        it.item_kind    = static_cast<std::uint16_t>(r.get<int>(1));
                        it.level        = static_cast<std::uint8_t>(r.get<int>(2));
                        it.grade_effect = static_cast<std::uint8_t>(r.get<int>(3));
                        it.color        = static_cast<std::uint16_t>(r.get<int>(4));
                        it.custom_tex   = static_cast<std::uint16_t>(r.get<int>(5));
                        it.reg_guild    = static_cast<std::uint8_t>(r.get<int>(6));
                        it.mogg_item_id = static_cast<std::uint16_t>(r.get<int>(7));
                        info.items.push_back(it);
                    }
                }
                catch (const std::exception&)
                {
                    // Older schema without wCustomTex — retry without
                    // the column. custom_tex defaults to 0.
                    customtex_supported = false;
                }
                if (!customtex_supported)
                {
                    soci::rowset<soci::row> items = (sql.prepare <<
                        "SELECT \"bItemID\", \"wItemID\", \"bLevel\", "
                        "       \"bGradeEffect\", \"dwTime3\", \"dwTime4\", "
                        "       \"wMoggItemID\" "
                        "FROM \"TITEMTABLE\" "
                        "WHERE \"dwOwnerID\" = :c "
                        "  AND \"bOwnerType\" = :ot "
                        "  AND \"bStorageType\" = :st "
                        "  AND \"dwStorageID\" = :sid",
                        soci::use(char_id),
                        soci::use(owner_type),
                        soci::use(storage_type),
                        soci::use(storage_id));
                    for (const auto& r : items)
                    {
                        EquipItem it{};
                        it.item_id      = static_cast<std::uint8_t>(r.get<int>(0));
                        it.item_kind    = static_cast<std::uint16_t>(r.get<int>(1));
                        it.level        = static_cast<std::uint8_t>(r.get<int>(2));
                        it.grade_effect = static_cast<std::uint8_t>(r.get<int>(3));
                        it.color        = static_cast<std::uint16_t>(r.get<int>(4));
                        // custom_tex stays 0 — TITEMTABLE column absent.
                        it.reg_guild    = static_cast<std::uint8_t>(r.get<int>(5));
                        it.mogg_item_id = static_cast<std::uint16_t>(r.get<int>(6));
                        info.items.push_back(it);
                    }
                }
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("char.List items char_id={} skipped: {}",
                    char_id, ex.what());
            }

            // Guild fame — TGetGuildInfo SP equivalent. Two-step lookup:
            // TGUILDMEMBERTABLE → dwGuildID; if found, TGUILDTABLE for
            // szName/dwFame/dwFameColor. No row in either → fame stays 0.
            try
            {
                int guild_id = 0;
                soci::indicator gid_ind = soci::i_null;
                {
                    soci::statement g = (sql.prepare <<
                        "SELECT \"dwGuildID\" FROM \"TGUILDMEMBERTABLE\" "
                        "WHERE \"dwCharID\" = :c",
                        soci::use(char_id),
                        soci::into(guild_id, gid_ind));
                    g.execute(true);
                }
                if (gid_ind != soci::i_null && guild_id > 0)
                {
                    int fame = 0;
                    int fame_color = 0;
                    soci::indicator fame_ind = soci::i_null, color_ind = soci::i_null;
                    {
                        soci::statement gt = (sql.prepare <<
                            "SELECT \"dwFame\", \"dwFameColor\" FROM \"TGUILDTABLE\" "
                            "WHERE \"dwID\" = :g",
                            soci::use(guild_id),
                            soci::into(fame, fame_ind),
                            soci::into(fame_color, color_ind));
                        gt.execute(true);
                    }
                    if (fame_ind != soci::i_null) info.fame = static_cast<std::uint32_t>(fame);
                    if (color_ind != soci::i_null) info.fame_color = static_cast<std::uint32_t>(fame_color);
                }
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("char.List guild char_id={} skipped: {}",
                    char_id, ex.what());
            }
        }
        return out;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("char.List(uid={}) DB error: {}", user_id, ex.what());
        return out; // return whatever was collected so far
    }
}

CharacterCreateResponse
SociCharService::Create(const CharacterCreateRequest& req)
{
    if (!IsValidCharName(req.name))
    {
        spdlog::warn("char.Create rejected (invalid name) name='{}'", req.name);
        return CharacterCreateResponse{ .status = CreateCharResult::OverChar };
    }

    // Name reservations live in TGLOBAL — check those first.
    {
        auto lease = m_global.Acquire();
        soci::session& sql = *lease;
        try
        {
            int reserved_hit = 0;
            sql << "SELECT COUNT(*) FROM \"TRESERVEDNAME\" WHERE \"szName\" = :n",
                soci::use(req.name), soci::into(reserved_hit);
            if (reserved_hit > 0)
            {
                spdlog::info("char.Create rejected (reserved) name='{}'", req.name);
                return CharacterCreateResponse{ .status = CreateCharResult::Protected };
            }

            // TKEEPINGNAME stores LIKE patterns, not literal names —
            // legacy TGLOBAL.TCreateChar SP uses `@szName LIKE szName`
            // (TCreateChar.sql:33). Entries like 'Admin%' or '_dmin'
            // ban whole families of names. The candidate name is the
            // haystack, the stored row is the pattern.
            int keep_hit = 0;
            sql << "SELECT COUNT(*) FROM \"TKEEPINGNAME\" "
                   "WHERE :n LIKE \"szName\"",
                soci::use(req.name), soci::into(keep_hit);
            if (keep_hit > 0)
            {
                spdlog::info("char.Create rejected (kept) name='{}'", req.name);
                return CharacterCreateResponse{ .status = CreateCharResult::Protected };
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::error("char.Create name-check (global) DB error: {}", ex.what());
            return CharacterCreateResponse{ .status = CreateCharResult::Internal };
        }
    }

    // Everything else (live name lookup, slot check, INSERT) goes against
    // the per-world TGAME.
    auto lease = m_world.Acquire();
    soci::session& sql = *lease;

    try
    {
        int name_hit = 0;
        sql << "SELECT COUNT(*) FROM \"TCHARTABLE\" "
               "WHERE \"szNAME\" = :n AND \"bDelete\" = 0",
            soci::use(req.name), soci::into(name_hit);
        if (name_hit > 0)
        {
            spdlog::info("char.Create rejected (duplicate name) name='{}' hits={}",
                req.name, name_hit);
            return CharacterCreateResponse{ .status = CreateCharResult::DuplicateName };
        }

        // Legacy TCreateChar.sql:88-99 also rejects names that collide
        // with NPCs (TNPCCHART) or monsters (TMONSTERCHART). Both
        // tables are chart data so name space is curated by ops, but
        // a player creating "Goblin" or "Innkeeper" would confuse the
        // world server's name-resolution paths. Modern mirrors the
        // legacy guard. Both tables are optional in dev fixtures —
        // missing-table errors swallowed.
        for (const auto* table : { "TNPCCHART", "TMONSTERCHART" })
        {
            try
            {
                int chart_hit = 0;
                const std::string q =
                    std::string("SELECT COUNT(*) FROM \"") + table +
                    "\" WHERE \"szNAME\" = :n";
                sql << q, soci::use(req.name), soci::into(chart_hit);
                if (chart_hit > 0)
                {
                    spdlog::info("char.Create rejected ({} collision) "
                                 "name='{}'", table, req.name);
                    return CharacterCreateResponse{ .status = CreateCharResult::DuplicateName };
                }
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("char.Create: {} name check skipped: {}",
                    table, ex.what());
            }
        }

        int slot_hit = 0;
        sql << "SELECT COUNT(*) FROM \"TCHARTABLE\" "
               "WHERE \"dwUserID\" = :uid AND \"bSlot\" = :s AND \"bDelete\" = 0",
            soci::use(req.user_id),
            soci::use(static_cast<int>(req.slot)),
            soci::into(slot_hit);
        if (slot_hit > 0)
        {
            spdlog::info("char.Create rejected (slot taken) uid={} slot={}",
                req.user_id, static_cast<int>(req.slot));
            return CharacterCreateResponse{ .status = CreateCharResult::InvalidSlot };
        }

        int live_count = 0;
        sql << "SELECT COUNT(*) FROM \"TCHARTABLE\" "
               "WHERE \"dwUserID\" = :uid AND \"bDelete\" = 0",
            soci::use(req.user_id), soci::into(live_count);
        if (live_count >= kMaxCharsPerUser)
        {
            spdlog::info("char.Create rejected (slot limit) uid={} live={}",
                req.user_id, live_count);
            return CharacterCreateResponse{ .status = CreateCharResult::OverChar };
        }

        const int       ucountry = static_cast<int>(req.country);
        const int       uclass   = static_cast<int>(req.char_class);
        const int       urace    = static_cast<int>(req.race);
        const int       usex     = static_cast<int>(req.sex);
        const int       uhair    = static_cast<int>(req.hair);
        const int       uface    = static_cast<int>(req.face);
        const int       ubody    = static_cast<int>(req.body);
        const int       upants   = static_cast<int>(req.pants);
        const int       uhand    = static_cast<int>(req.hand);
        const int       ufoot    = static_cast<int>(req.foot);
        const int       uslot    = static_cast<int>(req.slot);
        const int       uworld   = static_cast<int>(req.group_id);

        // Veteran level lookup (cached chart loaded at startup).
        std::uint8_t starting_level = (req.country == kCountryPeace)
            ? kPeaceStartLevel
            : kChoiceStartLevel;
        {
            std::lock_guard<std::mutex> vlock(m_veteran_mtx);
            if (auto it = m_veteran_levels.find(req.level_option);
                it != m_veteran_levels.end() && it->second > starting_level)
            {
                starting_level = it->second;
            }
        }
        const int ulevel = static_cast<int>(starting_level);

        // HP/MP computation — legacy TCreateChar.sql:80-86:
        //   dwHP = 7 * (2 + class.wCON + race.wCON) + 1
        //   dwMP = 9 * (2 + class.wMEN + race.wMEN) + 1
        // Lookup TCLASSCHART + TRACECHART; if either is unavailable
        // (dev fixture without the chart, missing row) fall back to a
        // sensible baseline so create still succeeds. The schema's
        // dwHP/dwMP columns are NOT NULL with no default in legacy
        // MSSQL — modern MUST supply explicit values, can't lean on
        // DB defaults like the PG dev fixture does.
        int class_con = 10, class_men = 5;
        int race_con  = 10, race_men  = 10;
        try
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"wCON\", \"wMEN\" FROM \"TCLASSCHART\" "
                "WHERE \"bClassID\" = :c",
                soci::use(uclass),
                soci::into(class_con),
                soci::into(class_men));
            st.execute(true);
            // got_data == false → use baseline above.
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TCLASSCHART lookup skipped "
                          "(class={}): {}", uclass, ex.what());
        }
        try
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"wCON\", \"wMEN\" FROM \"TRACECHART\" "
                "WHERE \"bRaceID\" = :r",
                soci::use(urace),
                soci::into(race_con),
                soci::into(race_men));
            st.execute(true);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TRACECHART lookup skipped "
                          "(race={}): {}", urace, ex.what());
        }
        const int dwHP = 7 * (2 + class_con + race_con) + 1;
        const int dwMP = 9 * (2 + class_men + race_men) + 1;

        // Veteran-level EXP — legacy TCreateChar.sql:125:
        //   SELECT dwExp FROM TLEVELCHART WHERE bLevel = @bLevel - 1
        // Look up the EXP threshold for the level *below* the target,
        // so the char arrives at exactly the threshold for its level.
        // Defaults to 1 for level=1 (the SP's default).
        int dwEXP = 1;
        int wSkillPoint = 0;
        if (req.level_option != 0 && ulevel > 1)
        {
            try
            {
                soci::statement st = (sql.prepare <<
                    "SELECT \"dwExp\" FROM \"TLEVELCHART\" "
                    "WHERE \"bLevel\" = :l",
                    soci::use(ulevel - 1),
                    soci::into(dwEXP));
                st.execute(true);
                if (!st.got_data()) dwEXP = 1;
                wSkillPoint = 200;  // legacy SP gives veteran chars 200 SP
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("char.Create: TLEVELCHART lookup skipped: {}",
                    ex.what());
                dwEXP = 1;
            }
        }

        // Spawn map ID — legacy SP picks the spawn based on the user's
        // existing PvP country (bOriCountry). For peaceful country=4
        // chars, the default 15003 stands. Veteran-boosted chars
        // adopt the user's PvP-country spawn (15001 / 15002).
        int wSpawnID = 15003;
        if (req.level_option != 0)
        {
            // Look up the user's existing PvP-country char to mirror
            // legacy's bOriCountry inheritance (TCreateChar.sql:119).
            int existing_ori = 4;
            try
            {
                soci::statement st = (sql.prepare <<
                    "SELECT TOP 1 \"bOriCountry\" FROM \"TCHARTABLE\" "
                    "WHERE \"dwUserID\" = :u "
                    "  AND \"bDelete\"  = 0 "
                    "  AND \"bOriCountry\" < 2",
                    soci::use(req.user_id),
                    soci::into(existing_ori));
                st.execute(true);
            }
            catch (const std::exception&) { existing_ori = 4; }
            if (existing_ori == 0)      wSpawnID = 15001;
            else if (existing_ori == 1) wSpawnID = 15002;
        }

        // TCHARTABLE.dwCharID is IDENTITY in the legacy MSSQL schema
        // (and `GENERATED BY DEFAULT AS IDENTITY` in PG dev fixture).
        // Earlier revisions computed MAX(dwCharID)+1 then inserted
        // explicitly — that races on multi-writer deploys and gets
        // rejected outright on MSSQL ("cannot insert explicit value
        // for identity column"). Drop dwCharID from the INSERT and
        // capture the auto-generated value via OUTPUT INSERTED
        // (MSSQL) / RETURNING (PG).
        // Full column INSERT — every NOT-NULL column in the legacy
        // TCHARTABLE schema must be provided explicitly. PG dev
        // fixture has DEFAULTs on many of these (dwHP/dwMP excepted)
        // so the previous abbreviated INSERT happened to work there;
        // against real MSSQL it would fail with a NOT NULL violation.
        // Constants below mirror TCreateChar.sql:135-138 + 222-233.
        // SOCI binds `double` for floating-point parameters; the
        // driver converts to REAL when writing TCHARTABLE.fPosX/Y/Z.
        const double fPosX = 3664.405;
        const double fPosY = 86.16578;
        const double fPosZ = 557.2542;
        const int    wDIR  = 762;
        const int    wMapID = 2010;
        const int    zero  = 0;
        const bool is_mssql_world =
            (m_world.GetBackend() == fourstory::db::Backend::Odbc);
        const std::string insert_chartable = is_mssql_world
            ? "INSERT INTO \"TCHARTABLE\" "
              "(\"dwUserID\", \"bSlot\", \"szNAME\", "
              " \"bClass\", \"bRace\", \"bCountry\", \"bOriCountry\", "
              " \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
              " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", "
              " \"bLevel\", \"dwEXP\", \"dwHP\", \"dwMP\", "
              " \"wSkillPoint\", \"dwGold\", \"dwSilver\", \"dwCooper\", "
              " \"wMapID\", \"wSpawnID\", \"wTemptedMon\", \"bAftermath\", "
              " \"fPosX\", \"fPosY\", \"fPosZ\", \"wDIR\") "
              "OUTPUT INSERTED.\"dwCharID\" "
              "VALUES (:uid, :slot, :n, "
              " :cls, :race, :country, :ori_country, "
              " :rsex, :sex, :hair, :face, "
              " :body, :pants, :hand, :foot, "
              " :lvl, :exp, :hp, :mp, "
              " :sp, :gold, :silver, :copper, "
              " :map, :spawn, :tmp_mon, :aftermath, "
              " :px, :py, :pz, :dir)"
            : "INSERT INTO \"TCHARTABLE\" "
              "(\"dwUserID\", \"bSlot\", \"szNAME\", "
              " \"bClass\", \"bRace\", \"bCountry\", \"bOriCountry\", "
              " \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
              " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", "
              " \"bLevel\", \"dwEXP\", \"dwHP\", \"dwMP\", "
              " \"wSkillPoint\", \"dwGold\", \"dwSilver\", \"dwCooper\", "
              " \"wMapID\", \"wSpawnID\", \"wTemptedMon\", \"bAftermath\", "
              " \"fPosX\", \"fPosY\", \"fPosZ\", \"wDIR\") "
              "VALUES (:uid, :slot, :n, "
              " :cls, :race, :country, :ori_country, "
              " :rsex, :sex, :hair, :face, "
              " :body, :pants, :hand, :foot, "
              " :lvl, :exp, :hp, :mp, "
              " :sp, :gold, :silver, :copper, "
              " :map, :spawn, :tmp_mon, :aftermath, "
              " :px, :py, :pz, :dir) "
              "RETURNING \"dwCharID\"";
        int next_id = 0;
        sql << insert_chartable,
            soci::use(req.user_id),
            soci::use(uslot),
            soci::use(req.name),
            soci::use(uclass),
            soci::use(urace),
            soci::use(ucountry),
            soci::use(ucountry),  // bOriCountry — same as bCountry on create
            soci::use(usex),      // bRealSex
            soci::use(usex),
            soci::use(uhair),
            soci::use(uface),
            soci::use(ubody),
            soci::use(upants),
            soci::use(uhand),
            soci::use(ufoot),
            soci::use(ulevel),
            soci::use(dwEXP),
            soci::use(dwHP),
            soci::use(dwMP),
            soci::use(wSkillPoint),
            soci::use(zero),  // dwGold
            soci::use(zero),  // dwSilver
            soci::use(zero),  // dwCooper
            soci::use(wMapID),
            soci::use(wSpawnID),
            soci::use(zero),  // wTemptedMon — recall mon set later if applicable
            soci::use(zero),  // bAftermath
            soci::use(fPosX),
            soci::use(fPosY),
            soci::use(fPosZ),
            soci::use(wDIR),
            soci::into(next_id);

        // Side-table INSERTs — port of legacy TCreateChar.sql:237-269.
        // Each is best-effort: PG dev fixture doesn't ship these
        // tables (only TCHARTABLE + TITEMTABLE + TALLCHARTABLE are in
        // postgres-dev.sql), and operators may opt-out individual
        // tables in slimmed-down deployments. Real MSSQL legacy
        // schema has all of them — char creation against the real
        // schema needs every INSERT to land or the world server
        // sees a partial char (missing inventory slots / skills /
        // hotkeys / title / cabinet).

        // TINVENTABLE — two default rows (legacy slots 255 = inventory
        // tab marker, 254 = INVEN_EQUIP equipped slot).
        try
        {
            sql << "INSERT INTO \"TINVENTABLE\" "
                   "(\"dwCharID\", \"bInvenID\", \"wItemID\", \"dEndTime\") "
                   "VALUES (:c, 255, 3, 0)", soci::use(next_id);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TINVENTABLE(255) skipped: {}", ex.what());
        }
        try
        {
            sql << "INSERT INTO \"TINVENTABLE\" "
                   "(\"dwCharID\", \"bInvenID\", \"wItemID\", \"dEndTime\") "
                   "VALUES (:c, 254, 2, 0)", soci::use(next_id);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TINVENTABLE(254) skipped: {}", ex.what());
        }

        // TTITLETABLE — default title row (no title equipped).
        try
        {
            sql << "INSERT INTO \"TTITLETABLE\" "
                   "(\"dwCharID\", \"wTitleID\", \"bSelected\") "
                   "VALUES (:c, 0, 1)", soci::use(next_id);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TTITLETABLE skipped: {}", ex.what());
        }

        // TCABINETTABLE — default cabinet (storage chest) row.
        try
        {
            sql << "INSERT INTO \"TCABINETTABLE\" "
                   "(\"dwCharID\", \"bCabinetID\", \"bUse\") "
                   "VALUES (:c, 0, 1)", soci::use(next_id);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TCABINETTABLE skipped: {}", ex.what());
        }

        // TSKILLTABLE — class starter skills (TCreateChar.sql:269).
        // INSERT-SELECT from TSTARTSKILL keyed by class. Legacy SP
        // uses `(@dwCharID, wSkillID, bLevel, 0)` — the trailing 0
        // matches the 4th column in TSKILLTABLE (name unknown
        // without schema introspection; legacy SP positional binding).
        try
        {
            sql << "INSERT INTO \"TSKILLTABLE\" "
                   "SELECT :c, \"wSkillID\", \"bLevel\", 0 "
                   "FROM \"TSTARTSKILL\" WHERE \"bClassID\" = :cl",
                soci::use(next_id), soci::use(uclass);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TSKILLTABLE bulk-insert skipped: {}",
                ex.what());
        }

        // THOTKEYTABLE — class starter hotkey bars (TCreateChar.sql:316).
        // 26-column copy from TSTARTHOTKEY keyed by class.
        try
        {
            sql << "INSERT INTO \"THOTKEYTABLE\" "
                   "SELECT :c, \"bInvenID\", "
                   "       \"bType1\",  \"wID1\",  \"bType2\",  \"wID2\", "
                   "       \"bType3\",  \"wID3\",  \"bType4\",  \"wID4\", "
                   "       \"bType5\",  \"wID5\",  \"bType6\",  \"wID6\", "
                   "       \"bType7\",  \"wID7\",  \"bType8\",  \"wID8\", "
                   "       \"bType9\",  \"wID9\",  \"bType10\", \"wID10\", "
                   "       \"bType11\", \"wID11\", \"bType12\", \"wID12\" "
                   "FROM \"TSTARTHOTKEY\" WHERE \"bClassID\" = :cl",
                soci::use(next_id), soci::use(uclass);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: THOTKEYTABLE bulk-insert skipped: {}",
                ex.what());
        }

        // Starter inventory in TGAME.TITEMTABLE — dlID is composed
        // deterministically from (char_id, slot) since the column is not
        // auto-increment in the legacy schema.
        //
        // bStorageType + dwStorageID + bOwnerType MUST match what the
        // CharList SELECT filters on (lines 189-191) — otherwise the
        // items become orphans, invisible to the lobby. The CharList
        // path uses the canonical "equipped items for char" triple:
        //   bStorageType = STORAGE_INVEN = kStorageInven (= 0)
        //   dwStorageID  = INVEN_EQUIP   = kInvenEquip   (= 0xFE)
        //   bOwnerType   = TOWNER_CHAR   = kOwnerChar    (= 0)
        // Earlier impls hardcoded different values for all three on
        // INSERT (storage_type from per-item starter struct = 1,
        // dwStorageID = char_id, bOwnerType = 1) which broke the
        // round-trip — items were inserted but never surfaced.
        // StarterItem::storage_type is no longer honoured here; if
        // future starter sets need items in non-equipped slots,
        // adjust both INSERT and the CharList query in tandem.
        // dlID generation — legacy TGenerateDBItemID SP increments the
        // TDBITEMINDEXTABLE counter and returns the new value. Items
        // across all chars share this global ID space, so a deterministic
        // (char_id << 16 | slot) scheme like the pre-Phase-3 fallback
        // would eventually collide with whatever the legacy world
        // server has allocated. Use the counter when the table exists;
        // fall back to the synthetic id when it doesn't (PG dev fixture).
        auto next_dl_id = [&sql, next_id](std::int64_t fallback_seed)
            -> std::int64_t
        {
            try
            {
                // UPDATE + SELECT, scoped so cursors close between
                // statements on ODBC/MSSQL.
                sql << "UPDATE \"TDBITEMINDEXTABLE\" SET \"dlID\" = \"dlID\" + 1";
                std::int64_t id = 0;
                {
                    soci::statement st = (sql.prepare <<
                        "SELECT TOP 1 \"dlID\" FROM \"TDBITEMINDEXTABLE\"",
                        soci::into(id));
                    st.execute(true);
                    if (st.got_data()) return id;
                }
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("char.Create: TDBITEMINDEXTABLE counter "
                              "unavailable ({}) — using synthetic dlID",
                    ex.what());
            }
            // Fallback — composes a per-char/slot dlID. Stable within
            // modern's writes but can collide with legacy IDs if both
            // ever share a DB; modern operators MUST deploy
            // TDBITEMINDEXTABLE for any prod use against legacy data.
            return (static_cast<std::int64_t>(next_id) << 16) | fallback_seed;
        };

        // Phase 4 — TSTARTITEMCHART cursor (legacy TCreateChar.sql:272-287).
        // For real MSSQL deployments the legacy SP cursors through
        // TSTARTITEMCHART rows keyed by (bCountry, bClass) and calls
        // TPutItemInInven per row, which itself branches on
        // bChartType:
        //   * 1 → look up TITEMCHART.dwDuraMax, INSERT a minimal row
        //         with zeros for the per-item enchant/effect fields.
        //   * 0 → INSERT-SELECT from TQUESTITEMCHART so all 30+ stat
        //         columns get the chart's curated values.
        //
        // Both branches write into TITEMTABLE's full 35-column legacy
        // schema; PG dev fixture has only ~14 of those columns, so
        // the full INSERT throws "column does not exist" on PG and
        // we fall back to the hardcoded StarterSet (the small modern
        // set that exercises the lobby render against dev fixtures).
        // No charts present → also fall back.
        bool used_chart = false;
        // chart_available distinguishes "table missing" from "table
        // present but no rows for this (class, country) combo".
        // Real MSSQL TSTARTITEMCHART has data for bCountry 0/1/4 only;
        // a kCountryChoice=2 char gets no starter items rather than
        // falling back to the PG-only hardcoded set (which lacks the
        // legacy MSSQL NOT-NULL columns).
        bool chart_available = false;
        std::vector<std::tuple<int,int,int,int,int>> chart_rows; // bInven, bSlot, bChartType, wItemID, bCount
        try
        {
            soci::rowset<soci::row> rs = (sql.prepare <<
                "SELECT \"bInven\", \"bSlot\", \"bChartType\", \"wItemID\", \"bCount\" "
                "FROM \"TSTARTITEMCHART\" "
                "WHERE \"bCountry\" = :ct AND \"bClass\" = :cl",
                soci::use(ucountry), soci::use(uclass));
            for (const auto& r : rs)
            {
                chart_rows.emplace_back(
                    r.get<int>(0), r.get<int>(1), r.get<int>(2),
                    r.get<int>(3), r.get<int>(4));
            }
            chart_available = true;
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TSTARTITEMCHART query skipped "
                          "(class={}, country={}): {}",
                uclass, ucountry, ex.what());
        }

        for (const auto& [bInven, bSlot, bChartType, wItemID, bCount] : chart_rows)
        {
            const std::int64_t dl_id = next_dl_id(
                static_cast<std::int64_t>(used_chart ? chart_rows.size() : 0));
            try
            {
                if (bChartType == 1)
                {
                    // Look up durability from TITEMCHART. Best-effort —
                    // missing item id leaves dura_max at 0.
                    int dura_max = 0;
                    try
                    {
                        soci::statement st = (sql.prepare <<
                            "SELECT \"dwDuraMax\" FROM \"TITEMCHART\" "
                            "WHERE \"wItemID\" = :w",
                            soci::use(wItemID), soci::into(dura_max));
                        st.execute(true);
                    }
                    catch (const std::exception&) { dura_max = 0; }

                    sql << "INSERT INTO \"TITEMTABLE\" "
                           "(\"dlID\", \"bStorageType\", \"dwStorageID\", "
                           " \"bOwnerType\", \"dwOwnerID\", "
                           " \"bItemID\", \"wItemID\", \"bLevel\", "
                           " \"bCount\", \"bGLevel\", "
                           " \"dwDuraMax\", \"dwDuraCur\", \"bRefineCur\", "
                           " \"dEndTime\", \"bGradeEffect\", "
                           " \"bMagic1\", \"bMagic2\", \"bMagic3\", "
                           " \"bMagic4\", \"bMagic5\", \"bMagic6\", "
                           " \"wValue1\", \"wValue2\", \"wValue3\", "
                           " \"wValue4\", \"wValue5\", \"wValue6\", "
                           " \"dwTime1\", \"dwTime2\", \"dwTime3\", "
                           " \"dwTime4\", \"dwTime5\", \"dwTime6\", "
                           " \"bGem\", \"wMoggItemID\") "
                           "VALUES (:dl, 0, :inv, 0, :own, :slot, :w, 0, "
                           " :cnt, 0, :dura, :dura2, 0, 0, 0, "
                           " 0, 0, 0, 0, 0, 0, "
                           " 0, 0, 0, 0, 0, 0, "
                           " 0, 0, 0, 0, 0, 0, 0, 0)",
                        soci::use(dl_id),
                        soci::use(bInven),
                        soci::use(next_id),
                        soci::use(bSlot),
                        soci::use(wItemID),
                        soci::use(bCount),
                        soci::use(dura_max),
                        soci::use(dura_max);
                }
                else
                {
                    // bChartType=0: copy curated quest-item stats.
                    sql << "INSERT INTO \"TITEMTABLE\" "
                           "(\"dlID\", \"bStorageType\", \"dwStorageID\", "
                           " \"bOwnerType\", \"dwOwnerID\", "
                           " \"bItemID\", \"wItemID\", \"bLevel\", "
                           " \"bCount\", \"bGLevel\", "
                           " \"dwDuraMax\", \"dwDuraCur\", \"bRefineCur\", "
                           " \"dEndTime\", \"bGradeEffect\", "
                           " \"bMagic1\", \"bMagic2\", \"bMagic3\", "
                           " \"bMagic4\", \"bMagic5\", \"bMagic6\", "
                           " \"wValue1\", \"wValue2\", \"wValue3\", "
                           " \"wValue4\", \"wValue5\", \"wValue6\", "
                           " \"dwTime1\", \"dwTime2\", \"dwTime3\", "
                           " \"dwTime4\", \"dwTime5\", \"dwTime6\", "
                           " \"bGem\", \"wMoggItemID\") "
                           "SELECT :dl, 0, :inv, 0, :own, :slot, "
                           "       \"wItemID\", \"bLevel\", :cnt, \"bGLevel\", "
                           "       \"dwDuraMax\", \"dwDuraCur\", \"bRefineCur\", "
                           "       0, \"bGradeEffect\", "
                           "       \"bMagic1\", \"bMagic2\", \"bMagic3\", "
                           "       \"bMagic4\", \"bMagic5\", \"bMagic6\", "
                           "       \"wValue1\", \"wValue2\", \"wValue3\", "
                           "       \"wValue4\", \"wValue5\", \"wValue6\", "
                           "       \"dwTime1\", \"dwTime2\", \"dwTime3\", "
                           "       \"dwTime4\", \"dwTime5\", \"dwTime6\", "
                           "       0, 0 "
                           "FROM \"TQUESTITEMCHART\" WHERE \"dwID\" = :w",
                        soci::use(dl_id),
                        soci::use(bInven),
                        soci::use(next_id),
                        soci::use(bSlot),
                        soci::use(bCount),
                        soci::use(wItemID);
                }
                used_chart = true;
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("char.Create: TSTARTITEMCHART row "
                              "(slot={}, type={}, item={}) skipped: {}",
                    bSlot, bChartType, wItemID, ex.what());
                used_chart = false;  // bail to fallback
                break;
            }
        }

        // Fallback for dev fixtures (no TSTARTITEMCHART) — the
        // hardcoded modern StarterSet, used since Phase B. Items go
        // into the equipped slot triple (STORAGE_INVEN, INVEN_EQUIP,
        // TOWNER_CHAR) so CharList shows them. Less faithful than the
        // real chart data but lets the dev/test pipeline exercise
        // the lobby render path. Only fires when the chart QUERY
        // failed (table missing) — if the table exists but has no
        // rows for this (class, country), the char gets no starter
        // items (legacy behavior — the cursor in TCreateChar SP
        // simply iterates 0 times for combos without chart data).
        if (!used_chart && !chart_available)
        {
            std::size_t starter_n = 0;
            const auto* starter = StarterSet(req.char_class, starter_n);
            for (std::size_t i = 0; i < starter_n; ++i)
            {
                const std::int64_t dl_id = next_dl_id(static_cast<std::int64_t>(i));
                const int item_kind  = starter[i].item_kind;
                const int item_id    = starter[i].item_id;
                const int item_level = starter[i].level;
                // PG dev fixture has only ~14 of legacy MSSQL's 35
                // TITEMTABLE columns — this fallback is the PG-side
                // path. MSSQL hits the chart-based path above
                // (full schema) when TSTARTITEMCHART exists, OR
                // skips starter items entirely when the chart has
                // no rows for this (class, country) combo (legacy
                // legitimately has no starter set for some combos).
                sql << "INSERT INTO \"TITEMTABLE\" "
                       "(\"dlID\", \"bStorageType\", \"dwStorageID\", "
                       " \"bOwnerType\", \"dwOwnerID\", "
                       " \"bItemID\", \"wItemID\", \"bLevel\") "
                       "VALUES (:dl, :st, :stid, :ot, :own, :ik, :iid, :lvl)",
                    soci::use(dl_id),
                    soci::use(kStorageInven),
                    soci::use(kInvenEquip),
                    soci::use(kOwnerChar),
                    soci::use(next_id),
                    soci::use(item_kind),
                    soci::use(item_id),
                    soci::use(item_level);
            }
        }

        // Phase 5a — welcome mail. Legacy TCreateChar.sql:289-311 calls
        // TSavePost SP to drop a "Welcome to 4Story" message into the
        // new char's TPOSTTABLE inbox. Length-prefixed text format
        // (legacy `fn_sqlvarbasetostr` trick): 8-char hex length
        // prepended so the client renders the rich-text body. Modern
        // mirrors the wire format and INSERTs directly into TPOSTTABLE
        // (the SP itself adds nothing we can't do inline). Best-effort:
        // missing TPOSTTABLE = no welcome message, char still created.
        try
        {
            const std::string raw_title   = "Welcome to 4Story!";
            const std::string raw_message =
                "Welcome to 4Story,\n"
                "if you find some bugs please report them in our Forum.\n"
                "\n"
                "Your 4Story Team!";
            auto prefix_len = [](const std::string& body) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%08X",
                    static_cast<unsigned>(body.size()));
                return std::string(buf) + body;
            };
            const std::string title_w   = prefix_len(raw_title);
            const std::string message_w = prefix_len(raw_message);
            const std::string sender    = "Mysterious helper";
            const int post_type  = 0;
            const int post_read  = 0;
            const int gold       = 9999;  // matches legacy SP call
            const int silver     = 0;
            const int cooper     = 0;
            sql << "INSERT INTO \"TPOSTTABLE\" "
                   "(\"dwCharID\", \"szSender\", \"dwSendID\", \"szRecvName\", "
                   " \"szTitle\", \"szMessage\", \"bType\", \"bRead\", "
                   " \"dwGold\", \"dwSilver\", \"dwCooper\", \"timeRecv\") "
                   "VALUES (:c, :s, 0, :rn, :t, :m, :ty, :rd, "
                   "        :g, :sv, :cp, CURRENT_TIMESTAMP)",
                soci::use(next_id),
                soci::use(sender),
                soci::use(req.name),
                soci::use(title_w),
                soci::use(message_w),
                soci::use(post_type),
                soci::use(post_read),
                soci::use(gold),
                soci::use(silver),
                soci::use(cooper);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TPOSTTABLE welcome mail skipped: {}",
                ex.what());
        }

        // Phase 5b — recall mon (starter combat pet). Legacy
        // TCreateChar.sql:323-353: if TSTARTRECALL has a row for
        // (class, country), look up wMonID + wSummonAttr from
        // TMONSTERCHART, HP/MP from TMONATTRCHART, then UPDATE
        // TCHARTABLE.wTemptedMon and INSERT TRECALLMONTABLE.
        // Skipped silently when any of the supporting chart tables
        // are missing — the char loses its starter pet but is
        // otherwise functional.
        try
        {
            int recall_mon = 0;
            {
                soci::indicator ind = soci::i_null;
                soci::statement st = (sql.prepare <<
                    "SELECT \"wMonID\" FROM \"TSTARTRECALL\" "
                    "WHERE \"bClassID\" = :cl AND \"bCountryID\" = :ct",
                    soci::use(uclass), soci::use(ucountry),
                    soci::into(recall_mon, ind));
                st.execute(true);
                if (!st.got_data() || ind == soci::i_null) recall_mon = 0;
            }
            if (recall_mon > 0)
            {
                int summon_attr = 0;
                {
                    soci::statement st = (sql.prepare <<
                        "SELECT \"wSummonAttr\" FROM \"TMONSTERCHART\" "
                        "WHERE \"wID\" = :w",
                        soci::use(recall_mon),
                        soci::into(summon_attr));
                    st.execute(true);
                }
                int mon_hp = 0, mon_mp = 0;
                {
                    soci::statement st = (sql.prepare <<
                        "SELECT \"dwMaxHP\", \"dwMaxMP\" FROM \"TMONATTRCHART\" "
                        "WHERE \"wID\" = :w AND \"bLevel\" = 1",
                        soci::use(summon_attr),
                        soci::into(mon_hp), soci::into(mon_mp));
                    st.execute(true);
                }
                // Legacy: @dwATTR = @dwATTR + POWER(2,16) — flips the
                // high bit indicating "summoned by char" vs "wild".
                const int dwAttr = summon_attr + (1 << 16);
                const int pet_id = 0, skill_level = 1;
                const int pos_x = static_cast<int>(fPosX);
                const int pos_y = static_cast<int>(fPosY);
                const int pos_z = static_cast<int>(fPosZ);
                const int dwTime = 0;
                const int bLevel = 1;
                sql << "UPDATE \"TCHARTABLE\" SET \"wTemptedMon\" = :m "
                       "WHERE \"dwCharID\" = :c",
                    soci::use(recall_mon), soci::use(next_id);
                sql << "INSERT INTO \"TRECALLMONTABLE\" "
                       "(\"dwOwnerID\", \"wMonID\", \"wPetID\", \"dwATTR\", "
                       " \"bLevel\", \"dwHP\", \"dwMP\", \"bSkillLevel\", "
                       " \"wPosX\", \"wPosY\", \"wPosZ\", \"dwTime\") "
                       "VALUES (:c, :m, :p, :a, :lv, :hp, :mp, :sl, "
                       "        :px, :py, :pz, :t)",
                    soci::use(next_id),
                    soci::use(recall_mon),
                    soci::use(pet_id),
                    soci::use(dwAttr),
                    soci::use(bLevel),
                    soci::use(mon_hp),
                    soci::use(mon_mp),
                    soci::use(skill_level),
                    soci::use(pos_x), soci::use(pos_y), soci::use(pos_z),
                    soci::use(dwTime);
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: recall mon setup skipped: {}",
                ex.what());
        }

        // Phase 5c — mount (TPETTABLE). Legacy TCreateChar.sql:357-360
        // builds the mount name as "<charname>'s Mount", then runs
        // a REPLACE("s's", "s'") fixup so names ending in 's' get
        // the proper possessive ("Boss's Mount" → "Boss' Mount").
        // DELETE old wPetID=2 row (idempotency for replays) then
        // INSERT the fresh one. Per-user (not per-char), wPetID=2
        // is the mount slot legacy reserved.
        try
        {
            std::string mount_name = req.name + "'s Mount";
            for (std::size_t pos = 0;
                 (pos = mount_name.find("s's", pos)) != std::string::npos; )
            {
                mount_name.replace(pos, 3, "s'");
                pos += 2;
            }
            const int pet_mount_id = 2;
            sql << "DELETE FROM \"TPETTABLE\" "
                   "WHERE \"dwUserID\" = :u AND \"wPetID\" = :p",
                soci::use(req.user_id), soci::use(pet_mount_id);
            sql << "INSERT INTO \"TPETTABLE\" "
                   "(\"dwUserID\", \"wPetID\", \"szName\", \"timeUse\") "
                   "VALUES (:u, :p, :n, 0)",
                soci::use(req.user_id),
                soci::use(pet_mount_id),
                soci::use(mount_name);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("char.Create: TPETTABLE mount skipped: {}",
                ex.what());
        }

        // Cross-world directory row in TGLOBAL.TALLCHARTABLE. dwSeq is
        // identity; we don't need it back here.
        {
            auto glease = m_global.Acquire();
            soci::session& gsql = *glease;
            gsql << "INSERT INTO \"TALLCHARTABLE\" "
                    "(\"dwUserID\", \"bWorldID\", \"dwCharID\", \"bSlot\", \"szName\", "
                    " \"bClass\", \"bRace\", \"bCountry\", \"bSex\", \"bHair\", "
                    " \"bFace\", \"bBody\", \"bPants\", \"bHand\", \"bFoot\", "
                    " \"bLevel\", \"dwEXP\") "
                    "VALUES (:uid, :w, :cid, :slot, :n, :cls, :race, :country, "
                    " :sex, :hair, :face, :body, :pants, :hand, :foot, :lvl, 0)",
                soci::use(req.user_id),
                soci::use(uworld),
                soci::use(next_id),
                soci::use(uslot),
                soci::use(req.name),
                soci::use(uclass),
                soci::use(urace),
                soci::use(ucountry),
                soci::use(usex),
                soci::use(uhair),
                soci::use(uface),
                soci::use(ubody),
                soci::use(upants),
                soci::use(uhand),
                soci::use(ufoot),
                soci::use(ulevel);
        }

        const std::uint8_t remaining = static_cast<std::uint8_t>(
            std::max(0, kMaxCharsPerUser - (live_count + 1)));

        spdlog::info("char.Create user_id={} world={} → char_id={} "
                     "(name='{}', level={}, items_source={})",
            req.user_id, static_cast<int>(req.group_id),
            next_id, req.name, ulevel,
            used_chart ? "TSTARTITEMCHART" : "fallback-StarterSet");

        return CharacterCreateResponse{
            .status          = CreateCharResult::Success,
            .char_id         = next_id,
            .remaining_slots = remaining,
            .starting_level  = starting_level,
        };
    }
    catch (const std::exception& ex)
    {
        spdlog::error("char.Create user_id={} name='{}' DB error: {}",
            req.user_id, req.name, ex.what());
        return CharacterCreateResponse{ .status = CreateCharResult::Internal };
    }
}

DeleteCharResult
SociCharService::Delete(std::int32_t user_id,
                        std::uint8_t /*group_id*/,
                        std::int32_t char_id,
                        const std::string& /*password*/)
{
    // Guild check + owner lookup + actual delete all live in TGAME.
    auto lease = m_world.Acquire();
    soci::session& sql = *lease;

    try
    {
        int guild_hit = 0;
        sql << "SELECT COUNT(*) FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" = :c",
            soci::use(char_id), soci::into(guild_hit);
        if (guild_hit > 0)
        {
            spdlog::info("char.Delete char_id={} blocked: still in guild", char_id);
            return DeleteCharResult::Failed;
        }

        int  owner_id = 0;
        int  level    = 0;
        soci::indicator owner_ind = soci::i_null;
        bool got_row = false;
        {
            soci::statement st = (sql.prepare <<
                "SELECT \"dwUserID\", \"bLevel\" FROM \"TCHARTABLE\" "
                "WHERE \"dwCharID\" = :c AND \"bDelete\" = 0",
                soci::use(char_id),
                soci::into(owner_id, owner_ind),
                soci::into(level));
            st.execute(true);
            got_row = st.got_data();
        }
        if (!got_row || owner_ind == soci::i_null || owner_id != user_id)
        {
            return DeleteCharResult::Failed;
        }

        if (level > 5)
        {
            sql << "UPDATE \"TCHARTABLE\" SET \"bDelete\" = 1, "
                   "       \"dDeleteDate\" = CURRENT_TIMESTAMP "
                   "WHERE \"dwCharID\" = :c",
                soci::use(char_id);
        }
        else
        {
            // Hard-delete cleanup — port of legacy TGAME.TDeleteChar SP
            // body (TDeleteChar.sql:51-82). Modern only used to clean
            // TCHARTABLE + TITEMTABLE, leaving ~23 tables of orphan
            // rows (friend lists, hotkeys, skills, quests, PvP records,
            // soulmate bonds, etc.) per deleted character.
            //
            // Each cleanup is best-effort: most of these tables aren't
            // in modern dev fixtures, only in the legacy production
            // schema. A missing table throws "relation does not exist"
            // (PG) / "Invalid object name" (MSSQL) which we swallow at
            // debug level so the cleanup chain doesn't abort on the
            // first missing table.
            auto try_delete = [&sql, char_id](const char* stmt,
                                              const char* table) {
                try
                {
                    sql << stmt, soci::use(char_id);
                }
                catch (const std::exception& ex)
                {
                    spdlog::debug("char.Delete: {} cleanup skipped: {}",
                        table, ex.what());
                }
            };

            // Tables keyed by dwCharID — straightforward sweep.
            try_delete("DELETE FROM \"TPOSTTABLE\"             WHERE \"dwCharID\" = :c", "TPOSTTABLE");
            try_delete("DELETE FROM \"TFRIENDGROUPTABLE\"      WHERE \"dwCharID\" = :c", "TFRIENDGROUPTABLE");
            try_delete("DELETE FROM \"TPROTECTEDTABLE\"        WHERE \"dwCharID\" = :c", "TPROTECTEDTABLE");
            try_delete("DELETE FROM \"TSKILLTABLE\"            WHERE \"dwCharID\" = :c", "TSKILLTABLE");
            try_delete("DELETE FROM \"TINVENTABLE\"            WHERE \"dwCharID\" = :c", "TINVENTABLE");
            try_delete("DELETE FROM \"TCABINETTABLE\"          WHERE \"dwCharID\" = :c", "TCABINETTABLE");
            try_delete("DELETE FROM \"TQUESTTERMTABLE\"        WHERE \"dwCharID\" = :c", "TQUESTTERMTABLE");
            try_delete("DELETE FROM \"TQUESTTABLE\"            WHERE \"dwCharID\" = :c", "TQUESTTABLE");
            try_delete("DELETE FROM \"TRECALLMAINTAINTABLE\"   WHERE \"dwCharID\" = :c", "TRECALLMAINTAINTABLE");
            try_delete("DELETE FROM \"TSKILLMAINTAINTABLE\"    WHERE \"dwCharID\" = :c", "TSKILLMAINTAINTABLE");
            try_delete("DELETE FROM \"THOTKEYTABLE\"           WHERE \"dwCharID\" = :c", "THOTKEYTABLE");
            try_delete("DELETE FROM \"TITEMUSEDTABLE\"         WHERE \"dwCharID\" = :c", "TITEMUSEDTABLE");
            try_delete("DELETE FROM \"TEXPITEMTABLE\"          WHERE \"dwCharID\" = :c", "TEXPITEMTABLE");
            try_delete("DELETE FROM \"TCASTLEAPPLICANTTABLE\"  WHERE \"dwCharID\" = :c", "TCASTLEAPPLICANTTABLE");
            try_delete("DELETE FROM \"TDUELCHARTABLE\"         WHERE \"dwCharID\" = :c", "TDUELCHARTABLE");
            try_delete("DELETE FROM \"TDUELSCORETABLE\"        WHERE \"dwCharID\" = :c", "TDUELSCORETABLE");
            try_delete("DELETE FROM \"TPVPOINTTABLE\"          WHERE \"dwCharID\" = :c", "TPVPOINTTABLE");
            try_delete("DELETE FROM \"TPVPRECENTTABLE\"        WHERE \"dwCharID\" = :c", "TPVPRECENTTABLE");
            try_delete("DELETE FROM \"TPVPRECORDTABLE\"        WHERE \"dwCharID\" = :c", "TPVPRECORDTABLE");
            // TEMP-prefixed mirrors of the main tables — same pattern.
            try_delete("DELETE FROM \"TTEMPINVENTABLE\"            WHERE \"dwCharID\" = :c", "TTEMPINVENTABLE");
            try_delete("DELETE FROM \"TTEMPSKILLTABLE\"            WHERE \"dwCharID\" = :c", "TTEMPSKILLTABLE");
            try_delete("DELETE FROM \"TTEMPEXPITEMTABLE\"          WHERE \"dwCharID\" = :c", "TTEMPEXPITEMTABLE");
            try_delete("DELETE FROM \"TTEMPCABINETTABLE\"          WHERE \"dwCharID\" = :c", "TTEMPCABINETTABLE");
            try_delete("DELETE FROM \"TTEMPITEMUSEDTABLE\"         WHERE \"dwCharID\" = :c", "TTEMPITEMUSEDTABLE");
            try_delete("DELETE FROM \"TTEMPSKILLMAINTAINTABLE\"    WHERE \"dwCharID\" = :c", "TTEMPSKILLMAINTAINTABLE");

            // TFRIENDTABLE + TSOULMATETABLE: char appears as either
            // side of the relation. Named `:c` substitutes once per
            // SOCI binding semantics, so reuse the same param across
            // both columns.
            try
            {
                sql << "DELETE FROM \"TFRIENDTABLE\" "
                       "WHERE \"dwCharID\" = :c OR \"dwFriendID\" = :c",
                    soci::use(char_id);
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("char.Delete: TFRIENDTABLE cleanup skipped: {}",
                    ex.what());
            }
            try
            {
                sql << "DELETE FROM \"TSOULMATETABLE\" "
                       "WHERE \"dwCharID\" = :c OR \"dwTarget\" = :c",
                    soci::use(char_id);
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("char.Delete: TSOULMATETABLE cleanup skipped: {}",
                    ex.what());
            }

            // Owner-typed item tables. bOwnerType = kOwnerChar (= 0)
            // matches Create + CharList paths and legacy TOWNER_CHAR.
            try_delete("DELETE FROM \"TRECALLMONTABLE\" WHERE \"dwOwnerID\" = :c", "TRECALLMONTABLE");
            try_delete("DELETE FROM \"TTEMPITEMTABLE\" "
                       "WHERE \"dwOwnerID\" = :c AND \"bOwnerType\" = 0",
                       "TTEMPITEMTABLE");

            // Existing modern cleanups — kept inline (these tables
            // exist in dev fixtures so the outer try/catch can still
            // catch genuine errors on them).
            sql << "DELETE FROM \"TITEMTABLE\" WHERE \"dwOwnerID\" = :c "
                   "  AND \"bOwnerType\" = :ot",
                soci::use(char_id), soci::use(kOwnerChar);
            sql << "DELETE FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
                soci::use(char_id);
        }

        // TALLCHARTABLE lives in TGLOBAL — always soft-delete so audit
        // can reconstruct.
        {
            auto glease = m_global.Acquire();
            soci::session& gsql = *glease;
            gsql << "UPDATE \"TALLCHARTABLE\" SET \"bDelete\" = 1, "
                    "       \"dDeleteDate\" = CURRENT_TIMESTAMP "
                    "WHERE \"dwCharID\" = :c",
                soci::use(char_id);
        }

        spdlog::info("char.Delete char_id={} (level={}, {})",
            char_id, level, level > 5 ? "soft" : "hard");
        return DeleteCharResult::Success;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("char.Delete char_id={} DB error: {}", char_id, ex.what());
        return DeleteCharResult::Internal;
    }
}

namespace {
// Single-table shard-membership lookup shared by GetBrCharId /
// GetBowCharId. Both tables have the same shape ((dwUserID, dwCharID)),
// only the table name differs. Legacy SPs TFindBRPlayer /
// TFindBOWPlayer also share the same SELECT shape.
std::int32_t LookupShardChar(fourstory::db::SessionPool& world_pool,
                             std::int32_t user_id,
                             const char* table,
                             const char* tag)
{
    if (user_id == 0) return 0;
    auto lease = world_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        int char_id = 0;
        soci::indicator ind = soci::i_null;
        bool got = false;
        {
            // Table name is hard-coded from a compile-time constant —
            // no untrusted input, no SQL-injection surface.
            const std::string q =
                std::string("SELECT TOP 1 \"dwCharID\" FROM \"") + table +
                "\" WHERE \"dwUserID\" = :u";
            soci::statement st = (sql.prepare << q,
                soci::use(user_id),
                soci::into(char_id, ind));
            st.execute(true);
            got = st.got_data();
        }
        if (!got || ind == soci::i_null) return 0;
        return char_id;
    }
    catch (const std::exception& ex)
    {
        // Table may not exist on every world DB. Quiet failure —
        // caller treats 0 as "no shard char" which is the correct UX
        // when the feature isn't deployed.
        spdlog::debug("char.{} uid={} skipped: {}", tag, user_id, ex.what());
        return 0;
    }
}
} // namespace

std::int32_t SociCharService::GetBrCharId(std::int32_t user_id)
{
    return LookupShardChar(m_world, user_id, "TBRPLAYERTABLE", "GetBrCharId");
}

std::int32_t SociCharService::GetBowCharId(std::int32_t user_id)
{
    return LookupShardChar(m_world, user_id, "TBOWPLAYERTABLE", "GetBowCharId");
}

VeteranLevels SociCharService::GetVeteranLevels() const
{
    std::lock_guard<std::mutex> lock(m_veteran_mtx);
    std::vector<std::pair<std::uint8_t, std::uint8_t>> sorted(
        m_veteran_levels.begin(), m_veteran_levels.end());
    std::sort(sorted.begin(), sorted.end());

    VeteranLevels out{};
    if (sorted.size() > 0) out.first  = sorted[0].second;
    if (sorted.size() > 1) out.second = sorted[1].second;
    if (sorted.size() > 2) out.third  = sorted[2].second;
    return out;
}

} // namespace tloginsvr::services
