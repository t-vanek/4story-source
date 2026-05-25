#include "services/world_senders.h"

#include "wire_codec.h"

namespace tmapsvr {

std::vector<std::byte> EncodeEnterSvrAck(const CharSnapshot& s,
                                         std::uint32_t key,
                                         std::uint8_t  aid_country,
                                         std::uint8_t  channel,
                                         std::uint8_t  logout,
                                         std::uint8_t  save,
                                         std::uint8_t  result,
                                         std::uint16_t title_id,
                                         std::uint32_t rank_point,
                                         std::uint32_t user_ip)
{
    std::vector<std::byte> b;
    // Field order matches CTMapSvrModule::SendMW_ENTERSVR_ACK
    // (SSSender.cpp:782) and TWorld's OnEnterSvrAck reader exactly.
    wire::WritePOD<std::uint32_t>(b, s.dwCharID);
    wire::WritePOD<std::uint32_t>(b, key);
    wire::WriteString(b, s.szNAME);
    wire::WritePOD<std::uint8_t>(b, s.bLevel);
    wire::WritePOD<std::uint8_t>(b, s.bRealSex);
    wire::WritePOD<std::uint8_t>(b, s.bClass);
    wire::WritePOD<std::uint8_t>(b, s.bRace);
    wire::WritePOD<std::uint8_t>(b, s.bSex);
    wire::WritePOD<std::uint8_t>(b, s.bFace);
    wire::WritePOD<std::uint8_t>(b, s.bHair);
    wire::WritePOD<std::uint8_t>(b, s.bHelmetHide);
    wire::WritePOD<std::uint8_t>(b, s.bCountry);
    wire::WritePOD<std::uint8_t>(b, aid_country);
    wire::WritePOD<std::uint32_t>(b, s.dwRegion);
    wire::WritePOD<std::uint8_t>(b, channel);
    wire::WritePOD<std::uint16_t>(b, s.wMapID);
    wire::WritePOD<float>(b, s.fPosX);
    wire::WritePOD<float>(b, s.fPosY);
    wire::WritePOD<float>(b, s.fPosZ);
    wire::WritePOD<std::uint8_t>(b, logout);
    wire::WritePOD<std::uint8_t>(b, save);
    wire::WritePOD<std::uint8_t>(b, result);
    wire::WritePOD<std::uint16_t>(b, title_id);
    wire::WritePOD<std::uint32_t>(b, rank_point);
    wire::WritePOD<std::uint32_t>(b, user_ip);
    return b;
}

} // namespace tmapsvr
