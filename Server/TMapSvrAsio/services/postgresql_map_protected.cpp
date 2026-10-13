#include "postgresql_map_service.h"
#include "native_wire_text.h"
#include "native_mail_lock.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>

namespace tmapsvr {
void PostgreSQLMapService::HydrateNativeProtected(soci::session& sql,const MapSessionClaim& c,CharacterPayload& p) {
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    p.protected_characters.clear();
    soci::rowset<soci::row> rows=(sql.prepare <<
        "SELECT \"dwProtected\",\"szNAME\",encode(name_bytes,'hex') AS raw,\"bOption\" "
        "FROM app_world.\"TPROTECTEDTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c "
        "ORDER BY (\"dwProtected\"::bigint & 4294967295)",soci::use(world,"w"),soci::use(owner,"c"));
    for(const auto& row:rows)p.protected_characters.push_back({static_cast<std::uint32_t>(row.get<int>(0)),
        native_wire_text::Original(row,"raw","szNAME"),static_cast<std::uint8_t>(row.get<int>(3))});
    if(p.protected_characters.size()>255)throw std::runtime_error("Protected list exceeds original BYTE count");
    if(p.transfer_state) {
        auto graph=std::make_shared<transfer::State>(*p.transfer_state);
        graph->protected_characters.clear();
        for(const auto& row:p.protected_characters)graph->protected_characters.push_back({row.id,row.name,row.option,0});
        p.transfer_state=std::move(graph);
    }
}
void PostgreSQLMapService::ListNativeProtected(const CharacterPayload& p,QuestPackets& packets) {
    if(p.protected_characters.size()>255)throw std::runtime_error("Protected list exceeds original BYTE count");
    std::vector<std::byte> ack;wire::WritePOD(ack,static_cast<std::uint8_t>(p.protected_characters.size()));
    for(const auto& row:p.protected_characters){wire::WritePOD(ack,row.id);wire::WriteString(ack,row.name);wire::WritePOD(ack,row.option);}
    packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_PROTECTEDLIST_ACK),std::move(ack));
}
bool PostgreSQLMapService::ChangeNativeProtected(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint16_t opcode,std::span<const std::byte> body,QuestPackets& packets) const {
    using tnetlib::protocol::MessageId;const auto message=static_cast<MessageId>(opcode);
    const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    // Never restore a transferred remembered list over committed SQL. The
    // account/primary/head locks in ChangeQuest serialize all native writers.
    LockNativeMailbox(sql,c.group,c.char_id);
    HydrateNativeProtected(sql,c,*p);
    wire::Reader reader(body.data(),body.size());std::string name;std::uint32_t target=0;std::uint8_t option=0;
    if(message==MessageId::CS_PROTECTEDOPTION_REQ) {
        if(!reader.Read(target)||!reader.Read(option)||!reader.Eof())throw std::runtime_error("Invalid PROTECTEDOPTION request");
        const auto found=std::find_if(p->protected_characters.begin(),p->protected_characters.end(),[&](const auto& row){return row.id==target;});
        if(found==p->protected_characters.end())return false; // source deliberate no response
        const int tid=std::bit_cast<std::int32_t>(target),bits=option;
        sql << "UPDATE app_world.\"TPROTECTEDTABLE\" SET \"bOption\"=:option WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwProtected\"=:t",
            soci::use(bits,"option"),soci::use(world,"w"),soci::use(owner,"c"),soci::use(tid,"t");
        HydrateNativeProtected(sql,c,*p);return true;
    }
    if(!reader.ReadString(name)||!reader.Eof())throw std::runtime_error("Invalid protected name request");
    const bool add=message==MessageId::CS_PROTECTEDADD_REQ;
    if(!add&&message!=MessageId::CS_PROTECTEDERASE_REQ)throw std::runtime_error("Unsupported protected request");
    const auto reply=[&](std::uint8_t result,std::uint32_t id=0,std::uint8_t bits=0) {
        std::vector<std::byte> ack;wire::WritePOD(ack,result);if(add)wire::WritePOD(ack,id);
        wire::WriteString(ack,name);if(add)wire::WritePOD(ack,bits);
        if(ack.size()>65527)throw std::runtime_error("Protected reply exceeds original frame");
        packets.emplace_back(static_cast<std::uint16_t>(add?MessageId::CS_PROTECTEDADD_ACK:MessageId::CS_PROTECTEDERASE_ACK),std::move(ack));
    };
    if(name==s.szNAME||name.empty()||name.size()>50||(add&&p->protected_characters.size()>200)) {reply(3);return false;}
    if(name.find('\0')!=std::string::npos)throw std::runtime_error("NUL in protected name");
    if(add) {
        const auto decoded=native_wire_text::Decode(sql,name,m_config.client_text_encoding);int tid=0;
        // Source TProtectedInsert deliberately includes soft-deleted targets.
        sql << "SELECT \"dwCharID\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w "
            "AND lower(rtrim(\"szNAME\") COLLATE \"C\")=lower(rtrim(CAST(:name AS text)) COLLATE \"C\")",
            soci::use(world,"w"),soci::use(decoded,"name"),soci::into(tid);
        if(!sql.got_data()){reply(1);return false;}target=static_cast<std::uint32_t>(tid);
        const auto exists=std::any_of(p->protected_characters.begin(),p->protected_characters.end(),[&](const auto& row){return row.id==target;});
        if(exists){reply(2,target,1);return false;}
        const auto hex=native_wire_text::Hex(name);
        sql << "INSERT INTO app_world.\"TPROTECTEDTABLE\"(\"bWorldID\",\"dwCharID\",\"dwProtected\",\"szNAME\",name_bytes,\"bOption\") "
            "VALUES(:w,:c,:t,:name,decode(:raw,'hex'),1)",soci::use(world,"w"),soci::use(owner,"c"),soci::use(tid,"t"),
            soci::use(decoded,"name"),soci::use(hex,"raw");
    }else {
        const auto found=std::find_if(p->protected_characters.begin(),p->protected_characters.end(),[&](const auto& row){return row.name==name;});
        if(found==p->protected_characters.end()){reply(1);return false;}target=found->id;
        const int tid=std::bit_cast<std::int32_t>(target);
        sql << "DELETE FROM app_world.\"TPROTECTEDTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwProtected\"=:t",
            soci::use(world,"w"),soci::use(owner,"c"),soci::use(tid,"t");
    }
    HydrateNativeProtected(sql,c,*p);
    // Source add: World presence first, client ACK second. Erase reverses it.
    std::vector<std::byte> presence;wire::WritePOD(presence,c.char_id);wire::WritePOD(presence,c.key);
    wire::WritePOD(presence,static_cast<std::uint8_t>(add?1:0));wire::WriteString(presence,name);
    if(!add)reply(0);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::MW_PROTECTEDCHECK_ACK),std::move(presence),QuestAudience::World);
    if(add)reply(0,target,1);
    return true;
}
}
