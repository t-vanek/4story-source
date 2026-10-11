#include "postgresql_map_service.h"
#include "inventory_move.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>
#include <limits>

namespace tmapsvr {
namespace {
std::uint64_t AllocateQuestItem(soci::session& sql,const MapSessionClaim& c) {
    const int world=c.group;long long id=0;
    sql << "UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=:w "
        "AND item_high_water::numeric+1 < (item_world::numeric+1)*72057594037927936 RETURNING item_high_water",
        soci::use(world),soci::into(id);
    if(!sql.got_data()||id<=0)throw std::runtime_error("Quest item identity range exhausted");
    return static_cast<std::uint64_t>(id);
}
struct Placement {std::size_t item{};std::uint8_t bag{},slot{},count{};bool create{};};
std::optional<std::vector<Placement>> Place(const CharacterPayload& p,const std::vector<ItemInstance>& input) {
    auto bags=p.bags;
    // CTObjBase::CanPush and CTPlayer::PushTItem first try matching stacks in
    // the default bag, then other bags, then empty slots in that same order.
    std::erase_if(bags,[](const auto& b){return b.bag.bInvenID==254;});
    std::sort(bags.begin(),bags.end(),[](const auto& a,const auto& b){
        return (a.bag.bInvenID==255?0:unsigned(a.bag.bInvenID)+1)<(b.bag.bInvenID==255?0:unsigned(b.bag.bInvenID)+1);
    });
    for(auto& bag:bags)std::sort(bag.items.begin(),bag.items.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
    std::vector<Placement> plan;
    for(std::size_t n=0;n<input.size();++n) {
        auto remaining=unsigned(input[n].bCount);
        if(!input[n].source||!input[n].stack_limit)throw std::runtime_error("Quest item has invalid source stack capacity");
        while(remaining) {
            CharacterBag* chosen=nullptr;ItemInstance* stack=nullptr;std::uint8_t slot=255;
            for(auto& bag:bags) {
                for(auto& candidate:bag.items)if(StackEquivalent(candidate,input[n])&&candidate.bCount<candidate.stack_limit) {
                    chosen=&bag;stack=&candidate;slot=candidate.bItemID;break;
                }
                if(chosen)break;
            }
            if(!chosen)for(auto& bag:bags) {
                for(unsigned at=0;at<bag.slot_count;++at)
                    if(std::none_of(bag.items.begin(),bag.items.end(),[&](const auto& i){return i.bItemID==at;})) {
                        chosen=&bag;slot=static_cast<std::uint8_t>(at);break;
                    }
                if(chosen)break;
            }
            if(!chosen)return std::nullopt;
            const auto count=static_cast<std::uint8_t>(std::min(remaining,stack?unsigned(stack->stack_limit-stack->bCount):unsigned(input[n].stack_limit)));
            if(!count)throw std::runtime_error("Quest placement made no progress");
            plan.push_back({n,chosen->bag.bInvenID,slot,count,!stack});
            if(stack)stack->bCount=static_cast<std::uint8_t>(stack->bCount+count);
            else {
                auto item=input[n];item.bCount=count;item.bItemID=slot;item.bInvenID=chosen->bag.bInvenID;
                chosen->items.push_back(std::move(item));
                std::sort(chosen->items.begin(),chosen->items.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
            }
            remaining-=count;
        }
    }
    return plan;
}
}
std::optional<ItemInstance> PostgreSQLMapService::CreateQuestItem(soci::session& sql,const MapSessionClaim& c,
    std::uint16_t item,std::uint8_t count) {
    if(!count)return std::nullopt;
    const int id=std::bit_cast<std::int16_t>(item);int durability=0,use_type=0,use_time=0;
    sql << "SELECT \"dwDuraMax\",\"bUseType\",\"wUseTime\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",
        soci::use(id),soci::into(durability),soci::into(use_type),soci::into(use_time);
    if(!sql.got_data())return std::nullopt; // Original missing template gives no item.
    transfer::Item raw;raw.id=AllocateQuestItem(sql,c);raw.owner_id=c.char_id;raw.item=item;raw.count=count;
    raw.durability=raw.durability_max=static_cast<std::uint32_t>(durability);
    std::uint32_t duration=0;
    if(use_type&0x80) {
        if(use_type&1)duration=static_cast<std::uint16_t>(use_time)*std::uint32_t{3600};
        else if(use_type&2)duration=static_cast<std::uint16_t>(use_time)*std::uint32_t{86400};
    }
    if(duration) {
        long long now=0;int hour=0,minute=0,second=0;
        // Native calendar is explicitly UTC, shared with existing item epoch
        // storage. Historical timestamps are not rewritten by this operation.
        sql << "SELECT floor(extract(epoch FROM transaction_timestamp()))::bigint,"
            "extract(hour FROM transaction_timestamp() AT TIME ZONE 'UTC')::integer,"
            "extract(minute FROM transaction_timestamp() AT TIME ZONE 'UTC')::integer,"
            "floor(extract(second FROM transaction_timestamp()))::integer",
            soci::into(now),soci::into(hour),soci::into(minute),soci::into(second);
        duration-=static_cast<std::uint32_t>(second);duration-=static_cast<std::uint32_t>(minute)*60;
        duration+=static_cast<std::uint32_t>(24-hour)*3600;
        raw.expires=now+duration; // CTItem::SetEndTime, including DWORD arithmetic.
    }
    return ProjectNativeItem(sql,raw);
}
bool PostgreSQLMapService::GrantQuestItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    const std::vector<ItemInstance>& input,QuestPackets& packets) {
    const auto plan=Place(*s.payload,input);if(!plan)return false;
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    std::vector<unsigned> remaining;for(const auto& i:input)remaining.push_back(i.bCount);
    for(const auto& step:*plan) {
        auto bag=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& b){return b.bag.bInvenID==step.bag;});
        auto found=std::find_if(bag->items.begin(),bag->items.end(),[&](const auto& i){return i.bItemID==step.slot;});
        transfer::Item raw;
        if(step.create) {
            if(found!=bag->items.end())throw std::runtime_error("Quest grant destination changed");
            raw=*input.at(step.item).source;
            // Original Copy generates IDs for the earlier split stacks and
            // keeps the preallocated item identity for the final stack.
            if(remaining.at(step.item)>step.count)raw.id=AllocateQuestItem(sql,c);
            raw.owner_id=c.char_id;raw.storage_id=step.bag;raw.slot=step.slot;raw.count=step.count;
        } else {
            if(found==bag->items.end()||!found->source)throw std::runtime_error("Quest grant stack disappeared");
            raw=*found->source;raw.count=static_cast<std::uint8_t>(found->bCount+step.count);
        }
        remaining.at(step.item)-=step.count;
        std::string hash;
        if(p->transfer_state)hash=GraphItemFingerprint(raw);
        else {
            const long long identity=std::bit_cast<long long>(raw.id),expires=raw.expires;
            const int inventory=step.bag,slot=step.slot,count=raw.count,item=std::bit_cast<std::int16_t>(raw.item),
                dura=std::bit_cast<std::int32_t>(raw.durability_max);
            if(step.create) {
                sql << "INSERT INTO app_world.\"TITEMTABLE\" AS i (\"bWorldID\",\"dlID\",\"bStorageType\",\"dwStorageID\",\"bOwnerType\",\"dwOwnerID\",\"bItemID\",\"wItemID\",\"bLevel\",\"bCount\",\"dwDuraMax\",\"dwDuraCur\",\"bRefineCur\",\"dEndTime\","
                    "\"bMagic1\",\"bMagic2\",\"bMagic3\",\"bMagic4\",\"bMagic5\",\"bMagic6\",\"wValue1\",\"wValue2\",\"wValue3\",\"wValue4\",\"wValue5\",\"wValue6\",\"dwTime1\",\"dwTime2\",\"dwTime3\",\"dwTime4\",\"dwTime5\",\"dwTime6\",\"bGem\",\"wMoggItemID\") "
                    "VALUES(:w,:id,0,:bag,0,:owner,:slot,:item,0,:count,:dura,:dura,0,to_timestamp(:expires) AT TIME ZONE 'UTC',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0) RETURNING app_world.item_fingerprint(i)",
                    soci::use(world,"w"),soci::use(identity,"id"),soci::use(inventory,"bag"),soci::use(owner,"owner"),soci::use(slot,"slot"),
                    soci::use(item,"item"),soci::use(count,"count"),soci::use(dura,"dura"),soci::use(expires,"expires"),soci::into(hash);
            } else {
                sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"bCount\"=:count WHERE \"bWorldID\"=:w AND \"dlID\"=:id AND \"dwOwnerID\"=:owner AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
                    soci::use(count,"count"),soci::use(world,"w"),soci::use(identity,"id"),soci::use(owner,"owner"),soci::use(found->durable_hash,"hash"),soci::into(hash);
            }
            if(!sql.got_data())throw std::runtime_error("Quest grant inventory ownership changed");
        }
        auto item=ProjectNativeItem(sql,raw);item.durable_hash=hash;
        auto bytes=EncodeItemDescriptor(item,c.char_id,true);bytes.insert(bytes.begin(),static_cast<std::byte>(step.bag));
        packets.emplace_back(static_cast<std::uint16_t>(step.create?tnetlib::protocol::MessageId::CS_ADDITEM_ACK:tnetlib::protocol::MessageId::CS_UPDATEITEM_ACK),std::move(bytes));
        if(step.create)bag->items.push_back(std::move(item));else *found=std::move(item);
    }
    return true;
}
}
