#pragma once

// FakeGuildRepository — in-memory IGuildRepository. Used by every
// test under Server/TWorldSvrAsio/tests/ that doesn't depend on a
// live DB. Seeded through AddGuild().

#include "services/guild_repository.h"

#include <mutex>
#include <unordered_map>

namespace tworldsvr {

class FakeGuildRepository : public IGuildRepository
{
public:
    // Seed a guild into the fake store. The repository takes a
    // strong reference; LoadAll / FindById return a fresh
    // shared_ptr to a deep copy so test-side mutation of the
    // returned guild doesn't bleed back into the seed.
    void AddGuild(std::shared_ptr<TGuild> g);

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
    bool AddVolunteerApp(std::uint32_t char_id,
                         std::uint32_t wanted_id) override;
    bool DelVolunteerApp(std::uint32_t char_id) override;
    bool UpdatePvPoints(std::uint32_t guild_id, std::uint32_t total_point,
                        std::uint32_t useable_point,
                        std::uint32_t month_point) override;
    bool UpdateLevel(std::uint32_t guild_id, std::uint8_t level) override;
    bool LogPointReward(std::uint32_t guild_id, std::uint32_t point,
                        const std::string& recipient_name,
                        std::uint32_t total_point,
                        std::uint32_t useable_point) override;
    std::optional<std::uint32_t> CreateGuild(const std::string& name,
                                              std::uint32_t chief_id,
                                              std::uint8_t  country,
                                              std::int64_t  establish_time_unix)
        override;
    bool LogPvPRecord(std::uint32_t guild_id,
                      std::uint32_t member_id,
                      std::uint32_t date,
                      std::uint16_t kill_count,
                      std::uint16_t die_count,
                      const std::array<std::uint32_t,
                                        guild::kPvPEventCount>&
                          points) override;
    bool UpdateGuildFull(std::uint32_t guild_id,
                         std::uint8_t  fame,
                         std::uint8_t  guild_points,
                         std::uint8_t  level,
                         std::uint8_t  status,
                         std::uint32_t chief_id,
                         std::uint32_t gi,
                         std::uint32_t exp,
                         std::uint32_t time_unix) override;

    // Test-only: snapshot of the mutating calls in arrival order.
    // Lets test_guild_mut_handlers assert that the right CSP-equivalent
    // ran after each handler runs.
    struct Call
    {
        enum class Kind { kSetDisorg, kUpdateMemberDuty, kUpdateFame,
                          kRemoveMember, kAddMember, kIncrementContribution,
                          kUpdateMemberPeer, kUpdateMaxCabinet,
                          kAddArticle, kDelArticle, kUpdateArticle,
                          kDeleteGuild, kAddWanted, kDeleteWanted,
                          kAddVolunteerApp, kDelVolunteerApp,
                          kUpdatePvPoints, kUpdateLevel, kLogPointReward,
                          kCreateGuild, kLogPvPRecord, kUpdateGuildFull };
        Kind          kind;
        std::uint32_t guild_id = 0;
        std::uint32_t char_id  = 0;
        std::uint32_t a        = 0;   // disorg / duty / fame / level / exp
        std::uint32_t b        = 0;   // time_unix / fame_color / duty / gold
        std::uint32_t c        = 0;   // silver
        std::uint32_t d        = 0;   // cooper
        std::uint32_t e        = 0;   // pvp_point
    };
    std::vector<Call> Calls() const;

private:
    mutable std::mutex                                         m_mtx;
    std::unordered_map<std::uint32_t, std::shared_ptr<TGuild>> m_guilds;
    std::vector<Call>                                          m_calls;
};

} // namespace tworldsvr
