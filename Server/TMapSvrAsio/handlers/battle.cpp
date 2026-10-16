#include "handlers.h"
#include "config.h"
#include "services/char_state_store.h"
#include "services/channel_presence.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "services/world_client.h"
#include "fourstory/db/co_offload.h"
#include "MessageId.h"
#include <stdexcept>

namespace tmapsvr {
boost::asio::awaitable<void> OnHeroListReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    // Source has no tutorial/death/guild/valid-region filter for this reader.
    if(ctx.mode==Mode::Bow)co_return;
    if(!body.empty())throw std::runtime_error("Invalid HEROLIST request length");
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary)co_return;
    if(!ctx.player_service||!ctx.char_state||!ctx.presence)throw std::runtime_error("HEROLIST dependencies unavailable");
    if(!ctx.world_client||!ctx.world_client->IsRegistered()||!ctx.player_service->BattleRegionsCurrent())co_return;
    const auto state=ctx.char_state->Get(identity->char_id);
    const auto entry=ctx.presence->FindEntry(identity->char_id);
    if(!state||state->persistence_uncertain||!entry||entry->map_id!=state->wMapID||
       entry->channel!=identity->channel||entry->session.lock()!=client)co_return;
    // Dispatch retains the per-session gameplay lease while the owned buffer
    // is built on a DB worker. Disconnect/transfer may still invalidate it.
    auto* service=ctx.player_service;
    auto packet=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group)] {
        return service->BattleHeroList(claim);
    });
    const auto current=ctx.session_reg->Identity(client.get());
    if(!client->IsOpen()||!ctx.world_client->IsRegistered()||!service->BattleRegionsCurrent()||
       !current||current->phase!=SessionPhase::Ready||current->role!=identity->role||
       current->char_id!=identity->char_id||current->user_id!=identity->user_id||current->key!=identity->key||
       current->channel!=identity->channel||current->connection_id!=identity->connection_id||current->authority_epoch!=identity->authority_epoch)co_return;
    co_await client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_HEROLIST_ACK),std::move(packet));
}
} // namespace tmapsvr
