#include "handlers.h"
#include "handlers_world.h"
#include "config.h"
#include "services/session_registry.h"
#include "services/player_service.h"
#include "services/world_client.h"
#include "services/char_state_store.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"
#include "MessageId.h"
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
boost::asio::awaitable<void> OnWorldRpsPacket(std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    using tnetlib::protocol::MessageId;
    if(!ctx.world_client||!ctx.world_client->IsRegistered()||!ctx.player_service)
        throw std::runtime_error("RPS packet requires the current registered World");
    if(static_cast<MessageId>(opcode)==MessageId::MW_RPSGAMECHANGE_REQ) {
        auto* service=ctx.player_service;
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,[service,body]{service->UpdateRpsChart(body);});
        co_return;
    }
    if(ctx.mode!=Mode::PvE)co_return;
    wire::Reader reader(body.data(),body.size());std::uint32_t cid=0,key=0;std::uint8_t result=0,card=0;
    if(!reader.Read(cid)||!reader.Read(key)||!reader.Read(result)||!reader.Read(card)||!reader.Eof()||result>1)
        throw std::runtime_error("Invalid source World RPS result");
    auto client=ctx.session_reg?ctx.session_reg->Find(cid,key):nullptr;
    const auto identity=client?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary||!ctx.char_state)co_return;
    const auto current=[&] {
        const auto now=ctx.session_reg->Identity(client.get());
        return client->IsOpen()&&ctx.world_client->IsRegistered()&&now&&now->phase==SessionPhase::Ready&&
            now->char_id==cid&&now->key==key&&now->user_id==identity->user_id&&now->channel==identity->channel&&
            now->role==identity->role&&now->authority_epoch==identity->authority_epoch&&now->connection_id==identity->connection_id;
    };
    boost::asio::steady_timer wait(co_await boost::asio::this_coro::executor);
    while(!ctx.session_reg->BeginGameplay(client.get())) {
        if(!current())co_return;
        wait.expires_after(std::chrono::milliseconds(5));boost::system::error_code error;
        co_await wait.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable,error));
        if(error)co_return;
    }
    SessionOperation operation(*ctx.session_reg,client.get());
    if(!current())co_return;
    const auto live=ctx.char_state->Get(cid);
    if(!live||!live->rps.pending||live->rps.card!=card||live->rps.epoch!=identity->authority_epoch||
       live->rps.connection!=identity->connection_id)co_return;
    // The real World opcode and original ten-byte body enter the same native
    // claim/inventory/checkpoint transaction as the client producer.
    co_await OnNativeQuestReq(client,opcode,std::move(body),ctx);
}
}
