#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "native_item_row.h"
#include "native_mail_lock.h"
#include "domain/quest_def.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <map>
#include <limits>

namespace tmapsvr {
namespace {
std::string Unhex(const std::string& hex) {
    if(hex.size()%2)throw std::runtime_error("Invalid original post text bytes");
    std::string out;out.reserve(hex.size()/2);
    for(std::size_t i=0;i<hex.size();i+=2)out+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));
    return out;
}
std::string Hex(const std::string& bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);
    for(unsigned char b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;
}
std::string NetString(const std::string& body,std::size_t source_length) {
    // TMapSvr::BuildNetString(NAME_NULL, body) in the MultiByte source build.
    if(source_length>65535)throw std::runtime_error("Source server message length overflow");
    constexpr char digits[]="0123456789ABCDEF";std::string header="00000000";
    for(unsigned n=0;n<4;++n)header[7-n]=digits[(source_length>>(n*4))&15];
    return header+body;
}
struct PostText {std::string sender,title,message,sender_bytes,title_bytes,message_bytes;};
PostText ExpiredBagText(soci::session& sql) {
    int compatible=0;
    sql << "SELECT count(*) FROM game_compat.server_message_release r JOIN game_compat.catalog_release m "
        "ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
        "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
        "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
        soci::into(compatible);
    if(compatible!=1)throw std::runtime_error("Expired bag requires published original server messages");
    std::map<int,std::pair<std::string,std::string>> messages;
    soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT \"dwID\",\"szMessage\",encode(message_bytes,'hex') AS raw FROM game_compat.server_messages WHERE \"dwID\" IN (1,6)");
    for(const auto& row:rows) {
        if(row.get_indicator(2)==soci::i_null||!messages.emplace(row.get<int>(0),
            std::pair{row.get<std::string>(1),Unhex(row.get<std::string>(2))}).second)
            throw std::runtime_error("Missing or ambiguous source server message");
    }
    if(!messages.contains(1)||!messages.contains(6))throw std::runtime_error("Expired bag source messages are absent");
    const auto& [decoded,bytes]=messages.at(1);const auto& [sender,sender_bytes]=messages.at(6);
    PostText text{sender,NetString(decoded,bytes.size()),NetString(decoded,bytes.size()),
        sender_bytes,NetString(bytes,bytes.size()),NetString(bytes,bytes.size())};
    if(text.sender_bytes.size()>50||text.title_bytes.size()>256||text.message_bytes.size()>2048
        ||text.sender_bytes.find('\0')!=std::string::npos||bytes.find('\0')!=std::string::npos)
        throw std::runtime_error("Expired bag source text exceeds original post buffers");
    return text;
}
}
void PostgreSQLMapService::HydrateNativePosts(soci::session& sql,const MapSessionClaim& c,CharacterPayload& p) {
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    p.mail_observed_notification=0;
    p.posts.clear();
    soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT *,floor(extract(epoch FROM \"timeRecv\" AT TIME ZONE 'UTC'))::bigint AS epoch,"
        "encode(sender_bytes,'hex') AS sender_hex,encode(title_bytes,'hex') AS title_hex,encode(message_bytes,'hex') AS message_hex, "
        "CASE WHEN \"dwGold\"<>0 OR \"dwSilver\"<>0 OR \"dwCooper\"<>0 OR EXISTS (SELECT 1 FROM app_world.\"TITEMTABLE\" i "
        "WHERE i.\"bWorldID\"=p.\"bWorldID\" AND i.\"dwOwnerID\"=p.\"dwCharID\" AND i.\"bOwnerType\"=0 AND i.\"bStorageType\"=2 "
        "AND i.\"dwStorageID\"=p.\"dwPostID\") THEN 1 ELSE 0 END AS contains "
        ",(SELECT COALESCE(max(notification_id),0) FROM app_world.mail_notifications WHERE world_id=:w AND char_id=:c) AS observed "
        "FROM app_world.\"TPOSTTABLE\" p WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c ORDER BY \"dwPostID\" DESC",
        soci::use(world,"w"),soci::use(owner,"c"));
    for(const auto& row:rows) {
        const auto number=[&](const char* name){return static_cast<std::uint32_t>(row.get<int>(name));};
        const auto text=[&](const char* decoded,const char* bytes){return row.get_indicator(bytes)==soci::i_null?
            row.get<std::string>(decoded):Unhex(row.get<std::string>(bytes));};
        CharacterPost post;post.id=number("dwPostID");post.sender_id=number("dwSendID");
        post.type=number("bType");post.read=number("bRead");post.received_at=row.get<long long>("epoch");
        post.recipient=row.get<std::string>("szRecvName");post.sender=text("szSender","sender_hex");
        post.title=text("szTitle","title_hex");post.message=text("szMessage","message_hex");
        post.gold=number("dwGold");post.silver=number("dwSilver");post.cooper=number("dwCooper");
        post.contains=number("contains");
        p.posts.push_back(std::move(post));
        p.mail_observed_notification=static_cast<std::uint64_t>(row.get<long long>("observed"));
    }
    if(p.transfer_state) {
        // Carried state retains its graph authority. Mail can be returned or
        // delivered by another primary, so STORAGE_POST is always normalized
        // authority and the graph holds only a refreshed read projection.
        auto graph=std::make_shared<transfer::State>(*p.transfer_state);
        std::erase_if(graph->items,[](const auto& item){return item.storage==2;});
        soci::rowset<soci::row> items=(sql.prepare << "SELECT i.*,CASE WHEN \"dEndTime\"<TIMESTAMP '2000-01-01' THEN 0 ELSE "
            "extract(epoch FROM \"dEndTime\" AT TIME ZONE 'UTC')::bigint END AS epoch FROM app_world.\"TITEMTABLE\" i "
            "WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"bOwnerType\"=0 AND \"bStorageType\"=2 ORDER BY \"dlID\"",
            soci::use(world,"w"),soci::use(owner,"c"));
        for(const auto& row:items)graph->items.push_back(native_item_row::Read(row));
        p.transfer_state=std::move(graph);
    }
    if(!p.post_counts&&p.transfer_state) {
        p.post_counts=std::pair{p.transfer_state->post_total,p.transfer_state->post_read};
        long long baseline=0;
        sql << "SELECT mail_notification_baseline FROM app_world.map_checkpoints WHERE world_id=:w AND char_id=:c",
            soci::use(world,"w"),soci::use(owner,"c"),soci::into(baseline);
        if(sql.got_data())p.mail_notification_baseline=static_cast<std::uint64_t>(baseline);
    }
    else if(!p.post_counts) {
        if(p.posts.size()>std::numeric_limits<std::uint16_t>::max())throw std::runtime_error("Source mailbox count overflow");
        p.post_counts=std::pair{static_cast<std::uint16_t>(p.posts.size()),static_cast<std::uint16_t>(
            std::count_if(p.posts.begin(),p.posts.end(),[](const auto& post){return post.read==0;}))};
        p.mail_notification_baseline=p.mail_observed_notification;
    }
}
void PostgreSQLMapService::PostExpiredBag(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint8_t inventory,QuestPackets& packets,QuestPackets& notifications) {
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    auto bag=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& b){return b.bag.bInvenID==inventory;});
    if(bag==p->bags.end()||!bag->bag.dEndTime||bag->bag.dEndTime>p->event_time)
        throw std::runtime_error("Expired bag disappeared or its deadline changed");
    if(!p->post_counts)throw std::runtime_error("Expired bag mailbox counters were not hydrated");
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id),bag_id=inventory;
    const bool graph=bool(p->transfer_state),tutorial=s.bStartAct==1&&s.wMapID==2010;
    auto contents=bag->items;
    if(contents.size()>255)throw std::runtime_error("Expired bag exceeds source BYTE item count");
    std::sort(contents.begin(),contents.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
    if(!contents.empty())LockNativeMailbox(sql,c.group,c.char_id);
    const auto text=contents.empty()?PostText{}:ExpiredBagText(sql);
    const auto sender_hex=Hex(text.sender_bytes),title_hex=Hex(text.title_bytes),message_hex=Hex(text.message_bytes);
    // Original TSavePost stores SMALLDATETIME while POSTRECV uses the live
    // second. Keep that rounding distinction, with explicit native UTC policy.
    const long long now=p->event_time,stored_time=((now+30)/60)*60;
    std::shared_ptr<transfer::State> state;
    if(graph)state=std::make_shared<transfer::State>(*p->transfer_state);
    int post_id=0;
    for(std::size_t n=0;n<contents.size();++n) {
        if(n%3==0) {
            sql << "INSERT INTO app_world.\"TPOSTTABLE\"(\"bWorldID\",\"dwCharID\",\"szRecvName\",\"bType\",\"bRead\","
                "\"timeRecv\",\"dwSendID\",\"szSender\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\","
                "\"bContain\",sender_bytes,title_bytes,message_bytes) VALUES(:w,:c,:recipient,1,0,to_timestamp(:time) AT TIME ZONE 'UTC',"
                "0,:sender,:title,:message,0,0,0,1,decode(:sb,'hex'),decode(:tb,'hex'),decode(:mb,'hex')) RETURNING \"dwPostID\"",
                soci::use(world,"w"),soci::use(owner,"c"),soci::use(s.szNAME,"recipient"),soci::use(stored_time,"time"),
                soci::use(text.sender,"sender"),soci::use(text.title,"title"),soci::use(text.message,"message"),
                soci::use(sender_hex,"sb"),soci::use(title_hex,"tb"),soci::use(message_hex,"mb"),soci::into(post_id);
            if(!sql.got_data()||post_id<=0)throw std::runtime_error("Expired bag post identity was not allocated");
            CharacterPost post;post.id=post_id;post.type=1;post.received_at=stored_time;post.recipient=s.szNAME;
            post.contains=1;
            post.sender=text.sender_bytes;post.title=text.title_bytes;post.message=text.message_bytes;p->posts.push_back(std::move(post));
            if(!tutorial) {
                std::vector<std::byte> ack;wire::WritePOD(ack,static_cast<std::uint32_t>(post_id));
                wire::WritePOD(ack,std::uint8_t{0});wire::WritePOD(ack,std::uint8_t{1});
                wire::WriteString(ack,text.sender_bytes);wire::WriteString(ack,text.title_bytes);wire::WritePOD(ack,static_cast<std::int64_t>(now));
                notifications.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_POSTRECV_ACK),std::move(ack));
                ++p->post_counts->first;++p->post_counts->second; // source WORD counters, read means unread
            }
        }
        const auto& item=contents[n];
        if(!item.source||item.source->id!=item.dlID||item.source->storage||item.source->owner_type
            ||item.source->owner_id!=c.char_id||item.source->storage_id!=inventory)
            throw std::runtime_error("Expired bag item raw ownership changed");
        const long long id=std::bit_cast<long long>(item.dlID);
        if(graph) {
            // The validated full graph owns every attribute. Retire only its
            // own stale carried projection; never overwrite a foreign identity
            // or an existing post attachment. Global item uniqueness fences it.
            sql << "DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c "
                "AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0",
                soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id");
            auto raw=*item.source;raw.storage=2;raw.storage_id=static_cast<std::uint32_t>(post_id);
            InsertNativeItem(sql,c,raw);
            std::erase_if(state->items,[&](const auto& old){return old.id==item.dlID;});state->items.push_back(std::move(raw));
        }else {
            long long moved=0;
            sql << "UPDATE app_world.\"TITEMTABLE\" i SET \"bStorageType\"=2,\"dwStorageID\"=:post,post_id=:post "
                "WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0 "
                "AND app_world.item_fingerprint(i)=:hash RETURNING \"dlID\"",
                soci::use(post_id,"post"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(id,"id"),
                soci::use(item.durable_hash,"hash"),soci::into(moved);
            if(!sql.got_data()||moved!=id)throw std::runtime_error("Expired bag attachment ownership changed");
        }
    }
    int removed=-1;
    if(graph) {
        sql << "DELETE FROM app_world.\"TINVENTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"bInvenID\"=:bag",
            soci::use(world,"w"),soci::use(owner,"c"),soci::use(bag_id,"bag");
        p->transfer_state=std::move(state);
    }else {
        const int template_id=std::bit_cast<std::int16_t>(bag->bag.wItemID),eld=bag->bag.bELD;
        const long long deadline=bag->bag.dEndTime;
        sql << "DELETE FROM app_world.\"TINVENTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"bInvenID\"=:bag "
            "AND \"wItemID\"=:item AND \"bELD\"=:eld AND floor(extract(epoch FROM \"dEndTime\" AT TIME ZONE 'UTC'))::bigint=:deadline "
            "RETURNING \"bInvenID\"",soci::use(world,"w"),soci::use(owner,"c"),soci::use(bag_id,"bag"),
            soci::use(template_id,"item"),soci::use(eld,"eld"),soci::use(deadline,"deadline"),soci::into(removed);
        if(!sql.got_data()||removed!=bag_id)throw std::runtime_error("Expired bag persisted identity changed");
    }
    p->bags.erase(bag);
    packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_INVENDEL_ACK),
        std::vector<std::byte>{std::byte{0},static_cast<std::byte>(inventory),std::byte{1}});
}
bool PostgreSQLMapService::NativeMail(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint16_t opcode,std::span<const std::byte> body,const QuestEvent& event,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;const auto message=static_cast<MessageId>(opcode);
    if(s.bStartAct==2||(s.bStartAct==1&&s.wMapID==2010)||
        (message!=MessageId::CS_POSTLIST_REQ&&s.payload->tournament_player))return false;
    if((message==MessageId::CS_POSTGETITEM_REQ||message==MessageId::CS_POSTDEL_REQ||message==MessageId::CS_POSTRETURN_REQ)&&s.payload->security.Blocks())
        throw std::runtime_error("Native post ownership change requires secure unlock");
    wire::Reader reader(body.data(),body.size());std::uint32_t id=0;std::uint16_t page=0;
    if(message==MessageId::CS_POSTLIST_REQ) {
        if(!reader.Read(page)||!reader.Eof())throw std::runtime_error("Invalid POSTLIST request");
    }else if(message==MessageId::CS_POSTRETURN_REQ) {
        std::uint8_t payment=0;
        if(!reader.Read(id)||!reader.Read(payment)||!reader.Eof())throw std::runtime_error("Invalid POSTRETURN request");
        if(s.payload->viewed_post!=id)return false;
        const auto post=std::find_if(s.payload->posts.begin(),s.payload->posts.end(),[&](const auto& row){return row.id==id;});
        if(post==s.payload->posts.end()||post->read)return false;
        return ReturnNativePost(sql,c,s,id,payment!=0,true,packets);
    }else if(!reader.Read(id)||!reader.Eof())throw std::runtime_error("Invalid post request");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id),post_id=std::bit_cast<std::int32_t>(id);
    const auto get_reply=[&](std::uint8_t result){packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTGETITEM_ACK),
        std::vector<std::byte>{static_cast<std::byte>(result)});};
    if(message==MessageId::CS_POSTGETITEM_REQ&&p->viewed_post!=id){get_reply(8);return false;}
    if(message==MessageId::CS_POSTDEL_REQ)p->viewed_post=0;
    // Account/claim/checkpoint locks are already held by ChangeQuest. Serialize
    // the actual post row as well; an ID from a client never grants ownership.
    if(message!=MessageId::CS_POSTLIST_REQ) {
        int locked=0;
        sql << "SELECT \"dwPostID\" FROM app_world.\"TPOSTTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c "
            "AND \"dwPostID\"=:id FOR UPDATE",soci::use(world,"w"),soci::use(owner,"c"),soci::use(post_id,"id"),soci::into(locked);
        if(!sql.got_data()) {
            if(message==MessageId::CS_POSTGETITEM_REQ)get_reply(8);
            else if(message==MessageId::CS_POSTDEL_REQ)packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTDEL_ACK),std::vector<std::byte>(4));
            else p->viewed_post=0; // source POSTVIEW failure has no client ACK
            return false;
        }
    }
    HydrateNativePosts(sql,c,*p);
    if(message==MessageId::CS_POSTLIST_REQ) {
        // TGetPostInfo advances by seven IDs, retaining the last ID when the
        // page runs beyond the end. CTBLPostTable returns ALL rows <=beginID.
        // Preserve that source quirk; a seven-row LIMIT would change packets.
        if(p->posts.size()>65535)throw std::runtime_error("Post list exceeds source WORD count");
        const auto unread=std::count_if(p->posts.begin(),p->posts.end(),[](const auto& post){return !post.read;});
        p->post_counts=std::pair{static_cast<std::uint16_t>(p->posts.size()),static_cast<std::uint16_t>(unread)};
        p->mail_notification_baseline=std::max(p->mail_notification_baseline,p->mail_observed_notification);
        const auto start=p->posts.empty()?0U:std::min<std::size_t>(page>1?7U*(page-1U):0U,p->posts.size()-1U);
        std::vector<std::byte> ack;wire::WritePOD(ack,p->post_counts->first);wire::WritePOD(ack,p->post_counts->second);
        wire::WritePOD(ack,page);wire::WritePOD(ack,static_cast<std::uint16_t>(p->posts.size()-start));
        for(std::size_t n=start;n<p->posts.size();++n) {
            const auto& post=p->posts[n];wire::WritePOD(ack,post.id);wire::WritePOD(ack,post.read);wire::WritePOD(ack,post.type);
            wire::WriteString(ack,post.sender);wire::WriteString(ack,post.title);wire::WritePOD(ack,post.received_at);wire::WritePOD(ack,post.contains);
        }
        if(ack.size()>65527)throw std::runtime_error("Source unbounded post list exceeds frame capacity");
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTLIST_ACK),std::move(ack));return true;
    }
    auto post=std::find_if(p->posts.begin(),p->posts.end(),[&](const auto& row){return row.id==id;});
    if(post==p->posts.end())throw std::runtime_error("Locked post disappeared");
    std::vector<ItemInstance> items;
    {soci::rowset<soci::row> rows=(sql.prepare << "SELECT i.*,CASE WHEN \"dEndTime\"<TIMESTAMP '2000-01-01' THEN 0 ELSE "
        "extract(epoch FROM \"dEndTime\" AT TIME ZONE 'UTC')::bigint END AS epoch,app_world.item_fingerprint(i) AS hash "
        "FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"bOwnerType\"=0 "
        "AND \"bStorageType\"=2 AND \"dwStorageID\"=:id ORDER BY \"bItemID\",\"dlID\" FOR UPDATE",
        soci::use(world,"w"),soci::use(owner,"c"),soci::use(post_id,"id"));
     for(const auto& row:rows){auto item=ProjectNativeItem(sql,native_item_row::Read(row));item.durable_hash=row.get<std::string>("hash");items.push_back(std::move(item));}}
    if(items.size()>255)throw std::runtime_error("Source post attachment count overflow");
    if(message==MessageId::CS_POSTVIEW_REQ) {
        if(post->type!=2&&!post->read) {
            sql << "UPDATE app_world.\"TPOSTTABLE\" SET \"bRead\"=1 WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwPostID\"=:id",
                soci::use(world,"w"),soci::use(owner,"c"),soci::use(post_id,"id");post->read=1;
        }
        p->viewed_post=id;
        std::vector<std::byte> ack;wire::WritePOD(ack,id);wire::WritePOD(ack,post->read);wire::WriteString(ack,post->message);
        wire::WritePOD(ack,post->gold);wire::WritePOD(ack,post->silver);wire::WritePOD(ack,post->cooper);
        wire::WritePOD(ack,post->contains);wire::WritePOD(ack,static_cast<std::uint8_t>(items.size()));
        for(const auto& item:items){auto descriptor=EncodeItemDescriptor(item,c.char_id,false);ack.insert(ack.end(),descriptor.begin(),descriptor.end());}
        if(ack.size()>65527)throw std::runtime_error("Post view exceeds frame capacity");
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTVIEW_ACK),std::move(ack));return true;
    }
    if(message==MessageId::CS_POSTDEL_REQ) {
        const bool empty=items.empty()&&!post->gold&&!post->silver&&!post->cooper;
        if(empty) {
            sql << "DELETE FROM app_world.\"TPOSTTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwPostID\"=:id",
                soci::use(world,"w"),soci::use(owner,"c"),soci::use(post_id,"id");p->posts.erase(post);
        }
        std::vector<std::byte> ack;wire::WritePOD(ack,empty?id:std::uint32_t{0});
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTDEL_ACK),std::move(ack));return empty;
    }
    if(message!=MessageId::CS_POSTGETITEM_REQ)throw std::runtime_error("Native mail operation is unavailable");
    // Source GETITEM lacks this bill gate. Do not port an unpaid-bill grant.
    // Full settlement/return delivery remains an explicit integration dependency.
    if(post->type==2&&!post->read){get_reply(10);return false;}
    if(!CanGrantQuestItems(*p,items)){get_reply(7);return false;}
    const auto money=static_cast<std::uint32_t>(post->cooper+post->silver*1000U)+std::uint64_t{post->gold}*1000000U;
    const bool changed=!items.empty()||money!=0;
    if(!changed){get_reply(2);return false;}
    if(p->transfer_state) {
        auto graph=std::make_shared<transfer::State>(*p->transfer_state);
        std::erase_if(graph->items,[&](const auto& item){return item.storage==2&&item.storage_id==id&&item.owner_type==0&&item.owner_id==c.char_id;});
        p->transfer_state=std::move(graph);
    }
    for(const auto& item:items) {
        if(!item.bCount||item.bCount>item.stack_limit)throw std::runtime_error("Invalid source post stack");
        const long long identity=std::bit_cast<long long>(item.dlID);long long removed=0;
        sql << "DELETE FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:item "
            "AND \"bOwnerType\"=0 AND \"bStorageType\"=2 AND \"dwStorageID\"=:post AND app_world.item_fingerprint(i)=:hash RETURNING \"dlID\"",
            soci::use(world,"w"),soci::use(owner,"c"),soci::use(identity,"item"),soci::use(post_id,"post"),soci::use(item.durable_hash,"hash"),soci::into(removed);
        if(!sql.got_data()||removed!=identity)throw std::runtime_error("Post attachment changed during claim");
    }
    if(!GrantQuestItems(sql,c,s,items,packets))throw std::runtime_error("Post placement changed after capacity gate");
    for(const auto& item:items)event(item.wItemID,QTT_GETITEM,TT_GETITEM,item.bCount);
    sql << "UPDATE app_world.\"TPOSTTABLE\" SET \"dwGold\"=0,\"dwSilver\"=0,\"dwCooper\"=0 "
        "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwPostID\"=:id",soci::use(world,"w"),soci::use(owner,"c"),soci::use(post_id,"id");
    if(money){EarnNativeMoney(sql,c,s,money,packets);packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));}
    p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    for(auto& row:p->posts)if(row.id==id){row.gold=row.silver=row.cooper=0;row.contains=0;}
    get_reply(0);return true;
}
}
