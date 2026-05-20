#pragma once

// Wire codec for the legacy EVENTINFO struct (~30 fields).
// Layout matches Server/TControlSvr/TControlType.h::WrapPacketIn /
// WrapPacketOut exactly so a legacy TController.exe + the
// legacy World/Map peers accept the bytes.
//
// Read order (WrapPacketOut, control ← operator):
//   dwIndex bID bState bGroupID bSvrType bSvrID
//   dStartDate dEndDate wValue wMapID
//   dwStartAlarm dwEndAlarm szStartMsg szEndMsg strTitle
//   bPartTime strLotMsg
//   wCount [wID bSaleValue]*count
//   bStartAction bEndAction wCount [wSpawnID]*count
//   wCount [wMonID dwDelay wMapID fPosX fPosY fPosZ]*count
//   wCount [wItemID bNum wWinner]*count
//
// Write order matches read 1:1.

#include "services/event_types.h"
#include "wire_codec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tcontrolsvr::event_codec {

// Read an EventInfo body from a CT_EVENTCHANGE_REQ packet. Returns
// false on a framing violation; the partial event left in `out` is
// undefined.
bool Read(wire::Reader& r, EventInfo& out);

// Append an EventInfo body to `buf` for CT_EVENTLIST_ACK or
// CT_EVENTCHANGE_ACK.
void Write(std::vector<std::byte>& buf, const EventInfo& ev);

} // namespace tcontrolsvr::event_codec
