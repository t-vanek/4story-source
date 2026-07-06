#pragma once

// LuckyEvent wire codec — byte-for-byte the legacy
// LUCKYEVENT::WrapPacketIn/Out order (TWorldType.h:1140): id, day,
// hour, minute, item ids 1..5, count, present, announce, title,
// message, item names 1..5.

#include "../wire_codec.h"
#include "lucky_event_repository.h"

#include <cstddef>
#include <vector>

namespace tworldsvr {

inline void WriteLuckyEvent(std::vector<std::byte>& body,
                            const LuckyEvent&       e)
{
    using namespace wire;
    WritePOD<std::uint16_t>(body, e.id);
    WritePOD<std::uint8_t>(body, e.day);
    WritePOD<std::uint8_t>(body, e.hour);
    WritePOD<std::uint8_t>(body, e.minute);
    for (int i = 0; i < 5; ++i)
        WritePOD<std::uint16_t>(body, e.item_id[i]);
    WritePOD<std::uint8_t>(body, e.count);
    WriteString(body, e.present);
    WriteString(body, e.announce);
    WriteString(body, e.title);
    WriteString(body, e.message);
    for (int i = 0; i < 5; ++i)
        WriteString(body, e.item_name[i]);
}

inline bool ReadLuckyEvent(wire::Reader& r, LuckyEvent& e)
{
    if (!r.Read(e.id) || !r.Read(e.day) || !r.Read(e.hour) ||
        !r.Read(e.minute))
        return false;
    for (int i = 0; i < 5; ++i)
        if (!r.Read(e.item_id[i]))
            return false;
    if (!r.Read(e.count) || !r.ReadString(e.present) ||
        !r.ReadString(e.announce) || !r.ReadString(e.title) ||
        !r.ReadString(e.message))
        return false;
    for (int i = 0; i < 5; ++i)
        if (!r.ReadString(e.item_name[i]))
            return false;
    return true;
}

} // namespace tworldsvr
