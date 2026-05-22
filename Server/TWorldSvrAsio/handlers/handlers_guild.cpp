#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
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

    // W3a-4: link the founding char to its guild via the new
    // TChar.guild_id back-pointer. Legacy sets pTCHAR->m_pGuild;
    // we keep just the id and let the GuildRegistry hold the
    // strong ref to TGuild. Mirrors SSHandler.cpp:9013.
    {
        std::lock_guard g(tchar->lock);
        tchar->guild_id = guild_id;
    }

    spdlog::info("OnGuildLoadAck[{}]: guild_id={} name='{}' level={} "
                 "members={} fame={} — registered (total={}), "
                 "char_id={} linked",
        ip, guild_id, reply_name, guild->level, guild->members.size(),
        guild->fame, ctx.guilds->Size(), char_id);

    // W3a-2: complete the legacy round-trip by ACKing the map server
    // that the guild is now world-side registered. Result code
    // kGuildSuccess + bEstablish=0 (this is a load, not a create).
    // Legacy: SSHandler.cpp:9019.
    co_await senders::SendMwGuildEstablishReq(
        peer, char_id, key, senders::kGuildSuccess, guild_id,
        reply_name, /*establish=*/0);
}

boost::asio::awaitable<void>
OnGuildLeaveAck(std::shared_ptr<PeerSession>  peer,
                std::vector<std::byte>        body,
                const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildLeaveAck[{}]: char/guild registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnGuildLeaveAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    // Legacy gate (SSHandler.cpp:3581-3589):
    //   FindTChar(char_id, key) → drop on miss
    //   pTCHAR->m_pGuild == nullptr → drop (char isn't in a guild)
    //   FindMember(char_id) miss → drop (member-list desync)
    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::info("OnGuildLeaveAck[{}]: char_id={} not in registry — "
                     "dropped", ip, char_id);
        co_return;
    }

    // Snapshot the per-char fields we need under the lock, then
    // release before touching the guild lock. Locking order:
    // char lock first → guild lock second (we never hold both at
    // once, avoiding deadlock with handlers that lock the guild
    // first and then a member's char).
    std::uint32_t guild_id   = 0;
    std::uint32_t actual_key = 0;
    std::string   char_name;
    {
        std::lock_guard g(tchar->lock);
        actual_key = tchar->key;
        guild_id   = tchar->guild_id;
        char_name  = tchar->name;
    }
    if (actual_key != key)
    {
        spdlog::warn("OnGuildLeaveAck[{}]: char_id={} key mismatch — "
                     "dropped", ip, char_id);
        co_return;
    }
    if (guild_id == 0)
    {
        spdlog::info("OnGuildLeaveAck[{}]: char_id={} has no guild — "
                     "dropped", ip, char_id);
        co_return;
    }

    auto guild = ctx.guilds->Find(guild_id);
    if (!guild)
    {
        // Char carries a guild_id but the guild was unloaded —
        // self-heal by clearing the back-pointer + replying so
        // the client at least observes the leave.
        spdlog::warn("OnGuildLeaveAck[{}]: char_id={} carried stale "
                     "guild_id={} (guild not in registry) — clearing "
                     "back-pointer", ip, char_id, guild_id);
        std::lock_guard g(tchar->lock);
        tchar->guild_id = 0;
    }
    else
    {
        // Remove the member under the guild's lock. RemoveMember
        // returns false if the member is already gone (e.g. a
        // double-leave from a flapping client); legacy treats
        // that as benign too.
        bool removed = false;
        std::size_t remaining = 0;
        {
            std::lock_guard g(guild->lock);
            removed = guild->RemoveMember(char_id);
            remaining = guild->members.size();
        }
        if (!removed)
        {
            spdlog::info("OnGuildLeaveAck[{}]: char_id={} not in "
                         "guild_id={} member list (legacy benign drop)",
                ip, char_id, guild_id);
        }
        else
        {
            spdlog::info("OnGuildLeaveAck[{}]: char_id={} left "
                         "guild_id={} (members remaining: {})",
                ip, char_id, guild_id, remaining);
        }

        // Always clear the back-pointer so a follow-up leave from
        // a stale broker is a no-op (and OnEnterCharReq sees no
        // guild).
        std::lock_guard g(tchar->lock);
        tchar->guild_id = 0;
    }

    // Forward the leave back to the requesting peer. Legacy
    // m_timeCurrent is a Unix epoch second the WorkThread refreshes
    // each tick; we just sample std::time(nullptr) here — it's
    // close enough for the 1-second-granularity guild log.
    const std::uint32_t now = static_cast<std::uint32_t>(std::time(nullptr));
    co_await senders::SendMwGuildLeaveReq(peer, char_id, key, char_name,
        senders::kGuildLeaveSelf, now);

    // TODO W3a-4b (DB write): SendDM_GUILDLEAVE_REQ to persist
    // the leave to TGUILDMEMBERTABLE via IGuildRepository::
    // RemoveMember. Currently the registry update is in-memory
    // only — the legacy SOCI write happens later via WorkThread →
    // BatchThread fan-out.
    // TODO W3a-4b (broadcast): notify other peers so the chat
    // window updates on every map server, not just the one this
    // request came in on.
    co_return;
}

} // namespace tworldsvr::handlers
