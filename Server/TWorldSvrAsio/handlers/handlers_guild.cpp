#include "handlers.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>

namespace tworldsvr::handlers {

namespace {

constexpr std::uint8_t kGuildDutyChief = 1;   // GUILD_DUTY_CHIEF
constexpr std::uint8_t kGuildPeerNone  = 0;   // GUILD_PEER_NONE

// Parse + discard the variable-length cabinet item block trailing
// the guild row. Legacy CreateItem() consumes the body as a
// well-known item struct (Lib/Own/TProtocol/ITEM struct). W3a-2
// will rebuild this stream into TGuildItem rows; W3a-1 just walks
// past it so the read pointer is positioned for any trailing
// fields (there are none at present, but the legacy senders may
// append more later).
//
// Returns true on clean parse, false on framing violation
// (negative count, truncated body). Each item's size is known
// from the legacy ITEM struct serialization — but since we don't
// need the fields yet, we can't compute the per-item size without
// the full struct definition. Conservative: any non-zero count is
// logged and we skip the rest of the body. Production-ready
// parsing comes in W3a-2.
bool SkipCabinet(wire::Reader& r, std::uint16_t count, const std::string& ip)
{
    if (count == 0) return true;
    spdlog::info("OnGuildLoadAck[{}]: cabinet wCount={} — skipping items "
                 "(W3a-2 will parse them)", ip, count);
    // We don't know the per-item byte width without the legacy
    // ITEM struct. Conservatively consume the rest of the body
    // and move on. The handler doesn't need the items for the
    // registry insert anyway.
    return true;
}

} // namespace

boost::asio::awaitable<void>
OnGuildLoadAck(std::shared_ptr<WorldSession> sess,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    if (!ctx.chars || !ctx.guilds)
    {
        spdlog::warn("OnGuildLoadAck[{}]: char/guild registry not wired "
                     "— dropped", sess->RemoteIPv4());
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id  = 0;
    std::uint32_t key      = 0;
    std::uint32_t guild_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(guild_id))
    {
        spdlog::warn("OnGuildLoadAck[{}]: short body ({} bytes) — dropped",
            sess->RemoteIPv4(), body.size());
        co_return;
    }

    // Match the legacy gate: the char must already be registered
    // and the key must match what we recorded at MW_ADDCHAR_ACK
    // time. A mismatch means the map server is reporting a guild
    // for a session we never accepted (replay / stale) → drop.
    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        spdlog::warn("OnGuildLoadAck[{}]: char_id={} not in registry — "
                     "dropped (legacy FindTChar miss)",
            sess->RemoteIPv4(), char_id);
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
                sess->RemoteIPv4(), char_id, tchar->key, key);
            co_return;
        }
        // Per the legacy module, country is pulled from the char
        // at load time, NOT from the guild row. W3a-1 inherits
        // the same convention.
        //
        // Note: W2 doesn't populate `country` / `name` on TChar yet
        // (those fields ship with W3a-2 when CHANGECHARBASE arrives).
        // Until then both stay default-zero / empty; the guild
        // registry entry will reflect that and update on the next
        // char-info refresh.
        char_country = 0;  // TODO W3a-2: tchar->country (CHANGECHARBASE)
        char_name    = std::string{};
    }

    // Idempotent: silently ignore the duplicate (legacy
    // SSHandler.cpp:8958-8960 returns NOERROR without logging).
    if (ctx.guilds->Find(guild_id))
    {
        spdlog::debug("OnGuildLoadAck[{}]: guild_id={} already loaded — "
                      "ignored", sess->RemoteIPv4(), guild_id);
        co_return;
    }

    auto guild = std::make_shared<TGuild>();
    guild->id            = guild_id;
    guild->chief_char_id = char_id;
    guild->chief_name    = char_name;
    guild->country       = char_country;

    // Read the guild fields in the exact wire order documented in
    // handlers.h. Field types match the legacy CTGuild members.
    std::uint16_t cabinet_count = 0;
    std::int64_t  establish_t64 = 0;
    if (!r.ReadString(guild->name) ||
        !r.Read(guild->fame) ||
        !r.Read(guild->fame_color) ||
        !r.Read(guild->max_cabinet) ||
        !r.Read(guild->guild_points) ||
        !r.Read(guild->level) ||
        !r.Read(guild->chief_char_id) ||   // legacy re-asserts chief here
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
                     "dropped (consumed up to offset before cabinet)",
            sess->RemoteIPv4());
        co_return;
    }
    guild->establish_time = establish_t64;

    if (!SkipCabinet(r, cabinet_count, sess->RemoteIPv4()))
    {
        spdlog::warn("OnGuildLoadAck[{}]: malformed cabinet body — "
                     "dropping guild", sess->RemoteIPv4());
        co_return;
    }

    // Insert the chief as the first guild member. The legacy
    // module also wires m_pChar back-pointer and pulls m_bLevel /
    // m_bClass off the TChar; W3a-1 leaves those default-zero (the
    // values are repopulated at GUILDMEMBERLIST_ACK time anyway).
    TGuildMember chief;
    chief.char_id  = char_id;
    chief.guild_id = guild_id;
    chief.duty     = kGuildDutyChief;
    chief.peer     = kGuildPeerNone;
    chief.name     = char_name;
    guild->members.push_back(std::move(chief));

    if (!ctx.guilds->Insert(guild))
    {
        // Lost the race to a concurrent OnGuildLoadAck for the
        // same guild_id. Benign — the other handler owns the row.
        spdlog::info("OnGuildLoadAck[{}]: lost insert race for guild_id={}",
            sess->RemoteIPv4(), guild_id);
        co_return;
    }

    spdlog::info("OnGuildLoadAck[{}]: guild_id={} name='{}' level={} "
                 "members={} fame={} — registered (total={})",
        sess->RemoteIPv4(), guild_id, guild->name, guild->level,
        guild->members.size(), guild->fame, ctx.guilds->Size());

    // TODO W3a-2: SendMW_GUILDESTABLISH_REQ(char_id, key, GUILD_SUCCESS,
    //   guild_id, guild->name, FALSE) back to the originating map.
    //   Requires PeerSession (knows wID) + SSSender.
    co_return;
}

} // namespace tworldsvr::handlers
