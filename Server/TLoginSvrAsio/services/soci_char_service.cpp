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

constexpr std::uint8_t kMaxCharsPerUser = 6;  // legacy CHARSLOT_MAX

} // namespace

SociCharService::SociCharService(db::SessionPool& pool)
    : m_pool(pool)
{
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
        const bool is_mssql = (m_pool.GetBackend() == db::Backend::Odbc);
        const char* insert_char_sql = is_mssql
            ? "INSERT INTO \"TCHARTABLE\" "
              "(\"dwUserID\", \"bSlot\", \"szNAME\", \"bClass\", \"bRace\", "
              " \"bCountry\", \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
              " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", \"bOriCountry\") "
              "OUTPUT INSERTED.\"dwCharID\" "
              "VALUES (:uid, :slot, :n, :cls, :race, :country, :rsex, :sex, "
              ":hair, :face, :body, :pants, :hand, :foot, :ori_country)"
            : "INSERT INTO \"TCHARTABLE\" "
              "(\"dwUserID\", \"bSlot\", \"szNAME\", \"bClass\", \"bRace\", "
              " \"bCountry\", \"bRealSex\", \"bSex\", \"bHair\", \"bFace\", "
              " \"bBody\", \"bPants\", \"bHand\", \"bFoot\", \"bOriCountry\") "
              "VALUES (:uid, :slot, :n, :cls, :race, :country, :rsex, :sex, "
              ":hair, :face, :body, :pants, :hand, :foot, :ori_country) "
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
            soci::into(new_id);

        // Cross-world directory row.
        sql << "INSERT INTO \"TALLCHARTABLE\" "
               "(\"dwUserID\", \"bWorldID\", \"dwCharID\", \"bSlot\", \"szName\", "
               " \"bClass\", \"bRace\", \"bCountry\", \"bSex\", \"bHair\", "
               " \"bFace\", \"bBody\", \"bPants\", \"bHand\", \"bFoot\", "
               " \"bLevel\", \"dwEXP\") "
               "VALUES (:uid, :w, :cid, :slot, :n, :cls, :race, :country, "
               ":sex, :hair, :face, :body, :pants, :hand, :foot, 1, 0)",
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
            soci::use(ufoot);

        const std::uint8_t remaining = static_cast<std::uint8_t>(
            std::max(0, kMaxCharsPerUser - (live_count + 1)));

        spdlog::info("char.Create user_id={} world={} → char_id={} (name='{}')",
            req.user_id, static_cast<int>(req.group_id), new_id, req.name);

        return CharacterCreateResponse{
            .status          = CreateCharResult::Success,
            .char_id         = new_id,
            .remaining_slots = remaining,
            .starting_level  = 1,
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
        }
        else
        {
            sql << "DELETE FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
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

} // namespace tloginsvr::services
