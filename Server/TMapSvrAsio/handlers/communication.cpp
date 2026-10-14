#include "handlers.h"
#include "handlers_world.h"
#include "config.h"
#include "services/char_state_store.h"
#include "services/channel_presence.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "services/skill_cooldown.h"
#include "services/world_client.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include "packet_codec.h"
#include <spdlog/spdlog.h>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
void Text(const std::string& value) {
    if(value.find('\0')!=std::string::npos)throw std::runtime_error("NUL in communication text");
}
bool Same(const SessionIdentity& a,const SessionIdentity& b) {
    return a.char_id==b.char_id&&a.key==b.key&&a.connection_id==b.connection_id&&a.authority_epoch==b.authority_epoch&&
        b.phase==SessionPhase::Ready&&b.role==a.role;
}
bool SameProjection(const SessionIdentity& a,const SessionIdentity& b) {
    return a.char_id==b.char_id&&a.key==b.key&&a.connection_id==b.connection_id&&a.authority_epoch==b.authority_epoch&&
        b.role==MapSessionRole::Primary&&
        (b.phase==SessionPhase::Loaded||b.phase==SessionPhase::Admitted||b.phase==SessionPhase::Ready);
}
std::vector<std::byte> Prefix(const SessionIdentity& id) {
    std::vector<std::byte> out;wire::WritePOD(out,id.char_id);wire::WritePOD(out,id.key);return out;
}
boost::asio::awaitable<std::optional<CommunicationAuthority>> Authorize(
    const std::shared_ptr<tnetlib::AsioSession>& client,const SessionIdentity& id,const HandlerContext& ctx,
    bool chat=false,std::uint32_t sender=0,std::uint8_t option=0) {
    if(!ctx.player_service)throw std::runtime_error("Communication authority unavailable");
    auto* service=ctx.player_service;
    auto result=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [service,claim=id.Claim(ctx.expected_group),chat,sender,option]{return service->AuthorizeCommunication(claim,chat,sender,option);});
    const auto now=ctx.session_reg->Identity(client.get());
    if(!result||!client->IsOpen()||!now||!Same(id,*now))co_return std::nullopt;
    co_return result;
}
boost::asio::awaitable<bool> Projection(const std::shared_ptr<tnetlib::AsioSession>& client,
    const SessionIdentity& id,const HandlerContext& ctx,bool admission=false,const std::function<bool()>& origin={},bool close_on_timeout=true) {
    if(origin&&!origin())co_return false;
    boost::asio::steady_timer wait(co_await boost::asio::this_coro::executor);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!ctx.session_reg->BeginProjection(client.get())) {
        if(origin&&!origin())co_return false;
        const auto now=ctx.session_reg->Identity(client.get());
        if(!client->IsOpen()||!now||!(admission?SameProjection(id,*now):Same(id,*now)))co_return false;
        if(std::chrono::steady_clock::now()>=deadline){if(close_on_timeout)client->Close();co_return false;}
        wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
    }
    co_return true;
}
boost::asio::awaitable<void> Relay(std::shared_ptr<tnetlib::AsioSession> client,MessageId op,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    if(!client->IsOpen())co_return;
    // Source Plain16 requires total <65535. A larger relay envelope is a known codec rejection, not
    // a transport failure or permission to replay the request.
    if(body.size()>=kWorldPlainMaxPacketSize-kWorldPlainHeaderSize) {
        spdlog::warn("communication not relayed: original SS frame exceeds packet cap opcode={} body={}",
            static_cast<std::uint16_t>(op),body.size());
        co_return;
    }
    if(!ctx.world_client||!ctx.world_client->IsRegistered()||
       !co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(op),std::move(body)))client->Close();
}
std::uint8_t Aid(const CharSnapshot& s) {
    if(s.payload)return s.payload->aid_country;
    if(s.cluster.hydrated)return s.cluster.aid_country;
    throw std::runtime_error("Chat country was not hydrated");
}
bool CanTalk(const CharSnapshot& local,std::uint8_t group,std::uint8_t country,std::uint8_t aid) {
    if(local.bCountry==4||country==4||country==3)return true;
    if(group==4||group==5||group==6||group==8)return true;
    if(group==1&&(local.bCountry==2||country==2)) {
        if(!std::isfinite(local.fPosX)||!std::isfinite(local.fPosZ)||
           local.fPosX<std::numeric_limits<int>::min()||local.fPosX>=std::numeric_limits<int>::max()||
           local.fPosZ<std::numeric_limits<int>::min()||local.fPosZ>=std::numeric_limits<int>::max())
            throw std::runtime_error("Chat special unit is outside the source INT range");
        // MAKEWORD(BYTE(INT(x)/UNIT_SIZE),BYTE(INT(z)/UNIT_SIZE)) == 3.
        if(static_cast<std::uint8_t>(static_cast<int>(local.fPosX)/1024)==3&&
           static_cast<std::uint8_t>(static_cast<int>(local.fPosZ)/1024)==0)return true;
    }
    return (group==0||group==1||group==2||group==3||group==9)&&
        (Aid(local)!=3?Aid(local):local.bCountry)==(aid!=3?aid:country);
}
std::string Fold(std::string text) {
    for(char& c:text)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');return text;
}
std::vector<std::byte> ChatAck(std::uint8_t group,std::uint32_t sender,const std::string& name,const std::string& talk) {
    std::vector<std::byte> out;wire::WritePOD(out,group);wire::WritePOD(out,sender);
    wire::WriteString(out,name);wire::WriteString(out,talk);return out;
}
std::vector<std::byte> BanRequest(const std::string& name,std::uint16_t minutes,std::uint32_t gm=0,std::uint32_t key=0) {
    std::vector<std::byte> out;wire::WriteString(out,name);wire::WritePOD(out,minutes);wire::WritePOD(out,gm);wire::WritePOD(out,key);return out;
}
bool CheckChat(ChatFloodState& log,const std::string& talk,std::uint32_t tick) {
    unsigned code=0;const auto first=talk.substr(0,8);std::sscanf(first.c_str(),"%X",&code);
    const auto size=static_cast<std::uint16_t>(code);
    if(log.recent&&static_cast<std::uint32_t>(tick-log.recent)<10000) {
        log.size+=size;
        if(log.size>100){log.recent=tick;log.size=0;return true;}
    }else {log.recent=tick;log.size=0;}
    log.observations.push_back({tick,size});std::int32_t interval=0;
    for(std::size_t i=1;i<log.observations.size();++i) {
        const auto delta=static_cast<std::int32_t>(log.observations[i].tick-log.observations[i-1].tick);
        if(!interval)interval=delta;
        else if(std::abs(std::int64_t(interval)-delta)>1000){log.observations.clear();break;}
        else interval=delta;
    }
    if(log.observations.size()>8){log.observations.clear();return true;}
    return false;
}
enum class ChatRoute {Unavailable,Handled,Stopped};
boost::asio::awaitable<ChatRoute> DeliverChat(std::shared_ptr<tnetlib::AsioSession> client,const SessionIdentity& id,
    std::uint8_t group,std::uint32_t sender,const std::string& name,const std::string& talk,const HandlerContext& ctx,
    const std::function<bool(const CharSnapshot&)>& gate,bool sender_lease=false,const std::function<bool()>& origin={}) {
    if(origin&&!origin())co_return ChatRoute::Stopped;
    std::optional<SessionOperation> operation;
    if(!sender_lease) {
        boost::asio::steady_timer wait(co_await boost::asio::this_coro::executor);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!ctx.session_reg->BeginGameplay(client.get())) {
            const auto now=ctx.session_reg->Identity(client.get());
            if(!client->IsOpen()||!now||!Same(id,*now))co_return ChatRoute::Unavailable;
            if(std::chrono::steady_clock::now()>=deadline){client->Close();co_return ChatRoute::Unavailable;}
            wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
        }
        operation.emplace(*ctx.session_reg,client.get());
    }
    // Read protection in the same native claim transaction. A simultaneous
    // protected-list writer may still be publishing its cache after COMMIT.
    const auto authority=co_await Authorize(client,id,ctx,false,sender,1);
    if(origin&&!origin())co_return ChatRoute::Stopped;
    if(!authority)co_return ChatRoute::Unavailable;
    const auto s=ctx.char_state->Get(id.char_id);
    if(!s||(!s->payload&&!s->cluster.hydrated)||s->persistence_uncertain||!gate(*s))co_return ChatRoute::Unavailable;
    // A protected target still exists: source sender echo is independent of
    // recipient CSSender's suppression, so it must not cause a World resend.
    if(authority->protected_sender)co_return ChatRoute::Handled;
    auto packet=ChatAck(group,sender,name,talk);
    if(packet.size()>=tnetlib::kMaxPacketSize-tnetlib::kPacketHeaderSize) {
        spdlog::warn("chat not delivered: original CS frame exceeds packet cap char={} body={}",id.char_id,packet.size());
        co_return ChatRoute::Stopped;
    }
    // This API returns void after attempting to enqueue; it is not a delivery
    // receipt. Once attempted, a closed/replaced recipient must never cause a
    // World fallback that could repeat the same message.
    co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHAT_ACK),packet);
    const auto now=ctx.session_reg->Identity(client.get());
    co_return client->IsOpen()&&now&&Same(id,*now)&&(!origin||origin())?ChatRoute::Handled:ChatRoute::Stopped;
}
boost::asio::awaitable<void> WorldChat(std::vector<std::byte> body,const HandlerContext& ctx) {
    wire::Reader r(body);std::uint32_t character=0,key=0,sender=0,target=0;
    std::uint8_t channel=0,country=0,aid=0,type=0,group=0;std::string name,talk;
    if(!r.Read(character)||!r.Read(key)||!r.Read(channel)||!r.Read(sender)||!r.ReadString(name)||
       !r.Read(country)||!r.Read(aid)||!r.Read(type)||!r.Read(group)||!r.Read(target)||!r.ReadString(talk)||!r.Eof())
        throw std::runtime_error("Invalid World chat");Text(name);Text(talk);
    if(ctx.mode==Mode::Bow&&group!=0&&group!=4)co_return; // source BOW receive no-ops
    const bool global=group==2||group==3||group==9;
    if(!global&&group!=0&&group!=4&&group!=5&&group!=6&&group!=8)co_return;
    const auto gate=[=](const CharSnapshot& s){return CanTalk(s,group,country,aid);};
    if(global) {
        for(const auto& [id,client]:ctx.session_reg->ReadyPrimaries())
            co_await DeliverChat(client,id,type,sender,name,talk,ctx,gate);
    }else {
        auto client=ctx.session_reg->Find(character,key);const auto id=client?ctx.session_reg->Identity(client.get()):std::nullopt;
        if(id&&id->phase==SessionPhase::Ready&&id->role==MapSessionRole::Primary)
            co_await DeliverChat(client,*id,character==sender?group:type,sender,name,talk,ctx,gate);
    }
}
boost::asio::awaitable<void> WorldChatBan(std::vector<std::byte> body,const HandlerContext& ctx) {
    wire::Reader r(body);std::string name;std::int64_t ends=0;std::uint8_t result=0;std::uint32_t gm=0,key=0;
    if(!r.ReadString(name)||!r.Read(ends)||!r.Read(result)||!r.Read(gm)||!r.Read(key)||!r.Eof()||ends<0)
        throw std::runtime_error("Invalid World chat ban");Text(name);
    for(const auto& [id,client]:ctx.session_reg->PrimarySessions()) {
        const auto s=ctx.char_state->Get(id.char_id);
        if(!s||Fold(s->szNAME)!=Fold(name)||!co_await Projection(client,id,ctx,true))continue;
        SessionOperation operation(*ctx.session_reg,client.get());
        if(!ctx.player_service)throw std::runtime_error("Chat ban projection authority unavailable");
        auto* service=ctx.player_service;
        const bool confirmed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [service,claim=id.Claim(ctx.expected_group),canonical=s->szNAME,ends]{return service->ConfirmChatBan(claim,canonical,ends);});
        const auto now=ctx.session_reg->Identity(client.get());
        if(confirmed&&client->IsOpen()&&now&&SameProjection(id,*now))ctx.char_state->Update(id.char_id,[&](CharSnapshot& state){state.cluster.chat_ban_time=ends;});
    }
    if(gm&&key) {
        auto client=ctx.session_reg->Find(gm,key);const auto id=client?ctx.session_reg->Identity(client.get()):std::nullopt;
        if(!id||id->phase!=SessionPhase::Ready||id->role!=MapSessionRole::Primary||!co_await Projection(client,*id,ctx))co_return;
        SessionOperation operation(*ctx.session_reg,client.get());
        if(!co_await Authorize(client,*id,ctx))co_return;
        if(!result){auto* service=ctx.player_service;
            const auto confirmed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [service,claim=id->Claim(ctx.expected_group),name,ends]{return service->ConfirmChatBan(claim,name,ends,true);});
            if(!confirmed)co_return;
        }
        std::vector<std::byte> answer;wire::WritePOD(answer,result);
        co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHATBAN_ACK),std::move(answer));
    }
}
std::string NoticeText(const std::string& message) {
    Text(message);
    if(message.size()>65535)throw std::runtime_error("Notice exceeds original net-string WORD length");
    constexpr char digits[]="0123456789ABCDEF";std::string formatted="00000000";
    for(unsigned n=0;n<4;++n)formatted[7-n]=digits[(message.size()>>(n*4))&15];
    return formatted+message; // source BuildNetString(NAME_NULL, plain message)
}
boost::asio::awaitable<void> DeliverNotice(const std::shared_ptr<tnetlib::AsioSession>& client,const SessionIdentity& id,
    std::uint8_t group,const std::string& formatted,const HandlerContext& ctx,const std::function<bool()>& origin={}) {
    // A Control announcement cannot become an unintended kick merely because
    // a recipient's durable checkpoint is slow. Skip that bounded wait.
    if(!co_await Projection(client,id,ctx,false,origin,!origin))co_return;
    SessionOperation operation(*ctx.session_reg,client.get());
    if(origin&&!origin())co_return;
    if(!co_await Authorize(client,id,ctx))co_return;
    if(origin&&!origin())co_return;
    auto* service=ctx.player_service;
    const auto name=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [service,claim=id.Claim(ctx.expected_group)]{return service->NoticeOperatorName(claim);});
    const auto now=ctx.session_reg->Identity(client.get());
    if(!name||!client->IsOpen()||!now||!Same(id,*now)||(origin&&!origin()))co_return;
    // The eight hex bytes and both INT32 string prefixes count toward the
    // actual frame cap. Preserve full text, but never close a valid recipient
    // because an administrative notice cannot fit its original CS envelope.
    const auto body_size=1U+4U+4U+name->size()+4U+formatted.size();
    if(body_size>=tnetlib::kMaxPacketSize-tnetlib::kPacketHeaderSize) {
        spdlog::warn("notice not delivered: original CS frame exceeds packet cap char={} body={}",id.char_id,body_size);
        co_return;
    }
    // Sender zero still has source PROTECTED_CHAT. Notices have no
    // CheckChat/chat-ban/country gate and do not truncate their plain text.
    co_await DeliverChat(client,id,group,0,*name,formatted,ctx,[](const CharSnapshot&){return true;},true,origin);
}
boost::asio::awaitable<void> WorldNotice(std::vector<std::byte> body,const HandlerContext& ctx) {
    wire::Reader r(body);std::string target,message;
    if(!r.ReadString(target)||!r.ReadString(message)||!r.Eof())throw std::runtime_error("Invalid targeted notice");
    Text(target);const auto formatted=NoticeText(message);
    for(const auto& [id,client]:ctx.session_reg->ReadyPrimaries()) {
        const auto s=ctx.char_state->Get(id.char_id);
        if(!s||Fold(s->szNAME)!=Fold(target))continue;
        co_await DeliverNotice(client,id,7,formatted,ctx);co_return;
    }
}
boost::asio::awaitable<void> WorldEventNotice(std::vector<std::byte> body,const HandlerContext& ctx) {
    wire::Reader r(body);std::uint8_t event=0,type=0;std::string message;
    if(!r.Read(event)||!r.Read(type)||!r.ReadString(message)||!r.Eof())throw std::runtime_error("Invalid World event notice");
    // SSHandler16026 only broadcasts this text. The two BYTE fields do not
    // mutate active gameplay events or authorize a substitute event catalog.
    const auto formatted=NoticeText(message);
    for(const auto& [id,client]:ctx.session_reg->ReadyPrimaries())co_await DeliverNotice(client,id,3,formatted,ctx);
}
}

boost::asio::awaitable<void> OnControlAnnouncement(std::string message,const HandlerContext& ctx,std::function<bool()> origin) {
    if(!origin||!origin()||!ctx.session_reg||!ctx.char_state)co_return;
    const auto formatted=NoticeText(message);
    for(const auto& [id,client]:ctx.session_reg->ReadyPrimaries()) {
        if(!origin())co_return;
        co_await DeliverNotice(client,id,3,formatted,ctx,origin);
    }
}

boost::asio::awaitable<void> OnChatReq(std::shared_ptr<tnetlib::AsioSession> client,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!id||id->phase!=SessionPhase::Ready||id->role!=MapSessionRole::Primary||!ctx.char_state)co_return;
    const auto state=ctx.char_state->Get(id->char_id);
    if(!state||!state->payload||state->persistence_uncertain)co_return;
    wire::Reader r(body);std::string sender,name,talk;std::uint8_t group=0;std::uint32_t target=0;
    if(!r.ReadString(sender)||!r.Read(group)||!r.Read(target)||!r.ReadString(name)||!r.ReadString(talk))
        throw std::runtime_error("Invalid chat request");
    if(group==3||group==7)co_return; // source refuses these before sender/ban/tail checks
    Text(sender);Text(name);Text(talk);
    // The ordinary client's Relay near request appends a recipient list. Map
    // deliberately selects its own source 3x3 cells, not those client IDs.
    if(group==1&&!r.Eof()){std::uint16_t count=0;
        if(!r.Read(count)||r.Remaining()!=4U*count)throw std::runtime_error("Invalid chat near list");
        for(unsigned i=0;i<count;++i){std::uint32_t ignored=0;r.Read(ignored);}
    }
    if(!r.Eof())throw std::runtime_error("Invalid chat request tail");
    const auto authority=co_await Authorize(client,*id,ctx,true);if(!authority)co_return;
    const std::uint8_t type=authority->is_operator?7:group;
    if(sender!=state->szNAME){co_await Relay(client,MessageId::MW_CHATBAN_ACK,BanRequest(state->szNAME,30000),ctx);co_return;}
    if(group==9) {
        if(talk.find(state->szNAME)==std::string::npos||target!=talk.size())co_return;
    }else {
        bool ban=false;ctx.char_state->Update(id->char_id,[&](CharSnapshot& s){
            auto p=std::make_shared<CharacterPayload>(*s.payload);ban=CheckChat(p->chat_log,talk,static_cast<std::uint32_t>(SkillClockMs()));s.payload=std::move(p);
        });
        // CheckChat's FALSE result is ignored in the source. The async World
        // ban request does not itself discard this message.
        if(ban)co_await Relay(client,MessageId::MW_CHATBAN_ACK,BanRequest(state->szNAME,5),ctx);
    }
    if(!client->IsOpen())co_return;
    const auto now=static_cast<std::int64_t>(std::time(nullptr));
    if(now<authority->chat_ban_time) {
        std::vector<std::byte> answer;wire::WritePOD<std::uint8_t>(answer,10);
        wire::WritePOD<std::uint32_t>(answer,static_cast<std::uint32_t>(authority->chat_ban_time-now));
        co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_SYSTEMMSG_ACK),std::move(answer));co_return;
    }
    // strTalk.Left(1024) in the reference discards its return value.
    // Preserve the full original text rather than inventing a truncation.
    const auto local_gate=[&](const CharSnapshot& recipient){return CanTalk(*state,group,recipient.bCountry,Aid(recipient));};
    if(ctx.mode==Mode::Br&&group!=0&&group!=4)co_return;
    if(ctx.mode==Mode::Bow&&(group==1||group==2||group==5||group==8||group==9)) {
        // Native BOW peace-status publication is still absent. Same-country
        // delivery is independent of that missing cross-country peace branch.
        for(const auto& [recipient_id,recipient]:ctx.session_reg->ReadyPlayers())
            co_await DeliverChat(recipient,recipient_id,group,id->char_id,sender,talk,ctx,
                [&](const CharSnapshot& s){return s.bCountry==state->bCountry;},recipient.get()==client.get());
        co_return;
    }
    if(ctx.mode==Mode::Bow&&group==6)co_return;
    if(group==1) {
        if(ctx.mode!=Mode::PvE)co_return;
        const auto cell=[](float value)->int{return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
        if(cell(state->fPosX)<0||cell(state->fPosZ)<0)throw std::runtime_error("Chat neighbor origin is outside the source WORD range");
        for(const auto& [recipient_id,recipient]:ctx.session_reg->ReadyPlayers()) {
            if(recipient_id.channel!=id->channel)continue;
            co_await DeliverChat(recipient,recipient_id,group,id->char_id,sender,talk,ctx,[&](const CharSnapshot& s){
                return s.wMapID==state->wMapID&&cell(s.fPosX)>=0&&cell(s.fPosZ)>=0&&
                    std::abs(cell(s.fPosX)-cell(state->fPosX))<=1&&std::abs(cell(s.fPosZ)-cell(state->fPosZ))<=1&&
                    ((state->wMapID>=1100&&state->wMapID<=1105)||local_gate(s));
            },recipient.get()==client.get());
        }
        co_return;
    }
    if(group==0&&ctx.mode!=Mode::Br) {
        auto recipient=ctx.session_reg->Find(target);
        const auto eligible=[&](const auto& candidate){
            const auto candidate_id=candidate?ctx.session_reg->Identity(candidate.get()):std::nullopt;
            const auto s=candidate_id?ctx.char_state->Get(candidate_id->char_id):std::nullopt;
            return candidate_id&&candidate_id->phase==SessionPhase::Ready&&s&&(s->payload||s->cluster.hydrated)&&local_gate(*s);
        };
        if(!eligible(recipient)&&ctx.mode==Mode::PvE)for(const auto& [candidate_id,candidate]:ctx.session_reg->ReadyPlayers()) {
            const auto s=ctx.char_state->Get(candidate_id.char_id);
            if(s&&Fold(s->szNAME)==Fold(name)&&eligible(candidate)){recipient=candidate;break;}
        }
        const auto recipient_id=recipient?ctx.session_reg->Identity(recipient.get()):std::nullopt;
        const auto s=recipient_id?ctx.char_state->Get(recipient_id->char_id):std::nullopt;
        if(recipient_id&&recipient_id->phase==SessionPhase::Ready&&s&&(s->payload||s->cluster.hydrated)&&local_gate(*s)) {
            const auto route=co_await DeliverChat(recipient,*recipient_id,type,id->char_id,sender,talk,ctx,local_gate,recipient.get()==client.get());
            if(route==ChatRoute::Stopped)co_return;
            if(route==ChatRoute::Handled) {
                co_await DeliverChat(client,*id,group,id->char_id,s->szNAME,talk,ctx,[](const CharSnapshot&){return true;},true);co_return;
            }
        }
    }
    if(group==4) {
        // BR CHAT_PARTY is a local team-registry fanout, not the normal World
        // party relay. That independent producer is still unavailable.
        if(ctx.mode==Mode::Br)co_return;
        if(!state->cluster.hydrated||!ctx.player_service)co_return;
        auto* service=ctx.player_service;
        const auto party_actor=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [service,claim=id->Claim(ctx.expected_group),&state]{return service->AuthorizeParty(claim,*state);});
        const auto current=ctx.session_reg->Identity(client.get());
        if(!party_actor||!client->IsOpen()||!current||!Same(*id,*current))co_return;
        // CSHandler CHAT_PARTY replaces the client-provided target with the
        // source getter. World independently verifies current registry membership;
        // verified partyless/SOLO zero remains its ordinary no-recipient request.
        target=state->cluster.party_type==1?0:state->cluster.party;
    }
    if(group==5||group==6||group==8) {
        // Corps/guild/tactics targets require their native registries.
        // A zero-valued cache is not evidence of source registry membership.
        co_return;
    }
    std::vector<std::byte> packet;wire::WritePOD(packet,id->channel);wire::WritePOD(packet,id->char_id);wire::WritePOD(packet,id->key);
    wire::WriteString(packet,sender);wire::WritePOD(packet,type);wire::WritePOD(packet,group);wire::WritePOD(packet,target);
    wire::WriteString(packet,name);wire::WriteString(packet,talk);co_await Relay(client,MessageId::MW_CHAT_ACK,std::move(packet),ctx);
}

boost::asio::awaitable<void> OnChatBanReq(std::shared_ptr<tnetlib::AsioSession> client,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!id||id->phase!=SessionPhase::Ready||id->role!=MapSessionRole::Primary)co_return;
    const auto authority=co_await Authorize(client,*id,ctx,true);if(!authority)co_return;
    if(!authority->is_operator){std::vector<std::byte> answer;wire::WritePOD<std::uint8_t>(answer,1);
        co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHATBAN_ACK),std::move(answer));co_return;}
    wire::Reader r(body);std::string target;std::uint16_t minutes=0;
    if(!r.ReadString(target)||!r.Read(minutes)||!r.Eof())throw std::runtime_error("Invalid operator chat ban");Text(target);
    co_await Relay(client,MessageId::MW_CHATBAN_ACK,BanRequest(target,minutes,id->char_id,id->key),ctx);
}

boost::asio::awaitable<void> OnTmsReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!id||id->phase!=SessionPhase::Ready||id->role!=MapSessionRole::Primary||!ctx.char_state)co_return;
    const auto s=ctx.char_state->Get(id->char_id);
    if(!s||!s->payload||s->persistence_uncertain)co_return;
    wire::Reader r(body);std::uint32_t tms=0;MessageId relay{};
    if(!r.Read(tms))throw std::runtime_error("Invalid TMS request");
    switch(static_cast<MessageId>(opcode)) {
    case MessageId::CS_TMSSEND_REQ: {
        std::string message;if(!r.ReadString(message))throw std::runtime_error("Invalid TMS message");Text(message);
        relay=MessageId::MW_TMSSEND_ACK;break;
    }
    case MessageId::CS_TMSINVITE_REQ: {
        std::uint8_t count=0;if(!r.Read(count)||r.Remaining()!=4U*count)throw std::runtime_error("Invalid TMS invitation");
        for(unsigned i=0;i<count;++i){std::uint32_t target=0;if(!r.Read(target))throw std::runtime_error("Invalid TMS target");}
        relay=MessageId::MW_TMSINVITE_ACK;break;
    }
    case MessageId::CS_TMSOUT_REQ:relay=MessageId::MW_TMSOUT_ACK;break;
    default:throw std::runtime_error("Unknown TMS request");
    }
    if(!r.Eof())throw std::runtime_error("Invalid TMS request tail");
    if(!co_await Authorize(client,*id,ctx))co_return;
    // Source TMS is deliberately independent of chat bans and CheckChat.
    auto packet=Prefix(*id);packet.insert(packet.end(),body.begin(),body.end());
    co_await Relay(client,relay,std::move(packet),ctx);
}

boost::asio::awaitable<void> OnWorldCommunication(std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    if(!ctx.session_reg||!ctx.char_state||!ctx.world_client||!ctx.world_client->IsRegistered())co_return;
    if(static_cast<MessageId>(opcode)==MessageId::MW_CHAT_REQ){co_await WorldChat(std::move(body),ctx);co_return;}
    if(static_cast<MessageId>(opcode)==MessageId::MW_CHATBAN_REQ){co_await WorldChatBan(std::move(body),ctx);co_return;}
    if(static_cast<MessageId>(opcode)==MessageId::MW_CHARMSG_REQ){co_await WorldNotice(std::move(body),ctx);co_return;}
    if(static_cast<MessageId>(opcode)==MessageId::MW_EVENTMSG_REQ){co_await WorldEventNotice(std::move(body),ctx);co_return;}
    wire::Reader r(body);std::uint32_t character=0,key=0,tms=0;
    if(!r.Read(character)||!r.Read(key))throw std::runtime_error("Invalid World communication identity");
    auto client=ctx.session_reg->Find(character,key);
    const auto id=client?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!id||id->phase!=SessionPhase::Ready||id->role!=MapSessionRole::Primary)co_return;
    if(!co_await Projection(client,*id,ctx))co_return;
    SessionOperation projection(*ctx.session_reg,client.get());
    if(!co_await Authorize(client,*id,ctx))co_return;
    const auto state=ctx.char_state->Get(character);
    if(!state||!state->payload||state->persistence_uncertain)co_return;
    MessageId ack{};
    switch(static_cast<MessageId>(opcode)) {
    case MessageId::MW_TMSINVITEASK_REQ: {
        std::uint32_t inviter=0,inviter_key=0;std::string message;
        if(!r.Read(inviter)||!r.Read(inviter_key)||!r.Read(tms)||!r.ReadString(message)||!r.Eof())
            throw std::runtime_error("Invalid TMS invitation protection query");
        Text(message);
        const auto authority=co_await Authorize(client,*id,ctx,false,inviter,2);if(!authority)co_return;
        std::vector<std::byte> answer;
        wire::WritePOD(answer,inviter);wire::WritePOD(answer,inviter_key);
        wire::WritePOD(answer,character);wire::WritePOD(answer,key);
        wire::WritePOD<std::uint8_t>(answer,authority->protected_sender?0:1);
        wire::WritePOD(answer,tms);wire::WriteString(answer,message);
        co_await Relay(client,MessageId::MW_TMSINVITEASK_ACK,std::move(answer),ctx);co_return;
    }
    case MessageId::MW_TMSRECV_REQ: {
        std::string sender,message;if(!r.Read(tms)||!r.ReadString(sender)||!r.ReadString(message))
            throw std::runtime_error("Invalid TMS receive");Text(sender);Text(message);ack=MessageId::CS_TMSRECV_ACK;break;
    }
    case MessageId::MW_TMSOUT_REQ: {
        std::string member;if(!r.Read(tms)||!r.ReadString(member))throw std::runtime_error("Invalid TMS departure");
        Text(member);ack=MessageId::CS_TMSOUT_ACK;break;
    }
    case MessageId::MW_TMSINVITE_REQ: {
        std::uint32_t inviter=0;std::uint8_t count=0;
        if(!r.Read(inviter)||!r.Read(tms)||!r.Read(count))throw std::runtime_error("Invalid TMS roster");
        for(unsigned i=0;i<count;++i){std::uint32_t target=0;std::uint8_t klass=0,level=0;std::string name;
            if(!r.Read(target)||!r.ReadString(name)||!r.Read(klass)||!r.Read(level))throw std::runtime_error("Invalid TMS roster member");Text(name);}
        ack=MessageId::CS_TMSINVITE_ACK;break;
    }
    default:throw std::runtime_error("Unknown World communication response");
    }
    if(!r.Eof())throw std::runtime_error("Invalid World TMS tail");
    // CSSender copies the complete original suffix after the two DWORDs.
    std::vector<std::byte> answer(body.begin()+8,body.end());
    co_await client->SendPacket(static_cast<std::uint16_t>(ack),std::move(answer));
}
}
