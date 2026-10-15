// World-owned transient parties: source CSHandler3419, SSHandler7839/10429.
#include "handlers.h"
#include "handlers_world.h"
#include "config.h"
#include "services/char_state_store.h"
#include "services/channel_presence.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "services/party_runtime.h"
#include "services/native_projection.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include "packet_codec.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
bool Same(const SessionIdentity& a,const SessionIdentity& b) {
    return a.char_id==b.char_id&&a.key==b.key&&a.connection_id==b.connection_id&&a.authority_epoch==b.authority_epoch&&a.role==b.role&&
        (b.phase==SessionPhase::Ready||(b.role==MapSessionRole::Primary&&(b.phase==SessionPhase::Loaded||b.phase==SessionPhase::Admitted)));
}
bool Current(const std::shared_ptr<tnetlib::AsioSession>& client,const SessionIdentity& id,const HandlerContext& ctx) {
    const auto now=ctx.session_reg->Identity(client.get());
    return client->IsOpen()&&now&&Same(id,*now)&&ctx.world_client&&ctx.world_client->IsRegistered();
}
void Name(const std::string& text) {if(text.find('\0')!=std::string::npos)throw std::runtime_error("Invalid party name");}
void Obtain(std::uint8_t value) {if(value>5)throw std::runtime_error("Invalid source party obtain enum");}
boost::asio::awaitable<void> Relay(const std::shared_ptr<tnetlib::AsioSession>& client,MessageId op,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    if(body.size()>=kWorldPlainMaxPacketSize-kWorldPlainHeaderSize) {
        spdlog::warn("Party relay exceeds original Plain16 frame opcode={}",static_cast<std::uint16_t>(op));co_return;
    }
    if(!client->IsOpen())co_return;
    if(!ctx.world_client||!ctx.world_client->IsRegistered()||!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(op),std::move(body)))client->Close();
}
boost::asio::awaitable<void> Send(const std::shared_ptr<tnetlib::AsioSession>& client,MessageId op,std::vector<std::byte> body) {
    if(body.size()>=tnetlib::kMaxPacketSize-tnetlib::kPacketHeaderSize) {
        spdlog::warn("Party client response exceeds original frame opcode={}",static_cast<std::uint16_t>(op));co_return;
    }
    co_await client->SendPacket(static_cast<std::uint16_t>(op),std::move(body));
}
void Vitals(std::vector<std::byte>& out,const CharSnapshot& s) {
    wire::WritePOD(out,s.dwMaxHP);wire::WritePOD(out,s.dwHP);wire::WritePOD(out,s.dwMaxMP);wire::WritePOD(out,s.dwMP);
}
boost::asio::awaitable<std::optional<PartyAuthority>> Authorize(const std::shared_ptr<tnetlib::AsioSession>& client,
    const SessionIdentity& id,const CharSnapshot& state,const HandlerContext& ctx,std::uint32_t inviter=0,bool invitation=false) {
    if(!ctx.player_service)throw std::runtime_error("Native party actor authority unavailable");
    auto* service=ctx.player_service;
    const auto result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [service,claim=id.Claim(ctx.expected_group),&state,inviter,invitation]{return service->AuthorizeParty(claim,state,inviter,invitation);});
    if(!Current(client,id,ctx))co_return std::nullopt;
    co_return result;
}
boost::asio::awaitable<bool> Projection(const std::shared_ptr<tnetlib::AsioSession>& client,const SessionIdentity& id,const HandlerContext& ctx) {
    boost::asio::steady_timer wait(co_await boost::asio::this_coro::executor);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!ctx.session_reg->BeginPartyProjection(client.get())) {
        if(!Current(client,id,ctx))co_return false;
        if(std::chrono::steady_clock::now()>=deadline) {
            // Missing a membership transition cannot leave stale membership
            // usable for another request. Use the normal teardown, never replay.
            spdlog::warn("Party projection lease timed out char={}",id.char_id);client->Close();co_return false;
        }
        wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
    }
    co_return true;
}
std::vector<std::byte> JoinAnswer(const std::string& origin,const std::string& target,std::uint8_t obtain,std::uint8_t answer,const CharSnapshot& s) {
    std::vector<std::byte> out;wire::WriteString(out,origin);wire::WriteString(out,target);
    wire::WritePOD(out,obtain);wire::WritePOD(out,answer);Vitals(out,s);return out;
}
std::vector<std::byte> BusyUnavailable(const std::string& origin,const std::string& target,std::uint8_t obtain) {
    // Original unavailable-recipient BUSY shape. This is a terminal refusal;
    // World must not import these absence fields as an accepted member's bars.
    std::vector<std::byte> out;wire::WriteString(out,origin);wire::WriteString(out,target);
    wire::WritePOD(out,obtain);wire::WritePOD(out,std::uint8_t{2});
    for(int n=0;n<4;++n)wire::WritePOD(out,std::uint32_t{0});return out;
}
std::uint32_t Chief(const CharSnapshot& s) {return s.cluster.party_type==1?0:s.cluster.party_chief;}
std::uint16_t Commander(const CharSnapshot& s) {return s.cluster.party_type==1?0:s.cluster.commander;}
std::uint16_t Party(const CharSnapshot& s) {return s.cluster.party_type==1?0:s.cluster.party;}
}

boost::asio::awaitable<void> OnPartyAddReq(std::shared_ptr<tnetlib::AsioSession> client,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=ctx.session_reg->Identity(client.get());
    if(!id||id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready||ctx.mode==Mode::Br)co_return;
    const auto s=ctx.char_state->Get(id->char_id);
    if(!s||!s->cluster.hydrated||!s->payload||s->persistence_uncertain||s->payload->arena_id||!ctx.presence||!ctx.presence->FindEntry(id->char_id))co_return;
    if(!s->payload->statistics) {
        spdlog::warn("Party ADD needs supported native current vitals char={}",id->char_id);co_return;
    }
    wire::Reader r(body);std::string target;std::uint8_t obtain=0;
    if(!r.ReadString(target)||!r.Read(obtain)||!r.Eof())throw std::runtime_error("Invalid PARTYADD body");Name(target);Obtain(obtain);
    const auto authority=co_await Authorize(client,*id,*s,ctx);if(!authority)co_return;
    std::vector<std::byte> request;wire::WriteString(request,authority->name);wire::WriteString(request,target);wire::WritePOD(request,obtain);Vitals(request,*s);
    co_await Relay(client,MessageId::MW_PARTYADD_ACK,std::move(request),ctx);
}
boost::asio::awaitable<void> OnPartyJoinReq(std::shared_ptr<tnetlib::AsioSession> client,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=ctx.session_reg->Identity(client.get());
    if(!id||id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready||ctx.mode==Mode::Br)co_return;
    const auto s=ctx.char_state->Get(id->char_id);if(!s||!s->cluster.hydrated||s->persistence_uncertain)co_return;
    wire::Reader r(body);std::string origin;std::uint8_t obtain=0,answer=0;
    if(!r.ReadString(origin)||!r.Read(obtain)||!r.Read(answer)||!r.Eof()||answer>2)throw std::runtime_error("Invalid PARTYJOIN body");Name(origin);Obtain(obtain);
    const auto authority=co_await Authorize(client,*id,*s,ctx);if(!authority)co_return;
    if(!s->payload||!s->payload->statistics) {
        spdlog::warn("Party JOIN refused: native current vitals unsupported char={}",id->char_id);
        co_await Relay(client,MessageId::MW_PARTYJOIN_ACK,BusyUnavailable(origin,authority->name,obtain),ctx);co_return;
    }
    co_await Relay(client,MessageId::MW_PARTYJOIN_ACK,JoinAnswer(origin,authority->name,obtain,answer,*s),ctx);
}
boost::asio::awaitable<void> OnPartyDelReq(std::shared_ptr<tnetlib::AsioSession> client,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=ctx.session_reg->Identity(client.get());
    if(!id||id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready)co_return;
    const auto s=ctx.char_state->Get(id->char_id);
    if(!s||!s->cluster.hydrated||s->persistence_uncertain||!Party(*s)||!ctx.presence||!ctx.presence->FindEntry(id->char_id))co_return;
    wire::Reader r(body);std::uint32_t member=0;
    if(!r.Read(member)||!r.Eof()||!member)throw std::runtime_error("Invalid PARTYDEL body");
    if(member!=id->char_id&&Chief(*s)!=id->char_id)co_return;
    if(!co_await Authorize(client,*id,*s,ctx))co_return;
    std::vector<std::byte> request;wire::WritePOD(request,Party(*s));wire::WritePOD(request,member);wire::WritePOD(request,static_cast<std::uint8_t>(member!=id->char_id));
    co_await Relay(client,MessageId::MW_PARTYDEL_ACK,std::move(request),ctx);
}

boost::asio::awaitable<void> OnPartyChangeReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto op=static_cast<MessageId>(opcode);
    const bool change_chief=op==MessageId::CS_CHGPARTYCHIEF_REQ;
    if(!change_chief&&op!=MessageId::CS_CHGPARTYTYPE_REQ)throw std::runtime_error("Unknown party change request");
    const auto id=ctx.session_reg->Identity(client.get());
    if(!id||id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready)co_return;
    const auto s=ctx.char_state->Get(id->char_id);
    if(!s||!s->cluster.hydrated||s->persistence_uncertain||!ctx.presence||!ctx.presence->FindEntry(id->char_id))co_return;
    // Source CSHandler3377/3400: map/main gate for both; BR suppresses
    // chief only. World owns actual membership/chief permission and errors.
    if(change_chief&&ctx.mode==Mode::Br)co_return;
    wire::Reader r(body);std::uint32_t target=0;std::uint8_t type=0;
    if(change_chief) {
        if(!r.Read(target)||!r.Eof())throw std::runtime_error("Invalid CHGPARTYCHIEF body");
    }else {
        if(!r.Read(type)||!r.Eof())throw std::runtime_error("Invalid CHGPARTYTYPE body");Obtain(type);
    }
    if(!co_await Authorize(client,*id,*s,ctx))co_return;
    std::vector<std::byte> request;wire::WritePOD(request,id->char_id);wire::WritePOD(request,id->key);
    if(change_chief)wire::WritePOD(request,target);else wire::WritePOD(request,type);
    co_await Relay(client,change_chief?MessageId::MW_CHGPARTYCHIEF_ACK:MessageId::MW_CHGPARTYTYPE_ACK,std::move(request),ctx);
}

boost::asio::awaitable<void> OnWorldParty(std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    wire::Reader r(body);std::uint32_t character=0,key=0;
    if(!r.Read(character)||!r.Read(key))throw std::runtime_error("Short World party prefix");
    const auto op=static_cast<MessageId>(opcode);
    // Validate the whole body before publishing any state. Source widths and
    // packet order are retained, including DEL's old cached client tail.
    std::string origin,target,guild;std::uint16_t party=0,commander=0;
    std::uint32_t member=0,chief=0,max_hp=0,hp=0,max_mp=0,mp=0,inviter=0;
    std::uint8_t obtain=0,reply=0,level=0,race=0,sex=0,face=0,hair=0,klass=0,type=0;
    if(op==MessageId::MW_PARTYADD_REQ) {
        if(!r.ReadString(origin)||!r.ReadString(target)||!r.Read(obtain)||!r.Read(reply)||!r.Read(inviter)||!r.Eof()||reply>11)throw std::runtime_error("Invalid World PARTYADD");
        Name(origin);Name(target);Obtain(obtain);
        if(!reply&&!inviter)throw std::runtime_error("Party invitation has no source inviter");
    }else if(op==MessageId::MW_PARTYJOIN_REQ) {
        if(!r.Read(party)||!r.ReadString(target)||!r.Read(member)||!r.Read(chief)||!r.Read(commander)||!r.ReadString(guild)||!r.Read(level)||
           !r.Read(max_hp)||!r.Read(hp)||!r.Read(max_mp)||!r.Read(mp)||!r.Read(race)||!r.Read(sex)||!r.Read(face)||!r.Read(hair)||
           !r.Read(obtain)||!r.Read(klass)||!r.Eof()||!party||!member||!chief)throw std::runtime_error("Invalid World PARTYJOIN");
        Name(target);Obtain(obtain);if(guild.find('\0')!=std::string::npos)throw std::runtime_error("Invalid party guild name");
    }else if(op==MessageId::MW_PARTYDEL_REQ) {
        if(!r.Read(member)||!r.Read(chief)||!r.Read(commander)||!r.Read(party)||!r.Read(reply)||!r.Eof()||reply>1||!member)throw std::runtime_error("Invalid World PARTYDEL");
    }else if(op==MessageId::MW_PARTYATTR_REQ) {
        if(!r.Read(party)||!r.Read(obtain)||!r.Read(chief)||!r.Read(commander)||!r.Eof())throw std::runtime_error("Invalid World PARTYATTR");Obtain(obtain);
    }else if(op==MessageId::MW_CHGPARTYCHIEF_REQ) {
        if(!r.Read(reply)||!r.Eof()||reply>11)throw std::runtime_error("Invalid World CHGPARTYCHIEF");
    }else if(op==MessageId::MW_CHGPARTYTYPE_REQ) {
        if(!r.Read(reply)||!r.Read(obtain)||!r.Eof()||reply>11)throw std::runtime_error("Invalid World CHGPARTYTYPE");Obtain(obtain);
    }else if(op==MessageId::MW_PARTYMANSTAT_REQ) {
        if(!r.Read(member)||!r.Read(type)||!r.Read(level)||!r.Read(max_hp)||!r.Read(hp)||!r.Read(max_mp)||!r.Read(mp)||!r.Eof()||!member)throw std::runtime_error("Invalid World PARTYMANSTAT");
    }else throw std::runtime_error("Unsupported World party opcode");
    auto client=ctx.session_reg->Find(character,key);auto id=client?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!id||!co_await Projection(client,*id,ctx))co_return;
    SessionOperation operation(*ctx.session_reg,client.get());
    // SQL ready may precede the local ready transition. The lease waits for
    // admission to finish; use its current phase, retaining the same identity.
    const auto refreshed=ctx.session_reg->Identity(client.get());
    if(!refreshed||!Same(*id,*refreshed)||!Current(client,*refreshed,ctx))co_return;
    id=refreshed;
    const auto s=ctx.char_state->Get(character);if(!s||!s->cluster.hydrated||s->persistence_uncertain)co_return;
    if((op==MessageId::MW_CHGPARTYCHIEF_REQ||op==MessageId::MW_CHGPARTYTYPE_REQ)&&
       (id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready))co_return;
    if(op==MessageId::MW_PARTYADD_REQ&&(id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready||!ctx.presence||!ctx.presence->FindEntry(character)))co_return;
    const bool unsupported_vitals=op==MessageId::MW_PARTYADD_REQ&&!reply&&(!s->payload||!s->payload->statistics);
    const auto authority=co_await Authorize(client,*id,*s,ctx,inviter,op==MessageId::MW_PARTYADD_REQ&&reply==0&&!unsupported_vitals);if(!authority)co_return;
    if(op==MessageId::MW_CHGPARTYCHIEF_REQ) {
        // PARTY_CHGCHIEF10 is the original successful result. ATTR alone
        // owns chief metadata, so forwarding does not invent a local change.
        co_await Send(client,MessageId::CS_CHGPARTYCHIEF_ACK,std::vector<std::byte>{static_cast<std::byte>(reply)});
    }else if(op==MessageId::MW_CHGPARTYTYPE_REQ) {
        if(!reply)ctx.char_state->Update(character,[&](CharSnapshot& state){state.cluster.party_type=obtain;});
        co_await Send(client,MessageId::CS_CHGPARTYTYPE_ACK,
            std::vector<std::byte>{static_cast<std::byte>(reply),static_cast<std::byte>(obtain)});
        // Source sends this only to party primaries and no extra ATTR.
        // Client raw party id/chief/commander therefore retain their source
        // values even when its loot-mode byte switches to SOLO1.
    }else if(op==MessageId::MW_PARTYADD_REQ) {
        if(reply) {
            std::vector<std::byte> answer;wire::WriteString(answer,origin);wire::WriteString(answer,target);wire::WritePOD(answer,reply);
            co_await Send(client,MessageId::CS_PARTYADD_ACK,std::move(answer));co_return;
        }
        if(unsupported_vitals) {
            spdlog::warn("Party invitation refused: native current vitals unsupported char={}",character);
            co_await Relay(client,MessageId::MW_PARTYJOIN_ACK,BusyUnavailable(origin,authority->name,obtain),ctx);co_return;
        }
        if(authority->action_blocked||authority->protected_inviter) {
            co_await Relay(client,MessageId::MW_PARTYJOIN_ACK,JoinAnswer(origin,authority->name,obtain,authority->action_blocked?2:1,*s),ctx);co_return;
        }
        std::vector<std::byte> answer;wire::WriteString(answer,origin);wire::WritePOD(answer,obtain);co_await Send(client,MessageId::CS_PARTYJOINASK_ACK,std::move(answer));
    }else if(op==MessageId::MW_PARTYJOIN_REQ) {
        ctx.char_state->Update(character,[&](CharSnapshot& state){state.cluster.party=party;state.cluster.party_chief=chief;state.cluster.commander=commander;state.cluster.party_type=obtain;});
        std::vector<std::byte> answer;wire::WritePOD(answer,obtain==1?std::uint16_t{0}:party);wire::WriteString(answer,target);
        wire::WritePOD(answer,member);wire::WritePOD(answer,obtain==1?std::uint32_t{0}:chief);wire::WritePOD(answer,obtain==1?std::uint16_t{0}:commander);
        wire::WriteString(answer,guild);wire::WritePOD(answer,level);wire::WritePOD(answer,max_hp);wire::WritePOD(answer,hp);
        wire::WritePOD(answer,max_mp);wire::WritePOD(answer,mp);wire::WritePOD(answer,race);wire::WritePOD(answer,sex);
        wire::WritePOD(answer,face);wire::WritePOD(answer,hair);wire::WritePOD(answer,obtain);wire::WritePOD(answer,klass);
        co_await Send(client,MessageId::CS_PARTYJOIN_ACK,std::move(answer));
    }else if(op==MessageId::MW_PARTYDEL_REQ) {
        std::vector<std::byte> answer;wire::WritePOD(answer,member);wire::WritePOD(answer,Chief(*s));wire::WritePOD(answer,Commander(*s));wire::WritePOD(answer,Party(*s));wire::WritePOD(answer,reply);
        co_await Send(client,MessageId::CS_PARTYDEL_ACK,std::move(answer));
    }else if(op==MessageId::MW_PARTYATTR_REQ) {
        ctx.char_state->Update(character,[&](CharSnapshot& state){state.cluster.party=party;state.cluster.party_chief=chief;state.cluster.commander=commander;state.cluster.party_type=obtain;});
        if(id->role!=MapSessionRole::Primary)co_return;
        std::vector<std::byte> answer;wire::WritePOD(answer,character);wire::WritePOD(answer,obtain==1?std::uint16_t{0}:party);wire::WritePOD(answer,obtain==1?std::uint32_t{0}:chief);wire::WritePOD(answer,obtain==1?std::uint16_t{0}:commander);
        co_await Send(client,MessageId::CS_PARTYATTR_ACK,answer);if(!Current(client,*id,ctx))co_return;
        const auto cell=[](float value)->int {return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
        const auto own_x=cell(s->fPosX),own_z=cell(s->fPosZ);
        if(own_x<0||own_z<0) {
            spdlog::warn("Party neighbor projection has no valid source cell char={}",character);co_return;
        }
        std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> neighbors;
        if(ctx.presence)ctx.presence->ForEachInChannel(id->channel,character,[&](const ChannelPresenceEntry& entry,auto recipient){
            const auto x=cell(entry.pos.x),z=cell(entry.pos.z);
            if(entry.map_id!=s->wMapID||x<0||z<0||std::abs(x-own_x)>1||std::abs(z-own_z)>1)return;
            const auto recipient_id=ctx.session_reg->Identity(recipient.get());if(recipient_id&&recipient_id->phase==SessionPhase::Ready)neighbors.emplace_back(*recipient_id,std::move(recipient));
        });
        for(const auto& [recipient_id,recipient]:neighbors) {
            if(!Current(client,*id,ctx))co_return;
            const auto now=ctx.session_reg->Identity(recipient.get());
            if(!recipient->IsOpen()||!now||!Same(recipient_id,*now)||!ctx.session_reg->BeginGameplay(recipient.get()))continue;
            SessionOperation neighbor_operation(*ctx.session_reg,recipient.get());
            const auto neighbor=ctx.char_state->Get(recipient_id.char_id);
            if(!neighbor||!co_await Authorize(recipient,recipient_id,*neighbor,ctx)||!Current(client,*id,ctx))continue;
            co_await Send(recipient,MessageId::CS_PARTYATTR_ACK,answer);
        }
        // Dynamic INDUN exit and arena cleanup await their native producers.
    }else co_await Send(client,MessageId::CS_PARTYMANSTAT_ACK,std::vector<std::byte>(body.begin()+8,body.end()));
}

boost::asio::awaitable<void> PublishPartyVitals(const std::shared_ptr<tnetlib::AsioSession>& client,
    const CharSnapshot& s,std::span<const std::byte> hpmp,const HandlerContext& ctx) {
    if(!s.cluster.hydrated||!Party(s)||s.persistence_uncertain)co_return;
    const auto id=ctx.session_reg->Identity(client.get());
    if(!id||id->role!=MapSessionRole::Primary||id->phase!=SessionPhase::Ready||id->char_id!=s.dwCharID||!Current(client,*id,ctx))co_return;
    const auto live=ctx.char_state?ctx.char_state->Get(id->char_id):std::nullopt;
    if(!live||live->persistence_uncertain||!SameNativePartyProjection(CaptureNativePartyProjection(s.cluster),live->cluster))co_return;
    wire::Reader r(hpmp.data(),hpmp.size());std::uint32_t object=0,max_hp=0,hp=0,max_mp=0,mp=0;std::uint8_t type=0;
    if(!r.Read(object)||!r.Read(type)||!r.Read(max_hp)||!r.Read(hp)||!r.Read(max_mp)||!r.Read(mp)||!r.Eof())throw std::runtime_error("Invalid native own HPMP for party");
    if(object!=id->char_id||type!=1)co_return;
    std::vector<std::byte> request;wire::WritePOD(request,Party(*live));wire::WritePOD(request,object);wire::WritePOD(request,type);wire::WritePOD(request,s.bLevel);
    wire::WritePOD(request,max_hp);wire::WritePOD(request,hp);wire::WritePOD(request,max_mp);wire::WritePOD(request,mp);
    co_await Relay(client,MessageId::MW_PARTYMANSTAT_ACK,std::move(request),ctx);
}
boost::asio::awaitable<void> RefreshReadyParty(const std::shared_ptr<tnetlib::AsioSession>& client,const HandlerContext& ctx) {
    if(!ctx.player_service||!ctx.player_service->HasNativePartyAuthority())co_return;
    const auto id=ctx.session_reg->Identity(client.get());
    if(!id||id->phase!=SessionPhase::Ready||!ctx.char_state)co_return;
    const auto before=ctx.char_state->Get(id->char_id);
    if(!before||!before->cluster.hydrated||before->persistence_uncertain) {
        client->Close();co_return;
    }
    if(!co_await Authorize(client,*id,*before,ctx)){client->Close();co_return;}
    // No new opcode or invented fields. The native World completed-connection
    // branch rechecks exact SQL ready actor/peer authority and refreshes ATTR;
    // it does not replay connection verdicts or import unknown replica vitals.
    co_await Relay(client,MessageId::MW_ENTERCHAR_ACK,EncodeEnterCharAck(id->char_id,id->key),ctx);
}
}
