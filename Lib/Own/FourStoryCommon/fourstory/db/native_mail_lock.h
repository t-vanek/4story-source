#pragma once
#include <cstdint>

namespace fourstory::db {
// Shared by native Map producers/block changes and the World reserved-mail
// queue. One bigint namespace, separate from account/Map lifetime owner locks.
inline constexpr std::int64_t NativeMailboxKey(std::uint8_t world,std::uint32_t character) {
    return 0x504f530000000000LL|(static_cast<std::int64_t>(world)<<32)|character;
}
inline constexpr std::int64_t ReservedMailBatchKey(std::uint8_t world) {
    return 0x52504f0000000000LL|world;
}
}
