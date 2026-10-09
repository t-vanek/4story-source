#pragma once

// Player service — loads a character snapshot from TCHARTABLE.
//
// The CharSnapshot struct holds the fields the F8 DM_LOADCHAR_ACK
// success branch encodes onto the wire. Later phases extend the
// snapshot (and the encoded body) with items (F9), skills (F11),
// quests (F12), …; each phase fills in the section it owns. The
// trailing sub-sections of legacy DM_LOADCHAR_ACK (secure code, aid
// table, PC bang, post info, inventory, cabinet, …) are emitted with
// default / empty values in F8 and replaced as the responsible
// service comes online.

#include "domain/character.h"
#include "domain/session.h"

#include <cstdint>
#include <optional>
#include <span>
#include <cstddef>
#include <stdexcept>

namespace tmapsvr {

struct TransferredCharacter {CharSnapshot snapshot;std::uint64_t authority_epoch{};};

class IPlayerService
{
public:
    virtual ~IPlayerService() = default;

    // Load the snapshot for `char_id`, or nullopt when the row is
    // missing / the DB is in trouble. The handler distinguishes
    // "no row" (CN_NOCHAR in legacy) from "lookup error" by checking
    // the optional and the configured pool's health independently.
    virtual std::optional<CharSnapshot>
        LoadChar(std::uint32_t char_id) = 0;

    // Persist the live snapshot back to TCHARTABLE + TALLCHARTABLE.
    // Called by the MapServer teardown hook on every authenticated
    // disconnect. Mirrors the combined effect of TMapSvr's char-save
    // path and legacy TLogout's TALLCHARTABLE update.
    virtual void SaveChar(const CharSnapshot& snap) = 0;
    virtual std::optional<CharSnapshot> LoadAuthorized(const MapSessionClaim& claim) {
        return LoadChar(claim.char_id);
    }
    virtual void SaveAuthorized(const MapSessionClaim&, const CharSnapshot& snap) {
        if(snap.persistence_uncertain)throw std::runtime_error("Uncertain durable gameplay outcome");
        SaveChar(snap);
    }
    virtual bool PrepareTransfer(const MapSessionClaim&,const CharSnapshot&,std::span<const std::byte>) {return false;}
    virtual std::optional<TransferredCharacter> AcceptTransfer(const MapSessionClaim&,std::span<const std::byte>) {return {};}
    virtual bool OutgoingTransferCommitted(const MapSessionClaim&) {return false;}
    virtual std::vector<std::string> ConsumeSkillItems(const MapSessionClaim&,std::uint16_t,std::uint8_t,
        const std::vector<SkillItemDebit>&,const CharSnapshot&) {throw std::runtime_error("Native skill item transaction unavailable");}
    virtual InventoryMoveCommit MoveInventoryItems(const MapSessionClaim&,const InventoryMoveRequest&,
        const CharSnapshot&,const CharSnapshot&) {throw std::runtime_error("Native inventory move transaction unavailable");}
    std::string ConsumeSkillItem(const MapSessionClaim& c,std::uint16_t skill,
        const ItemInstance& before,const CharSnapshot& after) {
        return ConsumeSkillItems(c,skill,1,{{before,1}},after).at(0);
    }

};

} // namespace tmapsvr
