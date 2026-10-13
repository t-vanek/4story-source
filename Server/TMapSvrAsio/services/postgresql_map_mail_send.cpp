#include "postgresql_map_service.h"
#include "native_wire_text.h"
#include "native_mail_lock.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>

namespace tmapsvr {
bool PostgreSQLMapService::SendNativePost(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,QuestPackets& packets) const {
    using tnetlib::protocol::MessageId;
    const auto reply=[&](std::uint8_t result){packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTSEND_ACK),
        std::vector<std::byte>{static_cast<std::byte>(result)});};
    if(s.payload->security.Blocks())throw std::runtime_error("Post sending requires secure unlock");
    if(s.bStartAct==2||(s.bStartAct==1&&s.wMapID==2010)||s.payload->tournament_player)return false;
    if(s.payload->deal_status||s.payload->personal_store){reply(12);return false;}
    wire::Reader r(body.data(),body.size());std::string target,title,message;
    std::uint8_t type=0,inventory=0,slot=0;std::uint32_t gold=0,silver=0,cooper=0;
    if(!r.ReadString(target)||!r.ReadString(title)||!r.ReadString(message)||!r.Read(type)||
        !r.Read(gold)||!r.Read(silver)||!r.Read(cooper)||!r.Read(inventory)||!r.Read(slot)||!r.Eof())
        throw std::runtime_error("Invalid POSTSEND request");
    if(target.size()>50||title.size()>256||message.size()>2048){reply(13);return false;}
    if(target==s.szNAME){reply(14);return false;} // source compares exact live bytes
    if(title.empty()){reply(5);return false;}
    const auto decoded_target=native_wire_text::Decode(sql,target,m_config.client_text_encoding);
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);int recipient=0,country=0;
    sql << "SELECT \"dwCharID\",\"bCountry\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"bDelete\"=0 "
        "AND lower(rtrim(\"szNAME\") COLLATE \"C\")=lower(rtrim(CAST(:name AS text)) COLLATE \"C\") FOR KEY SHARE",
        soci::use(world,"w"),soci::use(decoded_target,"name"),soci::into(recipient),soci::into(country);
    if(!sql.got_data()){reply(1);return false;}
    int sender_country=0;
    // TPostCanSend reads the saved sender country, not Map's live war/aid
    // country or a transferred newer display projection.
    sql << "SELECT \"bCountry\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
        soci::use(world,"w"),soci::use(owner,"c"),soci::into(sender_country);
    if(!sql.got_data()){reply(1);return false;}
    if(country<2&&sender_country<2&&country!=sender_country){reply(1);return false;}
    LockNativeMailbox(sql,c.group,static_cast<std::uint32_t>(recipient));
    int count=0;
    if(type<3) {
        sql << "SELECT count(*) FROM app_world.\"TPOSTTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
            soci::use(world,"w"),soci::use(recipient,"c"),soci::into(count);
        if(count>=20){reply(6);return false;}
    }
    sql << "SELECT count(*) FROM app_world.\"TPROTECTEDTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwProtected\"=:sender",
        soci::use(world,"w"),soci::use(recipient,"c"),soci::use(owner,"sender"),soci::into(count);
    if(count){reply(1);return false;} // TPostCanSend ignores bOption
    // Type3+ can bypass capacity in the reference, but privileged operator/NPC/
    // payment/cash types on the player opcode can forge authoritative messages.
    // Explicit security repair: only NORMAL/PACKAGE/BILLS may originate here.
    if(type>2){reply(12);return false;}
    const std::uint64_t fee=type==0?100:type==1?300:900;
    const auto funds=std::uint32_t(cooper+silver*1000U)+std::uint64_t{gold}*1000000U;
    const auto price=fee+(type==2?0:funds);
    // Source UseMoney(FALSE) can grant gold titles even if a later item gate
    // fails. Persist those effects with the ordinary operation/checkpoint.
    const auto titles_before=s.payload->titles;
    const auto selected_before=s.payload->selected_title;
    if(!UseNativeMoney(sql,c,s,price,false,packets)){reply(3);return false;}
    const bool title_changed=s.payload->titles.size()!=titles_before.size()||
        s.payload->selected_title!=selected_before||!packets.empty();
    std::optional<ItemInstance> attachment;
    if(inventory!=252) { // NetCode INVEN_NULL=0xFC; DEFAULT=0xFF is a valid bag
        const auto bag=std::find_if(s.payload->bags.begin(),s.payload->bags.end(),[&](const auto& row){return row.bag.bInvenID==inventory;});
        if(bag==s.payload->bags.end()){reply(4);return title_changed;}
        const auto item=std::find_if(bag->items.begin(),bag->items.end(),[&](const auto& row){return row.bItemID==slot;});
        if(item==bag->items.end()){reply(4);return title_changed;}
        if(!item->source)throw std::runtime_error("Post attachment has no authoritative raw item");
        const int item_id=std::bit_cast<std::int16_t>(item->wItemID);int trade=0;
        sql << "SELECT \"bIsSell\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(item_id),soci::into(trade);
        if(!sql.got_data())throw std::runtime_error("Post attachment source template missing");
        // Full DWORD IEV_WRAP controls CanDeal, not its narrowed client byte.
        // Equipped mailing was accepted by the legacy handler without any
        // ChangeEquipItem cleanup. Reject this forged transfer rather than
        // keep equipment buffs after changing ownership.
        if((!item->source->wrap&&!(trade&1))||!(trade&4)||inventory==254){reply(12);return title_changed;}
        attachment=*item;
    }
    if(type==2&&!attachment){reply(4);return title_changed;}
    const auto decoded_title=native_wire_text::Decode(sql,title,m_config.client_text_encoding);
    const auto decoded_message=native_wire_text::Decode(sql,message,m_config.client_text_encoding);
    if(!std::all_of(s.szNAME.begin(),s.szNAME.end(),[](unsigned char b){return b>0&&b<128;}))
        throw std::runtime_error("Native non-ASCII character name wire projection is not implemented");
    const auto decoded_sender=s.szNAME;
    const auto sender_hex=native_wire_text::Hex(s.szNAME),recipient_hex=native_wire_text::Hex(target);
    const auto title_hex=native_wire_text::Hex(title),message_hex=native_wire_text::Hex(message);
    const int btype=type,g=std::bit_cast<std::int32_t>(gold),v=std::bit_cast<std::int32_t>(silver),z=std::bit_cast<std::int32_t>(cooper);
    const long long now=s.payload->event_time,stored_time=((now+30)/60)*60;int post=0;
    sql << "INSERT INTO app_world.\"TPOSTTABLE\"(\"bWorldID\",\"dwCharID\",\"szRecvName\",\"bType\",\"bRead\",\"timeRecv\","
        "\"dwSendID\",\"szSender\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\","
        "sender_bytes,recipient_bytes,title_bytes,message_bytes,bill_due_at) "
        "VALUES(:w,:recipient,:target,:type,0,to_timestamp(:time) AT TIME ZONE 'UTC',:sender,:sender_name,:title,:message,"
        ":gold,:silver,:cooper,decode(:sh,'hex'),decode(:rh,'hex'),decode(:th,'hex'),decode(:mh,'hex'),"
        "CASE WHEN CAST(:type AS integer)=2 THEN to_timestamp(:now)+INTERVAL '259200 seconds' ELSE NULL END) RETURNING \"dwPostID\"",
        soci::use(world,"w"),soci::use(recipient,"recipient"),soci::use(decoded_target,"target"),soci::use(btype,"type"),
        soci::use(stored_time,"time"),soci::use(owner,"sender"),soci::use(decoded_sender,"sender_name"),
        soci::use(decoded_title,"title"),soci::use(decoded_message,"message"),soci::use(g,"gold"),soci::use(v,"silver"),soci::use(z,"cooper"),
        soci::use(sender_hex,"sh"),soci::use(recipient_hex,"rh"),soci::use(title_hex,"th"),soci::use(message_hex,"mh"),
        soci::use(now,"now"),soci::into(post);
    if(!sql.got_data()||post<=0)throw std::runtime_error("Native post identity not allocated");
    if(type==1&&g>10) {
        sql << "INSERT INTO app_world.\"TPOSTERRORTABLE\"(\"bWorldID\",\"dwCharID\",\"dwGold\",post_id) VALUES(:w,:c,:g,:p)",
            soci::use(world,"w"),soci::use(recipient,"c"),soci::use(g,"g"),soci::use(post,"p");
    }
    if(attachment) {
        const auto& item=*attachment;auto raw=*item.source;
        if(!raw.id||raw.owner_type||raw.owner_id!=c.char_id||raw.storage||raw.storage_id!=inventory||raw.slot!=slot)
            throw std::runtime_error("Post attachment raw ownership changed");
        const long long id=std::bit_cast<long long>(raw.id);
        if(s.payload->transfer_state) {
            // Graph authority retires only its own old carried projection; a
            // foreign/global identity or already mailed item must never be replaced.
            sql << "DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id "
                "AND \"bOwnerType\"=0 AND \"bStorageType\"=0",soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id");
            raw.owner_id=static_cast<std::uint32_t>(recipient);raw.storage=2;raw.storage_id=static_cast<std::uint32_t>(post);
            InsertNativeItem(sql,c,raw);
        }else {
            long long moved=0;
            sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"dwOwnerID\"=:recipient,\"bStorageType\"=2,\"dwStorageID\"=:post,post_id=:post "
                "WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0 "
                "AND app_world.item_fingerprint(i)=:hash RETURNING \"dlID\"",
                soci::use(recipient,"recipient"),soci::use(post,"post"),soci::use(world,"w"),soci::use(owner,"c"),
                soci::use(id,"id"),soci::use(item.durable_hash,"hash"),soci::into(moved);
            if(!sql.got_data()||moved!=id)throw std::runtime_error("Post attachment persisted ownership changed");
        }
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        for(auto& bag:p->bags)if(bag.bag.bInvenID==inventory)
            std::erase_if(bag.items,[&](const auto& owned){return owned.dlID==item.dlID;});
        if(p->transfer_state) {
            auto graph=std::make_shared<transfer::State>(*p->transfer_state);
            std::erase_if(graph->items,[&](const auto& owned){return owned.id==item.dlID;});p->transfer_state=std::move(graph);
        }
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),
            std::vector<std::byte>{static_cast<std::byte>(inventory),static_cast<std::byte>(slot)});
    }
    if(price) {
        if(!UseNativeMoney(sql,c,s,price,true,packets))throw std::runtime_error("Post affordability changed within transaction");
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
    }
    sql << "INSERT INTO app_world.mail_notifications(world_id,char_id,post_id,post_type,enqueue_second) VALUES(:w,:c,:id,:type,:time)",
        soci::use(world,"w"),soci::use(recipient,"c"),soci::use(post,"id"),soci::use(btype,"type"),soci::use(now,"time");
    reply(0);return true;
}
}
