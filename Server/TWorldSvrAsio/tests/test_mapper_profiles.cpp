// Unit test for the TWorldSvrAsio Automapper profiles
// (GuildMappingProfile + FriendMappingProfile). Pure logic — no asio,
// no DB. Verifies the field wiring the SOCI repositories rely on, which
// is the part most prone to silent copy-paste errors.
//
// Scenarios:
//   1. GuildRow → TGuild: every persisted scalar lands (AdaptTo, since
//      TGuild holds a std::mutex and can't be value-constructed).
//   2. GuildMemberRow → TGuildMember: all five columns.
//   3. FriendForwardRow → FriendRow: full display fields.
//   4. FriendReverseRow → FriendRow: id + name only, rest default-zero.

#include "../services/mapper_profiles.h"

#include "fourstory/mapper/mapper.h"

#include <cstdio>

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
    using namespace tworldsvr;
    using fourstory::mapper::Adapt;
    using fourstory::mapper::AdaptTo;

    // Configure the global TypeMaps once (mirrors MapperRegistry::ApplyAll
    // in main(); here we drive the profiles directly so the test owns no
    // singleton ordering assumptions).
    GuildMappingProfile{}.Configure();
    FriendMappingProfile{}.Configure();

    // --- 1: GuildRow → TGuild ---------------------------------------
    {
        GuildRow r;
        r.id                = 42;
        r.name              = "Knights";
        r.chief             = 7;
        r.level             = 5;
        r.fame              = 100;
        r.fame_color        = 3;
        r.max_cabinet       = 4;
        r.gold              = 10;
        r.silver            = 20;
        r.cooper            = 30;
        r.gi                = 40;
        r.exp               = 500;
        r.guild_points      = 2;
        r.status            = 1;
        r.disorg            = 0;
        r.disorg_time       = 0;
        r.establish_time    = 123456;
        r.pvp_total_point   = 999;
        r.pvp_useable_point = 111;

        TGuild g;
        AdaptTo(r, g);

        EXPECT(g.id == 42);
        EXPECT(g.name == "Knights");
        EXPECT(g.chief_char_id == 7);
        EXPECT(g.level == 5);
        EXPECT(g.fame == 100);
        EXPECT(g.fame_color == 3);
        EXPECT(g.max_cabinet == 4);
        EXPECT(g.gold == 10);
        EXPECT(g.silver == 20);
        EXPECT(g.cooper == 30);
        EXPECT(g.gi == 40);
        EXPECT(g.exp == 500);
        EXPECT(g.guild_points == 2);
        EXPECT(g.status == 1);
        EXPECT(g.disorg == 0);
        EXPECT(g.disorg_time == 0);
        EXPECT(g.establish_time == 123456);
        EXPECT(g.pvp_total_point == 999);
        EXPECT(g.pvp_useable_point == 111);
    }

    // --- 2: GuildMemberRow → TGuildMember ---------------------------
    {
        GuildMemberRow r;
        r.char_id  = 1001;
        r.guild_id = 42;
        r.duty     = 3;
        r.peer     = 2;
        r.service  = 777;

        TGuildMember m = Adapt<TGuildMember>(r);
        EXPECT(m.char_id == 1001);
        EXPECT(m.guild_id == 42);
        EXPECT(m.duty == 3);
        EXPECT(m.peer == 2);
        EXPECT(m.service == 777);
    }

    // --- 3: FriendForwardRow → FriendRow ----------------------------
    {
        FriendForwardRow r;
        r.friend_id = 55;
        r.name      = "Alice";
        r.group     = 2;
        r.klass     = 4;
        r.level     = 30;

        FriendRow f = Adapt<FriendRow>(r);
        EXPECT(f.id == 55);
        EXPECT(f.name == "Alice");
        EXPECT(f.group == 2);
        EXPECT(f.klass == 4);
        EXPECT(f.level == 30);
    }

    // --- 4: FriendReverseRow → FriendRow ----------------------------
    {
        FriendReverseRow r;
        r.char_id = 66;
        r.name    = "Bob";

        FriendRow f = Adapt<FriendRow>(r);
        EXPECT(f.id == 66);
        EXPECT(f.name == "Bob");
        EXPECT(f.group == 0);
        EXPECT(f.klass == 0);
        EXPECT(f.level == 0);
    }

    if (g_fails == 0)
        std::printf("test_mapper_profiles: all scenarios passed\n");
    return g_fails == 0 ? 0 : 1;
}
