#pragma once
#include <bit>
#include <cstdint>

namespace tmapsvr {
// Source DWORD identities in PostgreSQL INTEGER columns preserve all 32 bits,
// rather than widening an unsigned wire value to a different positive BIGINT.
// Operational session_key/unlocked_key BIGINT columns instead retain their
// positive wire value. Nor is this for connection generations, authority
// epochs, IPv4 addresses, item identities or other real BIGINT values.
constexpr std::int32_t SqlInt32Identity(std::uint32_t wire_value) noexcept {
    return std::bit_cast<std::int32_t>(wire_value);
}
}
