// Byte-layout suite for client_senders — the CS_ acks the map relays
// down to the client as a result of inbound World→Map cluster traffic.
// AsioSession::SendPacket needs a live socket, so the encoders are what
// the tests pin (same approach as test_world_senders for the MW_ side).

#include "services/client_senders.h"
#include "domain/character.h"
#include "wire_codec.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

} // namespace

int main()
{
    using namespace tmapsvr;

    // --- CS_ADDCONNECT_ACK: BYTE count + count×(u32 ip, u16 port, u8 id) ---
    {
        std::vector<ConnectRoute> routes = {
            { 0x0100007F, 5815, 0x05 },   // 127.0.0.1
            { 0x0A00000A, 7016, 0x42 },   // 10.0.0.10
        };
        auto b = EncodeAddConnectAck(routes);

        // 1 (count) + 2 × (4 + 2 + 1) = 1 + 14 = 15 bytes
        EXPECT(b.size() == 15);

        wire::Reader r(b.data(), b.size());
        std::uint8_t count = 0;
        EXPECT(r.Read(count));
        EXPECT(count == 2);

        std::uint32_t ip = 0; std::uint16_t port = 0; std::uint8_t sid = 0;
        EXPECT(r.Read(ip) && r.Read(port) && r.Read(sid));
        EXPECT(ip == 0x0100007F);
        EXPECT(port == 5815);
        EXPECT(sid == 0x05);

        EXPECT(r.Read(ip) && r.Read(port) && r.Read(sid));
        EXPECT(ip == 0x0A00000A);
        EXPECT(port == 7016);
        EXPECT(sid == 0x42);

        EXPECT(r.Eof());
    }

    // --- empty list → single zero count byte ---------------------------
    {
        auto b = EncodeAddConnectAck({});
        EXPECT(b.size() == 1);
        EXPECT(std::to_integer<int>(b[0]) == 0);
    }

    // --- CS_CONNECT_ACK: BYTE result + BYTE count + count×BYTE id -------
    {
        auto b = EncodeConnectAck(/*result=*/0, { 0x05, 0x42, 0x07 });
        EXPECT(b.size() == 5);   // 1 + 1 + 3
        wire::Reader r(b.data(), b.size());
        std::uint8_t result = 0xFF, count = 0, id0 = 0, id1 = 0, id2 = 0;
        EXPECT(r.Read(result) && r.Read(count));
        EXPECT(result == 0);
        EXPECT(count == 3);
        EXPECT(r.Read(id0) && r.Read(id1) && r.Read(id2));
        EXPECT(id0 == 0x05 && id1 == 0x42 && id2 == 0x07);
        EXPECT(r.Eof());
    }

    // --- rejection result, empty server list ---------------------------
    {
        auto b = EncodeConnectAck(/*result=*/3, {});
        EXPECT(b.size() == 2);
        EXPECT(std::to_integer<int>(b[0]) == 3);   // result
        EXPECT(std::to_integer<int>(b[1]) == 0);   // count
    }

    // --- CS_CHARINFO_ACK: full structure, scalars in slots, empty lists --
    {
        CharSnapshot s;
        s.dwCharID    = 0xABCDEF01;
        s.szNAME      = "Hero";
        s.bStartAct   = 1;
        s.bClass      = 3;
        s.bRace       = 2;
        s.bCountry    = 1;
        s.bOriCountry = 4;
        s.bLevel      = 77;
        s.dwGold      = 123456;
        s.dwSilver    = 22;
        s.dwCooper    = 7;
        s.dwEXP       = 999;
        s.dwHP        = 500;
        s.dwMP        = 250;
        s.dwRegion    = 0x0A;
        s.wMapID      = 60;
        s.fPosX       = 100.5f;
        s.fPosY       = 7.0f;
        s.fPosZ       = -50.25f;
        s.wDIR        = 180;
        s.wSkillPoint = 9;

        const std::string clock = "AM 09 : 05";
        auto b = EncodeCharInfoAck(s, clock);

        wire::Reader r(b.data(), b.size());
        auto u8  = [&](std::uint8_t  e) { std::uint8_t  v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto u16 = [&](std::uint16_t e) { std::uint16_t v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto u32 = [&](std::uint32_t e) { std::uint32_t v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto f32 = [&](float         e) { float         v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto str = [&](const std::string& e) { std::string v; EXPECT(r.ReadString(v)); EXPECT(v == e); };

        u32(0xABCDEF01);                 // char id
        u8(0); u8(0); u8(0);             // secure created / unlocked / disabled
        u16(0);                          // title id
        str("Hero");
        u8(1);                           // start act
        u8(3); u8(2);                    // class, race
        u8(1); u8(4);                    // country, aid country (bOriCountry)
        u8(0); u8(0); u8(0); u8(0); u8(0); u8(0); u8(0); u8(0); // sex,hair,face,body,pants,hand,foot,helmet
        u8(77);                          // level
        u16(0);                          // party id
        u32(0); u32(0); u32(0); u8(0); u8(0); // guild id, fame, fame color, duty, peer
        str("");                         // guild name
        u32(0); str("");                 // tactics id, tactics name
        u32(123456); u32(22); u32(7);    // gold, silver, cooper
        u32(0); u32(0);                  // prev/next exp
        u32(999);                        // exp
        u32(500); u32(500);              // max hp == hp
        u32(250); u32(250);              // max mp == mp
        u32(0); u16(0);                  // party chief, commander
        u32(0x0A); u16(60);              // region, map id
        f32(100.5f); f32(7.0f); f32(-50.25f);
        u16(180); u16(9);                // dir, skill point
        u8(0); u32(0);                   // lucky, aid left time
        u16(0); u16(0); u16(0); u16(0);  // skill-kind points
        u32(0);                          // rank point
        u8(0);                           // bow-death flag
        u8(0); u8(0); u8(0); u8(0); u8(0); // five list counts — all empty
        u32(0); u32(0); u32(0);          // pvp total / useable / month
        str(clock);                      // server clock
        u32(0);                          // medals

        EXPECT(r.Eof());                 // whole structure consumed
    }

    // --- CS_ENTER_ACK: appearance + live pos + empty lists + new flag ---
    {
        CharSnapshot s;
        s.dwCharID    = 0x00C0FFEE;
        s.szNAME      = "Mage";
        s.bClass      = 5;
        s.bRace       = 1;
        s.bCountry    = 2;
        s.bOriCountry = 3;
        s.bSex        = 1;
        s.bHair       = 7;
        s.bFace       = 4;
        s.bLevel      = 42;
        s.bHelmetHide = 1;
        s.dwHP        = 300;
        s.dwMP        = 800;
        s.wDIR        = 90;
        s.dwRegion    = 0x11;
        s.bAftermath  = 2;

        const Position pos{ 12.5f, 3.0f, -4.25f };
        auto b = EncodeEnterAck(s, pos, /*color=*/2, /*new_member=*/1);

        wire::Reader r(b.data(), b.size());
        auto u8  = [&](std::uint8_t  e) { std::uint8_t  v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto u16 = [&](std::uint16_t e) { std::uint16_t v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto u32 = [&](std::uint32_t e) { std::uint32_t v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto f32 = [&](float         e) { float         v = 0; EXPECT(r.Read(v)); EXPECT(v == e); };
        auto str = [&](const std::string& e) { std::string v; EXPECT(r.ReadString(v)); EXPECT(v == e); };

        u32(0x00C0FFEE);                 // char id
        str("Mage");
        u16(0);                          // title
        str("");                         // comment
        u32(0); u32(0); u32(0);          // guild id, fame, fame color
        str("");                         // guild name
        u8(0);                           // guild peer
        u32(0); str("");                 // tactics id, name
        u8(0); str("");                  // store, store name
        u32(0);                          // riding
        u8(5); u8(1);                    // class, race
        u8(2); u8(3);                    // country, aid (bOriCountry)
        u8(1); u8(7); u8(4);             // sex, hair, face
        u8(0); u8(0); u8(0); u8(0);      // body, pants, hand, foot
        u8(42); u8(1);                   // level, helmet hide
        u32(300); u32(300);              // max hp == hp
        u32(800); u32(800);              // max mp == mp
        u32(0); u16(0); u16(0);          // party chief, party id, commander
        f32(12.5f); f32(3.0f); f32(-4.25f);
        u8(0); u8(0); u8(0);             // action, block, mode
        u16(0); u16(90);                 // pitch, dir
        u8(0); u8(0);                    // mouse dir, key dir
        u8(2);                           // color (param)
        u32(0x11);                       // region
        u8(0); u8(2);                    // pc-bang, aftermath step
        u32(0); u16(0); u8(0); u16(0);   // rank, castle, camp, god ball
        u8(0); u8(0);                    // maintain-skill + equip lists empty
        u8(1);                           // new_member

        EXPECT(r.Eof());
    }

    if (g_fails == 0)
        std::printf("test_client_senders: addconnect + connect + charinfo + "
                    "enter layout OK\n");
    return g_fails == 0 ? 0 : 1;
}
