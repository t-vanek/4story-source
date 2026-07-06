#pragma once

// SOCI-backed player service. Reads TCHARTABLE for a single character
// id and packages the row as a CharSnapshot. Schema is validated at
// boot by tmapsvr::db::ValidateCharSchema.

#include "player_service.h"

#include <cstdint>
#include <optional>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class IStatChart;

// `stat` (optional) is the TFORMULACHART/TCLASSCHART/TRACECHART chart the
// max-HP/MP derivation reads. When supplied, LoadChar fills dwMaxHP/dwMaxMP
// from the stat engine (class+race+level formula); when null it falls back
// to the legacy placeholder (max = current), keeping the no-DB / unit-test
// path valid.
class SociPlayerService final : public IPlayerService
{
public:
    explicit SociPlayerService(fourstory::db::SessionPool& pool,
                               const IStatChart* stat = nullptr);

    std::optional<CharSnapshot>
        LoadChar(std::uint32_t char_id) override;

    void SaveChar(const CharSnapshot& snap) override;

private:
    fourstory::db::SessionPool& m_pool;
    const IStatChart*           m_stat = nullptr;
};

} // namespace tmapsvr
