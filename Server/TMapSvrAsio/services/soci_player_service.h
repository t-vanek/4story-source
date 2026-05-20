#pragma once

// SOCI-backed IPlayerService. Queries TCHARTABLE for base character
// data using the same (char_id, user_id) key pair that the legacy
// OnDM_LOADCHAR_REQ handler used for `CTBLChar::Open()`.
//
// Inventory items (TITEMTABLE) are PENDING (F5) — LoadChar returns
// an empty inventory vector so all downstream users get a valid
// CharSnapshot with zero items rather than a partial one.
//
// Wire-faithful to Server/TMapSvr/DBAccess.h::CTBLChar column order.

#include "services/player_service.h"

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociPlayerService final : public IPlayerService
{
public:
    explicit SociPlayerService(fourstory::db::SessionPool& pool);

    // Load base character record from TCHARTABLE WHERE dwID = char_id
    // AND dwUserID = user_id. The dw_key is validated by the caller
    // (IMapSessionValidator already confirmed it against TCURRENTUSER)
    // and is stored verbatim in the returned snapshot so downstream
    // code can re-check without a second DB hit.
    //
    // Returns nullopt on no-row, dw_key mismatch, or DB error.
    std::optional<legacy::CharSnapshot>
        LoadChar(std::uint32_t char_id,
                 std::uint32_t user_id,
                 std::uint32_t dw_key) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
