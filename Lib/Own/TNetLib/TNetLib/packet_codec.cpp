// PCH-free (no stdafx.h) — compiles on Linux without TNetLib.h → winsock2.h.
// MSVC vcxproj marks this file with PrecompiledHeader=NotUsing.

#include "packet_codec.h"

#include <cassert>
#include <cstring>

namespace tnetlib {

// Same constants as the legacy g_4skey[KEY_COUNT] in Session.cpp:7-14.
// DO NOT change values — they're part of the wire protocol; every
// legacy client computes the same per-packet key from them.
const std::int64_t kKeyTable[kKeyTableSize] = {
    static_cast<std::int64_t>(0x5193817ae183aceeLL),
    static_cast<std::int64_t>(0x3891aeacbed18eadLL),
    static_cast<std::int64_t>(0x549aeced13de13a1LL),
    static_cast<std::int64_t>(0x09aeb1498c1eade9LL),
    static_cast<std::int64_t>(0x19861acea1720ae7LL),
    static_cast<std::int64_t>(0x0139aecea89541a2LL),
    static_cast<std::int64_t>(0x6b97253c5fbb8b06LL),
};

namespace {

// Read/write a uint8 from a std::byte*. std::byte arithmetic doesn't
// implicitly convert to integers (C++20 requires a cast), so factor
// this out for clarity.
inline std::uint8_t Read8(const std::byte* p) noexcept
{
    return static_cast<std::uint8_t>(*p);
}
inline void Write8(std::byte* p, std::uint8_t v) noexcept
{
    *p = static_cast<std::byte>(v);
}

// Returns the i-th byte of an int64 in little-endian order. This is
// what the legacy code did via `(LPBYTE)&key`[i] on a little-endian
// host; spelling it out makes the codec portable to big-endian hosts
// (even though no shipped server target is BE).
//
// Caller MUST pass i in [0, 8). i >= 8 would shift by 64+ bits which
// is undefined behavior on the underlying uint64. All in-tree callers
// are the body tail loops (i < body_len % 8, capped at 7); the assert
// guards future misuse.
inline std::uint8_t KeyByteLE(std::int64_t key, std::size_t i) noexcept
{
    assert(i < sizeof(std::int64_t));
    return static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(key) >> (i * 8)) & 0xFFu);
}

// Loads an INT64 stored in native byte order at `p`. The legacy code
// reinterpreted body bytes as `PINT64*` directly; this preserves that
// behavior. (Body INT64 chunks XORed with `key` produce the same
// bytes on any endianness as long as both ends agree, and they do.)
inline std::int64_t LoadInt64(const std::byte* p) noexcept
{
    std::int64_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}
inline void StoreInt64(std::byte* p, std::int64_t v) noexcept
{
    std::memcpy(p, &v, sizeof(v));
}

} // namespace

std::int64_t EncryptBody(std::byte* body, std::size_t body_len, std::int64_t key) noexcept
{
    if (body_len == 0) return 0;

    std::int64_t checksum = 0;
    std::int64_t crc      = 0;

    const std::size_t chunks = body_len / sizeof(std::int64_t);
    const std::size_t tail   = body_len % sizeof(std::int64_t);

    // Body: XOR per INT64 chunk; checksum from plaintext.
    for (std::size_t i = 0; i < chunks; ++i)
    {
        std::byte* slot = body + i * sizeof(std::int64_t);
        const std::int64_t plain = LoadInt64(slot);
        checksum ^= plain;
        StoreInt64(slot, plain ^ key);
    }

    // Tail: 1..7 leftover bytes. XOR with successive key bytes (little-
    // endian). Rolling `crc` step matches the legacy mixing exactly.
    std::byte* tail_ptr = body + chunks * sizeof(std::int64_t);
    for (std::size_t i = 0; i < tail; ++i)
    {
        const std::uint8_t plain = Read8(tail_ptr + i);
        checksum ^= static_cast<std::int64_t>(plain);
        Write8(tail_ptr + i, plain ^ KeyByteLE(key, i));
        crc = ((static_cast<std::uint64_t>(crc) >> 4) & 0x0FFD) ^ key;
        checksum += crc;
    }

    return checksum;
}

bool DecryptBody(std::byte* body, std::size_t body_len, std::int64_t key,
                 std::int64_t expected_checksum) noexcept
{
    if (body_len == 0)
        return expected_checksum == 0;

    std::int64_t checksum = 0;
    std::int64_t crc      = 0;

    const std::size_t chunks = body_len / sizeof(std::int64_t);
    const std::size_t tail   = body_len % sizeof(std::int64_t);

    // Body: XOR first, then checksum from the recovered plaintext.
    for (std::size_t i = 0; i < chunks; ++i)
    {
        std::byte* slot = body + i * sizeof(std::int64_t);
        const std::int64_t plain = LoadInt64(slot) ^ key;
        StoreInt64(slot, plain);
        checksum ^= plain;
    }

    // Tail in the inverse interleaving of Encrypt.
    std::byte* tail_ptr = body + chunks * sizeof(std::int64_t);
    for (std::size_t i = 0; i < tail; ++i)
    {
        const std::uint8_t recovered =
            static_cast<std::uint8_t>(Read8(tail_ptr + i) ^ KeyByteLE(key, i));
        Write8(tail_ptr + i, recovered);
        checksum ^= static_cast<std::int64_t>(recovered);
        crc = ((static_cast<std::uint64_t>(crc) >> 4) & 0x0FFD) ^ key;
        checksum += crc;
    }

    return checksum == expected_checksum;
}

void EncryptHeader(PacketHeader* header, std::int64_t key) noexcept
{
    if (header == nullptr) return;

    // Reach the bytes from `wId` onward — wSize stays plaintext.
    std::byte* hdr_bytes = reinterpret_cast<std::byte*>(header) + sizeof(std::uint16_t);

    const std::uint16_t wSize = header->wSize;
    const std::uint16_t wId   = header->wId;

    const std::size_t span = kPacketHeaderSize - sizeof(std::uint16_t);
    for (std::size_t i = 0; i < span; ++i)
    {
        const std::uint8_t mix = (i < 2)
            ? static_cast<std::uint8_t>(key + wSize + static_cast<std::int64_t>(i))
            : static_cast<std::uint8_t>(key + wId   + static_cast<std::int64_t>(i));
        Write8(hdr_bytes + i, Read8(hdr_bytes + i) ^ mix);
    }
}

void DecryptHeader(PacketHeader* header, std::int64_t key) noexcept
{
    if (header == nullptr) return;

    std::byte* hdr_bytes = reinterpret_cast<std::byte*>(header) + sizeof(std::uint16_t);
    const std::uint16_t wSize = header->wSize;

    // wId is xored back during the loop using the (already-decrypted)
    // values of preceding bytes — matches Packet.cpp:124-131. The
    // re-read inside the loop is intentional: for i>=2, header->wId
    // holds the decrypted value after iterations i=0 and i=1.
    const std::size_t span = kPacketHeaderSize - sizeof(std::uint16_t);
    for (std::size_t i = 0; i < span; ++i)
    {
        const std::uint8_t mix = (i < 2)
            ? static_cast<std::uint8_t>(key + wSize + static_cast<std::int64_t>(i))
            : static_cast<std::uint8_t>(key + header->wId + static_cast<std::int64_t>(i));
        Write8(hdr_bytes + i, Read8(hdr_bytes + i) ^ mix);
    }
}

} // namespace tnetlib
