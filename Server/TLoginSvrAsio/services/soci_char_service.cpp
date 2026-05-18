#include "soci_char_service.h"
#include "../db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <algorithm>

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
constexpr std::int32_t kClassCount         = 6;  // NetCode.h: TCLASS_COUNT (WARRIOR..SORCERER)

// Starter inventory per class — placeholder mapping. The legacy SP
// CSPCreateChar inserts the real starter inventory and we don't have
// its source here; these item IDs are tuned to NOT collide with the
// 16-bit reserved ranges, but they're meant to be replaced once the
// prod item catalog is wired. Each tuple is (bItemID, wItemID, bLevel).
//
// Wired now because the wire-format CHARLIST ack expects bEquipCount + N
// equipment rows; without something here new chars show as naked which
// confuses the client. Treat as scaffolding, not gameplay-tuned.
struct StarterItem { std::int16_t storage_type; std::int16_t item_kind; std::int16_t item_id; std::int16_t level; };
const StarterItem* StarterSet(std::uint8_t char_class, std::size_t& out_n)
{
    // bStorageType 1 = equipped (legacy ITEMSTOR_EQUIP); 2 = inventory.
    // bItemID is the slot/category (legacy TItem.h enum), wItemID is
    // the specific item template id.
    static const StarterItem warrior[] = {
        { 1, 1,   1, 1 },  // sword (slot=weapon)
        { 1, 5,  10, 1 },  // tunic (slot=body)
    };
    static const StarterItem ranger[] = {
        { 1, 1,   2, 1 },  // dagger
        { 1, 5,  10, 1 },
    };
    static const StarterItem archer[] = {
        { 1, 1,   3, 1 },  // bow
        { 1, 5,  10, 1 },
    };
    static const StarterItem wizard[] = {
        { 1, 1,   4, 1 },  // staff
        { 1, 5,  11, 1 },  // robe
    };
    static const StarterItem priest[] = {
        { 1, 1,   4, 1 },
        { 1, 5,  11, 1 },
    };
    static const StarterItem sorcerer[] = {
        { 1, 1,   4, 1 },
        { 1, 5,  11, 1 },
    };
    switch (char_class)
    {
    case 0: out_n = std::size(warrior);  return warrior;
    case 1: out_n = std::size(ranger);   return ranger;
    case 2: out_n = std::size(archer);   return archer;
    case 3: out_n = std::size(wizard);   return wizard;
    case 4: out_n = std::size(priest);   return priest;
    case 5: out_n = std::size(sorcerer); return sorcerer;
    default:
        out_n = 0;
        return nullptr;
    }
}

} // namespace

SociCharService::SociCharService(db::SessionPool& pool)
    : m_pool(pool)
{
    // Best-effort TVETERANCHART load. Empty / missing is OK — chars
    // then just get the country-based starting level.
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        int opt = 0, lvl = 0;
        soci::statement st = (sql.prepare <<
            "SELECT \"bID\", \"bLevel\" FROM \"TVETERANCHART\"",
            soci::into(opt), soci::into(lvl));
        st.execute();
        while (st.fetch())
        {
            m_veteran_levels[static_cast<std::uint8_t>(opt)] =
                static_cast<std::uint8_t>(lvl);
        }
        spdlog::info("char_service: loaded {} veteran-chart row(s)",
            m_veteran_levels.size());
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("char_service: TVETERANCHART unreadable ({}) — "
                     "veteran bonus disabled",
            ex.what());
    }
}

std::vector<CharacterInfo>
SociCharService::List(std::int32_t user_id, std::uint8_t group_id)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Join TCHARTABLE to TALLCHARTABLE so we can filter by world.
        // In legacy DB-per-world deployments the join is implicit (one
        // DB per world), here it's explicit so a single PG/MSSQL works
        // for multi-world setups.
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT c.\"dwCharID\", c.\"szNAME\", c.\"bSlot\", c.\"bLevel\","
            "       c.\"bClass\", c.\"bRace\", c.\"bCountry\", c.\"bSex\","
            "       c.\"bHair\", c.\"bFace\", c.\"bBody\", c.\"bPants\","
            "       c.\"bHand\", c.\"bFoot\", c.\"bHelmetHide\","
            "       c.\"dwRegion\", c.\"bStartAct\" "
            "FROM \"TCHARTABLE\" c "
            "JOIN \"TALLCHARTABLE\" a ON a.\"dwCharID\" = c.\"dwCharID\" "
            "WHERE c.\"dwUserID\" = :uid "
            "  AND a.\"bWorldID\" = :w "
            "  AND c.\"bDelete\" = 0 "
            "ORDER BY c.\"bSlot\"",
            soci::use(user_id), soci::use(static_cast<int>(group_id)));

        std::vector<CharacterInfo> out;
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
        return out;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("char.List(uid={}, group={}) DB error: {}",
            user_id, static_cast<int>(group_id), ex.what());
        return {};
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

    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Name conflict: check live row in TCHARTABLE, plus the
        // reservation tables. Each is a separate small SELECT so the
        // DR_DuplicateName reason stays distinguishable in the log.
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

        int reserved_hit = 0;
        sql << "SELECT COUNT(*) FROM \"TRESERVEDNAME\" WHERE \"szName\" = :n",
            soci::use(req.name), soci::into(reserved_hit);
        if (reserved_hit > 0)
        {
            spdlog::info("char.Create rejected (reserved) name='{}'", req.name);
            return CharacterCreateResponse{ .status = CreateCharResult::Protected };
        }

        int keep_hit = 0;
        sql << "SELECT COUNT(*) FROM \"TKEEPINGNAME\" WHERE \"szName\" = :n",
            soci::use(req.name), soci::into(keep_hit);
        if (keep_hit > 0)
        {
            spdlog::info("char.Create rejected (kept) name='{}'", req.name);
            return CharacterCreateResponse{ .status = CreateCharResult::Protected };
        }

        // Slot availability — TCHARTABLE is the source of truth for live
        // chars; soft-deleted rows free their slot up.
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

        // INSERT TCHARTABLE, capture identity dwCharID via the
        // backend-appropriate clause: PG uses trailing RETURNING,
        // MSSQL uses OUTPUT INSERTED.col between the column list and
        // VALUES. SOCI's into() consumes the single-column result the
        // same way either way.
        //
        // Distinct placeholders for each column — SOCI's positional
        // binding tracks soci::use() calls one-to-one with the literal
        // placeholders in the query string, so repeating `:country`
        // would bind 15 values to 14 slots and silently corrupt the
        // INSERT.
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

        // Starting level: country picks the floor (PEACE=1, others=9),
        // veteran-bonus option may raise it. Same precedence as legacy
        // CSHandler.cpp:1083-1087 + SP's country-vs-CHOICE_COUNTRY_LEVEL
        // branch — the SP stamps the country-based level into TCHARTABLE
        // and the C++ overlays the veteran-chosen level for the ACK.
        std::uint8_t starting_level = (req.country == kCountryPeace)
            ? kPeaceStartLevel
            : kChoiceStartLevel;
        if (auto it = m_veteran_levels.find(req.level_option);
            it != m_veteran_levels.end() && it->second > starting_level)
        {
            starting_level = it->second;
        }
        const int ulevel = static_cast<int>(starting_level);

        const bool is_mssql = (m_pool.GetBackend() == db::Backend::Odbc);
        const char* insert_char_sql = is_mssql
            ? "INSERT INTO \"TCHARTABLE\" "
              "(\"dwUserID\", \"bSlot\", \"szNAME\", \"bClass\", \"bRace\", "
              " \"bCountry\", \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
              " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", \"bOriCountry\", "
              " \"bLevel\") "
              "OUTPUT INSERTED.\"dwCharID\" "
              "VALUES (:uid, :slot, :n, :cls, :race, :country, :rsex, :sex, "
              ":hair, :face, :body, :pants, :hand, :foot, :ori_country, :lvl)"
            : "INSERT INTO \"TCHARTABLE\" "
              "(\"dwUserID\", \"bSlot\", \"szNAME\", \"bClass\", \"bRace\", "
              " \"bCountry\", \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
              " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", \"bOriCountry\", "
              " \"bLevel\") "
              "VALUES (:uid, :slot, :n, :cls, :race, :country, :rsex, :sex, "
              ":hair, :face, :body, :pants, :hand, :foot, :ori_country, :lvl) "
              "RETURNING \"dwCharID\"";
        int new_id = 0;
        sql << insert_char_sql,
            soci::use(req.user_id),
            soci::use(uslot),
            soci::use(req.name),
            soci::use(uclass),
            soci::use(urace),
            soci::use(ucountry),
            soci::use(usex),   // bRealSex
            soci::use(usex),
            soci::use(uhair),
            soci::use(uface),
            soci::use(ubody),
            soci::use(upants),
            soci::use(uhand),
            soci::use(ufoot),
            soci::use(ucountry),  // bOriCountry — same as bCountry on create
            soci::use(ulevel),
            soci::into(new_id);

        // Cross-world directory row.
        sql << "INSERT INTO \"TALLCHARTABLE\" "
               "(\"dwUserID\", \"bWorldID\", \"dwCharID\", \"bSlot\", \"szName\", "
               " \"bClass\", \"bRace\", \"bCountry\", \"bSex\", \"bHair\", "
               " \"bFace\", \"bBody\", \"bPants\", \"bHand\", \"bFoot\", "
               " \"bLevel\", \"dwEXP\") "
               "VALUES (:uid, :w, :cid, :slot, :n, :cls, :race, :country, "
               ":sex, :hair, :face, :body, :pants, :hand, :foot, :lvl, 0)",
            soci::use(req.user_id),
            soci::use(uworld),
            soci::use(new_id),
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

        // Starter inventory. The legacy SP CSPCreateChar inserts the
        // gameplay-tuned starter set; the static table here is a
        // placeholder so a fresh char isn't naked in the lobby. dlID is
        // composed deterministically from (char_id, slot) — TITEMTABLE
        // doesn't auto-increment in our schema (mirrors legacy MSSQL).
        std::size_t starter_n = 0;
        const auto* starter = StarterSet(req.char_class, starter_n);
        for (std::size_t i = 0; i < starter_n; ++i)
        {
            const std::int64_t dl_id =
                (static_cast<std::int64_t>(new_id) << 16) | static_cast<std::int64_t>(i);
            const int store_type = starter[i].storage_type;
            const int item_kind  = starter[i].item_kind;
            const int item_id    = starter[i].item_id;
            const int item_level = starter[i].level;
            sql << "INSERT INTO \"TITEMTABLE\" "
                   "(\"dlID\", \"bStorageType\", \"dwStorageID\", "
                   " \"bOwnerType\", \"dwOwnerID\", "
                   " \"bItemID\", \"wItemID\", \"bLevel\") "
                   "VALUES (:dl, :st, :stid, 1, :own, :ik, :iid, :lvl)",
                soci::use(dl_id),
                soci::use(store_type),
                soci::use(new_id),
                soci::use(new_id),
                soci::use(item_kind),
                soci::use(item_id),
                soci::use(item_level);
        }

        const std::uint8_t remaining = static_cast<std::uint8_t>(
            std::max(0, kMaxCharsPerUser - (live_count + 1)));

        spdlog::info("char.Create user_id={} world={} → char_id={} "
                     "(name='{}', level={}, starter_items={})",
            req.user_id, static_cast<int>(req.group_id),
            new_id, req.name, ulevel, starter_n);

        return CharacterCreateResponse{
            .status          = CreateCharResult::Success,
            .char_id         = new_id,
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
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Guild block — legacy TDeleteChar returns FAILED if the char
        // is still in a guild. Player must leave first.
        int guild_hit = 0;
        sql << "SELECT COUNT(*) FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" = :c",
            soci::use(char_id), soci::into(guild_hit);
        if (guild_hit > 0)
        {
            spdlog::info("char.Delete char_id={} blocked: still in guild",
                char_id);
            return DeleteCharResult::Failed;
        }

        // Confirm ownership + read level for the soft-vs-hard split.
        // Also confirms the char actually exists alive. The statement
        // is scoped so its cursor closes before the next UPDATE/DELETE
        // runs on the same connection — required for ODBC/MSSQL.
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

        // > level 5 → soft delete: keep row for audit/restore window.
        // ≤ level 5 → hard delete: row removed entirely (legacy behavior
        // since low-level chars accumulate fast and clutter the table).
        if (level > 5)
        {
            sql << "UPDATE \"TCHARTABLE\" SET \"bDelete\" = 1, "
                   "       \"dDeleteDate\" = CURRENT_TIMESTAMP "
                   "WHERE \"dwCharID\" = :c",
                soci::use(char_id);
            // Soft path: leave TITEMTABLE rows intact so a restore
            // (legacy GM operation) can return the inventory.
        }
        else
        {
            sql << "DELETE FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
                soci::use(char_id);
            // Hard path: scrub the inventory too, otherwise orphan
            // TITEMTABLE rows accumulate forever (no FK in legacy
            // schema; this service is the only enforcement).
            sql << "DELETE FROM \"TITEMTABLE\" WHERE \"dwOwnerID\" = :c "
                   "  AND \"bOwnerType\" = 1",
                soci::use(char_id);
        }

        // TALLCHARTABLE always gets the soft-delete marker — audit row
        // stays even after hard-delete of TCHARTABLE.
        sql << "UPDATE \"TALLCHARTABLE\" SET \"bDelete\" = 1, "
               "       \"dDeleteDate\" = CURRENT_TIMESTAMP "
               "WHERE \"dwCharID\" = :c",
            soci::use(char_id);

        spdlog::info("char.Delete char_id={} (level={}, {})",
            char_id, level, level > 5 ? "soft" : "hard");
        return DeleteCharResult::Success;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("char.Delete char_id={} DB error: {}",
            char_id, ex.what());
        return DeleteCharResult::Internal;
    }
}

VeteranLevels SociCharService::GetVeteranLevels() const
{
    // m_veteran_levels is keyed by bID (the option index). The wire
    // ack returns three slots; sort by id and take the first three so
    // the result is deterministic across DB row order.
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
