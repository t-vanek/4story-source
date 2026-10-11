#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "services/skill_cooldown.h"
#include "secure_code.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>

namespace tmapsvr {
bool PostgreSQLMapService::UseNativeItem(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,const QuestEvent& event,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    // CSHandler::OnCS_ITEMUSE_REQ and Client/CSSenderAll, not the obsolete
    // CSProtocol.h comment (which incorrectly describes a BYTE delay group).
    wire::Reader r(body.data(),body.size());std::uint16_t requested=0,delay_group=0;
    std::uint8_t inventory=0,slot=0,count=0;
    if(!r.Read(requested)||!r.Read(inventory)||!r.Read(slot)||!r.Read(delay_group)||!r.Read(count))
        throw std::runtime_error("Truncated ITEMUSE request");
    for(unsigned i=0;i<count;++i) {
        std::uint32_t target=0;std::uint8_t type=0;
        if(!r.Read(target)||!r.Read(type))throw std::runtime_error("Truncated ITEMUSE target list");
        // Source reads and ignores targets for the item kinds handled below.
    }
    if(!r.Eof())throw std::runtime_error("Trailing ITEMUSE bytes");
    if(s.payload->security.Blocks()) {
        const auto prompt=SecureUnlockPrompt(s.payload->security);
        packets.emplace_back(prompt.first,prompt.second);return false;
    }
    std::uint8_t kind=0;
    const auto reply=[&](std::uint8_t result,std::uint32_t delay=0) {
        std::vector<std::byte> ack;wire::WritePOD(ack,result);wire::WritePOD(ack,delay_group);
        wire::WritePOD(ack,kind);wire::WritePOD(ack,delay);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_ITEMUSE_ACK),std::move(ack));
    };
    const int request_id=std::bit_cast<std::int16_t>(requested);int present=0;
    sql << "SELECT 1 FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(request_id),soci::into(present);
    if(!sql.got_data()){reply(1);return false;} // IU_NOTFOUND, kind still zero
    auto bag=std::find_if(s.payload->bags.begin(),s.payload->bags.end(),[&](const auto& row){return row.bag.bInvenID==inventory;});
    if(bag==s.payload->bags.end()){reply(1);return false;}
    auto item=std::find_if(bag->items.begin(),bag->items.end(),[&](const auto& row){return row.bItemID==slot;});
    if(item==bag->items.end()||!item->bCount){reply(1);return false;}
    if(!item->source)throw std::runtime_error("ITEMUSE requires original item state");
    auto raw=*item->source;
    if(raw.wrap){reply(14);return false;} // CanUse tests the entire DWORD IEV_WRAP
    const int id=std::bit_cast<std::int16_t>(raw.item);soci::row chart;
    sql << "SELECT \"bKind\",\"bType\",\"wDelayGroupID\",\"dwDelay\",\"bLevel\",\"bUseType\",\"wUseTime\",\"bConsumable\",\"wUseValue\",\"dwSpeedInc\" "
        "FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(chart);
    if(!sql.got_data())throw std::runtime_error("ITEMUSE has no pinned template");
    kind=static_cast<std::uint8_t>(chart.get<int>(0));
    // NetCode.h TITEM_KIND: HP26, MP27, MAXHP42, MAXMP43, EXPBONUS56,
    // GAINEXP81. Additional use/action types retain their explicit contract gate.
    if((kind!=26&&kind!=27&&kind!=42&&kind!=43&&kind!=56&&kind!=81)||chart.get<int>(1)!=7)
        throw std::runtime_error("ITEMUSE kind requires its remaining native gameplay contract");
    if(s.cluster.riding){reply(6);return false;} // none of these is IK_WHIP
    if(static_cast<std::uint16_t>(chart.get<int>(2))!=delay_group){reply(1);return false;}
    // Unlike the old unchecked trigger, require the claimed quest-use template
    // to be the actually owned item. This closes cross-item quest credit spoofing.
    if(raw.item!=requested){reply(1);return false;}
    if(s.bDead){reply(1);return false;}
    if(chart.get<int>(4)>s.bLevel){reply(4);return false;} // IU_NEEDLEVEL
    // Tournament participant state is still pending. Never let ordinary item
    // handling bypass its source gate (MAPID_TOURNAMENT501, ARENA_COUNT4).
    if(s.wMapID>=500&&s.wMapID<533)throw std::runtime_error("ITEMUSE requires native tournament participant authority");
    if(!s.payload->item_cooldowns||!s.payload->event_ms)throw std::runtime_error("ITEMUSE requires hydrated cooldowns and event clock");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    SampleItemCooldowns(*p->item_cooldowns,p->event_ms);
    const auto delay=static_cast<std::uint32_t>(chart.get<int>(3));
    const auto used=std::find_if(p->item_cooldowns->timers.begin(),p->item_cooldowns->timers.end(),
        [&](const auto& row){return row.group==delay_group;});
    if(delay&&used!=p->item_cooldowns->timers.end()&&used->remaining){reply(1);return false;}
    if(!s.payload->during||s.payload->during->pending_entry)throw std::runtime_error("ITEMUSE precedes native EnterMAP");
    const auto use=static_cast<std::uint8_t>(chart.get<int>(5));
    if(kind!=56&&(use&0x40))throw std::runtime_error("Item kind requires additional during-item semantics");
    if(kind==56) {
        if(!p->during->items.empty()&&p->during->items.front().type){reply(13);return false;} // IU_OVERLAPEXPBONUS
    } else if(kind==81) {
        GainQuestExperience(sql,c,s,static_cast<std::uint32_t>(chart.get<int>(9)),event,packets);
    } else {
        if(!s.payload->statistics)throw std::runtime_error("Resource item requires authoritative player statistics");
        const bool hp=kind==26||kind==42;
        auto& value=hp?s.dwHP:s.dwMP;const auto maximum=hp?s.dwMaxHP:s.dwMaxMP;
        if(value>=maximum){reply(3,delay);return false;} // IU_FULL; no consumption or rearming
        if(kind==42||kind==43)value=maximum;
        else {
            // Preserve the source DWORD addition followed by the maximum clamp.
            value+=static_cast<std::uint16_t>(chart.get<int>(8));
            if(value>maximum)value=maximum;
        }
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),
            EncodeHpMpAck(c.char_id,1,s.dwMaxHP,s.dwHP,s.dwMaxMP,s.dwMP),QuestAudience::Neighbors);
        if(s.cluster.party) {
            std::vector<std::byte> party;wire::WritePOD(party,s.cluster.party);wire::WritePOD(party,c.char_id);
            wire::WritePOD(party,std::uint8_t{1});wire::WritePOD(party,s.bLevel);
            wire::WritePOD(party,s.dwMaxHP);wire::WritePOD(party,s.dwHP);wire::WritePOD(party,s.dwMaxMP);wire::WritePOD(party,s.dwMP);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::MW_PARTYMANSTAT_ACK),std::move(party),QuestAudience::World);
        }
    }
    if(delay) {
        p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        auto& rows=p->item_cooldowns->timers;
        auto timer=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.group==delay_group;});
        if(timer==rows.end())rows.push_back({delay_group,delay});else timer->remaining=delay;
        OrderItemCooldowns(rows);
        if(rows.size()>255)throw std::runtime_error("Item delay groups exceed original CHARINFO count");
    }
    if(use&0x40) {
        const auto units=static_cast<std::uint16_t>(chart.get<int>(6));
        const auto seconds=static_cast<std::uint32_t>(units*((use&1)?3600U:(use&2)?86400U:0U));
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        p->during->items.clear();
        if(seconds)p->during->items.push_back({raw.item,static_cast<std::uint8_t>((use&1)?1:2),seconds,p->event_time+seconds});
        HangExpBenefit(sql,c,s,packets);
    }
    if(chart.get<int>(7)) {
        // GAINEXP can execute quest rewards which change stacks. Reacquire the
        // same owned instance after those actions, rather than using a dangling
        // source iterator or an earlier count/fingerprint.
        const auto current_bag=std::find_if(s.payload->bags.begin(),s.payload->bags.end(),
            [&](const auto& row){return row.bag.bInvenID==inventory;});
        if(current_bag==s.payload->bags.end())throw std::runtime_error("Used item bag disappeared during progression");
        const auto current=std::find_if(current_bag->items.begin(),current_bag->items.end(),
            [&](const auto& row){return row.bItemID==slot&&row.dlID==raw.id;});
        if(current==current_bag->items.end()||!current->source||!current->bCount)
            throw std::runtime_error("Used item disappeared during progression");
        raw=*current->source;const auto old_hash=current->durable_hash;
        --raw.count;std::string hash;
        if(!s.payload->transfer_state) {
            const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id),remaining=raw.count;
            const long long item_id=std::bit_cast<long long>(raw.id);long long removed=0;
            if(!remaining) {
                sql << "DELETE FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id "
                    "AND app_world.item_fingerprint(i)=:hash RETURNING \"dlID\"",
                    soci::use(world,"w"),soci::use(owner,"c"),soci::use(item_id,"id"),soci::use(old_hash,"hash"),soci::into(removed);
            }else {
                sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"bCount\"=:count WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id "
                    "AND app_world.item_fingerprint(i)=:hash RETURNING app_world.item_fingerprint(i)",
                    soci::use(remaining,"count"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(item_id,"id"),soci::use(old_hash,"hash"),soci::into(hash);
            }
            if(!sql.got_data()||(!remaining&&removed!=item_id))throw std::runtime_error("Consumed item ownership changed");
        }else if(raw.count)hash=GraphItemFingerprint(raw);
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        auto destination=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& row){return row.bag.bInvenID==inventory;});
        auto owned=std::find_if(destination->items.begin(),destination->items.end(),[&](const auto& row){return row.bItemID==slot;});
        if(raw.count) {
            *owned=ProjectNativeItem(sql,raw);owned->durable_hash=hash;
            auto ack=EncodeItemDescriptor(*owned,c.char_id,true);ack.insert(ack.begin(),static_cast<std::byte>(inventory));
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_UPDATEITEM_ACK),std::move(ack));
        }else {
            destination->items.erase(owned);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
                std::vector<std::byte>{static_cast<std::byte>(inventory),static_cast<std::byte>(slot)});
        }
    }
    reply(0,delay); // Source: effect/resource/EXP -> reuse -> consume -> success -> CheckQuest
    event(requested,QTT_USEITEM,TT_USEITEM,1);
    return true;
}
}
