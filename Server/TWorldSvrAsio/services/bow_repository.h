#pragma once

// IBowRepository — DB surface for the W6-54 Bow scheduler: the
// TBOWSETTINGSCHART config row + TCUSTOMTIMECHART BT_BOW start
// times at boot, and the roster persistence SPs the teleport
// fan-outs fire (TAddBOWPlayer / TClearBOWPlayers /
// TDeleteSingleBOWPlayer).

#include <cstdint>
#include <optional>
#include <vector>

namespace tworldsvr {

// TBOWSETTINGSCHART row (CTBLBOWSettingsChart, DBAccess.h:1548).
struct BowSettings
{
    std::uint16_t map_id                 = 0;
    std::uint8_t  min_players            = 0;
    std::uint8_t  max_nation_difference  = 0;
    std::uint32_t alarm_dur              = 0;
    std::uint32_t buy_dur                = 0;
    std::uint32_t battle_dur             = 0;
};

class IBowRepository
{
public:
    virtual ~IBowRepository() = default;

    // First TBOWSETTINGSCHART row (legacy breaks after one).
    virtual std::optional<BowSettings> LoadSettings() = 0;

    // TCUSTOMTIMECHART rows with bType = BT_BOW (5) — seconds of
    // the local day.
    virtual std::vector<std::uint32_t> LoadStartTimes() = 0;

    // TAddBOWPlayer(char, user) — fired per teleported-in player.
    virtual void AddPlayer(std::uint32_t char_id,
                           std::uint32_t user_id) = 0;

    // TClearBOWPlayers — fired on battle end.
    virtual void ClearPlayers() = 0;

    // TDeleteSingleBOWPlayer(user) — fired on a single release.
    virtual void DeleteSinglePlayer(std::uint32_t user_id) = 0;
};

} // namespace tworldsvr
