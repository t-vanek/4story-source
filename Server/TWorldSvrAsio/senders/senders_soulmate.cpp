#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {
boost::asio::awaitable<void> SendMwSoulmateReq(std::shared_ptr<PeerSession> peer,std::uint32_t character,
    std::uint32_t key,std::uint32_t target,const std::string& name,std::uint32_t time) {
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body,character);wire::WritePOD<std::uint32_t>(body,key);
    wire::WritePOD<std::uint32_t>(body,target);wire::WriteString(body,name);wire::WritePOD<std::uint32_t>(body,time);
    co_await peer->Wire()->SendPacket(tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATE_REQ),std::move(body));
}


std::vector<std::byte>
EncodeMwSoulmateSearchReq(std::uint32_t char_id, std::uint32_t key,
                        std::uint8_t result, std::uint32_t soul_id,
                        const std::string& soul_name, std::uint32_t region,
                        std::uint8_t npc_inven, std::uint8_t npc_item)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, soul_id);
    WriteString(body, soul_name);
    WritePOD<std::uint32_t>(body, region);
    WritePOD<std::uint8_t>(body, npc_inven);
    WritePOD<std::uint8_t>(body, npc_item);
    return body;
}

boost::asio::awaitable<void>
SendMwSoulmateSearchReq(std::shared_ptr<PeerSession> peer,std::uint32_t char_id,std::uint32_t key,
    std::uint8_t result,std::uint32_t soul_id,const std::string& soul_name,std::uint32_t region,
    std::uint8_t npc_inven,std::uint8_t npc_item)
{
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_SOULMATESEARCH_REQ),
        EncodeMwSoulmateSearchReq(char_id,key,result,soul_id,soul_name,region,npc_inven,npc_item));
}

std::vector<std::byte>
EncodeMwSoulmateRegReq(std::uint32_t char_id, std::uint32_t key,
                     std::uint8_t result, std::uint8_t reg,
                     std::uint8_t npc_inven, std::uint8_t npc_item,
                     std::uint32_t soulmate, const std::string& soul_name,
                     std::uint32_t region)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint8_t>(body, reg);
    WritePOD<std::uint8_t>(body, npc_inven);
    WritePOD<std::uint8_t>(body, npc_item);
    WritePOD<std::uint32_t>(body, soulmate);
    WriteString(body, soul_name);
    WritePOD<std::uint32_t>(body, region);
    return body;
}

boost::asio::awaitable<void>
SendMwSoulmateRegReq(std::shared_ptr<PeerSession> peer,std::uint32_t char_id,std::uint32_t key,
    std::uint8_t result,std::uint8_t reg,std::uint8_t npc_inven,std::uint8_t npc_item,
    std::uint32_t soulmate,const std::string& soul_name,std::uint32_t region)
{
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_SOULMATEREG_REQ),
        EncodeMwSoulmateRegReq(char_id,key,result,reg,npc_inven,npc_item,soulmate,soul_name,region));
}

boost::asio::awaitable<void>
SendMwSoulmateEndReq(std::shared_ptr<PeerSession> peer,
                     std::uint32_t char_id, std::uint32_t key,
                     std::uint8_t result, std::uint32_t time_unix)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, char_id);
    WritePOD<std::uint32_t>(body, key);
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, time_unix);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_SOULMATEEND_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
