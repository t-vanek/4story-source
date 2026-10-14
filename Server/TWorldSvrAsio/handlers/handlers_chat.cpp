#include "handlers.h"
#include "../senders/senders.h"
#include "../services/chat_constants.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"
#include "communication_authority.h"
#include "party_authority.h"
#include "MessageId.h"

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
    return communication::Map(ctx,msi);
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
    if (auto p = FindMapPeer(ctx, msi);communication::Current(ctx,p,c,key))
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
    auto operation=co_await ctx.communication_operations->Acquire();
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
        !r.Read(target) || !r.ReadString(name) || !r.ReadString(talk)||!r.Eof()||group>9||type>9||
        name.find('\0')!=std::string::npos||talk.find('\0')!=std::string::npos)
    {
        spdlog::warn("OnChatAck[{}]: short body ({} bytes)", ip, body.size());
        co_return;
    }

    std::unique_ptr<SocialOperationGate::Lease> party_operation;
    if(group==chat::kParty)party_operation=co_await ctx.party_operations->Acquire();
    auto sender = ctx.chars->Find(sender_id);
    if (!sender) co_return;
    const auto authorized=co_await communication::Authorize(ctx,peer,sender,sender_key);
    if(!authorized.valid)co_return;
    ChatMsg m;
    m.channel = channel; m.sender_id = sender_id; m.sender_name = sender_name;
    m.type = type; m.group = group; m.target = target; m.talk = talk;
    {
        std::lock_guard g(sender->lock);
        m.country = sender->country;
        m.aid     = sender->aid_country;
        if(ctx.chat_repo&&(sender_name!=sender->name||channel!=sender->channel||
            type!=(authorized.is_operator?chat::kOperator:group)))co_return;
        if(group==chat::kGuild&&target!=sender->guild_id)co_return;
        if(group==chat::kTactics&&target!=sender->guild_id&&target!=sender->tactics_guild_id)co_return;
    }
    // Ban/spam timing is decided by the original Map CS request. World does
    // not add another ban drop: CheckChat's async BAN can commit before this
    // already-authorized CHAT in the same stream, and source still relays it.

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
        if (!party||target>65535) co_return;
        std::vector<std::uint32_t> ids;
        {std::lock_guard g(party->lock);
         if(!party->IsMember(sender_id)||party->obtain_type==party::kObtainSolo||
            (ctx.chat_repo&&party->corps_id!=0))co_return;
         ids=party->members;}
        {std::lock_guard g(sender->lock);
         if(sender->party_id!=target||sender->main_server_id==50)co_return;}
        // Source BR party chat is its local team route, which remains OPEN.
        // The ordinary party audience cannot change while this gate is held.
        for(const auto id:ids) {
            auto recipient=ctx.chars->Find(id);if(!recipient)continue;
            std::uint32_t key=0;std::uint8_t server=0;
            {std::lock_guard g(recipient->lock);key=recipient->key;server=recipient->main_server_id;
             if(recipient->party_id!=target)continue;}
            auto map=FindMapPeer(ctx,server);
            const auto identities=co_await party_authority::AuthorizePair(ctx,peer,sender,map,recipient);
            if(!identities[0].valid||!party_authority::Current(ctx,map,recipient,key))continue;
            co_await senders::SendMwChatReq(map,id,key,m.channel,m.sender_id,m.sender_name,
                m.country,m.aid,m.type,m.group,m.target,m.talk);
        }
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
        for (auto& p : ctx.peers->Snapshot())if(ctx.peers->IsCurrentMap(p))
            co_await senders::SendMwChatReq(p, 0, 0, m.channel, m.sender_id,
                m.sender_name, m.country, m.aid, m.type, m.group, m.target,
                m.talk);
        break;
    }
    case chat::kWhisper:
    {
        const auto operator_name=ServerMessage(ctx.server_messages,kOperatorNameMessage);
        if(name==operator_name) {
            std::uint8_t count=0;
            if(ctx.operators)for(auto id:*ctx.operators) {
                auto op=ctx.chars->Find(id);if(!op)continue;
                std::uint32_t key=0;std::uint8_t server=0,country=0;
                {std::lock_guard guard(op->lock);key=op->key;server=op->main_server_id;country=op->country;}
                auto target_peer=FindMapPeer(ctx,server);
                if(country!=m.country||!communication::Current(ctx,target_peer,op,key))continue;
                co_await senders::SendMwChatReq(target_peer,id,key,m.channel,m.sender_id,m.sender_name,
                    m.country,m.aid,m.type,m.group,m.target,m.talk);++count;
            }
            const auto message=count?m.talk:ServerNetString({},ServerMessage(ctx.server_messages,7));
            if(communication::Current(ctx,peer,sender,sender_key))
                co_await senders::SendMwChatReq(peer,sender_id,sender_key,m.channel,count?sender_id:0,name,
                    m.country,m.aid,count?m.type:chat::kOperator,m.group,m.target,message);
            co_return;
        }
        // Source resolves by DWORD, then by the requested display name.
        for(unsigned attempt=0;attempt<2;++attempt) {
            auto tgt=attempt?ctx.chars->FindByName(name):ctx.chars->Find(target);
            if(!tgt)continue;
            std::uint32_t t_id=0,t_key=0;std::uint8_t t_country=0,t_aid=0,t_msi=0;std::string t_name;
            {std::lock_guard guard(tgt->lock);t_id=tgt->char_id;t_key=tgt->key;t_country=tgt->country;
             t_aid=tgt->aid_country;t_msi=tgt->main_server_id;t_name=tgt->name;}
            if(party::WarCountry(m.country,m.aid)!=party::WarCountry(t_country,t_aid)&&
               m.country!=chat::kCountryPeace&&t_country!=chat::kCountryPeace)co_return;
            auto target_peer=FindMapPeer(ctx,t_msi);
            if(!communication::Current(ctx,target_peer,tgt,t_key))continue;
            co_await senders::SendMwChatReq(target_peer,t_id,t_key,m.channel,m.sender_id,m.sender_name,
                m.country,m.aid,m.type,m.group,m.target,m.talk);
            // DWORD lookup echoes the actual name; name fallback echoes the
            // requested name, matching the two distinct original branches.
            if(communication::Current(ctx,peer,sender,sender_key))
                co_await senders::SendMwChatReq(peer,sender_id,sender_key,m.channel,sender_id,attempt?name:t_name,
                    m.country,m.aid,m.type,m.group,m.target,m.talk);
            break;
        }
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
    auto operation=co_await ctx.communication_operations->Acquire();
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
        !r.Read(char_id) || !r.Read(key)||!r.Eof()||target_name.find('\0')!=std::string::npos||((char_id==0)!=(key==0)))
    {
        spdlog::warn("OnChatBanAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto issuer=char_id?ctx.chars->Find(char_id):nullptr;
    if(char_id) {
        const auto auth=co_await communication::Authorize(ctx,peer,issuer,key);
        if(!auth.valid||!auth.is_operator)co_return;
    } else if(!ctx.peers->IsCurrentMap(peer))co_return;
    auto tgt = ctx.chars->FindByName(target_name);
    if (!tgt)
    {
        co_await senders::SendMwChatBanReq(peer, target_name, 0,
            chat::kChatBanInvalidChar, char_id, key);
        co_return;
    }

    std::int64_t ban_time = 0;
    std::uint8_t t_msi = 0;
    std::uint32_t target_id=0,target_key=0;std::string canonical_name;
    {std::lock_guard guard(tgt->lock);target_id=tgt->char_id;target_key=tgt->key;t_msi=tgt->main_server_id;canonical_name=tgt->name;}
    auto target_peer=FindMapPeer(ctx,t_msi);
    if(!communication::Current(ctx,target_peer,tgt,target_key)||(!char_id&&peer!=target_peer))co_return;
    if(ctx.chat_repo) {
        const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo=ctx.chat_repo,target_id,canonical_name,target_key,t_msi,minutes,char_id,key,server=static_cast<std::uint8_t>(peer->Wid())]
            {return repo->Ban(target_id,canonical_name,{target_key,t_msi},minutes,char_id,{key,server});});
        if(!saved)co_return;ban_time=*saved;
        if(!communication::Current(ctx,target_peer,tgt,target_key))co_return;
        {std::lock_guard guard(tgt->lock);tgt->chat_ban_time=ban_time;}
    } else
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

    // Use the resolved original wire name for the durable authority key and
    // Map confirmation, including a request with different ASCII casing.
    target_name=canonical_name;
    // Tell the target's map to enforce (recipient = target, no GM id).
    if (auto p = FindMapPeer(ctx, t_msi))
        co_await senders::SendMwChatBanReq(p, target_name, ban_time,
            chat::kChatBanSuccess, 0, 0);
    // Echo the result to the issuing GM's map.
    if (char_id && key&&communication::Current(ctx,peer,issuer,key))
        co_await senders::SendMwChatBanReq(peer, target_name, ban_time,
            chat::kChatBanSuccess, char_id, key);
    spdlog::info("OnChatBanAck[{}]: '{}' chat-ban until {} (by char {})",
        ip, target_name, ban_time, char_id);
    co_return;
}

boost::asio::awaitable<void> OnCtChatBanReq(std::shared_ptr<PeerSession> peer,std::vector<std::byte> body,const HandlerContext& ctx) {
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.chars||!ctx.peers||!ctx.ctrl_svr||ctx.ctrl_svr->Get()!=peer||!peer->Wire()->IsOpen())co_return;
    wire::Reader reader(body.data(),body.size());std::string name;std::uint16_t minutes=0;std::uint32_t sequence=0,manager=0;
    if(!reader.ReadString(name)||!reader.Read(minutes)||!reader.Read(sequence)||!reader.Read(manager)||!reader.Eof()||name.find('\0')!=std::string::npos)co_return;
    auto target=ctx.chars->FindByName(name);std::uint8_t accepted=0;
    if(target) {
        std::uint32_t id=0,key=0;std::uint8_t server=0;std::string canonical;
        {std::lock_guard guard(target->lock);id=target->char_id;key=target->key;server=target->main_server_id;canonical=target->name;}
        auto main=FindMapPeer(ctx,server);
        if(communication::Current(ctx,main,target,key)) {
            std::int64_t until=static_cast<std::int64_t>(std::time(nullptr))+static_cast<std::int64_t>(minutes)*60;
            if(ctx.chat_repo) {
                const auto result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                    [repo=ctx.chat_repo,id,key,server,canonical,minutes]{return repo->Ban(id,canonical,{key,server},minutes,0,{},true);});
                if(!result)co_return;until=*result;
            }
            if(!communication::Current(ctx,main,target,key))co_return;
            {std::lock_guard guard(target->lock);target->chat_ban_time=until;}
            co_await senders::SendMwChatBanReq(main,canonical,until,chat::kChatBanSuccess,0,0);accepted=1;
        }
    }
    if(ctx.ctrl_svr->Get()!=peer||!peer->Wire()->IsOpen())co_return;
    std::vector<std::byte> answer;wire::WritePOD(answer,accepted);wire::WritePOD(answer,sequence);wire::WritePOD(answer,manager);
    co_await peer->Wire()->SendPacket(tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_CHATBAN_ACK),std::move(answer));
}

boost::asio::awaitable<void>
OnCharMsgAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte>       body,
             const HandlerContext&        ctx)
{
    auto operation=co_await ctx.communication_operations->Acquire();
    if(!ctx.ctrl_svr||ctx.ctrl_svr->Get()!=peer||!peer->Wire()->IsOpen())co_return;
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnCharMsgAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string name, msg;
    if (!r.ReadString(name) || !r.ReadString(msg)||!r.Eof()||name.find('\0')!=std::string::npos||msg.find('\0')!=std::string::npos)
    {
        spdlog::warn("OnCharMsgAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }
    // Source strMsg.Left(ONE_KBYTE) ignores its return value. Preserve the
    // complete request within the existing transport frame bounds.

    auto tgt = ctx.chars->FindByName(name);
    if (!tgt) co_return;
    std::uint8_t msi = 0;std::uint32_t key=0;
    { std::lock_guard g(tgt->lock); msi = tgt->main_server_id;key=tgt->key; }
    auto main=FindMapPeer(ctx,msi);
    if(!(co_await communication::Authorize(ctx,main,tgt,key)).valid||ctx.ctrl_svr->Get()!=peer||!peer->Wire()->IsOpen())co_return;
    co_await senders::SendMwCharMsgReq(main,name,msg);
    co_return;
}

} // namespace tworldsvr::handlers
