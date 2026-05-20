// Spec test for LocalPartyService.
//
// §1  CreateParty + GetParty + GetCharParty
// §2  AddMember + party size grows
// §3  RemoveMember — single member dissolves party
// §4  RemoveMember — leader leaves, party persists if others remain
// §5  StorePendingInvite + TakePendingInvite (consume once)

#include "party_service.h"
#include <cstdio>
#include <exception>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

tmapsvr::PartyMemberInfo MakeMember(std::uint32_t id, const char* name,
                                    std::uint8_t level = 10)
{
    tmapsvr::PartyMemberInfo m{};
    m.char_id = id; m.name = name; m.level = level;
    return m;
}

void TestCreateAndGet()
{
    std::printf("[§1 CreateParty + GetParty + GetCharParty]\n");
    tmapsvr::LocalPartyService svc;

    auto pid = svc.CreateParty(MakeMember(100, "Leader"), 0);
    Check(pid > 0, "party_id > 0");

    const auto* party = svc.GetParty(pid);
    Check(party != nullptr, "GetParty returns non-null");
    if (party)
    {
        Check(party->chief_id == 100u, "chief_id = leader");
        Check(party->members.size() == 1, "one member initially");
    }

    auto char_party = svc.GetCharParty(100u);
    Check(char_party.has_value() && *char_party == pid,
        "GetCharParty returns party_id for leader");

    Check(!svc.GetCharParty(999u).has_value(),
        "GetCharParty returns nullopt for non-member");
}

void TestAddMember()
{
    std::printf("[§2 AddMember]\n");
    tmapsvr::LocalPartyService svc;

    auto pid = svc.CreateParty(MakeMember(1, "A"), 0);
    Check(svc.AddMember(pid, MakeMember(2, "B")), "AddMember returns true");
    Check(svc.AddMember(pid, MakeMember(3, "C")), "AddMember returns true (2)");

    const auto* party = svc.GetParty(pid);
    Check(party && party->members.size() == 3, "party has 3 members");

    Check(svc.GetCharParty(2u).has_value(), "B is in party");
    Check(svc.GetCharParty(3u).has_value(), "C is in party");

    // Add to unknown party
    Check(!svc.AddMember(999u, MakeMember(4, "D")),
        "AddMember to unknown party returns false");
}

void TestRemoveMember()
{
    std::printf("[§3 RemoveMember dissolves empty party]\n");
    tmapsvr::LocalPartyService svc;

    auto pid = svc.CreateParty(MakeMember(10, "Solo"), 0);
    svc.RemoveMember(pid, 10u);
    Check(svc.GetParty(pid) == nullptr, "party dissolved when last member leaves");
    Check(!svc.GetCharParty(10u).has_value(), "char no longer in party");
}

void TestRemoveMemberPersists()
{
    std::printf("[§4 RemoveMember — party persists if others remain]\n");
    tmapsvr::LocalPartyService svc;

    auto pid = svc.CreateParty(MakeMember(1, "A"), 0);
    svc.AddMember(pid, MakeMember(2, "B"));
    svc.AddMember(pid, MakeMember(3, "C"));

    svc.RemoveMember(pid, 2u);
    Check(svc.GetParty(pid) != nullptr, "party persists after B leaves");
    Check(!svc.GetCharParty(2u).has_value(), "B removed from char→party map");
    const auto* p = svc.GetParty(pid);
    Check(p && p->members.size() == 2, "2 members remain");
}

void TestPendingInvite()
{
    std::printf("[§5 StorePendingInvite + TakePendingInvite]\n");
    tmapsvr::LocalPartyService svc;

    tmapsvr::PartyInvite invite{};
    invite.inviter_id  = 42u;
    invite.obtain_type = 1;

    svc.StorePendingInvite("TargetPlayer", invite);

    auto taken = svc.TakePendingInvite("TargetPlayer");
    Check(taken.has_value(), "TakePendingInvite returns invite");
    if (taken)
    {
        Check(taken->inviter_id == 42u, "inviter_id correct");
        Check(taken->obtain_type == 1, "obtain_type correct");
    }

    // Second take → consumed
    auto second = svc.TakePendingInvite("TargetPlayer");
    Check(!second.has_value(), "invite consumed after take");

    // Unknown target
    auto miss = svc.TakePendingInvite("NoOne");
    Check(!miss.has_value(), "unknown target returns nullopt");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalPartyService spec ===\n\n");
    try
    {
        TestCreateAndGet();
        TestAddMember();
        TestRemoveMember();
        TestRemoveMemberPersists();
        TestPendingInvite();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
