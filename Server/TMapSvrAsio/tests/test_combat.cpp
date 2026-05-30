// Unit test: the combat mechanics that back OnActionReq — the registry
// ApplyDamage (HP subtract, clamp-to-zero death, miss) and the byte
// layouts of the HP / DelMon / Exp acks. The handler's broadcast + EXP
// award are client-send glue (live socket), so the testable surface is
// the damage state machine + the wire encoders.

#include "services/monster_registry.h"
#include "services/client_senders.h"
#include "domain/monster.h"
#include "wire_codec.h"

#include <cstdint>
#include <cstdio>
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

    // --- registry ApplyDamage: subtract, clamp-to-zero, miss ----------
    {
        InMemoryMonsterRegistry reg;
        MonsterInstance m;
        m.dwInstanceID = 5000;
        m.wTemplateID  = 42;
        m.bChannel     = 0;
        m.wMapID       = 60;
        m.dwMaxHP      = 100;
        m.dwHP         = 100;
        reg.Insert(m);

        // A normal hit subtracts and reports the survivor.
        const auto a1 = reg.ApplyDamage(5000, 30);
        EXPECT(a1.has_value());
        if (a1)
        {
            EXPECT(a1->dwHP == 70);
            EXPECT(a1->dwMaxHP == 100);
        }
        // Registry holds the mutated HP (not a copy-only).
        EXPECT(reg.Find(5000) && reg.Find(5000)->dwHP == 70);

        // Overkill clamps to 0 (== just died); the row still exists until
        // the caller Removes it.
        const auto a2 = reg.ApplyDamage(5000, 9999);
        EXPECT(a2.has_value());
        if (a2) EXPECT(a2->dwHP == 0);
        EXPECT(reg.Find(5000).has_value());

        // Unknown instance → nullopt.
        EXPECT(!reg.ApplyDamage(7777, 10).has_value());
    }

    // --- CS_HPMP_ACK: id + maxHP + HP + maxMP + MP --------------------
    {
        auto b = EncodeHpMpAck(0xABCD, 1500, 1200, 0, 0);
        EXPECT(b.size() == 20);
        wire::Reader r(b.data(), b.size());
        std::uint32_t id = 0, mh = 0, h = 0, mm = 0, mp = 0;
        EXPECT(r.Read(id) && r.Read(mh) && r.Read(h) && r.Read(mm) && r.Read(mp));
        EXPECT(id == 0xABCD && mh == 1500 && h == 1200 && mm == 0 && mp == 0);
        EXPECT(r.Eof());
    }

    // --- CS_DELMON_ACK: mon id + exit_map ----------------------------
    {
        auto b = EncodeDelMonAck(0xDEAD, /*exit_map=*/0);
        EXPECT(b.size() == 5);
        wire::Reader r(b.data(), b.size());
        std::uint32_t id = 0; std::uint8_t ex = 0xFF;
        EXPECT(r.Read(id) && r.Read(ex));
        EXPECT(id == 0xDEAD && ex == 0);
        EXPECT(r.Eof());
    }

    // --- CS_EXP_ACK: exp + prev + next + soul ------------------------
    {
        auto b = EncodeExpAck(123456, 0, 0, 0);
        EXPECT(b.size() == 16);
        wire::Reader r(b.data(), b.size());
        std::uint32_t e = 0, p = 0xFF, n = 0xFF, s = 0xFF;
        EXPECT(r.Read(e) && r.Read(p) && r.Read(n) && r.Read(s));
        EXPECT(e == 123456 && p == 0 && n == 0 && s == 0);
        EXPECT(r.Eof());
    }

    if (g_fails == 0)
        std::printf("test_combat: ApplyDamage + hpmp/delmon/exp layout OK\n");
    return g_fails == 0 ? 0 : 1;
}
