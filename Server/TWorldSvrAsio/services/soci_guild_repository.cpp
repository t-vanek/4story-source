#include "services/soci_guild_repository.h"

#include "services/guild_entities.h"

#include "fourstory/db/orm/db_context.h"
#include "fourstory/mapper/mapper.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>

namespace tworldsvr {

using fourstory::db::orm::DbContext;
using fourstory::mapper::Adapt;
using fourstory::mapper::AdaptAll;
using fourstory::mapper::AdaptTo;

std::vector<std::shared_ptr<TGuild>> SociGuildRepository::LoadAll()
{
    std::vector<std::shared_ptr<TGuild>> out;
    try
    {
        DbContext ctx(m_pool);

        // First pass: every non-disbanded guild row via the ORM, mapped
        // into a live TGuild by the Automapper. TGuild holds a mutex, so
        // it is populated in place (AdaptTo) rather than constructed by
        // value.
        std::unordered_map<std::uint32_t, std::shared_ptr<TGuild>> by_id;
        for (const auto& row : ctx.Set<GuildRow>().Where("\"bDisorg\" = 0"))
        {
            auto g = std::make_shared<TGuild>();
            AdaptTo(row, *g);
            by_id.emplace(g->id, g);
            out.push_back(std::move(g));
        }

        // Second pass: members in one rowset, dispatched into their
        // guild's members vector. One DB roundtrip total beats N+1
        // per-guild fetches.
        for (const auto& mrow : ctx.Set<GuildMemberRow>().All())
        {
            auto it = by_id.find(mrow.guild_id);
            if (it == by_id.end()) continue; // orphan row, skip
            it->second->members.push_back(Adapt<TGuildMember>(mrow));
        }

        // Third pass: PvP point reward log, newest-first. Mirrors legacy
        // CTBLGuildPvPointReward (DBAccess.h:710 — SELECT TOP 50 ... ORDER
        // BY dlDate DESC). We sort across all guilds in one rowset and cap
        // each guild's in-memory log at kPointLogMaxEntries on insert
        // (matching the CTGuild::PointLog 50-entry bound). The std::tm
        // conversion + per-guild cap don't fit generic CRUD, so this pass
        // reads through the shared session directly. Optional table — a
        // SOCI error here just leaves point_log empty (the schema
        // validator already warns at boot).
        try
        {
            soci::session& sql = ctx.Session();
            soci::rowset<soci::row> prs = (sql.prepare <<
                "SELECT \"dwGuildID\", \"szName\", \"dwPoint\", \"dlDate\" "
                "FROM \"TGUILDPVPOINTREWARDTABLE\" "
                "ORDER BY \"dlDate\" DESC");
            for (const auto& r : prs)
            {
                const auto guild_id = static_cast<std::uint32_t>(
                    r.get<int>("dwGuildID"));
                auto it = by_id.find(guild_id);
                if (it == by_id.end()) continue;
                auto& log = it->second->point_log;
                if (log.size() >= guild::kPointLogMaxEntries) continue;
                TPointRewardEntry e;
                e.recipient_name = r.get<std::string>("szName");
                e.point          = static_cast<std::uint32_t>(
                    r.get<int>("dwPoint"));
                std::tm tm = r.get<std::tm>("dlDate");
                e.date_unix = static_cast<std::int64_t>(std::mktime(&tm));
                log.push_back(std::move(e));
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("SociGuildRepository::LoadAll point_log pass "
                         "skipped: {}", ex.what());
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::LoadAll failed: {}", ex.what());
        return {};
    }
    spdlog::info("SociGuildRepository::LoadAll loaded {} guild(s)",
        out.size());
    return out;
}

std::optional<std::shared_ptr<TGuild>>
SociGuildRepository::FindById(std::uint32_t guild_id)
{
    try
    {
        DbContext ctx(m_pool);

        auto row = ctx.Set<GuildRow>().FindById(static_cast<int>(guild_id));
        if (!row) return std::nullopt;

        auto g = std::make_shared<TGuild>();
        AdaptTo(*row, *g);
        g->members = AdaptAll<TGuildMember>(
            ctx.Set<GuildMemberRow>().Where(
                "\"dwGuildID\" = " + std::to_string(guild_id)));
        return g;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::FindById({}) failed: {}",
            guild_id, ex.what());
        return std::nullopt;
    }
}

bool SociGuildRepository::SetDisorg(std::uint32_t guild_id,
                                     std::uint8_t  disorg,
                                     std::uint32_t time_unix)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDTABLE\" SET \"bDisorg\" = :d, "
               "\"dwTime\" = :t WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(disorg)),
            soci::use(static_cast<int>(time_unix)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::SetDisorg({}, {}) failed: {}",
            guild_id, disorg, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdateMemberDuty(std::uint32_t char_id,
                                            std::uint32_t guild_id,
                                            std::uint8_t  new_duty)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDMEMBERTABLE\" SET \"bDuty\" = :d "
               "WHERE \"dwCharID\" = :c AND \"dwGuildID\" = :g",
            soci::use(static_cast<int>(new_duty)),
            soci::use(static_cast<int>(char_id)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateMemberDuty({}, {}, {}) "
                      "failed: {}", char_id, guild_id, new_duty, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdateFame(std::uint32_t guild_id,
                                      std::uint32_t fame,
                                      std::uint32_t fame_color)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDTABLE\" SET \"dwFame\" = :f, "
               "\"dwFameColor\" = :c WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(fame)),
            soci::use(static_cast<int>(fame_color)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateFame({}, {}, {}) "
                      "failed: {}", guild_id, fame, fame_color, ex.what());
        return false;
    }
}

bool SociGuildRepository::RemoveMember(std::uint32_t char_id,
                                        std::uint32_t guild_id)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "DELETE FROM \"TGUILDMEMBERTABLE\" "
               "WHERE \"dwCharID\" = :c AND \"dwGuildID\" = :g",
            soci::use(static_cast<int>(char_id)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::RemoveMember({}, {}) failed: {}",
            char_id, guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::AddMember(std::uint32_t char_id,
                                     std::uint32_t guild_id,
                                     std::uint8_t  level,
                                     std::uint8_t  duty)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        // Mirror CSPGuildMemberAdd. Legacy uses MERGE / UPDATE-or-
        // INSERT semantics via the SP — we replicate by trying
        // INSERT first and falling back to UPDATE on PK conflict.
        // The bPeer column defaults to 0 (GUILD_PEER_NONE);
        // dwService starts at 0 too.
        try
        {
            sql << "INSERT INTO \"TGUILDMEMBERTABLE\" "
                   "(\"dwCharID\", \"dwGuildID\", \"bDuty\", \"bPeer\", "
                   " \"dwService\") "
                   "VALUES (:c, :g, :d, 0, 0)",
                soci::use(static_cast<int>(char_id)),
                soci::use(static_cast<int>(guild_id)),
                soci::use(static_cast<int>(duty));
        }
        catch (const std::exception&)
        {
            sql << "UPDATE \"TGUILDMEMBERTABLE\" SET "
                   "\"dwGuildID\" = :g, \"bDuty\" = :d "
                   "WHERE \"dwCharID\" = :c",
                soci::use(static_cast<int>(guild_id)),
                soci::use(static_cast<int>(duty)),
                soci::use(static_cast<int>(char_id));
        }
        (void)level;  // bLevel lives on TCHARTABLE; legacy CSP
                      // updates a cached column we don't persist.
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::AddMember({}, {}, …) failed: {}",
            char_id, guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdateMemberPeer(std::uint32_t char_id,
                                            std::uint32_t guild_id,
                                            std::uint8_t  new_peer)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDMEMBERTABLE\" SET \"bPeer\" = :p "
               "WHERE \"dwCharID\" = :c AND \"dwGuildID\" = :g",
            soci::use(static_cast<int>(new_peer)),
            soci::use(static_cast<int>(char_id)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateMemberPeer({}, {}, {}) "
                      "failed: {}", char_id, guild_id, new_peer, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdateMaxCabinet(std::uint32_t guild_id,
                                            std::uint8_t  max_cabinet)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDTABLE\" SET \"bMaxCabinet\" = :m "
               "WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(max_cabinet)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateMaxCabinet({}, {}) "
                      "failed: {}", guild_id, max_cabinet, ex.what());
        return false;
    }
}

bool SociGuildRepository::AddArticle(std::uint32_t      guild_id,
                                      std::uint32_t      article_id,
                                      std::uint8_t       duty,
                                      const std::string& writer,
                                      const std::string& title,
                                      const std::string& body,
                                      std::uint32_t      time_unix)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "INSERT INTO \"TGUILDARTICLETABLE\" (\"dwGuildID\", "
               "\"dwID\", \"bDuty\", \"szWritter\", \"szTitle\", "
               "\"szArticle\", \"dwTime\") "
               "VALUES (:g, :a, :d, :w, :t, :b, :ts)",
            soci::use(static_cast<int>(guild_id)),
            soci::use(static_cast<int>(article_id)),
            soci::use(static_cast<int>(duty)),
            soci::use(writer),
            soci::use(title),
            soci::use(body),
            soci::use(static_cast<int>(time_unix));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::AddArticle({}, {}) failed: {}",
            guild_id, article_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::DelArticle(std::uint32_t guild_id,
                                      std::uint32_t article_id)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "DELETE FROM \"TGUILDARTICLETABLE\" "
               "WHERE \"dwGuildID\" = :g AND \"dwID\" = :a",
            soci::use(static_cast<int>(guild_id)),
            soci::use(static_cast<int>(article_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::DelArticle({}, {}) failed: {}",
            guild_id, article_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdateArticle(std::uint32_t      guild_id,
                                         std::uint32_t      article_id,
                                         const std::string& title,
                                         const std::string& body)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDARTICLETABLE\" SET \"szTitle\" = :t, "
               "\"szArticle\" = :b "
               "WHERE \"dwGuildID\" = :g AND \"dwID\" = :a",
            soci::use(title),
            soci::use(body),
            soci::use(static_cast<int>(guild_id)),
            soci::use(static_cast<int>(article_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateArticle({}, {}) "
                      "failed: {}", guild_id, article_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::DeleteGuild(std::uint32_t guild_id)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        // Legacy CSPGuildDelete is a single-statement SP that assumes the
        // production schema has FK CASCADE on the guild children
        // (TGUILDMEMBERTABLE.dwGuildID, TGUILDARTICLETABLE.dwGuildID).
        // We issue explicit DELETEs in dependency order instead so dev /
        // test schemas that omit the FK CASCADE clause still get the
        // children swept. On the production schema this is two extra
        // round-trips on a cold path — negligible vs. the safety win on
        // misconfigured deploys.
        sql << "DELETE FROM \"TGUILDARTICLETABLE\" WHERE \"dwGuildID\" = :g",
            soci::use(static_cast<int>(guild_id));
        sql << "DELETE FROM \"TGUILDMEMBERTABLE\" WHERE \"dwGuildID\" = :g",
            soci::use(static_cast<int>(guild_id));
        sql << "DELETE FROM \"TGUILDTABLE\" WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::DeleteGuild({}) failed: {}",
            guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::AddWanted(std::uint32_t      guild_id,
                                     std::uint8_t       min_level,
                                     std::uint8_t       max_level,
                                     const std::string& title,
                                     const std::string& text,
                                     std::int64_t       end_time_unix)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        // Upsert: legacy CSPGuildWantedAdd is a single SP that handles
        // "row exists → UPDATE; missing → INSERT" on the DB side. We emit
        // the same shape via DELETE + INSERT to stay portable across
        // backends without MERGE syntax.
        sql << "DELETE FROM \"TGUILDWANTEDTABLE\" WHERE \"dwGuildID\" = :g",
            soci::use(static_cast<int>(guild_id));
        sql << "INSERT INTO \"TGUILDWANTEDTABLE\" (\"dwGuildID\", "
               "\"bMinLevel\", \"bMaxLevel\", \"szTitle\", \"szText\", "
               "\"dEndTime\") VALUES (:g, :mn, :mx, :t, :x, :e)",
            soci::use(static_cast<int>(guild_id)),
            soci::use(static_cast<int>(min_level)),
            soci::use(static_cast<int>(max_level)),
            soci::use(title),
            soci::use(text),
            soci::use(static_cast<int>(end_time_unix));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::AddWanted({}) failed: {}",
            guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::DeleteWanted(std::uint32_t guild_id)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "DELETE FROM \"TGUILDWANTEDTABLE\" "
               "WHERE \"dwGuildID\" = :g",
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::DeleteWanted({}) failed: {}",
            guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::AddVolunteerApp(std::uint32_t char_id,
                                           std::uint32_t wanted_id)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        // bType column is GUILDAPP_MEMBER (legacy enum constant for
        // "applying to join as a regular member"). Tactics applications
        // use a different bType in W3a-13+; we just hardcode the member
        // value here.
        constexpr int kAppTypeMember = 0;
        sql << "DELETE FROM \"TGUILDVOLUNTEERTABLE\" WHERE \"dwCharID\" = :c",
            soci::use(static_cast<int>(char_id));
        sql << "INSERT INTO \"TGUILDVOLUNTEERTABLE\" (\"dwCharID\", "
               "\"bType\", \"dwID\") VALUES (:c, :t, :w)",
            soci::use(static_cast<int>(char_id)),
            soci::use(kAppTypeMember),
            soci::use(static_cast<int>(wanted_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::AddVolunteerApp({}, {}) "
                      "failed: {}", char_id, wanted_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::DelVolunteerApp(std::uint32_t char_id)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "DELETE FROM \"TGUILDVOLUNTEERTABLE\" "
               "WHERE \"dwCharID\" = :c",
            soci::use(static_cast<int>(char_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::DelVolunteerApp({}) failed: {}",
            char_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdatePvPoints(std::uint32_t guild_id,
                                          std::uint32_t total_point,
                                          std::uint32_t useable_point,
                                          std::uint32_t month_point)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDTABLE\" SET \"dwPvPTotalPoint\" = :t, "
               "\"dwPvPUseablePoint\" = :u, \"dwPvPMonthPoint\" = :m "
               "WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(total_point)),
            soci::use(static_cast<int>(useable_point)),
            soci::use(static_cast<int>(month_point)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdatePvPoints({}) failed: {}",
            guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::UpdateLevel(std::uint32_t guild_id,
                                      std::uint8_t  level)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDTABLE\" SET \"bLevel\" = :l "
               "WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(level)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateLevel({}, {}) failed: {}",
            guild_id, level, ex.what());
        return false;
    }
}

bool SociGuildRepository::LogPointReward(std::uint32_t      guild_id,
                                         std::uint32_t      point,
                                         const std::string& recipient_name,
                                         std::uint32_t      total_point,
                                         std::uint32_t      useable_point)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        auto tr = ctx.BeginTransaction();
        sql << "INSERT INTO \"TGUILDPVPOINTREWARDTABLE\" "
               "(\"dwGuildID\", \"szName\", \"dwPoint\", \"dlDate\") "
               "VALUES (:g, :n, :p, CURRENT_TIMESTAMP)",
            soci::use(static_cast<int>(guild_id)),
            soci::use(recipient_name),
            soci::use(static_cast<int>(point));
        sql << "UPDATE \"TGUILDTABLE\" SET \"dwPvPTotalPoint\" = :t, "
               "\"dwPvPUseablePoint\" = :u WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(total_point)),
            soci::use(static_cast<int>(useable_point)),
            soci::use(static_cast<int>(guild_id));
        tr.Commit();
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::LogPointReward({}) failed: {}",
            guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::IncrementContribution(std::uint32_t char_id,
                                                 std::uint32_t guild_id,
                                                 std::uint32_t exp,
                                                 std::uint32_t gold,
                                                 std::uint32_t silver,
                                                 std::uint32_t cooper,
                                                 std::uint32_t pvp_point)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        // Guild totals delta (mirror CSPGuildContribution's arithmetic
        // side).
        sql << "UPDATE \"TGUILDTABLE\" SET "
               "\"dwGold\" = \"dwGold\" + :gold, "
               "\"dwSilver\" = \"dwSilver\" + :silver, "
               "\"dwCooper\" = \"dwCooper\" + :cooper, "
               "\"dwExp\" = \"dwExp\" + :exp, "
               "\"dwPvPTotalPoint\" = \"dwPvPTotalPoint\" + :pvp, "
               "\"dwPvPUseablePoint\" = \"dwPvPUseablePoint\" + :pvp "
               "WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(gold)),
            soci::use(static_cast<int>(silver)),
            soci::use(static_cast<int>(cooper)),
            soci::use(static_cast<int>(exp)),
            soci::use(static_cast<int>(pvp_point)),
            soci::use(static_cast<int>(guild_id));
        // Member service score delta (dwService accumulates EXP
        // contribution, mirroring legacy GainEXP path).
        sql << "UPDATE \"TGUILDMEMBERTABLE\" SET "
               "\"dwService\" = \"dwService\" + :exp "
               "WHERE \"dwCharID\" = :c AND \"dwGuildID\" = :g",
            soci::use(static_cast<int>(exp)),
            soci::use(static_cast<int>(char_id)),
            soci::use(static_cast<int>(guild_id));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::IncrementContribution({}, {}) "
                      "failed: {}", char_id, guild_id, ex.what());
        return false;
    }
}

std::optional<std::uint32_t>
SociGuildRepository::CreateGuild(const std::string& name,
                                  std::uint32_t      chief_id,
                                  std::uint8_t       country,
                                  std::int64_t       establish_time_unix)
{
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        auto tr = ctx.BeginTransaction();

        // Reject duplicate names — legacy CSPGuildEstablish returns
        // bRet=2 for this. The check + INSERT live in one transaction so
        // a concurrent attempt with the same name still fails the UNIQUE
        // constraint at INSERT time (defensive both ways).
        int existing = 0;
        sql << "SELECT COUNT(*) FROM \"TGUILDTABLE\" "
               "WHERE \"szName\" = :n",
            soci::use(name), soci::into(existing);
        if (existing != 0) return std::nullopt;

        // Portable next-id: SELECT MAX(dwID)+1. Production schemas use
        // IDENTITY but our portable migration may not, so we compute it
        // client-side under the same transaction. The legacy SP relies on
        // the SP body to pick the id too — semantically equivalent.
        int next_id = 0;
        sql << "SELECT COALESCE(MAX(\"dwID\"), 0) + 1 "
               "FROM \"TGUILDTABLE\"",
            soci::into(next_id);

        sql << "INSERT INTO \"TGUILDTABLE\" "
               "(\"dwID\", \"szName\", \"dwChief\", \"bCountry\", "
               " \"bLevel\", \"timeEstablish\") "
               "VALUES (:i, :n, :c, :y, 1, :t)",
            soci::use(next_id),
            soci::use(name),
            soci::use(static_cast<int>(chief_id)),
            soci::use(static_cast<int>(country)),
            soci::use(static_cast<long long>(establish_time_unix));
        tr.Commit();
        return static_cast<std::uint32_t>(next_id);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::CreateGuild('{}', chief={}) "
                      "failed: {}", name, chief_id, ex.what());
        return std::nullopt;
    }
}

bool SociGuildRepository::UpdateGuildFull(
    std::uint32_t                     guild_id,
    std::uint8_t                      fame,
    std::uint8_t                      guild_points,
    std::uint8_t                      level,
    std::uint8_t                      status,
    std::uint32_t                     chief_id,
    std::uint32_t                     gi,
    std::uint32_t                     exp,
    std::uint32_t                     time_unix,
    const std::vector<std::uint32_t>& alliance_ids,
    const std::vector<std::uint32_t>& enemy_ids)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "UPDATE \"TGUILDTABLE\" SET "
               "\"dwFame\" = :f, \"bGPoint\" = :gp, \"bLevel\" = :l, "
               "\"bStatus\" = :s, \"dwChief\" = :c, \"dwGI\" = :gi, "
               "\"dwExp\" = :e, \"dwTime\" = :t "
               "WHERE \"dwID\" = :g",
            soci::use(static_cast<int>(fame)),
            soci::use(static_cast<int>(guild_points)),
            soci::use(static_cast<int>(level)),
            soci::use(static_cast<int>(status)),
            soci::use(static_cast<int>(chief_id)),
            soci::use(static_cast<int>(gi)),
            soci::use(static_cast<int>(exp)),
            soci::use(static_cast<int>(time_unix)),
            soci::use(static_cast<int>(guild_id));
        // Alliance + enemy IDs are kept in memory by the handler
        // (TGuild.alliance_ids / .enemy_ids) but not persisted — the
        // legacy szAllience / szEnemy CSV columns aren't in our portable
        // schema yet (deferred to a future W5+ migration alongside the
        // rest of the war system). Log when non-empty so operators can
        // spot the mismatch if they care.
        if (!alliance_ids.empty() || !enemy_ids.empty())
        {
            spdlog::info("SociGuildRepository::UpdateGuildFull({}): "
                         "alliance_ids={} enemy_ids={} kept in memory "
                         "only (schema lacks szAllience / szEnemy)",
                guild_id, alliance_ids.size(), enemy_ids.size());
        }
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::UpdateGuildFull({}) "
                      "failed: {}", guild_id, ex.what());
        return false;
    }
}

bool SociGuildRepository::LogPvPRecord(
    std::uint32_t guild_id,
    std::uint32_t member_id,
    std::uint32_t date,
    std::uint16_t kill_count,
    std::uint16_t die_count,
    const std::array<std::uint32_t, guild::kPvPEventCount>& points)
{
    try
    {
        DbContext ctx(m_pool);
        ctx.Session() << "INSERT INTO \"TGUILDPVPRECORDTABLE\" "
               "(\"dwGuildID\", \"dwCharID\", \"dwDate\", \"wKillCount\", "
               " \"wDieCount\", "
               " \"dwPoint_1\", \"dwPoint_2\", \"dwPoint_3\", \"dwPoint_4\", "
               " \"dwPoint_5\", \"dwPoint_6\", \"dwPoint_7\", \"dwPoint_8\") "
               "VALUES (:g, :c, :d, :k, :y, :p1, :p2, :p3, :p4, "
               "        :p5, :p6, :p7, :p8)",
            soci::use(static_cast<int>(guild_id)),
            soci::use(static_cast<int>(member_id)),
            soci::use(static_cast<int>(date)),
            soci::use(static_cast<int>(kill_count)),
            soci::use(static_cast<int>(die_count)),
            soci::use(static_cast<int>(points[0])),
            soci::use(static_cast<int>(points[1])),
            soci::use(static_cast<int>(points[2])),
            soci::use(static_cast<int>(points[3])),
            soci::use(static_cast<int>(points[4])),
            soci::use(static_cast<int>(points[5])),
            soci::use(static_cast<int>(points[6])),
            soci::use(static_cast<int>(points[7]));
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::LogPvPRecord(guild={}, "
                      "member={}) failed: {}",
            guild_id, member_id, ex.what());
        return false;
    }
}

} // namespace tworldsvr
