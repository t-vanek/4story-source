#include "handlers.h"
#include "services/inventory_move.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/main_transfer_runtime.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "fourstory/db/co_offload.h"
#include "MessageId.h"

namespace tmapsvr {
boost::asio::awaitable<void> OnMoveItemReq(std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    using tnetlib::protocol::MessageId;
    if(body.size()!=5)throw std::runtime_error("Invalid MOVEITEM request length");
    InventoryMoveRequest request{std::to_integer<std::uint8_t>(body[0]),std::to_integer<std::uint8_t>(body[1]),
        std::to_integer<std::uint8_t>(body[2]),std::to_integer<std::uint8_t>(body[3]),std::to_integer<std::uint8_t>(body[4])};
    if(!ctx.session_reg||!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown)
        throw std::runtime_error("Inventory transaction context missing");
    const auto identity=ctx.session_reg->Identity(sess.get());
    if(!identity||identity->role!=MapSessionRole::Primary)co_return;
    const auto cid=identity->char_id;
    const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    if(original->bDead||!original->dwHP){ctx.char_state->Store(cid,*original);co_return;}
    InventoryMovePlan plan;CharSnapshot before,after;
    try {
        before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        plan=PlanInventoryMove(before,request);after=before;
        if(plan.result==InventoryMoveResult::Success)ApplyInventoryMove(after,plan);
    }catch(...){ctx.char_state->Store(cid,*original);throw;}
    if(plan.result!=InventoryMoveResult::Success) {
        ctx.char_state->Store(cid,*original);
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK),
            std::vector<std::byte>{static_cast<std::byte>(plan.result)});
        co_return;
    }
    try {
        auto* service=ctx.player_service;
        const auto committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&request,&before,&after]{
            return service->MoveInventoryItems(claim,request,before,after);
        });
        after=before;PublishInventoryMove(after,plan,committed);
        ctx.char_state->Store(cid,after);
    }catch(...) {
        // Even a reported failure may follow commit. Retain ownership and let
        // recovery inspect the durable receipt, without retry or stale save.
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);sess->Close();throw;
    }
    if(plan.kind==InventoryMoveKind::Move) {
        const auto& item=plan.items.front().before;
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
            std::vector<std::byte>{static_cast<std::byte>(item.bInvenID),static_cast<std::byte>(item.bItemID)});
    }
    // Source order: DEL+ADD for a move; source-at-destination UPDATE followed by
    // destination-at-source UPDATE for a swap; one private MOVEITEM result last.
    for(const auto& move:plan.items) {
        if(!move.count) {
            co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
                std::vector<std::byte>{static_cast<std::byte>(move.bag),static_cast<std::byte>(move.slot)});
            continue;
        }
        auto item=move.before;item.bItemID=move.slot;item.bInvenID=move.bag;item.bCount=move.count;
        auto descriptor=EncodeItemDescriptor(item,cid,true);
        descriptor.insert(descriptor.begin(),static_cast<std::byte>(move.bag));
        co_await sess->SendPacket(static_cast<std::uint16_t>(plan.kind==InventoryMoveKind::Move||move.created?MessageId::CS_ADDITEM_ACK:MessageId::CS_UPDATEITEM_ACK),descriptor);
    }
    co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK),std::vector<std::byte>{std::byte{0}});
}
}
