#include "event_codec.h"

namespace tcontrolsvr::event_codec {

bool Read(wire::Reader& r, EventInfo& out)
{
    if (!r.Read(out.index))       return false;
    if (!r.Read(out.kind))        return false;
    if (!r.Read(out.state))       return false;
    if (!r.Read(out.group_id))    return false;
    if (!r.Read(out.server_type)) return false;
    if (!r.Read(out.server_id))   return false;
    if (!r.Read(out.start_unix))  return false;
    if (!r.Read(out.end_unix))    return false;
    if (!r.Read(out.value))       return false;
    if (!r.Read(out.map_id))      return false;
    if (!r.Read(out.start_alarm)) return false;
    if (!r.Read(out.end_alarm))   return false;
    if (!r.ReadString(out.start_msg)) return false;
    if (!r.ReadString(out.end_msg))   return false;
    if (!r.ReadString(out.title))     return false;
    if (!r.Read(out.part_time))   return false;
    if (!r.ReadString(out.lottery_msg)) return false;

    std::uint16_t count = 0;
    if (!r.Read(count)) return false;
    out.cash_items.clear();
    out.cash_items.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        CashItemSale s{};
        if (!r.Read(s.item_id) || !r.Read(s.sale_value)) return false;
        out.cash_items.push_back(s);
    }

    if (!r.Read(out.mon_event.start_action)) return false;
    if (!r.Read(out.mon_event.end_action))   return false;
    if (!r.Read(count)) return false;
    out.mon_event.spawn_ids.clear();
    out.mon_event.spawn_ids.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint16_t v = 0;
        if (!r.Read(v)) return false;
        out.mon_event.spawn_ids.push_back(v);
    }

    if (!r.Read(count)) return false;
    out.mon_regens.clear();
    out.mon_regens.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        MonRegen m{};
        if (!r.Read(m.mon_id) || !r.Read(m.delay) || !r.Read(m.map_id) ||
            !r.Read(m.pos_x)  || !r.Read(m.pos_y) || !r.Read(m.pos_z))
            return false;
        out.mon_regens.push_back(m);
    }

    if (!r.Read(count)) return false;
    out.lotteries.clear();
    out.lotteries.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        Lottery l{};
        if (!r.Read(l.item_id) || !r.Read(l.count) || !r.Read(l.winner))
            return false;
        out.lotteries.push_back(l);
    }
    return true;
}

void Write(std::vector<std::byte>& buf, const EventInfo& ev)
{
    wire::WritePOD<std::uint32_t>(buf, ev.index);
    wire::WritePOD<std::uint8_t >(buf, ev.kind);
    wire::WritePOD<std::uint8_t >(buf, ev.state);
    wire::WritePOD<std::uint8_t >(buf, ev.group_id);
    wire::WritePOD<std::uint8_t >(buf, ev.server_type);
    wire::WritePOD<std::uint8_t >(buf, ev.server_id);
    wire::WritePOD<std::int64_t >(buf, ev.start_unix);
    wire::WritePOD<std::int64_t >(buf, ev.end_unix);
    wire::WritePOD<std::uint16_t>(buf, ev.value);
    wire::WritePOD<std::uint16_t>(buf, ev.map_id);
    wire::WritePOD<std::uint32_t>(buf, ev.start_alarm);
    wire::WritePOD<std::uint32_t>(buf, ev.end_alarm);
    wire::WriteString(buf, ev.start_msg);
    wire::WriteString(buf, ev.end_msg);
    wire::WriteString(buf, ev.title);
    wire::WritePOD<std::uint8_t >(buf, ev.part_time);
    wire::WriteString(buf, ev.lottery_msg);

    wire::WritePOD<std::uint16_t>(buf,
        static_cast<std::uint16_t>(ev.cash_items.size()));
    for (const auto& s : ev.cash_items)
    {
        wire::WritePOD<std::uint16_t>(buf, s.item_id);
        wire::WritePOD<std::uint8_t >(buf, s.sale_value);
    }

    wire::WritePOD<std::uint8_t >(buf, ev.mon_event.start_action);
    wire::WritePOD<std::uint8_t >(buf, ev.mon_event.end_action);
    wire::WritePOD<std::uint16_t>(buf,
        static_cast<std::uint16_t>(ev.mon_event.spawn_ids.size()));
    for (auto v : ev.mon_event.spawn_ids)
        wire::WritePOD<std::uint16_t>(buf, v);

    wire::WritePOD<std::uint16_t>(buf,
        static_cast<std::uint16_t>(ev.mon_regens.size()));
    for (const auto& m : ev.mon_regens)
    {
        wire::WritePOD<std::uint16_t>(buf, m.mon_id);
        wire::WritePOD<std::uint32_t>(buf, m.delay);
        wire::WritePOD<std::uint16_t>(buf, m.map_id);
        wire::WritePOD<float>(buf, m.pos_x);
        wire::WritePOD<float>(buf, m.pos_y);
        wire::WritePOD<float>(buf, m.pos_z);
    }

    wire::WritePOD<std::uint16_t>(buf,
        static_cast<std::uint16_t>(ev.lotteries.size()));
    for (const auto& l : ev.lotteries)
    {
        wire::WritePOD<std::uint16_t>(buf, l.item_id);
        wire::WritePOD<std::uint8_t >(buf, l.count);
        wire::WritePOD<std::uint16_t>(buf, l.winner);
    }
}

} // namespace tcontrolsvr::event_codec
