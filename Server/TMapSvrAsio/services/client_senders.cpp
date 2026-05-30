#include "services/client_senders.h"

#include "wire_codec.h"

namespace tmapsvr {

std::vector<std::byte> EncodeAddConnectAck(
    const std::vector<ConnectRoute>& routes)
{
    std::vector<std::byte> b;
    b.reserve(1 + routes.size() * 7);
    wire::WritePOD<std::uint8_t>(b, static_cast<std::uint8_t>(routes.size()));
    for (const auto& r : routes)
    {
        wire::WritePOD<std::uint32_t>(b, r.ip_addr);
        wire::WritePOD<std::uint16_t>(b, r.port);
        wire::WritePOD<std::uint8_t> (b, r.server_id);
    }
    return b;
}

std::vector<std::byte> EncodeConnectAck(
    std::uint8_t result, const std::vector<std::uint8_t>& server_ids)
{
    std::vector<std::byte> b;
    b.reserve(2 + server_ids.size());
    wire::WritePOD<std::uint8_t>(b, result);
    wire::WritePOD<std::uint8_t>(b, static_cast<std::uint8_t>(server_ids.size()));
    for (const auto id : server_ids)
        wire::WritePOD<std::uint8_t>(b, id);
    return b;
}

std::vector<std::byte> EncodeCharInfoAck(
    const CharSnapshot& s, const std::string& time_str)
{
    std::vector<std::byte> b;
    b.reserve(256);

    // --- identity + secure code (secure not modeled → 0) -------------
    wire::WritePOD<std::uint32_t>(b, s.dwCharID);
    wire::WritePOD<std::uint8_t> (b, 0);            // secure created
    wire::WritePOD<std::uint8_t> (b, 0);            // secure cur-unlocked
    wire::WritePOD<std::uint8_t> (b, 0);            // secure disabled
    wire::WritePOD<std::uint16_t>(b, 0);            // title id (World-sourced)
    wire::WriteString            (b, s.szNAME);
    wire::WritePOD<std::uint8_t> (b, s.bStartAct);
    wire::WritePOD<std::uint8_t> (b, s.bClass);
    wire::WritePOD<std::uint8_t> (b, s.bRace);
    wire::WritePOD<std::uint8_t> (b, s.bCountry);
    wire::WritePOD<std::uint8_t> (b, s.bOriCountry);   // aid country
    wire::WritePOD<std::uint8_t> (b, s.bSex);
    wire::WritePOD<std::uint8_t> (b, s.bHair);
    wire::WritePOD<std::uint8_t> (b, s.bFace);
    wire::WritePOD<std::uint8_t> (b, s.bBody);
    wire::WritePOD<std::uint8_t> (b, s.bPants);
    wire::WritePOD<std::uint8_t> (b, s.bHand);
    wire::WritePOD<std::uint8_t> (b, s.bFoot);
    wire::WritePOD<std::uint8_t> (b, s.bHelmetHide);
    wire::WritePOD<std::uint8_t> (b, s.bLevel);

    // --- party + guild + tactics (World-sourced cluster state → 0) ---
    wire::WritePOD<std::uint16_t>(b, 0);            // party id
    wire::WritePOD<std::uint32_t>(b, 0);            // guild id
    wire::WritePOD<std::uint32_t>(b, 0);            // fame
    wire::WritePOD<std::uint32_t>(b, 0);            // fame color
    wire::WritePOD<std::uint8_t> (b, 0);            // guild duty
    wire::WritePOD<std::uint8_t> (b, 0);            // guild peer
    wire::WriteString            (b, std::string{});// guild name
    wire::WritePOD<std::uint32_t>(b, 0);            // tactics id
    wire::WriteString            (b, std::string{});// tactics name

    // --- money + exp + hp/mp -----------------------------------------
    wire::WritePOD<std::uint32_t>(b, s.dwGold);
    wire::WritePOD<std::uint32_t>(b, s.dwSilver);
    wire::WritePOD<std::uint32_t>(b, s.dwCooper);
    wire::WritePOD<std::uint32_t>(b, 0);            // prev-level exp (level chart)
    wire::WritePOD<std::uint32_t>(b, 0);            // next-level exp (level chart)
    wire::WritePOD<std::uint32_t>(b, s.dwEXP);
    wire::WritePOD<std::uint32_t>(b, s.dwHP);       // max HP → current (full bar)
    wire::WritePOD<std::uint32_t>(b, s.dwHP);
    wire::WritePOD<std::uint32_t>(b, s.dwMP);       // max MP → current (full bar)
    wire::WritePOD<std::uint32_t>(b, s.dwMP);
    wire::WritePOD<std::uint32_t>(b, 0);            // party chief id
    wire::WritePOD<std::uint16_t>(b, 0);            // commander id (corps)

    // --- region + position -------------------------------------------
    wire::WritePOD<std::uint32_t>(b, s.dwRegion);
    wire::WritePOD<std::uint16_t>(b, s.wMapID);
    wire::WritePOD<float>        (b, s.fPosX);
    wire::WritePOD<float>        (b, s.fPosY);
    wire::WritePOD<float>        (b, s.fPosZ);
    wire::WritePOD<std::uint16_t>(b, s.wDIR);
    wire::WritePOD<std::uint16_t>(b, s.wSkillPoint);
    wire::WritePOD<std::uint8_t> (b, 0);            // lucky number
    wire::WritePOD<std::uint32_t>(b, 0);            // aid left time

    // --- skill-kind points (4) + rank + bow-death flag ---------------
    wire::WritePOD<std::uint16_t>(b, 0);            // arPoint[0]
    wire::WritePOD<std::uint16_t>(b, 0);            // arPoint[1]
    wire::WritePOD<std::uint16_t>(b, 0);            // arPoint[2]
    wire::WritePOD<std::uint16_t>(b, 0);            // arPoint[3]
    wire::WritePOD<std::uint32_t>(b, 0);            // rank point
    wire::WritePOD<std::uint8_t> (b, 0);            // non-BOW death flag (FALSE)

    // --- five list sections, all empty (count = 0) -------------------
    wire::WritePOD<std::uint8_t>(b, 0);             // inventory
    wire::WritePOD<std::uint8_t>(b, 0);             // skills
    wire::WritePOD<std::uint8_t>(b, 0);             // maintain skills
    wire::WritePOD<std::uint8_t>(b, 0);             // hotkeys
    wire::WritePOD<std::uint8_t>(b, 0);             // item cooldowns

    // --- PvP points + server clock + medals --------------------------
    wire::WritePOD<std::uint32_t>(b, 0);            // pvp total
    wire::WritePOD<std::uint32_t>(b, 0);            // pvp useable
    wire::WritePOD<std::uint32_t>(b, 0);            // month pvp
    wire::WriteString            (b, time_str);
    wire::WritePOD<std::uint32_t>(b, 0);            // medals

    return b;
}

} // namespace tmapsvr
