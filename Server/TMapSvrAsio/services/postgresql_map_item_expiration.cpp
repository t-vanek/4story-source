#include "postgresql_map_service.h"
#include "item_expiration.h"
#include "posture_effects.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <tuple>

namespace tmapsvr {
void PostgreSQLMapService::AdvanceItemExpirations(soci::session& sql,const MapSessionClaim& c,
    CharSnapshot& s,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    if(s.payload->personal_store)return;
    const auto now=s.payload->event_time;
    // Freeze source unsigned bag/slot order. Each deletion may replace the
    // immutable payload and remove equipment-dependent maintained effects.
    std::vector<std::tuple<std::uint8_t,std::uint8_t,std::uint64_t>> ordered;
    std::vector<std::uint8_t> inventories;
    for(const auto& bag:s.payload->bags)inventories.push_back(bag.bag.bInvenID);
    std::sort(inventories.begin(),inventories.end());
    for(const auto& bag:s.payload->bags)for(const auto& item:bag.items)
        ordered.emplace_back(bag.bag.bInvenID,item.bItemID,item.dlID);
    std::sort(ordered.begin(),ordered.end());
    QuestPackets notifications;
    bool deleted=false;
    for(const auto inventory:inventories) {
      for(const auto& [item_inventory,slot,identity]:ordered) {
        if(item_inventory!=inventory)continue;
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        auto bag=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& row){return row.bag.bInvenID==inventory;});
        if(bag==p->bags.end())throw std::runtime_error("Expiration inventory disappeared");
        auto item=std::find_if(bag->items.begin(),bag->items.end(),[&](const auto& row){return row.bItemID==slot&&row.dlID==identity;});
        if(item==bag->items.end()||!item->source)throw std::runtime_error("Expiration item identity disappeared");
        if(!item->dEndTime)continue;
        if(item->alarm_time&&item->alarm_time<=now) {
            // CSSender SYSTEMMSG SM_ITEM_EXPIRE17: DWORD seconds, BYTE bag/slot.
            // Delayed ticks report the scheduled interval, not remaining time.
            std::vector<std::byte> ack;wire::WritePOD(ack,std::uint8_t{17});
            wire::WritePOD(ack,static_cast<std::uint32_t>(item->dEndTime-item->alarm_time));
            wire::WritePOD(ack,inventory);wire::WritePOD(ack,slot);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SYSTEMMSG_ACK),std::move(ack));
            item->alarm_time=SourceItemAlarm(item->dEndTime,now,item->alarm_time);
        }
        if(item->dEndTime>now)continue;
        // Expiration removes the entire instance, even a historical count0.
        // It is not a consumable debit and has no USEITEM/GETITEM quest hook.
        if(!p->transfer_state) {
            const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
            const long long id=std::bit_cast<long long>(identity);long long removed=0;
            sql << "DELETE FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c "
                "AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0 "
                "AND app_world.item_fingerprint(i)=:hash RETURNING \"dlID\"",
                soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),soci::use(item->durable_hash,"hash"),soci::into(removed);
            if(!sql.got_data()||removed!=id)throw std::runtime_error("Expired item ownership changed");
        }
        bag->items.erase(item);deleted=true;
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
            std::vector<std::byte>{static_cast<std::byte>(inventory),static_cast<std::byte>(slot)});
        if(inventory!=254)continue;
        // Same ChangeEquipItem/CheckEquipSkill contract as a native manual move.
        InventoryMoveCommit change;
        ApplyEquipmentPostures(s,0,change,[&](auto& state){RefreshEquipment(sql,state,true);});
        if(!change.equipment_display||!change.equipment_display->payload->statistics)
            throw std::runtime_error("Expired equipment lacks source statistics");
        const auto& display=*change.equipment_display;
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_EQUIP_ACK),EncodeEquipment(display),QuestAudience::Neighbors);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK),std::vector<std::byte>{std::byte{0}});
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),
            EncodeCharacterStatistics(display,*display.payload->statistics));
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),
            EncodeHpMpAck(c.char_id,1,display.dwMaxHP,display.dwHP,display.dwMaxMP,display.dwMP));
        for(const auto& effect:change.effects_after) {
            if(effect.added||!effect.state||!effect.state->payload->statistics)
                throw std::runtime_error("Expired equipment produced an invalid effect removal");
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLEND_ACK),
                EncodeSkillEnd(c.char_id,effect.skill),QuestAudience::Neighbors);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),
                EncodeCharacterStatistics(*effect.state,*effect.state->payload->statistics));
        }
      }
      const auto bag=std::find_if(s.payload->bags.begin(),s.payload->bags.end(),[&](const auto& b){return b.bag.bInvenID==inventory;});
      if(bag!=s.payload->bags.end()&&bag->bag.dEndTime&&bag->bag.dEndTime<=now)
          PostExpiredBag(sql,c,s,inventory,packets,notifications);
    }
    if(deleted)packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK),std::vector<std::byte>{std::byte{0}});
    // Source POSTINVENITEM callbacks occur after the immediate INVENDEL/item
    // responses. Native notifications are likewise queued after those, but all
    // database ownership changes and the graph checkpoint commit beforehand.
    for(auto& packet:notifications)packets.push_back(std::move(packet));
}
}
