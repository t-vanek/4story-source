#pragma once

// W3a-37 — guild cabinet item wire codec. Symmetric read/write of
// the per-instance item the legacy CreateItem / WrapItem pair
// serialises (TServer.cpp:16 + TWorldSvr.cpp:5498). The on-wire
// layout is a fixed 17-field head + a variable-length magic list:
//
//   INT64  m_dlID
//   BYTE   m_bItemID
//   WORD   m_wItemID
//   BYTE   m_bLevel
//   BYTE   m_bGem
//   WORD   m_wMoggItemID
//   BYTE   m_bCount
//   BYTE   m_bGLevel
//   DWORD  m_dwDuraMax
//   DWORD  m_dwDuraCur
//   BYTE   m_bRefineCur
//   INT64  m_dEndTime
//   BYTE   m_bGradeEffect
//   DWORD  m_dwExtValue[IEV_ELD]
//   DWORD  m_dwExtValue[IEV_WRAP]
//   DWORD  m_dwExtValue[IEV_COLOR]
//   DWORD  m_dwExtValue[IEV_GUILD]
//   BYTE   m_bMagicCount
//   × m_bMagicCount: BYTE magic_id, WORD magic_value
//
// The cabinet `slot_id` (legacy m_dwItemID) is NOT part of this
// codec — it's a separate DWORD that the PUTIN handler reads
// before the wrapped item and the LIST sender writes before each
// wrapped item. ReadCabinetItem fills everything except slot_id;
// the caller sets that.

#include "services/guild_registry.h"
#include "wire_codec.h"

#include <cstdint>
#include <vector>

namespace tworldsvr {

// Unwrap a cabinet item from `r` (legacy CreateItem). Returns
// false on a short read; `out.slot_id` is left untouched (the
// caller sets it from the preceding DWORD). A magic_count that
// would overrun the buffer fails cleanly.
bool ReadCabinetItem(wire::Reader& r, TGuildCabinetItem& out);

// Wrap a cabinet item into `body` (legacy WrapItem). Does NOT
// emit slot_id — the LIST sender writes that before calling.
void WriteCabinetItem(std::vector<std::byte>& body,
                      const TGuildCabinetItem& item);

} // namespace tworldsvr
