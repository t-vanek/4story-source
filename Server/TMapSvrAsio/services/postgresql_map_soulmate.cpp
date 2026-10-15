#include "postgresql_map_service.h"
#include "native_wire_text.h"
#include "postgresql_map_owner.h"
#include "services/client_senders.h"
#include "services/secure_code.h"
#include "MessageId.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
std::string Hex(std::span<const std::byte> bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(auto value:bytes){const auto b=std::to_integer<unsigned>(value);out+=digits[b>>4];out+=digits[b&15];}return out;
}
bool IsSearch(std::uint16_t op) {
    return op==static_cast<std::uint16_t>(MessageId::CS_SOULMATESEARCH_REQ)||
        op==static_cast<std::uint16_t>(MessageId::MW_SOULMATESEARCH_REQ);
}
bool IsResponse(std::uint16_t op) {
    return op==static_cast<std::uint16_t>(MessageId::MW_SOULMATESEARCH_REQ)||
        op==static_cast<std::uint16_t>(MessageId::MW_SOULMATEREG_REQ);
}
struct Reply {
    std::uint32_t character{},key{},target{},region{};
    std::uint8_t result{},reg{},bag{},slot{};
    std::string name;
};
Reply ReadReply(std::uint16_t opcode,std::span<const std::byte> body) {
    Reply out;wire::Reader r(body.data(),body.size());
    if(!r.Read(out.character)||!r.Read(out.key)||!r.Read(out.result))throw std::runtime_error("Invalid soulmate reply identity");
    if(IsSearch(opcode)) {
        if(!r.Read(out.target)||!r.ReadString(out.name)||!r.Read(out.region)||!r.Read(out.bag)||!r.Read(out.slot))
            throw std::runtime_error("Invalid soulmate search reply");
    }else if(!r.Read(out.reg)||!r.Read(out.bag)||!r.Read(out.slot)||!r.Read(out.target)||!r.ReadString(out.name)||!r.Read(out.region))
        throw std::runtime_error("Invalid soulmate registration reply");
    if(!r.Eof()||out.result>7||out.reg>1||out.name.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid soulmate reply fields");
    return out;
}
std::uint32_t Cost(soci::session& sql,std::uint8_t level,bool search) {
    const auto field=search?"dwSearchCost":"dwRegCost";const int id=level;int cost=0;
    sql<<"SELECT \""+std::string(field)+"\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:id",soci::use(id),soci::into(cost);
    if(!sql.got_data()) {
        sql<<"SELECT \""+std::string(field)+"\" FROM character_compat.\"TLEVELCHART\" ORDER BY \"bLevel\" LIMIT 1",soci::into(cost);
        if(!sql.got_data())throw std::runtime_error("Soulmate price has no source level fallback");
    }
    return static_cast<std::uint32_t>(cost);
}
void ClientReply(const CharSnapshot& s,bool search,bool ready,std::uint8_t result,std::uint32_t target,
    const std::string& name,std::uint32_t region,std::uint8_t bag,std::uint8_t slot,QuestPackets& packets) {
    std::vector<std::byte> bytes;wire::WritePOD(bytes,result);
    if(ready){wire::WriteString(bytes,name);wire::WritePOD(bytes,bag);wire::WritePOD(bytes,slot);}
    else {
        wire::WritePOD(bytes,target);wire::WriteString(bytes,name);wire::WritePOD(bytes,region);
        // Original CSSender includes the purse even on failure. The client
        // conditionally reads it, but the server's wire contract has all 12 bytes.
        wire::WritePOD(bytes,s.dwGold);wire::WritePOD(bytes,s.dwSilver);wire::WritePOD(bytes,s.dwCooper);
    }
    packets.emplace_back(static_cast<std::uint16_t>(ready?MessageId::CS_SOULMATEREGREADY_ACK:
        search?MessageId::CS_SOULMATESEARCH_ACK:MessageId::CS_SOULMATEREG_ACK),std::move(bytes));
}
const ItemInstance* Caller(const CharSnapshot& s,std::uint8_t bag,std::uint8_t slot) {
    for(const auto& inventory:s.payload->bags)if(inventory.bag.bInvenID==bag)
        for(const auto& item:inventory.items)if(item.bItemID==slot)return &item;
    return nullptr;
}
}

bool PostgreSQLMapService::ConfirmSoulmateState(const MapSessionClaim& c,std::uint32_t target,const std::string& name,std::uint32_t silence) {
    if(c.role!=MapSessionRole::Primary||name.find('\0')!=std::string::npos)return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,false,true);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||(phase!="loaded"&&phase!="ready"))return false;
    const int world=c.group,character=SqlInt32Identity(c.char_id);soci::row row;
    sql<<"SELECT s.\"dwTarget\",s.\"dwTime\",t.\"szNAME\" FROM app_world.\"TSOULMATETABLE\" s "
         "JOIN app_world.\"TCHARTABLE\" t ON t.\"bWorldID\"=s.\"bWorldID\" AND t.\"dwCharID\"=s.\"dwTarget\" "
         "WHERE s.\"bWorldID\"=:w AND s.\"dwCharID\"=:c FOR SHARE OF s,t",
        soci::use(world,"w"),soci::use(character,"c"),soci::into(row);
    bool confirmed=false;
    if(!sql.got_data())confirmed=!target&&!silence&&name.empty();
    else {
        const auto time=static_cast<std::uint32_t>(row.get<int>(1));
        if(time)confirmed=!target&&name.empty()&&silence==time;
        else confirmed=!silence&&target==static_cast<std::uint32_t>(row.get<int>(0))&&
            native_wire_text::Decode(sql,name,m_config.client_text_encoding)==row.get<std::string>(2);
    }
    tx->commit();return confirmed;
}

void PostgreSQLMapService::CheckNativeSoulmateDeadline(soci::session& sql,const MapSessionClaim& c) const {
    const int world=c.group,character=SqlInt32Identity(c.char_id),key=SqlInt32Identity(c.key);const long long generation=c.connection_id,epoch=c.authority_epoch;
    int overdue=0;
    sql<<"SELECT count(*) FROM app_world.soulmate_offers WHERE world_id=:w AND char_id=:c AND session_key=:k "
         "AND source_server=:s AND map_owner_token=:t AND connection_id=:g AND authority_epoch=:e AND status IN ('pending','prepared') "
         "AND created_at<clock_timestamp()-interval '15 seconds'",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::use(m_config.server,"s"),soci::use(m_config.owner_token,"t"),
        soci::use(generation,"g"),soci::use(epoch,"e"),soci::into(overdue);
    if(overdue)throw std::runtime_error("Soulmate reply deadline expired; close without replay");
}

void PostgreSQLMapService::LockNativeSoulmateTarget(soci::session& sql,const MapSessionClaim& c,
    std::uint16_t opcode,std::span<const std::byte> body) const {
    if(!IsResponse(opcode))return;
    const auto reply=ReadReply(opcode,body);
    if(reply.character!=c.char_id||reply.key!=c.key)throw std::runtime_error("Soulmate response claim mismatch");
    if(!IsSearch(opcode)&&!reply.reg)return; // REGREADY is a read-only World preview.
    const int world=c.group,character=SqlInt32Identity(c.char_id),key=SqlInt32Identity(c.key),server=m_config.server,op=opcode;
    const long long generation=c.connection_id,epoch=c.authority_epoch;const auto bytes=Hex(body);soci::row offer;
    sql<<"SELECT f.target_id,f.target_server FROM app_world.soulmate_offers f JOIN app_world.world_runtime_owner o "
         "ON o.world_id=f.world_id AND o.owner_token=f.world_owner_token WHERE f.world_id=:w AND f.char_id=:c "
         "AND f.session_key=:k AND f.source_server=:s AND f.map_owner_token=:t AND f.connection_id=:g AND f.authority_epoch=:e "
         "AND f.status='prepared' AND f.response_opcode=:op AND f.response=decode(:bytes,'hex')",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
        soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(op,"op"),soci::use(bytes,"bytes"),soci::into(offer);
    if(!sql.got_data())throw std::runtime_error("Unsolicited or stale soulmate reply");
    if(reply.result||!reply.target)return;
    if(offer.get_indicator(0)==soci::i_null||static_cast<std::uint32_t>(offer.get<int>(0))!=reply.target)
        throw std::runtime_error("Soulmate selected target changed");
    const int target_server=offer.get<int>(1);int live=0;
    sql<<"SELECT 1 FROM app_world.map_runtime_owner o WHERE o.world_id=:w AND o.server_id=:s "
         "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
         "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((o.world_id::integer<<8)|o.server_id::integer AS oid) "
         "AND l.objsubid=2 AND l.granted) FOR SHARE OF o",soci::use(world,"w"),soci::use(target_server,"s"),soci::into(live);
    if(!sql.got_data()||live!=1)throw std::runtime_error("Soulmate target Map ownership lost");
    const int target=SqlInt32Identity(reply.target);int user=0;
    sql<<"SELECT \"dwUserID\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"bDelete\"=0",
        soci::use(world,"w"),soci::use(target,"c"),soci::into(user);
    if(!sql.got_data())throw std::runtime_error("Soulmate target was deleted");
    std::set<std::uint32_t> users{c.user_id,static_cast<std::uint32_t>(user)};
    for(auto identity:users) {
        const int account=SqlInt32Identity(identity);int found=0;
        sql<<"SELECT 1 FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",soci::use(account),soci::into(found);
        if(!sql.got_data()||found!=1)throw std::runtime_error("Soulmate account no longer exists");
    }
}

bool PostgreSQLMapService::ChangeNativeSoulmate(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint16_t opcode,std::span<const std::byte> body,const QuestMapFacts& facts,QuestPackets& packets,
    std::optional<SoulmateOfferCommit>& consumed_offer) const {
    const bool response=IsResponse(opcode),search=IsSearch(opcode);
    const bool ready=opcode==static_cast<std::uint16_t>(MessageId::CS_SOULMATEREGREADY_REQ);
    const int world=c.group,character=SqlInt32Identity(c.char_id),key=SqlInt32Identity(c.key),server=m_config.server;
    const long long generation=c.connection_id,epoch=c.authority_epoch;
    if(!response) {
        // Source REG prompts before decoding; its repository repeats the gate
        // after the security/current-primary checkpoint has been fenced.
        if(!search&&!ready&&s.payload->security.Blocks()) {
            const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return false;
        }
        if(s.bStartAct==2||(s.bStartAct==1&&s.wMapID==2010))return false;
        wire::Reader r(body.data(),body.size());std::string name;std::uint8_t bag=0,slot=0;
        if((!search&&!r.ReadString(name))||!r.Read(bag)||!r.Read(slot)||!r.Eof()||name.find('\0')!=std::string::npos)
            throw std::runtime_error("Invalid soulmate request");
        const auto failure=[&](std::uint8_t result){ClientReply(s,search,ready,result,0,{},0,bag,slot,packets);};
        long long current=0;sql<<"SELECT floor(extract(epoch FROM clock_timestamp()))::bigint",soci::into(current);
        if(s.cluster.soul_silence&&static_cast<std::uint32_t>(static_cast<std::uint32_t>(current)-s.cluster.soul_silence)<86400U){failure(2);return false;}
        if(!search&&name==s.szNAME)return false;
        if(!search&&name==s.cluster.soulmate_name){failure(5);return false;}
        bool touched=false;
        if(!ready) {
            // Keep old unresolved rows as evidence; never replay them into a
            // replacement connection/primary. Same-claim requests stay single.
            sql<<"UPDATE app_world.soulmate_offers SET status='abandoned' WHERE world_id=:w AND char_id=:c "
                 "AND status IN ('pending','prepared') AND (session_key<>:k OR source_server<>:s OR map_owner_token<>:t "
                 "OR connection_id<>:g OR authority_epoch<>:e)",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
                soci::use(generation,"g"),soci::use(epoch,"e");
            int pending=0;sql<<"SELECT count(*) FROM app_world.soulmate_offers WHERE world_id=:w AND char_id=:c "
                "AND status IN ('pending','prepared')",soci::use(world,"w"),soci::use(character,"c"),soci::into(pending);
            if(pending)return false;
            if(!UseNativeMoney(sql,c,s,Cost(sql,s.bLevel,search),false,packets)){failure(4);return false;}
            touched=true; // UseMoney(FALSE) may award a source gold title.
        }
        if(bag!=252&&slot!=255) {
            if(s.wMapID!=0&&s.wMapID!=8){failure(search||ready?7:6);return touched;}
            const auto caller=Caller(s,bag,slot);
            if(!caller||caller->bKind!=55||!caller->bCount){failure(6);return touched;}
        }
        if(!facts.world_registered)throw std::runtime_error("Soulmate relay requires registered World");
        if(!ready) {
            const int op=opcode;const auto bytes=Hex(body);
            sql<<"INSERT INTO app_world.soulmate_offers(world_id,char_id,session_key,source_server,map_owner_token,connection_id,authority_epoch,request_opcode,request) "
                 "VALUES(:w,:c,:k,:s,:t,:g,:e,:op,decode(:bytes,'hex'))",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
                soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(op,"op"),soci::use(bytes,"bytes");
            touched=true;
        }
        std::vector<std::byte> relay;wire::WritePOD(relay,c.char_id);wire::WritePOD(relay,c.key);
        if(search)wire::WritePOD(relay,facts.soulmate_min_level);
        else {wire::WriteString(relay,name);wire::WritePOD(relay,static_cast<std::uint8_t>(ready?0:1));}
        wire::WritePOD(relay,bag);wire::WritePOD(relay,slot);
        packets.emplace_back(static_cast<std::uint16_t>(search?MessageId::MW_SOULMATESEARCH_ACK:MessageId::MW_SOULMATEREG_ACK),
            std::move(relay),QuestAudience::World);
        return touched;
    }
    const auto reply=ReadReply(opcode,body);
    if(reply.character!=c.char_id||reply.key!=c.key)throw std::runtime_error("Soulmate reply owner changed");
    if(!search&&!reply.reg) {
        ClientReply(s,false,true,reply.result,reply.target,reply.name,reply.region,reply.bag,reply.slot,packets);return false;
    }
    const int op=opcode;const auto bytes=Hex(body);soci::row offer;
    sql<<"SELECT f.offer_id,f.target_id,encode(f.target_name,'hex'),f.target_region,f.target_level,f.target_class,f.target_session_key,f.target_server "
         "FROM app_world.soulmate_offers f JOIN app_world.world_runtime_owner o ON o.world_id=f.world_id AND o.owner_token=f.world_owner_token "
         "WHERE f.world_id=:w AND f.char_id=:c AND f.session_key=:k AND f.source_server=:s AND f.map_owner_token=:t "
         "AND f.connection_id=:g AND f.authority_epoch=:e AND f.status='prepared' AND f.response_opcode=:op AND f.response=decode(:bytes,'hex') "
         "AND f.created_at>=clock_timestamp()-interval '15 seconds' FOR UPDATE OF f",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
        soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(op,"op"),soci::use(bytes,"bytes"),soci::into(offer);
    if(!sql.got_data())throw std::runtime_error("Soulmate offer changed or was already consumed");
    consumed_offer=SoulmateOfferCommit{offer.get<long long>(0),false};
    if(!search&&!reply.result&&s.payload->security.Blocks()) {
        const auto prompt=SecureUnlockPrompt(s.payload->security);packets.emplace_back(prompt.first,prompt.second);return true;
    }
    const auto failure=[&](std::uint8_t result){ClientReply(s,search,false,result,0,{},0,reply.bag,reply.slot,packets);};
    const auto cost=Cost(sql,s.bLevel,search);
    // Search charges even a World NOTFOUND reply. REG only charges success.
    if(search&&!UseNativeMoney(sql,c,s,cost,false,packets)){failure(4);return true;}
    if(search||!reply.result) {
        std::uint64_t caller_id=0;
        if(reply.bag!=252&&reply.slot!=255) {
            if(s.wMapID!=0&&s.wMapID!=8){failure(7);return true;}
            const auto caller=Caller(s,reply.bag,reply.slot);
            if(!caller||caller->bKind!=55||!caller->bCount){failure(6);return true;}
            caller_id=caller->dlID;
        }
        // REG's response checks the caller first, as the source does. Its new
        // affordability guard still precedes every irreversible debit.
        if(!search&&!UseNativeMoney(sql,c,s,cost,false,packets)){failure(4);return true;}
        if(caller_id)DebitNativeItem(sql,c,s,reply.bag,reply.slot,caller_id,1,packets);
        if(!UseNativeMoney(sql,c,s,cost,true,packets))throw std::runtime_error("Soulmate purse changed after validation");
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
    }
    if(!reply.result) {
        if(!reply.target||reply.target==c.char_id||offer.get_indicator(1)==soci::i_null||
           static_cast<std::uint32_t>(offer.get<int>(1))!=reply.target||native_wire_text::Unhex(offer.get<std::string>(2))!=reply.name||
           static_cast<std::uint32_t>(offer.get<long long>(3))!=reply.region)throw std::runtime_error("Soulmate descriptor changed");
        const int target=SqlInt32Identity(reply.target),target_key=offer.get<int>(6),target_server=offer.get<int>(7);soci::row target_state;
        sql<<"SELECT c.\"szNAME\",c.\"bCountry\",c.\"bLevel\",c.\"bClass\" FROM app_world.map_sessions m "
             "JOIN app_world.map_runtime_owner o ON o.world_id=m.world_id AND o.server_id=m.server_id AND o.owner_token=m.owner_token "
             "JOIN app_global.\"TCURRENTUSER\" u ON u.\"dwKEY\"=m.session_key AND u.\"dwUserID\"=m.user_id AND u.\"dwCharID\"=m.char_id "
             "AND u.\"bGroupID\"=m.world_id AND u.\"bChannel\"=m.channel AND u.\"bLocked\"=0 "
             "JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=m.world_id AND c.\"dwCharID\"=m.char_id AND c.\"dwUserID\"=m.user_id "
             "WHERE m.world_id=:w AND m.char_id=:c AND m.session_key=:k AND m.server_id=:s AND m.phase='ready' AND c.\"bDelete\"=0 "
             "FOR SHARE OF m,o,u,c",soci::use(world,"w"),soci::use(target,"c"),soci::use(target_key,"k"),soci::use(target_server,"s"),soci::into(target_state);
        if(!sql.got_data()||target_state.get<int>(1)!=s.bCountry||target_state.get<int>(2)!=offer.get<int>(4)||
           target_state.get<int>(3)!=offer.get<int>(5)||native_wire_text::Decode(sql,reply.name,m_config.client_text_encoding)!=target_state.get<std::string>(0)||
           std::abs(int(s.bLevel)-target_state.get<int>(2))>(search?2:3))
            throw std::runtime_error("Soulmate target session or source eligibility changed");
        sql<<"INSERT INTO app_world.\"TSOULMATETABLE\"(\"bWorldID\",\"dwCharID\",\"dwTarget\",\"dwTime\") VALUES(:w,:c,:t,0) "
             "ON CONFLICT(\"bWorldID\",\"dwCharID\") DO UPDATE SET \"dwTarget\"=EXCLUDED.\"dwTarget\",\"dwTime\"=0",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(target,"t");
        const auto id=consumed_offer->id;
        sql<<"INSERT INTO app_world.social_mutations(world_id,owner_token,char_id,session_key,source_server,kind,target_id) "
             "SELECT world_id,world_owner_token,char_id,session_key,source_server,'soulmate_reg',target_id "
             "FROM app_world.soulmate_offers WHERE offer_id=:id",soci::use(id);
        consumed_offer->linked=true;
        s.cluster.soulmate=reply.target;s.cluster.soulmate_name=reply.name;s.cluster.soul_silence=0;
    }
    ClientReply(s,search,false,reply.result,reply.target,reply.name,reply.region,reply.bag,reply.slot,packets);
    return true;
}
}
