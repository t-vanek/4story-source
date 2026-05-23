#include "services/guild_cabinet_codec.h"

namespace tworldsvr {

bool ReadCabinetItem(wire::Reader& r, TGuildCabinetItem& out)
{
    std::uint8_t magic_count = 0;
    if (!r.Read(out.id)           || !r.Read(out.item_id_b)   ||
        !r.Read(out.item_id_w)    || !r.Read(out.level)       ||
        !r.Read(out.gem)          || !r.Read(out.mogg_item_id)||
        !r.Read(out.count)        || !r.Read(out.glevel)      ||
        !r.Read(out.dura_max)     || !r.Read(out.dura_cur)    ||
        !r.Read(out.refine_cur)   || !r.Read(out.end_time)    ||
        !r.Read(out.grade_effect) || !r.Read(out.ext_eld)     ||
        !r.Read(out.ext_wrap)     || !r.Read(out.ext_color)   ||
        !r.Read(out.ext_guild)    || !r.Read(magic_count))
    {
        return false;
    }
    out.magic.clear();
    out.magic.reserve(magic_count);
    for (std::uint8_t i = 0; i < magic_count; ++i)
    {
        std::uint8_t  id = 0;
        std::uint16_t value = 0;
        if (!r.Read(id) || !r.Read(value)) return false;
        out.magic.emplace_back(id, value);
    }
    return true;
}

void WriteCabinetItem(std::vector<std::byte>& body,
                      const TGuildCabinetItem& item)
{
    using namespace wire;
    WritePOD<std::int64_t>(body, item.id);
    WritePOD<std::uint8_t>(body, item.item_id_b);
    WritePOD<std::uint16_t>(body, item.item_id_w);
    WritePOD<std::uint8_t>(body, item.level);
    WritePOD<std::uint8_t>(body, item.gem);
    WritePOD<std::uint16_t>(body, item.mogg_item_id);
    WritePOD<std::uint8_t>(body, item.count);
    WritePOD<std::uint8_t>(body, item.glevel);
    WritePOD<std::uint32_t>(body, item.dura_max);
    WritePOD<std::uint32_t>(body, item.dura_cur);
    WritePOD<std::uint8_t>(body, item.refine_cur);
    WritePOD<std::int64_t>(body, item.end_time);
    WritePOD<std::uint8_t>(body, item.grade_effect);
    WritePOD<std::uint32_t>(body, item.ext_eld);
    WritePOD<std::uint32_t>(body, item.ext_wrap);
    WritePOD<std::uint32_t>(body, item.ext_color);
    WritePOD<std::uint32_t>(body, item.ext_guild);
    WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(item.magic.size()));
    for (const auto& [id, value] : item.magic)
    {
        WritePOD<std::uint8_t>(body, id);
        WritePOD<std::uint16_t>(body, value);
    }
}

} // namespace tworldsvr
