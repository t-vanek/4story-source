#pragma once
#include "MessageId.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace world_test {
// Consume exactly the fresh-admission request triggered by ADDCHAR. Do not
// filter arbitrary frames: an unexpected message or different identity fails.
template<class Reader, class Socket>
bool ReadFreshEnter(Reader read, Socket& socket, const std::vector<std::byte>& registration)
{
    const auto [id, body] = read(socket);
    return id == tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_ENTERSVR_REQ) &&
        registration.size() >= 8 && body.size() == 9 && body[0] == std::byte{1} &&
        std::memcmp(body.data() + 1, registration.data(), 8) == 0;
}
// Plan a secondary endpoint through the actual main -> World route exchange.
template<class Sender, class Reader, class Socket>
bool PlanSecondary(Sender send, Reader read, Socket& main,
                   const std::vector<std::byte>& registration, std::uint8_t server)
{
    if (registration.size() != 18) return false;
    std::vector<std::byte> route(registration.begin(), registration.begin() + 8);
    route.push_back(std::byte{1});
    route.insert(route.end(), registration.begin() + 8, registration.begin() + 14);
    route.push_back(static_cast<std::byte>(server));
    send(main, tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_ROUTE_ACK), route);
    const auto [id, body] = read(main);
    return id == tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_ADDCONNECT_REQ) && body == route;
}

template<class Reader, class Socket>
bool ReadSecondaryDataRequest(Reader read, Socket& main, const std::vector<std::byte>& registration)
{
    const auto [id, body] = read(main);
    return id == tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_CHARDATA_REQ) &&
        registration.size() >= 8 && body.size() == 8 &&
        std::memcmp(body.data(), registration.data(), 8) == 0;
}
// Arm the source CHECKMAIN round over the actual World dispatch path.
// Older subsystem fixtures explicitly seed post-ENTERCHAR readiness before this
// helper; test_main_handoff exercises the complete wire hydration sequence.
template<class Sender,class Reader,class Socket>
bool RequestMainCheck(Sender send,Reader read,Socket& main,Socket& second,
                      const std::vector<std::byte>& registration,std::uint8_t secondary)
{
    if(registration.size()<8)return false;
    std::vector<std::byte> body(registration.begin(),registration.begin()+8);
    body.push_back(std::byte{1});body.push_back(static_cast<std::byte>(secondary));
    send(main,tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_CONLIST_ACK),body);
    for(auto* socket:{&main,&second}) {
        auto [id,got]=read(*socket);
        if(id!=tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_CHECKMAIN_REQ)||got.size()!=23||
           std::memcmp(got.data(),registration.data(),8)!=0)return false;
    }
    return true;
}
template<class Sender,class Reader,class Socket>
bool StartMainHandoff(Sender send,Reader read,Socket& main,Socket& second,
                      const std::vector<std::byte>& registration,std::uint8_t secondary)
{
    if(!RequestMainCheck(send,read,main,second,registration,secondary))return false;
    std::vector<std::byte> body(registration.begin(),registration.begin()+8);
    send(second,tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_CHECKMAIN_ACK),body);
    auto [id,got]=read(main);
    return id==tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_RELEASEMAIN_REQ)&&got.size()==23&&
        std::memcmp(got.data(),registration.data(),8)==0;
}
} // namespace world_test
