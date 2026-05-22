#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>

namespace tworldsvr::handlers {

namespace {

constexpr std::uint8_t kGuildDutyChief = 1;   // GUILD_DUTY_CHIEF
constexpr std::uint8_t kGuildPeerNone  = 0;   // GUILD_PEER_NONE

bool SkipCabinet(wire::Reader&, std::uint16_t count, const std::string& ip)
{
    if (count == 0) return true;
    spdlog::info("OnGuildLoadAck[{}]: cabinet wCount={} — skipping items "
                 "(W3a-3 will parse them)", ip, count);
    return true;
}

} // namespace

boost::asio::awaitable<void>
OnGuildLoadAck(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildLoadAck[{}]: char/guild registry not wired "
                     "— dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id  = 0;
    std::uint32_t key      = 0;
    std::uint32_t guild_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(guild_id))
    {
        spdlog::warn("OnGuildLoadAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::warn("OnGuildLoadAck[{}]: char_id={} not in registry — "
                     "dropped (legacy FindTChar miss)", ip, char_id);
        co_return;
    }
    std::uint8_t char_country = 0;
    std::string  char_name;
    {
        std::lock_guard g(tchar->lock);
        if (tchar->key != key)
        {
            spdlog::warn("OnGuildLoadAck[{}]: char_id={} key mismatch "
                         "(registry=0x{:08X} incoming=0x{:08X}) — dropped",
                ip, char_id, tchar->key, key);
            co_return;
        }
        // TODO W3a-3: tchar->country, tchar->name (need CHANGECHARBASE).
        char_country = 0;
        char_name    = std::string{};
    }

    if (ctx.guilds->Find(guild_id))
    {
        spdlog::debug("OnGuildLoadAck[{}]: guild_id={} already loaded — "
                      "ignored", ip, guild_id);
        co_return;
    }

    auto guild = std::make_shared<TGuild>();
    guild->id            = guild_id;
    guild->chief_char_id = char_id;
    guild->chief_name    = char_name;
    guild->country       = char_country;

    std::uint16_t cabinet_count = 0;
    std::int64_t  establish_t64 = 0;
    if (!r.ReadString(guild->name) ||
        !r.Read(guild->fame) ||
        !r.Read(guild->fame_color) ||
        !r.Read(guild->max_cabinet) ||
        !r.Read(guild->guild_points) ||
        !r.Read(guild->level) ||
        !r.Read(guild->chief_char_id) ||
        !r.Read(guild->exp) ||
        !r.Read(guild->gi) ||
        !r.Read(guild->status) ||
        !r.Read(guild->gold) ||
        !r.Read(guild->silver) ||
        !r.Read(guild->cooper) ||
        !r.Read(guild->disorg) ||
        !r.Read(guild->disorg_time) ||
        !r.Read(establish_t64) ||
        !r.Read(guild->pvp_total_point) ||
        !r.Read(guild->pvp_useable_point) ||
        !r.Read(cabinet_count))
    {
        spdlog::warn("OnGuildLoadAck[{}]: truncated guild row body — "
                     "dropped", ip);
        co_return;
    }
    guild->establish_time = establish_t64;

    if (!SkipCabinet(r, cabinet_count, ip))
    {
        spdlog::warn("OnGuildLoadAck[{}]: malformed cabinet body — "
                     "dropping guild", ip);
        co_return;
    }

    TGuildMember chief;
    chief.char_id  = char_id;
    chief.guild_id = guild_id;
    chief.duty     = kGuildDutyChief;
    chief.peer     = kGuildPeerNone;
    chief.name     = char_name;

    // Capture the values we need for the reply before the move into
    // the registry empties our local copy.
    const std::string reply_name = guild->name;

    guild->members.push_back(std::move(chief));

    if (!ctx.guilds->Insert(guild))
    {
        spdlog::info("OnGuildLoadAck[{}]: lost insert race for guild_id={}",
            ip, guild_id);
        co_return;
    }

    spdlog::info("OnGuildLoadAck[{}]: guild_id={} name='{}' level={} "
                 "members={} fame={} — registered (total={})",
        ip, guild_id, reply_name, guild->level, guild->members.size(),
        guild->fame, ctx.guilds->Size());

    // W3a-2: complete the legacy round-trip by ACKing the map server
    // that the guild is now world-side registered. Result code
    // kGuildSuccess + bEstablish=0 (this is a load, not a create).
    // Legacy: SSHandler.cpp:9019.
    co_await senders::SendMwGuildEstablishReq(
        peer, char_id, key, senders::kGuildSuccess, guild_id,
        reply_name, /*establish=*/0);
}

} // namespace tworldsvr::handlers
