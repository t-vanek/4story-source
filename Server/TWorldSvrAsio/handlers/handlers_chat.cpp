#include "handlers.h"
#include "../senders/senders.h"
#include "../services/chat_constants.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {

namespace {

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// The immutable bits of a chat message, snapshotted once.
struct ChatMsg
{
    std::uint8_t  channel = 0;
    std::uint32_t sender_id = 0;
    std::string   sender_name;
    std::uint8_t  country = 0;
    std::uint8_t  aid = 0;
    std::uint8_t  type = 0;
    std::uint8_t  group = 0;
    std::uint32_t target = 0;
    std::string   talk;
};

// Deliver to one online char's map peer.
boost::asio::awaitable<void>
RelayToChar(const HandlerContext& ctx, std::uint32_t cid, const ChatMsg& m)
{
    auto c = ctx.chars->Find(cid);
    if (!c) co_return;
    std::uint32_t key = 0;
    std::uint8_t  msi = 0;
    { std::lock_guard g(c->lock); key = c->key; msi = c->main_server_id; }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwChatReq(p, cid, key, m.channel, m.sender_id,
            m.sender_name, m.country, m.aid, m.type, m.group, m.target,
            m.talk);
}

} // namespace

boost::asio::awaitable<void>
OnChatAck(std::shared_ptr<PeerSession> peer,
          std::vector<std::byte>       body,
          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnChatAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t sender_id = 0, sender_key = 0, target = 0;
    std::uint8_t  channel = 0, type = 0, group = 0;
    std::string   sender_name, name, talk;
    if (!r.Read(channel) || !r.Read(sender_id) || !r.Read(sender_key) ||
        !r.ReadString(sender_name) || !r.Read(type) || !r.Read(group) ||
        !r.Read(target) || !r.ReadString(name) || !r.ReadString(talk))
    {
        spdlog::warn("OnChatAck[{}]: short body ({} bytes)", ip, body.size());
        co_return;
    }

    auto sender = ctx.chars->Find(sender_id);
    if (!sender) co_return;
    ChatMsg m;
    m.channel = channel; m.sender_id = sender_id; m.sender_name = sender_name;
    m.type = type; m.group = group; m.target = target; m.talk = talk;
    {
        std::lock_guard g(sender->lock);
        m.country = sender->country;
        m.aid     = sender->aid_country;
    }

    switch (group)
    {
    case chat::kGuild:
    case chat::kTactics:
    {
        if (!ctx.guilds) co_return;
        auto guild = ctx.guilds->Find(target);
        if (!guild) co_return;
        std::vector<std::uint32_t> ids;
        {
            std::lock_guard g(guild->lock);
            for (const auto& mem : guild->members) ids.push_back(mem.char_id);
            if (group == chat::kTactics)
                for (const auto& t : guild->tactics_members) ids.push_back(t.id);
        }
        for (auto id : ids) co_await RelayToChar(ctx, id, m);
        break;
    }
    case chat::kParty:
    {
        if (!ctx.parties) co_return;
        auto party = ctx.parties->Find(static_cast<std::uint16_t>(target));
        if (!party) co_return;
        std::vector<std::uint32_t> ids;
        { std::lock_guard g(party->lock); ids = party->members; }
        for (auto id : ids) co_await RelayToChar(ctx, id, m);
        break;
    }
    case chat::kForce:
    {
        if (!ctx.parties || !ctx.corps) co_return;
        std::uint16_t sp = 0;
        { std::lock_guard g(sender->lock);
          sp = sender->party_id; }
        if (sp == 0) co_return;
        std::uint16_t corps_id = 0;
        if (auto p = ctx.parties->Find(sp))
        { std::lock_guard g(p->lock); corps_id = p->corps_id; }
        if (corps_id == 0) co_return;
        std::vector<std::uint16_t> squads;
        if (auto c = ctx.corps->Find(corps_id))
        { std::lock_guard g(c->lock); squads = c->squads; }
        std::vector<std::uint32_t> ids;
        for (auto sid : squads)
            if (auto p = ctx.parties->Find(sid))
            {
                std::lock_guard g(p->lock);
                ids.insert(ids.end(), p->members.begin(), p->members.end());
            }
        for (auto id : ids) co_await RelayToChar(ctx, id, m);
        break;
    }
    case chat::kMap:
    case chat::kWorld:
    case chat::kShow:
    {
        // Global — one packet per map peer (char_id/key = 0).
        for (auto& p : ctx.peers->Snapshot())
            co_await senders::SendMwChatReq(p, 0, 0, m.channel, m.sender_id,
                m.sender_name, m.country, m.aid, m.type, m.group, m.target,
                m.talk);
        break;
    }
    case chat::kWhisper:
    {
        // Operator-whisper sub-case deferred (needs the operator
        // list). Direct whisper: resolve by target id, else by name.
        auto tgt = ctx.chars->Find(target);
        if (!tgt && !name.empty()) tgt = ctx.chars->FindByName(name);
        if (!tgt) co_return;
        std::uint32_t t_id = 0, t_key = 0;
        std::uint8_t  t_country = 0, t_aid = 0, t_msi = 0;
        std::string   t_name;
        {
            std::lock_guard g(tgt->lock);
            t_id = tgt->char_id; t_key = tgt->key;
            t_country = tgt->country; t_aid = tgt->aid_country;
            t_msi = tgt->main_server_id; t_name = tgt->name;
        }
        // War-country gate, waived if either side is in peace country.
        if (party::WarCountry(m.country, m.aid) !=
                party::WarCountry(t_country, t_aid) &&
            m.country != chat::kCountryPeace &&
            t_country != chat::kCountryPeace)
            co_return;
        if (auto tp = FindMapPeer(ctx, t_msi))
            co_await senders::SendMwChatReq(tp, t_id, t_key, m.channel,
                m.sender_id, m.sender_name, m.country, m.aid, m.type, m.group,
                m.target, m.talk);
        // Echo back to the sender (display name = the recipient).
        co_await senders::SendMwChatReq(peer, sender_id, sender_key, m.channel,
            sender_id, t_name, m.country, m.aid, m.type, m.group, m.target,
            m.talk);
        break;
    }
    default:
        break;
    }
    co_return;
}

boost::asio::awaitable<void>
OnChatBanAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte>       body,
             const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnChatBanAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string   target_name;
    std::uint16_t minutes = 0;
    std::uint32_t char_id = 0, key = 0;
    if (!r.ReadString(target_name) || !r.Read(minutes) ||
        !r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnChatBanAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto tgt = ctx.chars->FindByName(target_name);
    if (!tgt)
    {
        co_await senders::SendMwChatBanReq(peer, target_name, 0,
            chat::kChatBanInvalidChar, char_id, key);
        co_return;
    }

    std::int64_t ban_time = 0;
    std::uint8_t t_msi = 0;
    {
        std::lock_guard g(tgt->lock);
        const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
        if (minutes > 0)
            // Stack onto an active ban, else start from now (legacy).
            tgt->chat_ban_time =
                (now < tgt->chat_ban_time ? tgt->chat_ban_time : now)
                + std::int64_t(minutes) * 60;
        else
            tgt->chat_ban_time = 0;   // unban
        ban_time = tgt->chat_ban_time;
        t_msi    = tgt->main_server_id;
    }

    // Tell the target's map to enforce (recipient = target, no GM id).
    if (auto p = FindMapPeer(ctx, t_msi))
        co_await senders::SendMwChatBanReq(p, target_name, ban_time,
            chat::kChatBanSuccess, 0, 0);
    // Echo the result to the issuing GM's map.
    if (char_id && key)
        co_await senders::SendMwChatBanReq(peer, target_name, ban_time,
            chat::kChatBanSuccess, char_id, key);
    spdlog::info("OnChatBanAck[{}]: '{}' chat-ban until {} (by char {})",
        ip, target_name, ban_time, char_id);
    co_return;
}

boost::asio::awaitable<void>
OnCharMsgAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte>       body,
             const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnCharMsgAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string name, msg;
    if (!r.ReadString(name) || !r.ReadString(msg))
    {
        spdlog::warn("OnCharMsgAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }
    if (msg.size() > 1024) msg.resize(1024);   // legacy strMsg.Left(ONE_KBYTE)

    auto tgt = ctx.chars->FindByName(name);
    if (!tgt) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(tgt->lock); msi = tgt->main_server_id; }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwCharMsgReq(p, name, msg);
    co_return;
}

} // namespace tworldsvr::handlers
