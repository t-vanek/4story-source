#pragma once

// IItemStateRepository — write interface for the operator item-state
// tool (W6-36). One method mirroring the legacy `TItemStateChange`
// stored procedure (Server/TWorldSvr/DBAccess.h:2180): flip
// TITEMCHART.bInitState for one wItemID, failing when the item id
// doesn't exist. Concrete impls: SociItemStateRepository (direct
// UPDATE against TITEMCHART) and FakeItemStateRepository (in-memory
// for tests).
//
// The legacy flow batches N changes per CT_ITEMSTATE_REQ and stops at
// the first failing item (SSHandler.cpp:10045-10068 breaks out of the
// per-item SP loop); the handler owns that loop so the repo stays a
// single-item primitive.

#include <cstdint>
#include <string>
#include <vector>

namespace tworldsvr {

// One (wItemID, bInitState) change row as carried on the wire by
// CT_ITEMSTATE_REQ / MW_ITEMSTATE_REQ / CT_ITEMSTATE_ACK.
struct ItemStateChange
{
    std::uint16_t item_id    = 0;
    std::uint8_t  init_state = 0;
};

// One TITEMCHART row as returned by the W6-40 operator item search
// (legacy CTBLItemFind, DBAccess.h:679).
struct ItemFindRow
{
    std::uint16_t item_id    = 0;
    std::uint8_t  init_state = 0;
    std::string   name;
};

class IItemStateRepository
{
public:
    virtual ~IItemStateRepository() = default;

    // UPDATE TITEMCHART.bInitState for `item_id`. Returns false when
    // the item id has no chart row (legacy SP RETURN 1) or the DB
    // call fails. Called from the CT_ITEMSTATE_REQ handler via
    // CoOffloadIf so SOCI never blocks the io_context.
    virtual bool ChangeState(std::uint16_t item_id,
                             std::uint8_t  init_state) = 0;

    // W6-40: SELECT wItemID, bInitState, szName FROM TITEMCHART WHERE
    // szName LIKE :name OR wItemID = :id — the operator item search
    // behind CT_ITEMFIND_REQ. `name_pattern` is passed to LIKE as-is
    // (the operator console supplies its own % wildcards — legacy
    // parity). Empty result → empty vector (the ACK still fires with
    // count=0).
    virtual std::vector<ItemFindRow> FindItems(
        std::uint16_t item_id, const std::string& name_pattern) = 0;
};

} // namespace tworldsvr
