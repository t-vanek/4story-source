#include "handlers.h"
#include "handlers_world.h"
#include "config.h"
#include "services/char_state_store.h"
#include "services/session_registry.h"
#include "services/world_client.h"
#include "services/secure_code.h"
#include "services/player_service.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
void Text(const std::string& value) {
    if(value.find('\0')!=std::string::npos)throw std::runtime_error("NUL in friend wire text");
}
std::vector<std::byte> IdentityPrefix(const SessionIdentity& identity) {
    std::vector<std::byte> out;wire::WritePOD<std::uint32_t>(out,identity.char_id);
    wire::WritePOD<std::uint32_t>(out,identity.key);return out;
}
std::vector<std::byte> GroupError(std::uint8_t group,const std::string& name) {
    std::vector<std::byte> out;wire::WritePOD<std::uint8_t>(out,3); // FRIEND_NOTFOUND
    wire::WritePOD<std::uint8_t>(out,group);wire::WriteString(out,name);return out;
}
bool Protected(const CharSnapshot& state,const std::string& name) {
    if(!state.payload)throw std::runtime_error("Friend protection state unavailable");
    return std::any_of(state.payload->protected_characters.begin(),state.payload->protected_characters.end(),
        [&](const auto& p){return p.name==name;}); // Source ignores option here.
}
}
boost::asio::awaitable<void> OnFriendReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto id=static_cast<MessageId>(opcode);
    const bool basic=id==MessageId::CS_FRIENDLIST_REQ||id==MessageId::CS_FRIENDASK_REQ||
        id==MessageId::CS_FRIENDREPLY_REQ||id==MessageId::CS_FRIENDERASE_REQ;
    if((basic||id==MessageId::CS_SOULMATEEND_REQ)&&ctx.mode==Mode::Bow)co_return; // Original BOW_COMPILE_MODE no-op.
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary)co_return;
    if(!ctx.char_state||!ctx.world_client||!ctx.world_client->IsRegistered())
        throw std::runtime_error("Friend relay dependencies unavailable");
    const auto state=ctx.char_state->Get(identity->char_id);
    if(!state||!state->payload||state->persistence_uncertain)co_return;
    wire::Reader r(body);MessageId relay{};std::uint8_t group=0,reply=0;std::uint32_t target=0;std::string name;
    const bool secured=id==MessageId::CS_FRIENDERASE_REQ||id==MessageId::CS_FRIENDGROUPDELETE_REQ;
    // Source security checks precede decoding the protected operation.
    if(secured) {
        if(!state->payload->security.hydrated)throw std::runtime_error("Friend secure state not hydrated");
        if(state->payload->security.Blocks()) {
            const auto prompt=SecureUnlockPrompt(state->payload->security);
            co_await client->SendPacket(prompt.first,prompt.second);co_return;
        }
    }
    switch(id) {
    case MessageId::CS_SOULMATEEND_REQ:
        // CTPlayer::ProtectTutorial and OnCS_SOULMATEEND_REQ. The relation
        // belongs to World SQL; a Map checkpoint never restores this cache.
        if(state->bStartAct==2||(state->bStartAct==1&&state->wMapID==2010)||!state->cluster.soulmate)co_return;
        relay=MessageId::MW_SOULMATEEND_ACK;break;
    case MessageId::CS_FRIENDLIST_REQ:relay=MessageId::MW_FRIENDLIST_ACK;break;
    case MessageId::CS_FRIENDASK_REQ:
        if(!r.ReadString(name))throw std::runtime_error("Invalid FRIENDASK");
        Text(name);
        if(name==state->szNAME)co_return;
        if(Protected(*state,name)) {
            std::vector<std::byte> answer;wire::WritePOD<std::uint8_t>(answer,1); // FRIEND_REFUSE
            wire::WritePOD<std::uint32_t>(answer,0);wire::WriteString(answer,name);
            for(unsigned i=0;i<3;++i)wire::WritePOD<std::uint8_t>(answer,0);
            wire::WritePOD<std::uint32_t>(answer,0);
            co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_FRIENDADD_ACK),answer);co_return;
        }
        relay=MessageId::MW_FRIENDASK_ACK;break;
    case MessageId::CS_FRIENDREPLY_REQ:
        if(!r.ReadString(name)||!r.Read(reply))throw std::runtime_error("Invalid FRIENDREPLY");
        Text(name);
        relay=MessageId::MW_FRIENDREPLY_ACK;break;
    case MessageId::CS_FRIENDERASE_REQ:
        if(!r.Read(target))throw std::runtime_error("Invalid FRIENDERASE");
        relay=MessageId::MW_FRIENDERASE_ACK;break;
    case MessageId::CS_FRIENDGROUPMAKE_REQ:case MessageId::CS_FRIENDGROUPNAME_REQ:
        if(!r.Read(group)||!r.ReadString(name))throw std::runtime_error("Invalid FRIENDGROUP text");
        Text(name);
        if(name.size()>50) {
            co_await client->SendPacket(static_cast<std::uint16_t>(id==MessageId::CS_FRIENDGROUPMAKE_REQ?
                MessageId::CS_FRIENDGROUPMAKE_ACK:MessageId::CS_FRIENDGROUPNAME_ACK),GroupError(group,name));
            if(id==MessageId::CS_FRIENDGROUPMAKE_REQ)co_return;
            // Original rename sends NOTFOUND but still forwards to World,
            // whose MAX_GROUPNAME20 validation silently declines the rename.
        }
        relay=id==MessageId::CS_FRIENDGROUPMAKE_REQ?MessageId::MW_FRIENDGROUPMAKE_ACK:MessageId::MW_FRIENDGROUPNAME_ACK;break;
    case MessageId::CS_FRIENDGROUPDELETE_REQ:
        if(!r.Read(group))throw std::runtime_error("Invalid FRIENDGROUPDELETE");
        relay=MessageId::MW_FRIENDGROUPDELETE_ACK;break;
    case MessageId::CS_FRIENDGROUPCHANGE_REQ:
        if(!r.Read(target)||!r.Read(group))throw std::runtime_error("Invalid FRIENDGROUPCHANGE");
        relay=MessageId::MW_FRIENDGROUPCHANGE_ACK;break;
    default:throw std::runtime_error("Unknown client friend operation");
    }
    if(!r.Eof())throw std::runtime_error("Invalid friend request tail");
    auto packet=IdentityPrefix(*identity);packet.insert(packet.end(),body.begin(),body.end());
    if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(relay),std::move(packet)))client->Close();
}
boost::asio::awaitable<void> OnWorldFriendReq(std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    if(!ctx.session_reg||!ctx.char_state||!ctx.world_client||!ctx.world_client->IsRegistered())co_return;
    wire::Reader r(body);std::uint32_t character=0,key=0,target=0,region=0;
    if(!r.Read(character)||!r.Read(key))throw std::runtime_error("Invalid World friend identity");
    const auto client=ctx.session_reg->Find(character,key);
    const auto identity=client?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->role!=MapSessionRole::Primary||
       (identity->phase!=SessionPhase::Loaded&&identity->phase!=SessionPhase::Admitted&&identity->phase!=SessionPhase::Ready))co_return;
    // A pending native protection/secure/item transaction must publish its
    // committed projection before a remote invitation reads the current list.
    // Client friend sends do not wait for a World reply, so there is no held
    // request lease waiting for this callback.
    boost::asio::steady_timer wait(co_await boost::asio::this_coro::executor);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(identity->phase==SessionPhase::Ready&&ctx.session_reg->HasOperation(client.get())) {
        if(std::chrono::steady_clock::now()>=deadline){client->Close();co_return;}
        const auto current=ctx.session_reg->Identity(client.get());
        if(!client->IsOpen()||!current||current->key!=key||current->connection_id!=identity->connection_id||current->phase!=SessionPhase::Ready)co_return;
        wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
    }
    const auto current=ctx.session_reg->Identity(client.get());
    if(!client->IsOpen()||!current||current->key!=key||current->connection_id!=identity->connection_id||current->role!=MapSessionRole::Primary||
       (current->phase!=SessionPhase::Loaded&&current->phase!=SessionPhase::Admitted&&current->phase!=SessionPhase::Ready))co_return;
    const auto state=ctx.char_state->Get(character);if(!state||!state->payload||state->persistence_uncertain)co_return;
    if(static_cast<MessageId>(opcode)==MessageId::MW_SOULMATESEARCH_REQ||static_cast<MessageId>(opcode)==MessageId::MW_SOULMATEREG_REQ) {
        if(current->phase!=SessionPhase::Ready)co_return;
        // The original request lease ended after its World relay. Take an
        // exclusive checkpoint lease for its delayed, atomic Map finalizer.
        while(!ctx.session_reg->BeginCheckpoint(client.get())) {
            if(std::chrono::steady_clock::now()>=deadline){client->Close();co_return;}
            const auto now=ctx.session_reg->Identity(client.get());
            if(!client->IsOpen()||!now||now->phase!=SessionPhase::Ready||now->role!=MapSessionRole::Primary||
               now->key!=key||now->connection_id!=identity->connection_id)co_return;
            wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
        }
        SessionOperation operation(*ctx.session_reg,client.get());
        co_await OnNativeQuestReq(client,opcode,std::move(body),ctx);co_return;
    }
    MessageId response{};std::string name;std::uint8_t result=0,group=0,level=0,klass=0,connected=0,count=0;
    switch(static_cast<MessageId>(opcode)) {
    case MessageId::MW_SOULMATE_REQ: {
        std::uint32_t silence=0;
        if(!r.Read(target)||!r.ReadString(name)||!r.Read(silence)||!r.Eof())throw std::runtime_error("Invalid soulmate state");
        Text(name);
        while(!ctx.session_reg->BeginProjection(client.get())) {
            if(std::chrono::steady_clock::now()>=deadline){client->Close();co_return;}
            const auto now=ctx.session_reg->Identity(client.get());
            if(!client->IsOpen()||!now||now->key!=key||now->connection_id!=identity->connection_id||now->role!=MapSessionRole::Primary||
               (now->phase!=SessionPhase::Loaded&&now->phase!=SessionPhase::Admitted&&now->phase!=SessionPhase::Ready))co_return;
            wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
        }
        SessionOperation projection(*ctx.session_reg,client.get());
        if(!ctx.player_service)throw std::runtime_error("Soulmate projection authority unavailable");
        auto* service=ctx.player_service;
        const auto confirmed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [service,claim=current->Claim(ctx.expected_group),target,name,silence]{return service->ConfirmSoulmateState(claim,target,name,silence);});
        const auto now=ctx.session_reg->Identity(client.get());
        if(!confirmed||!client->IsOpen()||!now||now->role!=MapSessionRole::Primary||now->key!=key||now->connection_id!=identity->connection_id)co_return;
        ctx.char_state->Update(character,[&](CharSnapshot& s){
            s.cluster.soulmate=target;s.cluster.soulmate_name=name;s.cluster.soul_silence=silence;
        });
        // Source OnMW_SOULMATE_REQ suppresses CS_ACK for a silence-only row.
        if(target) {
            std::vector<std::byte> answer;wire::WritePOD(answer,target);wire::WriteString(answer,name);
            co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_SOULMATE_ACK),answer);
        }
        co_return;
    }
    case MessageId::MW_SOULMATEEND_REQ: {
        std::uint32_t silence=0;
        if(!r.Read(result)||!r.Read(silence)||!r.Eof())throw std::runtime_error("Invalid soulmate end result");
        std::optional<SessionOperation> projection;
        if(result==0) {
            while(!ctx.session_reg->BeginProjection(client.get())) {
                if(std::chrono::steady_clock::now()>=deadline){client->Close();co_return;}
                const auto now=ctx.session_reg->Identity(client.get());
                if(!client->IsOpen()||!now||now->key!=key||now->connection_id!=identity->connection_id||now->role!=MapSessionRole::Primary||
                   (now->phase!=SessionPhase::Loaded&&now->phase!=SessionPhase::Admitted&&now->phase!=SessionPhase::Ready))co_return;
                wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
            }
            projection.emplace(*ctx.session_reg,client.get());
            if(!ctx.player_service)throw std::runtime_error("Soulmate end projection authority unavailable");
            auto* service=ctx.player_service;
            const auto confirmed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [service,claim=current->Claim(ctx.expected_group),silence]{return service->ConfirmSoulmateState(claim,0,{},silence);});
            const auto now=ctx.session_reg->Identity(client.get());
            if(!confirmed||!client->IsOpen()||!now||now->role!=MapSessionRole::Primary||now->key!=key||now->connection_id!=identity->connection_id)co_return;
        }
        if(result==0)ctx.char_state->Update(character,[&](CharSnapshot& s){
            s.cluster.soulmate=0;s.cluster.soulmate_name.clear();s.cluster.soul_silence=silence;
        });
        std::vector<std::byte> answer;wire::WritePOD(answer,result);
        co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_SOULMATEEND_ACK),answer);
        co_return;
    }
    case MessageId::MW_FRIENDLIST_REQ:
        if(!r.Read(target))throw std::runtime_error("Invalid friend list soulmate");
        if(target&&(!r.ReadString(name)||!r.Read(level)||!r.Read(klass)||!r.Read(connected)||!r.Read(region)))
            throw std::runtime_error("Missing original soulmate descriptor");
        if(!r.Read(count))throw std::runtime_error("Invalid friend group count");
        for(unsigned n=0;n<count;++n)if(!r.Read(group)||!r.ReadString(name))throw std::runtime_error("Invalid friend groups");
        if(!r.Read(count))throw std::runtime_error("Invalid friend count");
        for(unsigned n=0;n<count;++n)if(!r.Read(target)||!r.ReadString(name)||!r.Read(level)||!r.Read(group)||
            !r.Read(klass)||!r.Read(connected)||!r.Read(region))throw std::runtime_error("Invalid friend rows");
        response=MessageId::CS_FRIENDLIST_ACK;break;
    case MessageId::MW_FRIENDADD_REQ:
        if(!r.Read(result)||!r.Read(target)||!r.ReadString(name)||!r.Read(level)||!r.Read(group)||!r.Read(klass)||!r.Read(region))
            throw std::runtime_error("Invalid friend add result");
        response=MessageId::CS_FRIENDADD_ACK;break;
    case MessageId::MW_FRIENDASK_REQ:
        if(!r.ReadString(name)||!r.Read(target)||!r.Eof())throw std::runtime_error("Invalid friend dialog");
        Text(name);
        if(Protected(*state,name)) {
            auto answer=IdentityPrefix(*identity);wire::WriteString(answer,name);
            if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_FRIENDPROTECTEDASK_ACK),answer))client->Close();
            co_return;
        } else {
            std::vector<std::byte> answer;wire::WriteString(answer,name);
            co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_FRIENDASK_ACK),answer);co_return;
        }
    case MessageId::MW_FRIENDERASE_REQ:
        if(!r.Read(result)||!r.Read(target))throw std::runtime_error("Invalid friend erase result");
        response=MessageId::CS_FRIENDERASE_ACK;break;
    case MessageId::MW_FRIENDGROUPMAKE_REQ:case MessageId::MW_FRIENDGROUPNAME_REQ:
        if(!r.Read(result)||!r.Read(group)||!r.ReadString(name))throw std::runtime_error("Invalid friend group result");
        response=static_cast<MessageId>(opcode)==MessageId::MW_FRIENDGROUPMAKE_REQ?MessageId::CS_FRIENDGROUPMAKE_ACK:MessageId::CS_FRIENDGROUPNAME_ACK;break;
    case MessageId::MW_FRIENDGROUPDELETE_REQ:
        if(!r.Read(result)||!r.Read(group))throw std::runtime_error("Invalid group erase result");
        response=MessageId::CS_FRIENDGROUPDELETE_ACK;break;
    case MessageId::MW_FRIENDGROUPCHANGE_REQ:
        if(!r.Read(result)||!r.Read(group)||!r.Read(target))throw std::runtime_error("Invalid friend bucket result");
        response=MessageId::CS_FRIENDGROUPCHANGE_ACK;break;
    case MessageId::MW_FRIENDCONNECTION_REQ:
        if(!r.Read(result)||!r.ReadString(name)||!r.Read(region))throw std::runtime_error("Invalid friend presence");
        response=MessageId::CS_FRIENDCONNECTION_ACK;break;
    case MessageId::MW_FRIENDREGION_REQ:
        if(!r.ReadString(name)||!r.Read(region))throw std::runtime_error("Invalid friend region");
        response=MessageId::CS_FRIENDREGION_ACK;break;
    default:throw std::runtime_error("Unknown World friend operation");
    }
    if(!r.Eof())throw std::runtime_error("Invalid World friend result tail");
    // Original SSHandler/CSSender copy every post-identity field unchanged.
    // No struct memcpy, UTF reinterpretation or recomputed response values.
    co_await client->SendPacket(static_cast<std::uint16_t>(response),std::vector<std::byte>(body.begin()+8,body.end()));
}
}
