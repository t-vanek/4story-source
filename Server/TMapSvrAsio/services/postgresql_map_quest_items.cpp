#include "postgresql_map_service.h"
#include "inventory_move.h"
#include "native_quest_magic_item.h"
#include "item_expiration.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>
#include <limits>
#include <map>

namespace tmapsvr {
std::uint64_t PostgreSQLMapService::AllocateNativeItem(soci::session& sql,const MapSessionClaim& c) {
    const int world=c.group;long long id=0;
    sql << "UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=:w "
        "AND item_high_water::numeric+1 < (item_world::numeric+1)*72057594037927936 RETURNING item_high_water",
        soci::use(world),soci::into(id);
    if(!sql.got_data()||id<=0)throw std::runtime_error("Quest item identity range exhausted");
    return static_cast<std::uint64_t>(id);
}
namespace {
struct Placement {std::size_t item{};std::uint8_t bag{},slot{},count{};bool create{};};
std::optional<std::vector<Placement>> Place(const CharacterPayload& p,const std::vector<ItemInstance>& input,bool merge_stacks=true) {
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
            if(merge_stacks)for(auto& bag:bags) {
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
std::int64_t PostgreSQLMapService::NativeItemExpiry(soci::session& sql,std::uint32_t duration,int use_type,int use_time,std::optional<std::int64_t> observed) {
    if(!duration&&(use_type&0x80)) {
        if(use_type&1)duration=static_cast<std::uint16_t>(use_time)*std::uint32_t{3600};
        else if(use_type&2)duration=static_cast<std::uint16_t>(use_time)*std::uint32_t{86400};
    }
    if(!duration)return 0;
    long long now=0;int hour=0,minute=0,second=0;
    // Explicit native UTC policy. Historical wall-clock values stay archival.
    if(observed) {
        // A source event that already owns its clock must not be rebased to
        // the transaction start before a long account/claim wait or midnight.
        const long long at=*observed;
        sql << "SELECT floor(extract(epoch FROM to_timestamp(:at)))::bigint,"
            "extract(hour FROM to_timestamp(:at) AT TIME ZONE 'UTC')::integer,"
            "extract(minute FROM to_timestamp(:at) AT TIME ZONE 'UTC')::integer,"
            "floor(extract(second FROM to_timestamp(:at)))::integer",
            soci::use(at,"at"),soci::into(now),soci::into(hour),soci::into(minute),soci::into(second);
    } else sql << "SELECT floor(extract(epoch FROM transaction_timestamp()))::bigint,"
        "extract(hour FROM transaction_timestamp() AT TIME ZONE 'UTC')::integer,"
        "extract(minute FROM transaction_timestamp() AT TIME ZONE 'UTC')::integer,"
        "floor(extract(second FROM transaction_timestamp()))::integer",
        soci::into(now),soci::into(hour),soci::into(minute),soci::into(second);
    duration-=static_cast<std::uint32_t>(second);duration-=static_cast<std::uint32_t>(minute)*60;
    duration+=static_cast<std::uint32_t>(24-hour)*3600;
    return now+duration; // CTItem::SetEndTime, including DWORD arithmetic.
}
std::string PostgreSQLMapService::InsertNativeItem(soci::session& sql,const MapSessionClaim& c,const transfer::Item& raw) {
    // All values are typed integers, never interpolated SQL. Preserve raw magic
    // percentages and all six DWORD extensions rather than their wire projection.
    std::string json="{";
    const auto field=[&](const std::string& name,auto value){
        if(json.size()>1)json+=',';
        json+='"'+name+"\":"+std::to_string(value);
    };
    field("bWorldID",c.group);field("dlID",std::bit_cast<std::int64_t>(raw.id));
    field("bStorageType",raw.storage);field("dwStorageID",std::bit_cast<std::int32_t>(raw.storage_id));
    field("bOwnerType",raw.owner_type);field("dwOwnerID",std::bit_cast<std::int32_t>(raw.owner_id));
    field("bItemID",raw.slot);field("wItemID",std::bit_cast<std::int16_t>(raw.item));
    field("bLevel",raw.level);field("bCount",raw.count);field("bGLevel",raw.grade);
    field("dwDuraMax",std::bit_cast<std::int32_t>(raw.durability_max));field("dwDuraCur",std::bit_cast<std::int32_t>(raw.durability));
    field("bRefineCur",raw.refine);field("bGradeEffect",raw.grade_effect);
    if(raw.magic.size()>6)throw std::runtime_error("Quest reward exceeds source magic capacity");
    for(unsigned i=0;i<6;++i) {
        field("bMagic"+std::to_string(i+1),i<raw.magic.size()?raw.magic[i].id:0);
        field("wValue"+std::to_string(i+1),i<raw.magic.size()?std::bit_cast<std::int16_t>(raw.magic[i].value):0);
    }
    const std::uint32_t extensions[]={raw.eld,raw.wrap,raw.color,raw.guild,raw.companion,raw.texture};
    for(unsigned i=0;i<6;++i)field("dwTime"+std::to_string(i+1),std::bit_cast<std::int32_t>(extensions[i]));
    field("bGem",raw.gem);field("wMoggItemID",std::bit_cast<std::int16_t>(raw.appearance));
    field("wCustomTex",static_cast<std::uint16_t>(raw.texture));
    json+=",\"post_id\":"+(raw.storage==2?std::to_string(std::bit_cast<std::int32_t>(raw.storage_id)):std::string("null"))+'}';
    const long long expires=raw.expires;std::string hash;
    sql << "INSERT INTO app_world.\"TITEMTABLE\" AS i SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\","
        "CAST(:item AS jsonb)||jsonb_build_object('dEndTime',to_timestamp(:expires) AT TIME ZONE 'UTC'))).* "
        "RETURNING app_world.item_fingerprint(i)",soci::use(json,"item"),soci::use(expires,"expires"),soci::into(hash);
    if(!sql.got_data())throw std::runtime_error("Quest reward insertion did not return its identity");
    return hash;
}
bool PostgreSQLMapService::CanGrantQuestItems(const CharacterPayload& p,const std::vector<ItemInstance>& items) {
    return Place(p,items).has_value();
}
void PostgreSQLMapService::UpgradeNativeItemAttributes(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    ItemInstance before,const transfer::Item& raw,bool rearm) {
    if(!before.source)throw std::runtime_error("Item upgrade lost its raw source");
    auto expected=*before.source;
    expected.level=raw.level;expected.gem=raw.gem;expected.appearance=raw.appearance;
    expected.durability=raw.durability;expected.durability_max=raw.durability_max;
    expected.refine=raw.refine;expected.grade_effect=raw.grade_effect;
    expected.eld=raw.eld;expected.wrap=raw.wrap;expected.color=raw.color;expected.magic=raw.magic;
    if(raw.owner_id!=c.char_id||raw.owner_type||raw.storage||GraphItemFingerprint(expected)!=GraphItemFingerprint(raw))
        throw std::runtime_error("Item upgrade changed identity, ownership, count or unrelated attributes");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    ItemInstance* target=nullptr;
    for(auto& bag:p->bags)if(bag.bag.bInvenID==before.bInvenID)
        for(auto& item:bag.items)if(item.bItemID==before.bItemID&&item.dlID==before.dlID&&item.durable_hash==before.durable_hash)target=&item;
    if(!target||raw.magic.size()>6)throw std::runtime_error("Item upgrade target or magic shape changed");
    std::string hash;
    if(p->transfer_state)hash=GraphItemFingerprint(raw);
    else {
        std::string json="{",fields,values;
        const auto field=[&](const std::string& name,auto value) {
            if(json.size()>1){json+=',';fields+=',';values+=',';}
            json+='"'+name+"\":"+std::to_string(value);fields+='"'+name+'"';values+="v.\""+name+'"';
        };
        field("bLevel",raw.level);field("bGem",raw.gem);field("wMoggItemID",std::bit_cast<std::int16_t>(raw.appearance));
        field("dwDuraMax",std::bit_cast<std::int32_t>(raw.durability_max));field("dwDuraCur",std::bit_cast<std::int32_t>(raw.durability));
        field("bRefineCur",raw.refine);field("bGradeEffect",raw.grade_effect);
        field("dwTime1",std::bit_cast<std::int32_t>(raw.eld));field("dwTime2",std::bit_cast<std::int32_t>(raw.wrap));
        field("dwTime3",std::bit_cast<std::int32_t>(raw.color));
        for(unsigned i=0;i<6;++i) {
            field("bMagic"+std::to_string(i+1),i<raw.magic.size()?raw.magic[i].id:0);
            field("wValue"+std::to_string(i+1),i<raw.magic.size()?std::bit_cast<std::int16_t>(raw.magic[i].value):0);
        }
        json+='}';const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
        const long long id=std::bit_cast<long long>(raw.id);
        sql << "WITH v AS (SELECT * FROM jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",CAST(:item AS jsonb))) "
            "UPDATE app_world.\"TITEMTABLE\" i SET ("+fields+")=("+values+") FROM v "
            "WHERE i.\"bWorldID\"=:w AND i.\"dwOwnerID\"=:c AND i.\"dlID\"=:id AND i.\"bOwnerType\"=0 AND i.\"bStorageType\"=0 "
            "AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
            soci::use(json,"item"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),
            soci::use(before.durable_hash,"hash"),soci::into(hash);
        if(!sql.got_data())throw std::runtime_error("Upgraded item ownership changed");
    }
    *target=ProjectNativeItem(sql,raw);target->durable_hash=hash;
    target->alarm_time=rearm?SourceItemAlarm(raw.expires,s.payload->event_time,before.alarm_time):before.alarm_time;
}
void PostgreSQLMapService::ReplaceNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    ItemInstance before,const transfer::Item& raw) {
    if(!before.source||raw.owner_id!=c.char_id||raw.owner_type||raw.storage||
       raw.storage_id!=before.bInvenID||raw.slot!=before.bItemID||!raw.id||!raw.count)
        throw std::runtime_error("Native item replacement changed ownership or position");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    ItemInstance* target=nullptr;
    for(auto& bag:p->bags)if(bag.bag.bInvenID==before.bInvenID)
        for(auto& item:bag.items)if(item.bItemID==before.bItemID&&item.dlID==before.dlID&&item.durable_hash==before.durable_hash)target=&item;
    if(!target)throw std::runtime_error("Native item replacement lost its source");
    std::string hash;
    if(p->transfer_state)hash=GraphItemFingerprint(raw);
    else if(raw.id!=before.dlID) {
        // CTItem::Copy(TRUE) retires the old identity. Delete/insert and the
        // checkpoint are in the caller's fenced transaction, with no interim ACK.
        const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
        const long long identity=std::bit_cast<long long>(before.dlID);long long removed=0;
        sql << "DELETE FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c "
            "AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0 AND app_world.item_fingerprint(i)=:hash RETURNING \"dlID\"",
            soci::use(world,"w"),soci::use(owner,"c"),soci::use(identity,"id"),soci::use(before.durable_hash,"hash"),soci::into(removed);
        if(!sql.got_data())throw std::runtime_error("Native replacement item changed before deletion");
        hash=InsertNativeItem(sql,c,raw);
    } else {
        // The identity-preserving branches only remove wrapping or appearance.
        auto expected=*before.source;expected.wrap=raw.wrap;expected.appearance=raw.appearance;
        if(GraphItemFingerprint(expected)!=GraphItemFingerprint(raw))
            throw std::runtime_error("Identity-preserving replacement changed unexpected fields");
        const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
        const long long identity=std::bit_cast<long long>(before.dlID);
        const int wrap=std::bit_cast<std::int32_t>(raw.wrap),appearance=std::bit_cast<std::int16_t>(raw.appearance);
        sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"dwTime2\"=:wrap,\"wMoggItemID\"=:appearance "
            "WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0 "
            "AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
            soci::use(wrap,"wrap"),soci::use(appearance,"appearance"),soci::use(world,"w"),soci::use(owner,"c"),
            soci::use(identity,"id"),soci::use(before.durable_hash,"hash"),soci::into(hash);
        if(!sql.got_data())throw std::runtime_error("Native replacement item changed before update");
    }
    *target=ProjectNativeItem(sql,raw);target->durable_hash=hash;
    if(raw.id==before.dlID)target->alarm_time=before.alarm_time;
}
std::optional<ItemInstance> PostgreSQLMapService::CreateQuestItem(soci::session& sql,const MapSessionClaim& c,
    std::uint16_t item,std::uint8_t count,std::optional<std::int64_t> observed) {
    if(!count)return std::nullopt;
    const int id=std::bit_cast<std::int16_t>(item);int durability=0,use_type=0,use_time=0;
    sql << "SELECT \"dwDuraMax\",\"bUseType\",\"wUseTime\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",
        soci::use(id),soci::into(durability),soci::into(use_type),soci::into(use_time);
    if(!sql.got_data())return std::nullopt;
    transfer::Item raw;raw.id=AllocateNativeItem(sql,c);raw.owner_id=c.char_id;raw.item=item;raw.count=count;
    raw.durability=raw.durability_max=static_cast<std::uint32_t>(durability);
    raw.expires=NativeItemExpiry(sql,0,use_type,use_time,observed);
    return ProjectNativeItem(sql,raw);
}
std::vector<ItemInstance> PostgreSQLMapService::CreateQuestRewardItems(soci::session& sql,const MapSessionClaim& c,
    const CharSnapshot& s,const QuestRewardDef& reward) {
    std::vector<ItemInstance> items;
    for(const auto& raw:CreateQuestRewardRawItems(sql,c,s,reward,false))items.push_back(ProjectNativeItem(sql,raw));
    return items;
}
std::vector<transfer::Item> PostgreSQLMapService::CreateQuestRewardRawItems(soci::session& sql,const MapSessionClaim& c,
    const CharSnapshot& s,const QuestRewardDef& reward,bool mail) {
    transfer::Item raw;std::uint32_t duration=0;
    if(reward.bRewardType==RT_MAGICITEM) {
        const auto magic=ReadQuestMagicItem(sql,reward.dwRewardID);
        if(!magic)return {};
        raw=magic->item;duration=magic->days*std::uint32_t{86400};
    } else raw.item=static_cast<std::uint16_t>(reward.dwRewardID);
    const int id=std::bit_cast<std::int16_t>(raw.item);int classes=0,stack=0,durability=0,use_type=0,use_time=0;
    sql << "SELECT \"dwClassID\",\"bStack\",\"dwDuraMax\",\"bUseType\",\"wUseTime\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",
        soci::use(id),soci::into(classes),soci::into(stack),soci::into(durability),soci::into(use_type),soci::into(use_time);
    if(!sql.got_data())return {};
    if(s.bClass>=32)throw std::runtime_error("Quest reward class shift out of range");
    if(!(static_cast<std::uint32_t>(classes)&(std::uint32_t{1}<<s.bClass)))return {};
    if(reward.bCount&&!stack)throw std::runtime_error("Quest reward has zero source stack capacity");
    if(reward.bRewardType!=RT_MAGICITEM)raw.durability=raw.durability_max=static_cast<std::uint32_t>(durability);
    raw.expires=NativeItemExpiry(sql,duration,use_type,use_time);raw.owner_id=c.char_id;
    std::vector<transfer::Item> items;
    for(unsigned remaining=reward.bCount;remaining;) {
        raw.count=static_cast<std::uint8_t>(std::min(remaining,unsigned(stack)));remaining-=raw.count;
        // SENDPOST's ordinary constructor keeps ID0; magic Copy(TRUE) consumes
        // a temporary Map ID. Neither is the separately allocated delivery ID.
        raw.id=(!mail||reward.bRewardType==RT_MAGICITEM)?AllocateNativeItem(sql,c):0;
        items.push_back(raw);
    }
    return items;
}
bool PostgreSQLMapService::OpenSpecialBox(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint16_t group,QuestPackets& packets) {
    int compatible=0;
    sql << "SELECT count(*) FROM game_compat.special_box_release r JOIN game_compat.catalog_release m "
        "ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
        "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
        "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
        soci::into(compatible);
    if(compatible!=1)throw std::runtime_error("Special box requires the published original content catalog");
    const int id=std::bit_cast<std::int16_t>(group);
    std::vector<ItemInstance> items;unsigned group_size=0;
    // Source CTBLSpecialBoxChart has no ORDER BY. Preserve the archived extract
    // order explicitly, not an invented wID order. CreateItem skips rows whose
    // item template is absent before the runtime group/class selection.
    soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT b.* FROM game_compat.\"TSPECIALBOXCHART\" b JOIN character_compat.\"TITEMCHART\" i "
        "ON i.\"wItemID\"=b.\"wItemID\" WHERE b.\"wGroup\"=:id ORDER BY b.\"_source_row_number\"",soci::use(id));
    for(const auto& row:rows) {
        if(++group_size>255)throw std::runtime_error("Special box exceeds source BYTE loop");
        const auto klass=row.get<std::string>("bClass");
        if(klass.size()!=1||klass[0]<'0'||klass[0]>'9')throw std::runtime_error("Special box class cannot convert to original BYTE");
        if(klass[0]-'0'!=s.bClass&&klass[0]!='6')continue; // TCLASS_COUNT, not template class mask
        transfer::Item raw;
        raw.id=AllocateNativeItem(sql,c);raw.owner_id=c.char_id;
        raw.item=static_cast<std::uint16_t>(row.get<int>("wItemID"));
        raw.count=static_cast<std::uint8_t>(row.get<int>("bCount"));
        raw.level=static_cast<std::uint8_t>(row.get<int>("bLevel"));raw.grade=static_cast<std::uint8_t>(row.get<int>("bGLevel"));
        raw.durability_max=static_cast<std::uint32_t>(row.get<int>("dwDuraMax"));raw.durability=static_cast<std::uint32_t>(row.get<int>("dwDuraCur"));
        raw.refine=static_cast<std::uint8_t>(row.get<int>("bRefineCur"));raw.grade_effect=static_cast<std::uint8_t>(row.get<int>("bGradeEffect"));
        raw.gem=static_cast<std::uint8_t>(row.get<int>("bGem"));raw.appearance=static_cast<std::uint16_t>(row.get<int>("wMoggItemID"));
        raw.eld=static_cast<std::uint32_t>(row.get<int>("dwTime1"));raw.wrap=static_cast<std::uint32_t>(row.get<int>("dwTime2"));
        raw.color=static_cast<std::uint32_t>(row.get<int>("dwTime3"));raw.guild=static_cast<std::uint32_t>(row.get<int>("dwTime4"));
        raw.companion=static_cast<std::uint32_t>(row.get<int>("dwTime5"));raw.texture=static_cast<std::uint32_t>(row.get<int>("dwTime6"));
        const auto days=static_cast<std::uint16_t>(row.get<int>("wUseTime"));
        // Unlike quest reward SetEndTime, this is current time plus whole days,
        // without midnight rounding. Native calendar policy is explicit UTC.
        // The source query never binds m_dEndTime; zero-day contents use a
        // defined no-expiry value instead of reading uninitialized memory.
        raw.expires=days?s.payload->event_time+std::int64_t{days}*86400:0;
        std::map<std::uint8_t,std::uint16_t> magic;
        for(unsigned i=1;i<=6;++i) {
            const int code=row.get<int>("bMagic"+std::to_string(i));
            const auto value=static_cast<std::uint16_t>(row.get<int>("wValue"+std::to_string(i)));
            if(!code||!value)continue;
            int found=0;sql << "SELECT 1 FROM actor_compat.\"TITEMMAGICCHART\" WHERE \"bMagic\"=:id",soci::use(code),soci::into(found);
            if(sql.got_data())magic[static_cast<std::uint8_t>(code)]=value;
        }
        for(const auto& [code,value]:magic)raw.magic.push_back({code,value});
        if(!raw.count)throw std::runtime_error("Special box contains a zero-count source item");
        items.push_back(ProjectNativeItem(sql,raw));
    }
    if(!group_size)return false;
    bool blank=false;
    for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID!=254)
        for(unsigned slot=0;slot<bag.slot_count;++slot)
            if(std::none_of(bag.items.begin(),bag.items.end(),[&](const auto& i){return i.bItemID==slot;})){blank=true;break;}
    // HaveInvenBlank is an independent source gate, even when all rewards would
    // merge into stacks or class filtering selected zero items. Capacity is
    // checked before the original box is consumed.
    return blank&&GrantQuestItems(sql,c,s,items,packets);
}
bool PostgreSQLMapService::GrantQuestItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    const std::vector<ItemInstance>& input,QuestPackets& packets,bool merge_stacks) {
    const auto plan=Place(*s.payload,input,merge_stacks);if(!plan)return false;
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
            if(remaining.at(step.item)>step.count)raw.id=AllocateNativeItem(sql,c);
            raw.owner_type=0;raw.owner_id=c.char_id;raw.storage=0;raw.storage_id=step.bag;raw.slot=step.slot;raw.count=step.count;
        } else {
            if(found==bag->items.end()||!found->source)throw std::runtime_error("Quest grant stack disappeared");
            raw=*found->source;raw.count=static_cast<std::uint8_t>(found->bCount+step.count);
        }
        remaining.at(step.item)-=step.count;
        std::string hash;
        if(p->transfer_state)hash=GraphItemFingerprint(raw);
        else {
            const long long identity=std::bit_cast<long long>(raw.id);
            const int count=raw.count;
            if(step.create)hash=InsertNativeItem(sql,c,raw);
            else {
                sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"bCount\"=:count WHERE \"bWorldID\"=:w AND \"dlID\"=:id AND \"dwOwnerID\"=:owner AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
                    soci::use(count,"count"),soci::use(world,"w"),soci::use(identity,"id"),soci::use(owner,"owner"),soci::use(found->durable_hash,"hash"),soci::into(hash);
            }
            if(!sql.got_data())throw std::runtime_error("Quest grant inventory ownership changed");
        }
        auto item=ProjectNativeItem(sql,raw);item.durable_hash=hash;
        if(!step.create)item.alarm_time=found->alarm_time; // stack merge never calls Copy/SetItemAttr
        auto bytes=EncodeItemDescriptor(item,c.char_id,true);bytes.insert(bytes.begin(),static_cast<std::byte>(step.bag));
        packets.emplace_back(static_cast<std::uint16_t>(step.create?tnetlib::protocol::MessageId::CS_ADDITEM_ACK:tnetlib::protocol::MessageId::CS_UPDATEITEM_ACK),std::move(bytes));
        if(step.create)bag->items.push_back(std::move(item));else *found=std::move(item);
    }
    return true;
}
bool PostgreSQLMapService::ReclaimNativeRefine(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    if(body.size()!=2)throw std::runtime_error("Invalid WASTEREFINE request length");
    const auto inventory=std::to_integer<std::uint8_t>(body[0]),slot=std::to_integer<std::uint8_t>(body[1]);
    const auto reply=[&](std::uint8_t result){packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_WASTEREFINE_ACK),
        std::vector<std::byte>{static_cast<std::byte>(result)});};
    std::optional<ItemInstance> source;
    for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==inventory)
        for(const auto& item:bag.items)if(item.bItemID==slot)source=item;
    if(!source||!source->bCount){reply(1);return false;}
    const int id=std::bit_cast<std::int16_t>(source->wItemID);int type=0,use=0;
    sql << "SELECT \"bType\",\"wUseValue\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",
        soci::use(id),soci::into(type),soci::into(use);
    if(!sql.got_data())throw std::runtime_error("Reclaimed refine item template disappeared");
    if(type!=14){reply(2);return false;} // IT_REFINE
    int stack=0;
    sql << "SELECT \"bStack\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(use),soci::into(stack);
    if(!sql.got_data()){reply(1);return false;}
    if(!stack)throw std::runtime_error("Reclaimed item has a zero source stack size");
    // The original temporary CTItem has a NULL attribute pointer until Copy.
    // CanPush/GetEasePos therefore cannot merge it with hydrated carried items.
    // It needs a blank BEFORE consuming the material, even if that material
    // would free one. Do not substitute the more permissive generic placement.
    bool blank=false;
    for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID!=254)
        for(unsigned at=0;at<bag.slot_count;++at)
            if(std::none_of(bag.items.begin(),bag.items.end(),[&](const auto& item){return item.bItemID==at;}))blank=true;
    if(!blank){reply(3);return false;}
    DebitNativeItem(sql,c,s,inventory,slot,source->dlID,1,packets);
    transfer::Item raw;raw.id=AllocateNativeItem(sql,c);raw.owner_id=c.char_id;
    raw.item=static_cast<std::uint16_t>(use);raw.count=1;
    // CTItem constructor values: no template durability, expiry, magic or
    // extensions are generated by this handler. Copy only assigns attributes.
    const std::vector<ItemInstance> items{ProjectNativeItem(sql,raw)};
    if(!GrantQuestItems(sql,c,s,items,packets,false))throw std::runtime_error("Reclaimed item placement changed after debit");
    if(inventory==254)RefreshEquipment(sql,s,true); // source getters see removal immediately
    reply(0);
    // Source has no secure-code, dead/riding, reuse or quest-use gate/hook.
    return true;
}
void PostgreSQLMapService::ConsumeQuestItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    const QuestDef& def,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    std::sort(p->bags.begin(),p->bags.end(),[](const auto& a,const auto& b){return a.bag.bInvenID<b.bag.bInvenID;});
    bool equipment_changed=false;
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    for(const auto& term:def.terms)if(term.bTermType==QTT_GETITEM) {
        auto remaining=term.bGoalCount;
        for(auto& bag:p->bags) {
            std::sort(bag.items.begin(),bag.items.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
            for(auto it=bag.items.begin();it!=bag.items.end();) {
                if(it->wItemID==term.dwTermID) {
                    if(!it->source)throw std::runtime_error("Quest consumption has no authoritative item");
                    auto raw=*it->source;
                    const auto spent=std::min(raw.count,remaining);
                    raw.count=static_cast<std::uint8_t>(raw.count-spent);remaining=static_cast<std::uint8_t>(remaining-spent);
                    std::string hash;
                    if(!p->transfer_state) {
                        const long long id=std::bit_cast<long long>(raw.id);long long erased=0;const int count=raw.count;
                        if(!count) {
                            sql << "DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND app_world.item_fingerprint(\"TITEMTABLE\")=:hash RETURNING \"dlID\"",
                                soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),soci::use(it->durable_hash,"hash"),soci::into(erased);
                        } else {
                            sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"bCount\"=:count WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
                                soci::use(count,"count"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),soci::use(it->durable_hash,"hash"),soci::into(hash);
                        }
                        if(!sql.got_data()||(!count&&erased!=id))throw std::runtime_error("Quest consumption ownership changed");
                    } else if(raw.count)hash=GraphItemFingerprint(raw);
                    if(!raw.count) {
                        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
                            std::vector<std::byte>{static_cast<std::byte>(bag.bag.bInvenID),static_cast<std::byte>(raw.slot)});
                        it=bag.items.erase(it);
                        if(bag.bag.bInvenID==254) {
                            equipment_changed=true;
                            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_EQUIP_ACK),EncodeEquipment(s),QuestAudience::Neighbors);
                        }
                    } else {
                        auto item=ProjectNativeItem(sql,raw);item.durable_hash=hash;
                        item.alarm_time=it->alarm_time;
                        auto bytes=EncodeItemDescriptor(item,c.char_id,true);bytes.insert(bytes.begin(),static_cast<std::byte>(bag.bag.bInvenID));
                        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_UPDATEITEM_ACK),std::move(bytes));
                        *it=std::move(item);++it;
                    }
                } else ++it;
                // Source tests remaining even after a nonmatching first item.
                // Each subsequent bag is visited with remaining=0, possibly
                // sending an unchanged UPDATE when its first item matches.
                if(!remaining)break;
            }
        }
    }
    if(equipment_changed)RefreshEquipment(sql,s,true);
}

}
