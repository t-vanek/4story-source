// Byte-layout test for the world_senders encoders (MW_/RW_ bodies the
// map sends to TWorld). Pure encoding — no socket, no DB. Pins the wire
// shapes so a field reorder / width change is caught here rather than as
// a runtime checksum/parse failure on the live link.

#include "services/world_senders.h"
#include "domain/character.h"

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

std::uint8_t  U8 (const std::vector<std::byte>& b, std::size_t o)
{ return std::to_integer<std::uint8_t>(b[o]); }
std::uint16_t U16(const std::vector<std::byte>& b, std::size_t o)
{ return static_cast<std::uint16_t>(U8(b,o) | (U8(b,o+1) << 8)); }
std::uint32_t U32(const std::vector<std::byte>& b, std::size_t o)
{ return U8(b,o) | (U8(b,o+1)<<8) | (U8(b,o+2)<<16) | (std::uint32_t(U8(b,o+3))<<24); }
std::string   Str(const std::vector<std::byte>& b, std::size_t o, std::size_t n)
{ std::string s; for (std::size_t i=0;i<n;++i) s.push_back(char(U8(b,o+i))); return s; }
} // namespace

int main()
{
    using namespace tmapsvr;

    // --- MW_ADDCHAR_ACK: 18 bytes, fixed scalars -------------------
    {
        auto b = EncodeAddCharAck(0x11223344, 0x55667788,
                                  0x0100007F, 0xABCD, 0x99);
        EXPECT(b.size() == 18);
        EXPECT(U32(b, 0)  == 0x11223344);   // char_id
        EXPECT(U32(b, 4)  == 0x55667788);   // key
        EXPECT(U32(b, 8)  == 0x0100007F);   // ip
        EXPECT(U16(b, 12) == 0xABCD);       // port
        EXPECT(U32(b, 14) == 0x99);         // user_id
    }

    // --- RW_ENTERCHAR_REQ: char_id + length-prefixed name ----------
    {
        const std::string name = "Hero";
        auto b = EncodeEnterCharReq(0xDEADBEEF, name);
        EXPECT(b.size() == 4 + 4 + name.size());
        EXPECT(U32(b, 0) == 0xDEADBEEF);            // char_id
        EXPECT(U32(b, 4) == name.size());           // szName length prefix
        EXPECT(Str(b, 8, name.size()) == name);     // szName bytes
    }

    // --- MW_ENTERSVR_ACK: char_id, key, name, then level ----------
    {
        CharSnapshot s;
        s.dwCharID = 0xCAFEBABE;
        s.szNAME   = "Mage";
        s.bLevel   = 42;
        s.wMapID   = 0x0708;
        auto b = EncodeEnterSvrAck(s, /*key=*/0x01020304, /*aid_country=*/0,
                                   /*channel=*/0, /*logout=*/0, /*save=*/0,
                                   /*result=*/0, /*title_id=*/0,
                                   /*rank_point=*/0, /*user_ip=*/0);
        // 8 (char_id+key) + 4+len (name) + 42 fixed tail = 54 + len.
        EXPECT(b.size() == 54 + s.szNAME.size());
        EXPECT(U32(b, 0) == 0xCAFEBABE);                 // char_id
        EXPECT(U32(b, 4) == 0x01020304);                 // key
        EXPECT(U32(b, 8) == s.szNAME.size());            // name length
        EXPECT(Str(b, 12, s.szNAME.size()) == s.szNAME); // name bytes
        EXPECT(U8(b, 12 + s.szNAME.size()) == 42);       // bLevel right after
    }

    if (g_fails == 0)
        std::printf("test_world_senders: all byte layouts OK\n");
    return g_fails == 0 ? 0 : 1;
}
