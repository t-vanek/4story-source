#pragma once

// Guild DB-row entities + fourstory::db::orm::EntityMapping<> specializations.
//
// These are the *persistence shape* of the guild tables — one struct per
// table row, mapped to/from SOCI by the generic Repository<T> in the ORM
// framework (Lib/Own/FourStoryCommon/fourstory/db/orm/). The *domain shape*
// (TGuild / TGuildMember in guild_registry.h) is reached from these via the
// Automapper profile in mapper_profiles.h — so the wire/registry types never
// touch SOCI directly.
//
//   TGUILDTABLE        row  ── GuildRow        ─(Adapt)→ TGuild
//   TGUILDMEMBERTABLE  row  ── GuildMemberRow  ─(Adapt)→ TGuildMember
//
// Read path only: SELECT-shaped FromRow + SelectAllSql/SelectByIdSql are
// implemented so Repository<GuildRow>::Where()/FindById() work. The write
// paths (partial-column UPDATEs, additive deltas, the bespoke CreateGuild
// transaction) don't fit generic full-row CRUD, so SociGuildRepository
// issues those through DbContext::Session() directly — same precedent as
// TLogSvrAsio's AuditQueryRepository (read via Repository, write raw).

#include "fourstory/db/orm/entity_mapping.h"

#include <soci/soci.h>

#include <cstdint>
#include <ctime>
#include <string>

namespace tworldsvr {

// One TGUILDTABLE row — the 19 columns the boot warmup / demand lookup read.
struct GuildRow
{
    std::uint32_t id                = 0;   // dwID (PK)
    std::string   name;                    // szName
    std::uint32_t chief             = 0;   // dwChief
    std::uint8_t  level             = 0;   // bLevel
    std::uint32_t fame              = 0;   // dwFame
    std::uint32_t fame_color        = 0;   // dwFameColor
    std::uint8_t  max_cabinet       = 0;   // bMaxCabinet
    std::uint32_t gold              = 0;   // dwGold
    std::uint32_t silver            = 0;   // dwSilver
    std::uint32_t cooper            = 0;   // dwCooper
    std::uint32_t gi                = 0;   // dwGI
    std::uint32_t exp               = 0;   // dwExp
    std::uint8_t  guild_points      = 0;   // bGPoint
    std::uint8_t  status            = 0;   // bStatus
    std::uint8_t  disorg            = 0;   // bDisorg
    std::uint32_t disorg_time       = 0;   // dwTime
    std::int64_t  establish_time    = 0;   // timeEstablish (SMALLDATETIME → epoch)
    std::uint32_t pvp_total_point   = 0;   // dwPvPTotalPoint
    std::uint32_t pvp_useable_point = 0;   // dwPvPUseablePoint
};

// One TGUILDMEMBERTABLE row.
struct GuildMemberRow
{
    std::uint32_t char_id  = 0;   // dwCharID (PK)
    std::uint32_t guild_id = 0;   // dwGuildID
    std::uint8_t  duty     = 0;   // bDuty
    std::uint8_t  peer     = 0;   // bPeer
    std::uint32_t service  = 0;   // dwService
};

} // namespace tworldsvr

namespace fourstory::db::orm {

template<>
struct EntityMapping<tworldsvr::GuildRow>
{
    using T = tworldsvr::GuildRow;

    static constexpr const char* Table    = "TGUILDTABLE";
    static constexpr const char* PkColumn = "dwID";

    static T FromRow(const soci::row& r)
    {
        T g;
        g.id                = static_cast<std::uint32_t>(r.get<int>("dwID"));
        g.name              = r.get<std::string>("szName");
        g.chief             = static_cast<std::uint32_t>(r.get<int>("dwChief"));
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

        // timeEstablish is SMALLDATETIME on the legacy MSSQL side; SOCI
        // surfaces it as std::tm. A missing / NULL column leaves the
        // legacy "unknown start date" sentinel of 0 (blank in the UI).
        try
        {
            auto tm = r.get<std::tm>("timeEstablish");
            g.establish_time = static_cast<std::int64_t>(std::mktime(&tm));
        }
        catch (const std::exception&)
        {
            g.establish_time = 0;
        }
        return g;
    }

    static std::string SelectAllSql()
    {
        return "SELECT \"dwID\", \"szName\", \"dwChief\", \"bLevel\", "
               "\"dwFame\", \"dwFameColor\", \"bMaxCabinet\", \"dwGold\", "
               "\"dwSilver\", \"dwCooper\", \"dwGI\", \"dwExp\", \"bGPoint\", "
               "\"bStatus\", \"bDisorg\", \"dwTime\", \"timeEstablish\", "
               "\"dwPvPTotalPoint\", \"dwPvPUseablePoint\" "
               "FROM \"TGUILDTABLE\"";
    }
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE \"dwID\" = :pk";
    }
    static std::string DeleteSql()
    {
        return "DELETE FROM \"TGUILDTABLE\" WHERE \"dwID\" = :pk";
    }

    // Write path goes through SociGuildRepository's targeted SQL /
    // CreateGuild transaction (partial-column + bespoke id assignment
    // don't fit generic full-row CRUD). Same read-mostly contract as
    // TLogSvrAsio's LogAuditEntry mapping.
    static std::string InsertSql() { return ""; }
    static std::string UpdateSql() { return ""; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static int  GetPk(const T& g) { return static_cast<int>(g.id); }
};

template<>
struct EntityMapping<tworldsvr::GuildMemberRow>
{
    using T = tworldsvr::GuildMemberRow;

    static constexpr const char* Table    = "TGUILDMEMBERTABLE";
    static constexpr const char* PkColumn = "dwCharID";

    static T FromRow(const soci::row& r)
    {
        T m;
        m.char_id  = static_cast<std::uint32_t>(r.get<int>("dwCharID"));
        m.guild_id = static_cast<std::uint32_t>(r.get<int>("dwGuildID"));
        m.duty     = static_cast<std::uint8_t>(r.get<int>("bDuty"));
        m.peer     = static_cast<std::uint8_t>(r.get<int>("bPeer"));
        m.service  = static_cast<std::uint32_t>(r.get<int>("dwService"));
        return m;
    }

    static std::string SelectAllSql()
    {
        return "SELECT \"dwCharID\", \"dwGuildID\", \"bDuty\", \"bPeer\", "
               "\"dwService\" FROM \"TGUILDMEMBERTABLE\"";
    }
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE \"dwCharID\" = :pk";
    }
    static std::string DeleteSql()
    {
        return "DELETE FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" = :pk";
    }

    static std::string InsertSql() { return ""; }
    static std::string UpdateSql() { return ""; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static int  GetPk(const T& m) { return static_cast<int>(m.char_id); }
};

} // namespace fourstory::db::orm
