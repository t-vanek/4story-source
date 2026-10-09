#include "handlers.h"
#include "services/inventory_move.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/main_transfer_runtime.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "services/channel_presence.h"
#include "fourstory/db/co_offload.h"
#include "MessageId.h"
#include <cmath>

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
    InventoryMoveCommit committed;
    try {
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),&request,&before,&after]{
            return service->MoveInventoryItems(claim,request,before,after);
        });
        if(plan.kind==InventoryMoveKind::Equipment) {
            if(!committed.equipment_snapshot)throw std::runtime_error("Missing equipment commit state");
            after=*committed.equipment_snapshot;
        }else {after=before;PublishInventoryMove(after,plan,committed);}
        ctx.char_state->Store(cid,after);
    }catch(...) {
        // Even a reported failure may follow commit. Retain ownership and let
        // recovery inspect the durable receipt, without retry or stale save.
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);sess->Close();throw;
    }
    if(plan.kind==InventoryMoveKind::Equipment) {
        for(const auto& event:plan.wire) {
            const auto& item=event.item;
            if(event.kind==InventoryWireKind::Delete) {
                co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
                    std::vector<std::byte>{static_cast<std::byte>(item.bInvenID),static_cast<std::byte>(item.bItemID)});
            }else {
                auto bytes=EncodeItemDescriptor(item,cid,true);bytes.insert(bytes.begin(),static_cast<std::byte>(item.bInvenID));
                co_await sess->SendPacket(static_cast<std::uint16_t>(event.kind==InventoryWireKind::Add?MessageId::CS_ADDITEM_ACK:MessageId::CS_UPDATEITEM_ACK),std::move(bytes));
            }
        }
        const auto equipment=EncodeEquipment(after);
        std::vector<std::shared_ptr<tnetlib::AsioSession>> neighbors{sess};
        const auto cell=[](float value)->int{return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
        if(ctx.presence)ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& entry,auto client){
            if(entry.map_id==after.wMapID&&std::abs(cell(entry.pos.x)-cell(after.fPosX))<=1&&
               std::abs(cell(entry.pos.z)-cell(after.fPosZ))<=1)neighbors.push_back(std::move(client));
        });
        const auto send_effects=[&](const auto& events)->boost::asio::awaitable<void> {
            for(const auto& event:events) {
                if(!event.state||!event.state->payload||!event.state->payload->statistics)throw std::runtime_error("Posture commit lacks statistics");
                const auto bytes=event.added?EncodePostureDefend(*event.state,event.skill):EncodeSkillEnd(cid,event.skill);
                const auto id=event.added?MessageId::CS_DEFEND_ACK:MessageId::CS_SKILLEND_ACK;
                for(auto& client:neighbors)co_await client->SendPacket(static_cast<std::uint16_t>(id),bytes);
                if(!event.added)co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),
                    EncodeCharacterStatistics(*event.state,*event.state->payload->statistics));
            }
        };
        co_await send_effects(committed.effects_before);
        for(auto& client:neighbors)co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_EQUIP_ACK),equipment);
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK),std::vector<std::byte>{std::byte{0}});
        if(!committed.equipment_display)throw std::runtime_error("Equipment display state was not committed");
        const auto& display=*committed.equipment_display;
        if(!display.payload||!display.payload->statistics)throw std::runtime_error("Equipment statistics were not committed");
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),EncodeCharacterStatistics(display,*display.payload->statistics));
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),EncodeHpMpAck(cid,1,display.dwMaxHP,display.dwHP,display.dwMaxMP,display.dwMP));
        co_await send_effects(committed.effects_after);
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK),std::vector<std::byte>{std::byte{0}});
        co_return;
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
