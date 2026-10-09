#pragma once

#include <cstdint>

namespace tmapsvr {

// Lib/Own/TProtocol/include/NetCode.h, TCONNECT_RESULT.
enum class ConnectResult : std::uint8_t {
    Ok = 0, NoChannel = 1, InvalidChar = 2, Duplicate = 3,
    InvalidVer = 4, Internal = 5,
};

inline constexpr std::uint16_t kClientVersion = 0x2918;

// Client/TClient/CSSender.cpp::SendCS_CONNECT_REQ. Windows DWORD
// arithmetic wraps at 32 bits BEFORE assignment to the 64-bit accumulator.
// Explicit unsigned arithmetic also defines the subsequent INT64 wraparound.
constexpr std::uint64_t ConnectChecksum(std::uint16_t version,
    std::uint32_t user, std::uint32_t character, std::uint32_t key)
{
    const std::uint32_t initial = std::uint32_t(version) * user + key * character;
    std::uint64_t checksum = initial;
    const auto count = initial % 8;
    const std::uint64_t body = initial / 8;
    for (std::uint32_t i = 0; i < count; ++i)
        checksum = (checksum ^ body) + UINT64_C(0x336c3aebf71a8b08);
    return checksum;
}

} // namespace tmapsvr
