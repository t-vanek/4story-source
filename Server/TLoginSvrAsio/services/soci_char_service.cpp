#include "soci_char_service.h"
#include "fourstory/db/session_pool.h"

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
            // dwTime3 → wColor, dwTime4 → bRegGuild.
            try
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
                    it.reg_guild    = static_cast<std::uint8_t>(r.get<int>(5));
                    it.mogg_item_id = static_cast<std::uint16_t>(r.get<int>(6));
                    info.items.push_back(it);
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

            int keep_hit = 0;
            sql << "SELECT COUNT(*) FROM \"TKEEPINGNAME\" WHERE \"szName\" = :n",
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

        // TCHARTABLE.dwCharID is NOT IDENTITY in legacy schema — the SP
        // computes the next id manually. We do the same: SELECT MAX+1
        // inside the same connection. Race-safety: the world pool
        // serializes connections one-per-lease, and the MSSQL default
        // isolation level snapshots the MAX. For multi-instance writes
        // this would need a sequence table, but the login server is the
        // only writer.
        int next_id = 0;
        sql << "SELECT COALESCE(MAX(\"dwCharID\"), 0) + 1 FROM \"TCHARTABLE\"",
            soci::into(next_id);

        sql << "INSERT INTO \"TCHARTABLE\" "
               "(\"dwCharID\", \"dwUserID\", \"bSlot\", \"szNAME\", \"bClass\", \"bRace\", "
               " \"bCountry\", \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
               " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", \"bOriCountry\", "
               " \"bLevel\") "
               "VALUES (:cid, :uid, :slot, :n, :cls, :race, :country, :rsex, :sex, "
               " :hair, :face, :body, :pants, :hand, :foot, :ori_country, :lvl)",
            soci::use(next_id),
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
            soci::use(ulevel);

        // Starter inventory in TGAME.TITEMTABLE — dlID is composed
        // deterministically from (char_id, slot) since the column is not
        // auto-increment in the legacy schema.
        std::size_t starter_n = 0;
        const auto* starter = StarterSet(req.char_class, starter_n);
        for (std::size_t i = 0; i < starter_n; ++i)
        {
            const std::int64_t dl_id =
                (static_cast<std::int64_t>(next_id) << 16) | static_cast<std::int64_t>(i);
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
                soci::use(next_id),
                soci::use(next_id),
                soci::use(item_kind),
                soci::use(item_id),
                soci::use(item_level);
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
                     "(name='{}', level={}, starter_items={})",
            req.user_id, static_cast<int>(req.group_id),
            next_id, req.name, ulevel, starter_n);

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
            sql << "DELETE FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
                soci::use(char_id);
            sql << "DELETE FROM \"TITEMTABLE\" WHERE \"dwOwnerID\" = :c "
                   "  AND \"bOwnerType\" = 1",
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

std::int32_t SociCharService::GetBrCharId(std::int32_t user_id)
{
    if (user_id == 0) return 0;
    auto lease = m_world.Acquire();
    soci::session& sql = *lease;
    try
    {
        int char_id = 0;
        soci::indicator ind = soci::i_null;
        bool got = false;
        {
            // TBRPLAYERTABLE schema (per TGAME): one row per user
            // currently enrolled in BR. Legacy SP TFindBRPlayer returns
            // the dwCharID for the user_id. Same shape inlined here so
            // we don't depend on the SP existing on every world DB.
            soci::statement st = (sql.prepare <<
                "SELECT TOP 1 \"dwCharID\" FROM \"TBRPLAYERTABLE\" "
                "WHERE \"dwUserID\" = :u",
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
        // TBRPLAYERTABLE may not exist on every world DB. Quiet
        // failure — the caller treats 0 as "no BR char" which is the
        // correct UX when the feature isn't deployed.
        spdlog::debug("char.GetBrCharId uid={} skipped: {}", user_id, ex.what());
        return 0;
    }
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
