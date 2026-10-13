#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>

namespace tmapsvr {
namespace {
std::string HexBytes(const std::string& bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(unsigned char b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;
}
std::string OriginalTextHex(const soci::row& row,const char* raw,const char* decoded) {
    if(row.get_indicator(raw)!=soci::i_null)return row.get<std::string>(raw);
    const auto text=row.get<std::string>(decoded);
    // ASCII has the same representation in the source ANSI profiles. A NULL
    // byte column cannot justify inventing a non-ASCII SQL Server binding.
    if(!std::all_of(text.begin(),text.end(),[](unsigned char b){return b>0&&b<128;}))
        throw std::runtime_error("Post transfer requires original non-ASCII text bytes");
    return HexBytes(text);
}
void QueueNotification(soci::session& sql,int world,int post) {
    sql << "INSERT INTO app_world.mail_notifications(world_id,char_id,post_id,post_type) "
        "SELECT \"bWorldID\",\"dwCharID\",\"dwPostID\",\"bType\" FROM app_world.\"TPOSTTABLE\" "
        "WHERE \"bWorldID\"=:w AND \"dwPostID\"=:id",soci::use(world,"w"),soci::use(post,"id");
}
}
bool PostgreSQLMapService::ReturnNativePost(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint32_t id,bool payment,bool requested,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id),post=std::bit_cast<std::int32_t>(id);
    const auto failure=[&](std::uint8_t result) {
        if(requested)packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTRETURN_ACK),
            std::vector<std::byte>{static_cast<std::byte>(result)});
    };
    soci::row row;
    const std::string authorized=requested?"\"dwCharID\"=:c":"\"dwSendID\"=:c";
    // CTBLPostBill is keyed by dwSendID: online timers belong to the sender.
    // A client return instead requires the current recipient and viewed post.
    sql << std::string("SELECT *,encode(sender_bytes,'hex') AS sh,encode(recipient_bytes,'hex') AS rh,")+
        "encode(title_bytes,'hex') AS th,encode(message_bytes,'hex') AS mh FROM app_world.\"TPOSTTABLE\" "
        "WHERE \"bWorldID\"=:w AND "+authorized+" AND \"dwPostID\"=:id AND \"bRead\"=0 FOR UPDATE",
        soci::use(world,"w"),soci::use(owner,"c"),soci::use(post,"id"),soci::into(row);
    if(!sql.got_data()){failure(8);return false;}
    if(!requested&&row.get<int>("bType")!=2)return false;
    const int sender=row.get<int>("dwSendID"),recipient=row.get<int>("dwCharID");int exists=0;
    sql << "SELECT 1 FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:sender FOR KEY SHARE",
        soci::use(world,"w"),soci::use(sender,"sender"),soci::into(exists);
    // Original TPostBillsUpdate can orphan returned items when the sender has
    // been deleted. Retain the recipient's post instead; never debit or discard.
    if(!sql.got_data()){failure(8);return false;}
    const auto sender_hex=OriginalTextHex(row,"sh","szSender"),recipient_hex=OriginalTextHex(row,"rh","szRecvName");
    const auto title_hex=OriginalTextHex(row,"th","szTitle"),message_hex=OriginalTextHex(row,"mh","szMessage");
    const auto number=[&](const char* name){return static_cast<std::uint32_t>(row.get<int>(name));};
    const auto amount=static_cast<std::uint32_t>(number("dwCooper")+number("dwSilver")*1000U)+std::uint64_t{number("dwGold")}*1000000U;
    if(payment&&amount) {
        if(!UseNativeMoney(sql,c,s,amount,true,packets)){failure(3);return false;}
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
    }
    const long long stored_time=((s.payload->event_time+30)/60)*60;
    int delivered=post;
    if(payment) {
        sql << "INSERT INTO app_world.\"TPOSTTABLE\"(\"bWorldID\",\"dwCharID\",\"szRecvName\",\"bType\",\"bRead\","
            "\"timeRecv\",\"dwSendID\",\"szSender\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\","
            "sender_bytes,recipient_bytes,title_bytes,message_bytes) "
            "SELECT \"bWorldID\",\"dwSendID\",\"szSender\",4,0,to_timestamp(:time) AT TIME ZONE 'UTC',"
            "\"dwCharID\",\"szRecvName\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\","
            "decode(:rh,'hex'),decode(:sh,'hex'),decode(:th,'hex'),decode(:mh,'hex') "
            "FROM app_world.\"TPOSTTABLE\" WHERE \"bWorldID\"=:w AND \"dwPostID\"=:id RETURNING \"dwPostID\"",
            soci::use(stored_time,"time"),soci::use(recipient_hex,"rh"),soci::use(sender_hex,"sh"),
            soci::use(title_hex,"th"),soci::use(message_hex,"mh"),soci::use(world,"w"),soci::use(post,"id"),soci::into(delivered);
        if(!sql.got_data())throw std::runtime_error("Paid post disappeared");
        sql << "UPDATE app_world.\"TPOSTTABLE\" SET \"dwGold\"=0,\"dwSilver\"=0,\"dwCooper\"=0,\"bRead\"=1,bill_due_at=NULL "
            "WHERE \"bWorldID\"=:w AND \"dwPostID\"=:id",soci::use(world,"w"),soci::use(post,"id");
    }else {
        sql << "SET CONSTRAINTS app_world.item_post_recipient DEFERRED";
        sql << "UPDATE app_world.\"TPOSTTABLE\" SET \"dwCharID\"=\"dwSendID\",\"dwSendID\"=\"dwCharID\","
            "\"szRecvName\"=\"szSender\",\"szSender\"=\"szRecvName\",\"bType\"=3,\"bRead\"=0,"
            "\"dwGold\"=0,\"dwSilver\"=0,\"dwCooper\"=0,\"timeRecv\"=to_timestamp(:time) AT TIME ZONE 'UTC',bill_due_at=NULL,"
            "sender_bytes=decode(:rh,'hex'),recipient_bytes=decode(:sh,'hex'),title_bytes=decode(:th,'hex'),message_bytes=decode(:mh,'hex') "
            "WHERE \"bWorldID\"=:w AND \"dwPostID\"=:id",
            soci::use(stored_time,"time"),soci::use(recipient_hex,"rh"),soci::use(sender_hex,"sh"),
            soci::use(title_hex,"th"),soci::use(message_hex,"mh"),soci::use(world,"w"),soci::use(post,"id");
        sql << "UPDATE app_world.\"TITEMTABLE\" SET \"dwOwnerID\"=:sender WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c "
            "AND \"bOwnerType\"=0 AND \"bStorageType\"=2 AND \"dwStorageID\"=:id",
            soci::use(sender,"sender"),soci::use(world,"w"),soci::use(recipient,"c"),soci::use(post,"id");
        sql << "SET CONSTRAINTS app_world.item_post_recipient IMMEDIATE";
    }
    QueueNotification(sql,world,delivered);
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    if(!payment&&p->viewed_post==id)p->viewed_post=0;
    // STORAGE_POST is normalized authority, including for graph primaries. A
    // stale transfer projection must not regain attachments returned elsewhere.
    HydrateNativePosts(sql,c,*p);
    if(requested) {
        if(payment)packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTRETURN_ACK),std::vector<std::byte>{std::byte{0}});
        else {std::vector<std::byte> ack;wire::WritePOD(ack,id);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTDEL_ACK),std::move(ack));}
    }
    return true;
}
bool PostgreSQLMapService::PollNativeMail(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,QuestPackets& packets) const {
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    const long long now=s.payload->event_time;
    std::vector<std::uint32_t> expired;
    {soci::rowset<soci::row> rows=(sql.prepare << "SELECT \"dwPostID\" FROM app_world.\"TPOSTTABLE\" "
        "WHERE \"bWorldID\"=:w AND \"dwSendID\"=:c AND \"bRead\"=0 AND \"bType\"=2 "
        // POST_DURATION is 86400*3 seconds. Calendar '3 days' on timestamptz
        // would depend on the connection timezone across a DST transition.
        "AND COALESCE(bill_due_at,(\"timeRecv\" AT TIME ZONE 'UTC')+INTERVAL '259200 seconds')<=to_timestamp(:now) "
        "ORDER BY \"dwPostID\" LIMIT 256 FOR UPDATE",
        soci::use(world,"w"),soci::use(owner,"c"),soci::use(now,"now"));
     for(const auto& row:rows)expired.push_back(static_cast<std::uint32_t>(row.get<int>(0)));}
    bool changed=false;
    for(auto id:expired)changed=ReturnNativePost(sql,c,s,id,false,false,packets)||changed;
    std::vector<std::pair<long long,std::uint32_t>> pending;
    {soci::rowset<soci::row> rows=(sql.prepare << "SELECT n.notification_id,n.post_id FROM app_world.mail_notifications n "
        "JOIN app_world.\"TPOSTTABLE\" p ON p.\"bWorldID\"=n.world_id AND p.\"dwPostID\"=n.post_id "
        "AND p.\"dwCharID\"=n.char_id AND p.\"bType\"=n.post_type "
        "WHERE n.world_id=:w AND n.char_id=:c AND n.status='pending' ORDER BY n.notification_id LIMIT 256 FOR UPDATE OF n,p",
        soci::use(world,"w"),soci::use(owner,"c"));
     for(const auto& row:rows)pending.emplace_back(row.get<long long>(0),static_cast<std::uint32_t>(row.get<int>(1)));}
    if(pending.empty())return changed;
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;HydrateNativePosts(sql,c,*p);
    if(!p->post_counts)throw std::runtime_error("Mail notification counters unavailable");
    for(const auto& [notification,id]:pending) {
        const auto post=std::find_if(p->posts.begin(),p->posts.end(),[&](const auto& row){return row.id==id;});
        if(post==p->posts.end())throw std::runtime_error("Pending mail recipient changed");
        const int server=m_config.server;const long long connection=c.connection_id,epoch=c.authority_epoch,key=c.key;
        const int suppressed=s.bStartAct==1&&s.wMapID==2010;
        sql << "UPDATE app_world.mail_notifications SET status='committed',server_id=:server,owner_token=:token,"
            "connection_id=:connection,authority_epoch=:epoch,session_key=:key,committed_at=clock_timestamp() "
            ",tutorial_suppressed=CAST(:suppressed AS integer)<>0 "
            "WHERE notification_id=:id AND status='pending'",soci::use(server,"server"),soci::use(m_config.owner_token,"token"),
            soci::use(connection,"connection"),soci::use(epoch,"epoch"),soci::use(key,"key"),soci::use(notification,"id"),soci::use(suppressed,"suppressed");
        if(suppressed||static_cast<std::uint64_t>(notification)<=p->mail_notification_baseline)continue;
        std::vector<std::byte> ack;wire::WritePOD(ack,id);wire::WritePOD(ack,std::uint8_t{0});wire::WritePOD(ack,post->type);
        wire::WriteString(ack,post->sender);wire::WriteString(ack,post->title);wire::WritePOD(ack,now);
        if(ack.size()>65527)throw std::runtime_error("Mail notification exceeds source frame");
        packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_POSTRECV_ACK),std::move(ack));
        ++p->post_counts->first;++p->post_counts->second;
    }
    return true;
}
}
