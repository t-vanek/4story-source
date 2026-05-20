#pragma once

// IEventRepository — persistence layer for the event manager.
// Backs the legacy CSPEventUpdate stored procedure + the
// TEVENTCHART / TCASHSHOPITEMCHART table reads:
//
//   { ? = CALL TEventUpdate(idx, kind_id, op, title, group, svr_type,
//                           svr_id, start_date, end_date, value,
//                           map_id, start_alarm, end_alarm,
//                           part_time, start_msg, mid_msg, end_msg,
//                           value_blob) }
//
// `op` is the event_op::* enum (DEL / ADD / UPDATE).
// The SP returns 0 on success, non-zero error code on failure.
//
// LoadAll() runs once at startup and rebuilds the in-memory
// m_mapEVENT.

#include "event_types.h"

#include <cstdint>
#include <vector>

namespace tcontrolsvr {

class IEventRepository
{
public:
    virtual ~IEventRepository() = default;

    // Bring every event row from TEVENTCHART into memory. Throws on
    // DB error so main() can fail fast at boot.
    virtual std::vector<EventInfo> LoadAll() = 0;

    // Read TCASHSHOPITEMCHART (WHERE bCanSell=1). Used by
    // CT_CASHITEMLIST_REQ.
    virtual std::vector<CashItem> ListCashItems() = 0;

    // CSPEventUpdate-equivalent. Returns the SP return code (0 on
    // success). `value_blob` is the encoded m_szValue string the
    // legacy MakeStrValue() builds.
    virtual std::uint8_t Persist(const EventInfo& ev,
                                 std::uint8_t op,
                                 const std::string& value_blob) = 0;
};

} // namespace tcontrolsvr
