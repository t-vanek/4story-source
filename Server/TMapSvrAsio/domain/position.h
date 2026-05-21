#pragma once

// Position — 3D world coordinate in the legacy float space.
// Shared across player snapshots, monster instances, presence
// updates, and AOI broadcasts. Kept as a POD on its own so any
// layer can include just this header without dragging in service
// interfaces.

namespace tmapsvr {

struct Position
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

} // namespace tmapsvr
