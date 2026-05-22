#pragma once

// IGuildRepository — read/write interface to the guild persistence
// layer. Concrete impls: SociGuildRepository (TGUILDTABLE +
// TGUILDMEMBERTABLE via SOCI) and FakeGuildRepository (in-memory
// for tests).
//
// W3a-1 covers the **read path** only — LoadAll for the boot
// warmup (legacy CSPLoadGuilds) and Find by id for the demand
// path. The write path (Establish / Update / Disorg / member
// add/leave/duty/kickout) lands in W3a-2 with the matching
// handlers and the sender layer that ACKs back to the requesting
// map server.

#include "services/guild_registry.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace tworldsvr {

class IGuildRepository
{
public:
    virtual ~IGuildRepository() = default;

    // Boot warmup — read every non-disbanded guild from the
    // backing store and return a fresh, fully-populated TGuild for
    // each. Caller hands them to GuildRegistry::Insert. Heavy
    // (single-shot at boot); not on a hot path.
    //
    // Legacy parity: the bulk of CSPGetGuilds + a per-guild
    // CSPGetGuildMembers join. Modern impl fans both out in one
    // round trip.
    virtual std::vector<std::shared_ptr<TGuild>> LoadAll() = 0;

    // Demand-path lookup — called by handlers when a packet
    // references a guild_id that's not in the cache yet. Returns
    // nullopt if the row doesn't exist.
    virtual std::optional<std::shared_ptr<TGuild>> FindById(
        std::uint32_t guild_id) = 0;

    // --- W3a-4b write API -----------------------------------------
    //
    // Each method maps onto one legacy CSP wrapper (Server/TWorldSvr/
    // DBAccess.h CSPGuild*). All take the in-memory mutation as
    // already done by the calling handler — they only persist. On
    // SOCI failure they spdlog::error + return false; handlers
    // currently don't reverse the in-memory mutation (legacy
    // doesn't either — the WorkThread keeps the registry coherent
    // and the BatchThread's DB write is best-effort).

    // Update TGUILDTABLE.bDisorg + dwTime for the matching guild.
    // Mirrors CSPGuildDisorg.
    virtual bool SetDisorg(std::uint32_t guild_id, std::uint8_t disorg,
                           std::uint32_t time_unix) = 0;

    // Update TGUILDMEMBERTABLE.bDuty for one member. Mirrors
    // CSPGuildDuty.
    virtual bool UpdateMemberDuty(std::uint32_t char_id,
                                  std::uint32_t guild_id,
                                  std::uint8_t  new_duty) = 0;

    // Update TGUILDTABLE.dwFame + dwFameColor. Mirrors CSPGuildFame.
    virtual bool UpdateFame(std::uint32_t guild_id,
                            std::uint32_t fame,
                            std::uint32_t fame_color) = 0;

    // Remove a member from TGUILDMEMBERTABLE (member left or got
    // kicked). Mirrors CSPGuildLeave. Used by OnGuildLeaveAck's
    // persistence path in W3a-4b once handlers wire CoOffloadIf.
    virtual bool RemoveMember(std::uint32_t char_id,
                              std::uint32_t guild_id) = 0;

    // --- W3a-4c additions ----------------------------------------

    // Insert a TGUILDMEMBERTABLE row for a freshly-invited member.
    // Mirrors CSPGuildMemberAdd (SSHandler.cpp:3330).
    virtual bool AddMember(std::uint32_t char_id,
                           std::uint32_t guild_id,
                           std::uint8_t  level,
                           std::uint8_t  duty) = 0;

    // Apply a contribution delta to TGUILDTABLE (gold/silver/cooper/
    // exp/pvp) + TGUILDMEMBERTABLE.dwService for one member.
    // Mirrors CSPGuildContribution (SSHandler.cpp:4103) — the
    // legacy CSP takes the same args and runs the math on the DB
    // side. Returns true on a clean roundtrip even if the delta
    // gets clamped to zero by guild caps (the in-memory bookkeeping
    // already enforces those before calling us).
    virtual bool IncrementContribution(std::uint32_t char_id,
                                       std::uint32_t guild_id,
                                       std::uint32_t exp,
                                       std::uint32_t gold,
                                       std::uint32_t silver,
                                       std::uint32_t cooper,
                                       std::uint32_t pvp_point) = 0;

    // --- W3a-5 additions ----------------------------------------

    // Update TGUILDMEMBERTABLE.bPeer for one member. Mirrors
    // CSPGuildPeer (SSHandler.cpp:3551 → DM_GUILDPEER_REQ).
    virtual bool UpdateMemberPeer(std::uint32_t char_id,
                                  std::uint32_t guild_id,
                                  std::uint8_t  new_peer) = 0;

    // Update TGUILDTABLE.bMaxCabinet for the guild. Mirrors
    // CSPGuildMaxCabinet (SSHandler.cpp:4086 → DM_GUILDCABINETMAX_REQ).
    virtual bool UpdateMaxCabinet(std::uint32_t guild_id,
                                  std::uint8_t  max_cabinet) = 0;

    // --- W3a-8 article board ----------------------------------

    // Insert a row into TGUILDARTICLETABLE. Mirrors
    // CSPGuildArticleAdd (SSHandler.cpp:4220). article_id comes
    // from TGuild.article_index — caller bumps + passes in.
    virtual bool AddArticle(std::uint32_t      guild_id,
                            std::uint32_t      article_id,
                            std::uint8_t       duty,
                            const std::string& writer,
                            const std::string& title,
                            const std::string& body,
                            std::uint32_t      time_unix) = 0;

    // Delete a TGUILDARTICLETABLE row. Mirrors CSPGuildArticleDel.
    virtual bool DelArticle(std::uint32_t guild_id,
                            std::uint32_t article_id) = 0;


    // Update title + body of an existing article. Mirrors
    // CSPGuildArticleUpdate (SSHandler.cpp:4323).
    virtual bool UpdateArticle(std::uint32_t      guild_id,
                               std::uint32_t      article_id,
                               const std::string& title,
                               const std::string& body) = 0;

    // --- W3a-11 guild wanted board --------------------------------

    // Upsert into TGUILDWANTEDTABLE (legacy CSPGuildWantedAdd
    // creates a new row or updates the existing one for this
    // guild). The end_time field is computed by the handler
    // (now + kGuildWantedPeriodSec) — repo just persists.
    virtual bool AddWanted(std::uint32_t      guild_id,
                           std::uint8_t       min_level,
                           std::uint8_t       max_level,
                           const std::string& title,
                           const std::string& text,
                           std::int64_t       end_time_unix) = 0;

    // Delete the wanted row for one guild. Mirrors
    // CSPGuildWantedDel.
    virtual bool DeleteWanted(std::uint32_t guild_id) = 0;

    // --- W3a-12 volunteer applications ---------------------------

    // Insert an applicant row into TGUILDVOLUNTEERTABLE. Mirrors
    // CSPGuildVolunteerAdd (legacy uses a single SP keyed by
    // (bType=GUILDAPP_MEMBER, dwCharID, dwID)).
    virtual bool AddVolunteerApp(std::uint32_t char_id,
                                 std::uint32_t wanted_id) = 0;

    // Delete the applicant row for one char.
    virtual bool DelVolunteerApp(std::uint32_t char_id) = 0;

    // --- W3a-10 guild lifecycle (extinction) ---------------------

    // Delete the guild row + sweep the children. Mirrors
    // CSPGuildDelete (SSHandler.cpp:3290) — the legacy SP is a
    // single DELETE on TGUILDTABLE that assumes the production
    // schema has FK CASCADE on TGUILDMEMBERTABLE +
    // TGUILDARTICLETABLE. The SOCI impl issues explicit DELETEs
    // in dependency order so dev / test schemas without the FK
    // CASCADE clause still cleanup the children.
    virtual bool DeleteGuild(std::uint32_t guild_id) = 0;
};

} // namespace tworldsvr
