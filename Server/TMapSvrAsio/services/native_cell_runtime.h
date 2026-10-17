#pragma once
#include "native_map_cells.h"
#include <cstddef>
#include <span>

namespace tmapsvr {
struct HandlerContext;
// Samples/encodes and posts every owned packet synchronously on the game
// executor. No coroutine suspension can reorder these cell transitions.
void PublishNativeCellNotices(const HandlerContext&,const NativeCellNotices&,
                             std::span<const std::byte> movement={});
}
