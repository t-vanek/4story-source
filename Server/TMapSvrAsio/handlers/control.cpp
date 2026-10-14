#include "handlers.h"
#include "map_server.h"
#include "services/char_state_store.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <stdexcept>

namespace tmapsvr {
namespace {
std::string Fold(std::string name) {for(char& c:name)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');return name;}
bool Same(const SessionIdentity& a,const SessionIdentity& b) {
    return a.char_id==b.char_id&&a.key==b.key&&a.connection_id==b.connection_id&&a.authority_epoch==b.authority_epoch&&
        a.role==b.role&&b.phase==SessionPhase::Ready;
}
void ClientControl(const std::shared_ptr<tnetlib::AsioSession>& client) {
    spdlog::warn("Map rejected Control entry on game-client transport");client->Close();
}
boost::asio::awaitable<void> Kick(const std::shared_ptr<ControlSession>& peer,const std::string& target,
    const HandlerContext& ctx,MapServer& server) {
    if(!ctx.session_reg||!ctx.char_state||!ctx.player_service)throw std::runtime_error("Native Control kick services absent");
    // Source name index contains entered primary AND replica players. It does
    // not name pre-auth connections or permit a guessed account-name match.
    for(const auto& [id,client]:ctx.session_reg->ReadyPlayers()) {
        const auto state=ctx.char_state->Get(id.char_id);
        if(!state||state->persistence_uncertain||Fold(state->szNAME)!=Fold(target))continue;
        boost::asio::steady_timer wait(co_await boost::asio::this_coro::executor);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!ctx.session_reg->BeginGameplay(client.get())) {
            const auto current=ctx.session_reg->Identity(client.get());
            if(!server.IsCurrentControl(peer)||!client->IsOpen()||!current||!Same(id,*current))co_return;
            if(std::chrono::steady_clock::now()>=deadline)co_return;
            wait.expires_after(std::chrono::milliseconds(5));co_await wait.async_wait(boost::asio::use_awaitable);
        }
        SessionOperation operation(*ctx.session_reg,client.get());
        if(!server.IsCurrentControl(peer))co_return;
        auto* service=ctx.player_service;
        const auto allowed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [service,claim=id.Claim(ctx.expected_group),canonical=state->szNAME]{return service->AuthorizeControlKick(claim,canonical);});
        const auto current=ctx.session_reg->Identity(client.get());const auto after=ctx.char_state->Get(id.char_id);
        if(!allowed||!server.IsCurrentControl(peer)||!client->IsOpen()||!current||!Same(id,*current)||
           !after||after->persistence_uncertain||after->szNAME!=state->szNAME)co_return;
        // Normal HandleConnection drains operations, saves the primary,
        // releases its exact claim and sends MW_CLOSE. Replicas release only
        // their own projection. Unknown saves retain their reservation.
        client->Close();co_return;
    }
}
}

boost::asio::awaitable<void> OnControlPacket(std::shared_ptr<ControlSession> peer,std::uint16_t opcode,
    std::vector<std::byte> body,const HandlerContext& ctx,MapServer& server) {
    if(!server.IsCurrentControl(peer))co_return;
    using tnetlib::protocol::MessageId;wire::Reader r(body);
    switch(static_cast<MessageId>(opcode)) {
    case MessageId::CT_CTRLSVR_REQ:
        if(!body.empty())throw std::runtime_error("Invalid empty Control handshake");
        co_return; // actual source CSHandler230: no reply or mutation
    case MessageId::CT_SERVICEDATACLEAR_ACK:
        if(!body.empty()||!ctx.session_reg)throw std::runtime_error("Invalid Control service-data clear");
        ctx.session_reg->ClearServiceData();co_return; // historical account set only
    case MessageId::CT_SERVICEMONITOR_ACK: {
        std::uint32_t tick=0;if(!r.Read(tick)||!r.Eof()||!ctx.session_reg)throw std::runtime_error("Invalid Control monitor");
        const auto data=ctx.session_reg->ServiceData();std::vector<std::byte> answer;
        wire::WritePOD(answer,tick);wire::WritePOD(answer,server.SourceSessionCount());
        wire::WritePOD(answer,data.players);wire::WritePOD(answer,data.active_users);
        co_await peer->SendPacket(static_cast<std::uint16_t>(MessageId::CT_SERVICEMONITOR_REQ),std::move(answer));
        if(server.IsCurrentControl(peer))server.SweepControlClients();co_return;
    }
    case MessageId::CT_ANNOUNCEMENT_ACK:
    case MessageId::CT_USERKICKOUT_ACK: {
        std::string text;if(!r.ReadString(text)||!r.Eof()||text.find('\0')!=std::string::npos)throw std::runtime_error("Invalid Control text");
        if(static_cast<MessageId>(opcode)==MessageId::CT_USERKICKOUT_ACK) {
            if(text.empty()||text.size()>50)co_return;
            co_await Kick(peer,text,ctx,server);
        }else co_await OnControlAnnouncement(std::move(text),ctx,[&server,peer]{return server.IsCurrentControl(peer);});
        co_return;
    }
    default:
        // Source's other administrative/gameplay contracts remain unported.
        // Never interpret a CS packet as identity promotion on this transport.
        spdlog::warn("Map Control unsupported opcode={}",opcode);peer->Close();co_return;
    }
}

// Retain the old exported signatures for callers, but never expose effects
// through a game AsioSession. Actual transport uses OnControlPacket above.
boost::asio::awaitable<void> OnCtAnnouncementAck(std::shared_ptr<tnetlib::AsioSession> s,std::vector<std::byte>,const HandlerContext&){ClientControl(s);co_return;}
boost::asio::awaitable<void> OnCtUserKickoutAck(std::shared_ptr<tnetlib::AsioSession> s,std::vector<std::byte>,const HandlerContext&){ClientControl(s);co_return;}
boost::asio::awaitable<void> OnCtServiceMonitorAck(std::shared_ptr<tnetlib::AsioSession> s,std::vector<std::byte>,const HandlerContext&){ClientControl(s);co_return;}
boost::asio::awaitable<void> OnCtServiceDataClearAck(std::shared_ptr<tnetlib::AsioSession> s,std::vector<std::byte>,const HandlerContext&){ClientControl(s);co_return;}
boost::asio::awaitable<void> OnCtCtrlSvrReq(std::shared_ptr<tnetlib::AsioSession> s,std::vector<std::byte>,const HandlerContext&){ClientControl(s);co_return;}
}
