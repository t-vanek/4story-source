#pragma once

// SociGuildRepository — IGuildRepository backed by SOCI against
// TGUILDTABLE + TGUILDMEMBERTABLE. The legacy module fans LoadAll
// out as `CSPLoadGuilds` + per-guild `CSPGetGuildMembers`; this
// implementation does the equivalent in two batched queries
// (SELECT * FROM TGUILDTABLE, then SELECT * FROM TGUILDMEMBERTABLE
// joined back in memory by guild_id) which scales better for the
// ~100..1000-guild server population than the per-guild fan-out.

#include "services/guild_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociGuildRepository : public IGuildRepository
{
public:
    explicit SociGuildRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<std::shared_ptr<TGuild>> LoadAll() override;
    std::optional<std::shared_ptr<TGuild>> FindById(
        std::uint32_t guild_id) override;

    bool SetDisorg(std::uint32_t guild_id, std::uint8_t disorg,
                   std::uint32_t time_unix) override;
    bool UpdateMemberDuty(std::uint32_t char_id, std::uint32_t guild_id,
                          std::uint8_t new_duty) override;
    bool UpdateFame(std::uint32_t guild_id, std::uint32_t fame,
                    std::uint32_t fame_color) override;
    bool RemoveMember(std::uint32_t char_id, std::uint32_t guild_id) override;
    bool AddMember(std::uint32_t char_id, std::uint32_t guild_id,
                   std::uint8_t level, std::uint8_t duty) override;
    bool IncrementContribution(std::uint32_t char_id, std::uint32_t guild_id,
                               std::uint32_t exp, std::uint32_t gold,
                               std::uint32_t silver, std::uint32_t cooper,
                               std::uint32_t pvp_point) override;
    bool UpdateMemberPeer(std::uint32_t char_id, std::uint32_t guild_id,
                          std::uint8_t new_peer) override;
    bool UpdateMaxCabinet(std::uint32_t guild_id,
                          std::uint8_t max_cabinet) override;
    bool AddArticle(std::uint32_t guild_id, std::uint32_t article_id,
                    std::uint8_t duty, const std::string& writer,
                    const std::string& title, const std::string& body,
                    std::uint32_t time_unix) override;
    bool DelArticle(std::uint32_t guild_id,
                    std::uint32_t article_id) override;
    bool UpdateArticle(std::uint32_t guild_id, std::uint32_t article_id,
                       const std::string& title,
                       const std::string& body) override;
    bool DeleteGuild(std::uint32_t guild_id) override;
    bool AddWanted(std::uint32_t guild_id, std::uint8_t min_level,
                   std::uint8_t max_level, const std::string& title,
                   const std::string& text,
                   std::int64_t end_time_unix) override;
    bool DeleteWanted(std::uint32_t guild_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
