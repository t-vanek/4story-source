#include "services/soci_guild_repository.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <ctime>
#include <unordered_map>

namespace tworldsvr {

namespace {

// Map one row's columns onto the TGuild fields. SOCI's row API
// uses 0-based numeric indices; the SELECT below pins the order so
// the indices stay stable.
void FillFromRow(const soci::row& r, TGuild& g)
{
    g.id                = static_cast<std::uint32_t>(r.get<int>("dwID"));
    g.name              = r.get<std::string>("szName");
    g.chief_char_id     = static_cast<std::uint32_t>(r.get<int>("dwChief"));
    g.level             = static_cast<std::uint8_t>(r.get<int>("bLevel"));
    g.fame              = static_cast<std::uint32_t>(r.get<int>("dwFame"));
    g.fame_color        = static_cast<std::uint32_t>(r.get<int>("dwFameColor"));
    g.max_cabinet       = static_cast<std::uint8_t>(r.get<int>("bMaxCabinet"));
    g.gold              = static_cast<std::uint32_t>(r.get<int>("dwGold"));
    g.silver            = static_cast<std::uint32_t>(r.get<int>("dwSilver"));
    g.cooper            = static_cast<std::uint32_t>(r.get<int>("dwCooper"));
    g.gi                = static_cast<std::uint32_t>(r.get<int>("dwGI"));
    g.exp               = static_cast<std::uint32_t>(r.get<int>("dwExp"));
    g.guild_points      = static_cast<std::uint8_t>(r.get<int>("bGPoint"));
    g.status            = static_cast<std::uint8_t>(r.get<int>("bStatus"));
    g.disorg            = static_cast<std::uint8_t>(r.get<int>("bDisorg"));
    g.disorg_time       = static_cast<std::uint32_t>(r.get<int>("dwTime"));
    g.pvp_total_point   = static_cast<std::uint32_t>(r.get<int>("dwPvPTotalPoint"));
    g.pvp_useable_point = static_cast<std::uint32_t>(r.get<int>("dwPvPUseablePoint"));

    // timeEstablish is SMALLDATETIME on the legacy MSSQL side. SOCI
    // surfaces it as std::tm; convert to time_t for storage. On
    // backends where the column is missing or NULL, fall back to 0
    // — the legacy module treats `m_timeEstablish == 0` as "unknown
    // start date" and renders blank in the guild-info reply.
    try
    {
        auto tm = r.get<std::tm>("timeEstablish");
        g.establish_time = static_cast<std::int64_t>(std::mktime(&tm));
    }
    catch (const std::exception&)
    {
        g.establish_time = 0;
    }
}

std::vector<TGuildMember> LoadMembersFor(soci::session& sql,
                                          std::uint32_t guild_id)
{
    std::vector<TGuildMember> out;
    soci::rowset<soci::row> rs = (sql.prepare <<
        "SELECT \"dwCharID\", \"dwGuildID\", \"bDuty\", \"bPeer\", "
        "\"dwService\" "
        "FROM \"TGUILDMEMBERTABLE\" WHERE \"dwGuildID\" = :gid",
        soci::use(static_cast<int>(guild_id)));
    for (const auto& r : rs)
    {
        TGuildMember m;
        m.char_id  = static_cast<std::uint32_t>(r.get<int>("dwCharID"));
        m.guild_id = static_cast<std::uint32_t>(r.get<int>("dwGuildID"));
        m.duty     = static_cast<std::uint8_t>(r.get<int>("bDuty"));
        m.peer     = static_cast<std::uint8_t>(r.get<int>("bPeer"));
        m.service  = static_cast<std::uint32_t>(r.get<int>("dwService"));
        out.push_back(std::move(m));
    }
    return out;
}

} // namespace

std::vector<std::shared_ptr<TGuild>> SociGuildRepository::LoadAll()
{
    std::vector<std::shared_ptr<TGuild>> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;

        // First pass: every non-disbanded guild row.
        std::unordered_map<std::uint32_t, std::shared_ptr<TGuild>> by_id;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwID\", \"szName\", \"dwChief\", \"bLevel\", "
            "\"dwFame\", \"dwFameColor\", \"bMaxCabinet\", \"dwGold\", "
            "\"dwSilver\", \"dwCooper\", \"dwGI\", \"dwExp\", \"bGPoint\", "
            "\"bStatus\", \"bDisorg\", \"dwTime\", \"timeEstablish\", "
            "\"dwPvPTotalPoint\", \"dwPvPUseablePoint\" "
            "FROM \"TGUILDTABLE\" WHERE \"bDisorg\" = 0");
        for (const auto& r : rs)
        {
            auto g = std::make_shared<TGuild>();
            FillFromRow(r, *g);
            by_id.emplace(g->id, g);
            out.push_back(std::move(g));
        }

        // Second pass: members in one rowset, dispatched into
        // their guild's members vector. One DB roundtrip total
        // beats N+1 per-guild fetches.
        soci::rowset<soci::row> mrs = (sql.prepare <<
            "SELECT \"dwCharID\", \"dwGuildID\", \"bDuty\", \"bPeer\", "
            "\"dwService\" FROM \"TGUILDMEMBERTABLE\"");
        for (const auto& r : mrs)
        {
            const auto guild_id = static_cast<std::uint32_t>(
                r.get<int>("dwGuildID"));
            auto it = by_id.find(guild_id);
            if (it == by_id.end()) continue; // orphan row, skip
            TGuildMember m;
            m.char_id  = static_cast<std::uint32_t>(r.get<int>("dwCharID"));
            m.guild_id = guild_id;
            m.duty     = static_cast<std::uint8_t>(r.get<int>("bDuty"));
            m.peer     = static_cast<std::uint8_t>(r.get<int>("bPeer"));
            m.service  = static_cast<std::uint32_t>(r.get<int>("dwService"));
            it->second->members.push_back(std::move(m));
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

bool SociGuildRepository::SetDisorg(std::uint32_t guild_id,
                                     std::uint8_t  disorg,
                                     std::uint32_t time_unix)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "UPDATE \"TGUILDTABLE\" SET \"bDisorg\" = :d, "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "UPDATE \"TGUILDMEMBERTABLE\" SET \"bDuty\" = :d "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "UPDATE \"TGUILDTABLE\" SET \"dwFame\" = :f, "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TGUILDMEMBERTABLE\" "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "UPDATE \"TGUILDMEMBERTABLE\" SET \"bPeer\" = :p "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "UPDATE \"TGUILDTABLE\" SET \"bMaxCabinet\" = :m "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "INSERT INTO \"TGUILDARTICLETABLE\" (\"dwGuildID\", "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TGUILDARTICLETABLE\" "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        sql << "UPDATE \"TGUILDARTICLETABLE\" SET \"szTitle\" = :t, "
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        // Cascade-delete the related rows ourselves — the legacy
        // CSPGuildDelete relies on FK CASCADE which not every
        // deployed schema has. Explicit DELETEs make the behavior
        // independent of FK configuration.
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
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        // Guild totals delta (mirror CSPGuildContribution's
        // arithmetic side).
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

std::optional<std::shared_ptr<TGuild>>
SociGuildRepository::FindById(std::uint32_t guild_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::indicator ind = soci::i_null;
        soci::row r;
        sql << "SELECT \"dwID\", \"szName\", \"dwChief\", \"bLevel\", "
               "\"dwFame\", \"dwFameColor\", \"bMaxCabinet\", \"dwGold\", "
               "\"dwSilver\", \"dwCooper\", \"dwGI\", \"dwExp\", \"bGPoint\", "
               "\"bStatus\", \"bDisorg\", \"dwTime\", \"timeEstablish\", "
               "\"dwPvPTotalPoint\", \"dwPvPUseablePoint\" "
               "FROM \"TGUILDTABLE\" WHERE \"dwID\" = :gid",
            soci::use(static_cast<int>(guild_id)),
            soci::into(r, ind);
        if (!sql.got_data()) return std::nullopt;
        (void)ind;

        auto g = std::make_shared<TGuild>();
        FillFromRow(r, *g);
        g->members = LoadMembersFor(sql, guild_id);
        return g;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociGuildRepository::FindById({}) failed: {}",
            guild_id, ex.what());
        return std::nullopt;
    }
}

} // namespace tworldsvr
