#pragma once

// Mapper configuration registry — one TypeMap<Src, Dst> per (Src, Dst)
// pair, accessible globally via MapperConfig<Src, Dst>().
//
// Configuration is mutable until first use. Best practice: configure
// at process startup (main / DI bootstrap), then read-only afterwards.
// The registry is *not* thread-safe for concurrent configuration; once
// frozen by the first Adapt() call, concurrent reads are fine because
// std::function is not modified after construction.
//
// Usage:
//
//   // At startup:
//   MapperConfig<DbCharRow, CharSnapshot>()
//       .Set(&CharSnapshot::dwCharID, &DbCharRow::id)
//       .Set(&CharSnapshot::szNAME,   &DbCharRow::name);
//
//   // Anywhere in code:
//   CharSnapshot s = Adapt<CharSnapshot>(db_row);
//   auto list     = AdaptAll<CharSnapshot>(db_rows);

#include "type_map.h"

#include <vector>

namespace fourstory::mapper {

namespace detail {
    template<typename Src, typename Dst>
    inline TypeMap<Src, Dst>& Instance()
    {
        // Meyers singleton — thread-safe init, lives until exit.
        static TypeMap<Src, Dst> instance;
        return instance;
    }
}

// Access (or create) the global TypeMap<Src, Dst>.
template<typename Src, typename Dst>
inline TypeMap<Src, Dst>& MapperConfig()
{
    return detail::Instance<Src, Dst>();
}

// ── Adapt — convenience shortcuts using the global config ──────────

// Adapt<Dst>(src) — create new Dst from Src.
template<typename Dst, typename Src>
inline Dst Adapt(const Src& src)
{
    return MapperConfig<Src, Dst>().Map(src);
}

// AdaptTo(src, existing) — populate an existing Dst instance.
template<typename Src, typename Dst>
inline void AdaptTo(const Src& src, Dst& dst)
{
    MapperConfig<Src, Dst>().Apply(src, dst);
}

// AdaptAll<Dst>(src_collection) — vector<Src> → vector<Dst>.
template<typename Dst, typename SrcRange>
inline auto AdaptAll(const SrcRange& src)
{
    using Src = typename SrcRange::value_type;
    return MapperConfig<Src, Dst>().MapAll(src);
}

} // namespace fourstory::mapper
