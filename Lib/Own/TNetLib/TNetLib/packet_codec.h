#pragma once

// Portable 4Story wire-format codec — header + body XOR cipher with
// rolling checksum, sequence-number-keyed via a 7-slot table.
//
// Extracted from the legacy CPacket methods (Encrypt / Decrypt /
// EncryptHeader / DecryptHeader in Packet.cpp) so the same byte-level
// transforms are usable from code paths that don't drag in winsock.h /
// OVERLAPPED / ATL — specifically the new tnetlib::AsioSession.
//
// Wire-format invariants (do NOT change without breaking every legacy
// client and the encrypted server-to-server protocol):
//
//   * 16-byte header, layout in PacketHeader below. No padding (verified
//     by static_assert).
//   * Little-endian on the wire for every multi-byte field. The codec
//     uses memcpy / member access on a packed POD, so it is byte-for-byte
//     equivalent to the legacy code on any little-endian host (which is
//     every shipped target).
//   * Body length = wSize - 16. Body is XORed in INT64 chunks with `key`
//     directly; the trailing 1..7 bytes XOR with successive bytes of
//     `key` (little-endian order of the INT64).
//   * Header bytes [2..15] (everything after wSize) are XORed byte by
//     byte: first two bytes (wID) use `key + wSize + i`, remaining 12
//     bytes use `key + wID + i` — exactly per the legacy implementation.
//     wSize stays plaintext so the receiver can frame before decrypt.
//   * Per-packet `key` is derived from the sequence number:
//         key = kKeyTable[sequence_number % kKeyTableSize]
//   * Body checksum stored in PacketHeader::llChecksum; computed from
//     the *plaintext* body bytes (legacy code interleaves XOR with the
//     checksum accumulation so that on decrypt the checksum is checked
//     against the recovered plaintext).
//
// Phase 2 scope: this header + .cpp + tests. Does not yet wrap the
// legacy CPacket class (callers can adopt at their pace) and does not
// include the RC4-over-everything client→server pre-pass (that lives
// in tnetlib_crypto and Session::Decrypt already; AsioSession will
// compose them when packet I/O integrates in a follow-up commit).

#include <cstddef>
#include <cstdint>

namespace tnetlib {

// 16-byte wire header. Layout must match the legacy `_tagPACKETHEADER`
// struct in Packet.h byte-for-byte. Packed; verified below.
#pragma pack(push, 1)
struct PacketHeader
{
    std::uint16_t wSize;     // total packet size including this header
    std::uint16_t wId;       // message ID
    std::uint32_t dwNumber;  // sequence number — keys the per-packet XOR
    std::int64_t  llChecksum; // body checksum computed by EncryptBody
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes wire-compatible");

inline constexpr std::size_t kPacketHeaderSize = sizeof(PacketHeader);
inline constexpr std::size_t kMaxPacketSize    = 0xFFFF;

// 7-slot key table. Same constants as Session.cpp:7-14 in the legacy
// code; relocated here so the codec is self-contained.
inline constexpr std::size_t kKeyTableSize = 7;
extern const std::int64_t kKeyTable[kKeyTableSize];

// Returns the per-packet key derived from `sequence`.
inline std::int64_t KeyForSequence(std::uint32_t sequence) noexcept
{
    return kKeyTable[sequence % kKeyTableSize];
}

// Encrypts `body` (length `body_len` bytes) in place using XOR keyed
// by the 8 bytes of `key`. Returns the computed body checksum, which
// the caller should store into PacketHeader::llChecksum before sending.
//
// `body` does NOT include the 16-byte header; pass a pointer to the
// first body byte. body_len may be 0 (returns 0).
std::int64_t EncryptBody(std::byte* body, std::size_t body_len, std::int64_t key) noexcept;

// Decrypts `body` in place. Returns true iff the recovered plaintext
// checksum equals `expected_checksum` (i.e. the value the sender
// originally placed in PacketHeader::llChecksum). False indicates
// tamper or corruption — the body bytes have already been XOR'd back
// and may be partially intact, so caller should discard the packet.
bool DecryptBody(std::byte* body, std::size_t body_len, std::int64_t key,
                 std::int64_t expected_checksum) noexcept;

// XORs the 14 bytes after `wSize` in place. wSize itself is preserved
// (stays plaintext on the wire so the receiver can frame).
void EncryptHeader(PacketHeader* header, std::int64_t key) noexcept;

// Inverse of EncryptHeader. Note: wId is xored back during the loop
// using the (already-decrypted) values of preceding bytes — matches
// the legacy implementation at Packet.cpp:124-131 exactly.
void DecryptHeader(PacketHeader* header, std::int64_t key) noexcept;

} // namespace tnetlib
