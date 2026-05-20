// Spec test for LocalMapState (cell-based AOI grid).
//
// Verifies the porting of the legacy 3×3-cell AOI system from
// TMap::GetNeighbor + CTCell::EnterPlayer / LeavePlayer / OnMove.
//
// Cell coordinate formula (NetCode.h: CELL_SIZE = 64):
//   cell_x = int(pos_x) / 64
//   cell_z = int(pos_z) / 64
//   key    = (uint32_t(cell_z) << 16) | uint16_t(cell_x)
//
// Sections:
//   §1  CellKey formula                   (NetCode.h constants)
//   §2  EnterMap → returns existing AOI
//   §3  LeaveMap → returns neighbours to notify
//   §4  OnMove same cell → all common_aoi
//   §5  OnMove cell transition → entered/left/common split
//   §6  GetNeighborIds — 3×3 coverage
//   §7  Contains / PlayerCount
//   §8  Duplicate EnterMap overwrites (last-write wins)

#include "map_state.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

tmapsvr::PlayerPresence MakePresence(std::uint32_t id, float px, float pz,
                                     const char* name = "X")
{
    tmapsvr::PlayerPresence p{};
    p.char_id = id;
    p.pos_x   = px;
    p.pos_z   = pz;
    p.name    = name;
    return p;
}

bool Contains(const std::vector<std::uint32_t>& v, std::uint32_t id)
{
    return std::find(v.begin(), v.end(), id) != v.end();
}

// ---------------------------------------------------------------------------
// §1  CellKey formula
// ---------------------------------------------------------------------------
void TestCellKey()
{
    std::printf("[§1 CellKey formula — matches legacy MAKELONG(wCellX, wCellZ)]\n");
    using tmapsvr::LocalMapState;

    // Position (256, 128) → cell (4, 2) → key = (2<<16)|4 = 0x00020004
    Check(LocalMapState::MakeCellKey(256.0f, 128.0f) == 0x00020004u,
        "cell (4,2) key = 0x00020004");

    // Position (0, 0) → cell (0, 0)
    Check(LocalMapState::MakeCellKey(0.0f, 0.0f) == 0u,
        "origin → key 0");

    // Position (63.9, 63.9) → cell (0, 0) (within first cell)
    Check(LocalMapState::MakeCellKey(63.9f, 63.9f) == 0u,
        "pos (63.9, 63.9) → cell (0,0)");

    // Position (64.0, 64.0) → cell (1, 1)
    Check(LocalMapState::MakeCellKey(64.0f, 64.0f) == 0x00010001u,
        "pos (64,64) → cell (1,1)");

    // Symmetry: x-major in low 16 bits, z-major in high 16 bits
    const auto k = LocalMapState::MakeCellKey(192.0f, 320.0f);
    Check((k & 0xFFFF) == 3u, "cell_x stored in low 16 bits");
    Check((k >> 16)    == 5u, "cell_z stored in high 16 bits");
}

// ---------------------------------------------------------------------------
// §2  EnterMap → returns existing AOI char_ids
// ---------------------------------------------------------------------------
void TestEnterMap()
{
    std::printf("[§2 EnterMap returns existing AOI members]\n");
    tmapsvr::LocalMapState m;

    // First player — empty world
    auto r1 = m.EnterMap(1, MakePresence(1, 100.0f, 100.0f));
    Check(r1.empty(), "EnterMap first player → empty AOI list");
    Check(m.Contains(1), "player 1 registered");

    // Second player in same cell → first player visible
    auto r2 = m.EnterMap(2, MakePresence(2, 110.0f, 110.0f));
    Check(r2.size() == 1 && r2[0] == 1u,
        "player 2 enters same cell → sees player 1");
    Check(m.Contains(2), "player 2 registered");

    // Third player far away (different AOI)
    auto r3 = m.EnterMap(3, MakePresence(3, 1000.0f, 1000.0f));
    Check(r3.empty(), "player 3 far away → empty AOI");
}

// ---------------------------------------------------------------------------
// §3  LeaveMap → returns neighbours to notify
// ---------------------------------------------------------------------------
void TestLeaveMap()
{
    std::printf("[§3 LeaveMap returns neighbours to notify]\n");
    tmapsvr::LocalMapState m;

    m.EnterMap(10, MakePresence(10, 100.0f, 100.0f));
    m.EnterMap(11, MakePresence(11, 110.0f, 110.0f));  // same cell as 10
    m.EnterMap(12, MakePresence(12, 130.0f, 110.0f));  // adjacent cell

    auto notify = m.LeaveMap(10);
    Check(!m.Contains(10), "player 10 removed from registry");
    Check(Contains(notify, 11u), "player 11 in AOI notified about leave");
    Check(Contains(notify, 12u), "player 12 in adjacent cell notified");

    // Leaving an unregistered player is a no-op
    auto empty = m.LeaveMap(99u);
    Check(empty.empty(), "LeaveMap on unregistered char returns empty");
}

// ---------------------------------------------------------------------------
// §4  OnMove same cell → all common_aoi
// ---------------------------------------------------------------------------
void TestMoveSameCell()
{
    std::printf("[§4 OnMove within same cell → all common_aoi]\n");
    tmapsvr::LocalMapState m;

    m.EnterMap(20, MakePresence(20, 100.0f, 100.0f));
    m.EnterMap(21, MakePresence(21, 110.0f, 100.0f));

    // Move within cell (100 and 110 are in cell 1 for pos/64; 100/64=1, 110/64=1)
    auto d = m.OnMove(20, 105.0f, 100.0f);  // still cell (1, 1)
    Check(d.entered_aoi.empty(), "no new neighbours on intra-cell move");
    Check(d.left_aoi.empty(),    "no lost neighbours on intra-cell move");
    Check(Contains(d.common_aoi, 21u), "player 21 in common_aoi");

    // Position updated in presence
    const auto* p = m.GetPresence(20);
    Check(p && p->pos_x == 105.0f, "presence pos_x updated after move");
}

// ---------------------------------------------------------------------------
// §5  OnMove cell transition → entered/left/common split
// ---------------------------------------------------------------------------
void TestMoveCellTransition()
{
    std::printf("[§5 OnMove across cell boundary → entered/left/common split]\n");
    tmapsvr::LocalMapState m;

    // Player A starts at cell (1,1) = positions 64–127
    m.EnterMap(30, MakePresence(30, 80.0f,  80.0f));

    // Player B is in cell (1,1) — overlaps A's old AOI but also A's new AOI
    m.EnterMap(31, MakePresence(31, 90.0f,  80.0f));  // cell (1,1)

    // Player C is in cell (3,1) — far right, in A's new AOI but NOT old
    m.EnterMap(32, MakePresence(32, 220.0f, 80.0f));  // cell (3,1)

    // Player D is in cell (-1 equivalent but clamped) — in A's old AOI, NOT new
    m.EnterMap(33, MakePresence(33, 10.0f,  80.0f));  // cell (0,1)
    // A at cell (1,1); new AOI covers cells (0,0)–(2,2)
    // A moves to cell (4,1) = pos ~260; new AOI covers (3,0)–(5,2)

    auto d = m.OnMove(30, 260.0f, 80.0f);

    Check(Contains(d.entered_aoi, 32u), "player C enters new AOI on move right");
    Check(Contains(d.left_aoi,    33u), "player D leaves AOI on move right");
    // B at cell (1,1) is no longer in A's 3x3 around (4,1) — cells 3-5
    // Actually (1,1) is NOT in (3,4,5)×(0,1,2) range
    Check(!Contains(d.common_aoi, 31u) || !Contains(d.entered_aoi, 31u),
        "player B not double-counted");
}

// ---------------------------------------------------------------------------
// §6  GetNeighborIds — 3×3 coverage
// ---------------------------------------------------------------------------
void TestGetNeighborIds()
{
    std::printf("[§6 GetNeighborIds returns all players in 3×3 AOI]\n");
    tmapsvr::LocalMapState m;

    // Centre player at (128, 128) = cell (2,2)
    m.EnterMap(40, MakePresence(40, 128.0f, 128.0f));
    // Same cell
    m.EnterMap(41, MakePresence(41, 130.0f, 130.0f));  // cell (2,2)
    // Adjacent cell
    m.EnterMap(42, MakePresence(42, 200.0f, 200.0f));  // cell (3,3) — AOI edge
    // Far away
    m.EnterMap(43, MakePresence(43, 500.0f, 500.0f));  // cell (7,7) — out of AOI

    auto ids = m.GetNeighborIds(128.0f, 128.0f);
    Check(Contains(ids, 40u), "self visible in AOI query");
    Check(Contains(ids, 41u), "same-cell player visible");
    Check(Contains(ids, 42u), "adjacent-cell player visible");
    Check(!Contains(ids, 43u), "far player NOT visible (out of 3×3)");
}

// ---------------------------------------------------------------------------
// §7  Contains / PlayerCount
// ---------------------------------------------------------------------------
void TestContainsAndCount()
{
    std::printf("[§7 Contains + PlayerCount]\n");
    tmapsvr::LocalMapState m;

    Check(!m.Contains(1u),  "empty map: Contains(1) = false");
    Check(m.PlayerCount() == 0, "empty map: PlayerCount = 0");

    m.EnterMap(1, MakePresence(1, 50.0f, 50.0f));
    m.EnterMap(2, MakePresence(2, 60.0f, 50.0f));

    Check(m.Contains(1u),   "after enter: Contains(1) = true");
    Check(m.Contains(2u),   "after enter: Contains(2) = true");
    Check(!m.Contains(3u),  "unregistered id: Contains(3) = false");
    Check(m.PlayerCount() == 2, "PlayerCount = 2 after two enters");

    m.LeaveMap(1);
    Check(!m.Contains(1u), "after leave: Contains(1) = false");
    Check(m.PlayerCount() == 1, "PlayerCount = 1 after one leave");
}

// ---------------------------------------------------------------------------
// §8  UpdatePresence overwrites stored record
// ---------------------------------------------------------------------------
void TestUpdatePresence()
{
    std::printf("[§8 UpdatePresence overwrites stored record]\n");
    tmapsvr::LocalMapState m;

    m.EnterMap(50, MakePresence(50, 64.0f, 64.0f, "OldName"));

    tmapsvr::PlayerPresence updated = MakePresence(50, 64.0f, 64.0f, "NewName");
    updated.level = 99;
    updated.hp    = 12000;
    m.UpdatePresence(50, updated);

    const auto* p = m.GetPresence(50);
    Check(p != nullptr, "GetPresence returns non-null after UpdatePresence");
    if (p)
    {
        Check(p->name  == "NewName", "name updated");
        Check(p->level == 99,        "level updated");
        Check(p->hp    == 12000u,    "hp updated");
    }
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalMapState spec ===\n");
    std::printf("    Legacy: TMap.cpp + TCell.cpp (GetNeighbor / EnterMAP / LeaveMAP / OnMove)\n\n");
    try
    {
        TestCellKey();
        TestEnterMap();
        TestLeaveMap();
        TestMoveSameCell();
        TestMoveCellTransition();
        TestGetNeighborIds();
        TestContainsAndCount();
        TestUpdatePresence();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
