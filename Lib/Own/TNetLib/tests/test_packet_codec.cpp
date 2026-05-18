// Standalone tests for the portable packet codec (no PCH, no winsock).
// Verifies that the extracted EncryptBody / DecryptBody / EncryptHeader
// / DecryptHeader functions:
//   1. Round-trip cleanly (encrypt then decrypt = identity).
//   2. Detect a tampered body via the checksum mismatch.
//   3. Detect a tampered header — DecryptHeader gives back garbage when
//      the EncryptHeader-time wId/wSize differ, which subsequent
//      DecryptBody will then fail to checksum-validate.
//   4. Handle the tail-byte branch (body_len not a multiple of 8).
//   5. Use the same key constants as Session.cpp's g_4skey.

#include "../TNetLib/packet_codec.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

std::string HexDump(const std::byte* buf, std::size_t len)
{
    std::string out;
    out.reserve(len * 2);
    char hex[3];
    for (std::size_t i = 0; i < len; ++i)
    {
        std::snprintf(hex, sizeof(hex), "%02x", static_cast<unsigned>(buf[i]));
        out += hex;
    }
    return out;
}

void TestKeyTable()
{
    std::printf("[key table parity with legacy g_4skey]\n");
    Check(tnetlib::kKeyTable[0] == static_cast<std::int64_t>(0x5193817ae183aceeLL),
          "slot 0 matches legacy");
    Check(tnetlib::kKeyTable[6] == static_cast<std::int64_t>(0x6b97253c5fbb8b06LL),
          "slot 6 matches legacy");
    Check(tnetlib::KeyForSequence(8) == tnetlib::kKeyTable[1],
          "sequence wrap: 8 % 7 == 1");
}

void TestBodyRoundTrip_AlignedSize()
{
    std::printf("[body round-trip — aligned size, 16 bytes / 2 INT64 chunks]\n");
    std::byte plain[16];
    for (std::size_t i = 0; i < 16; ++i) plain[i] = static_cast<std::byte>(i + 1);

    std::byte cipher[16];
    std::memcpy(cipher, plain, 16);

    const std::int64_t key = tnetlib::KeyForSequence(1);
    const std::int64_t checksum = tnetlib::EncryptBody(cipher, 16, key);

    Check(std::memcmp(cipher, plain, 16) != 0, "cipher differs from plain");

    const bool ok = tnetlib::DecryptBody(cipher, 16, key, checksum);
    Check(ok, "decrypt succeeds");
    Check(std::memcmp(cipher, plain, 16) == 0, "decrypt restores plaintext");
}

void TestBodyRoundTrip_TailBytes()
{
    std::printf("[body round-trip — partial tail (5 bytes only)]\n");
    std::byte plain[5] = {
        static_cast<std::byte>(0xDE), static_cast<std::byte>(0xAD),
        static_cast<std::byte>(0xBE), static_cast<std::byte>(0xEF),
        static_cast<std::byte>(0x42)
    };
    std::byte cipher[5];
    std::memcpy(cipher, plain, 5);

    const std::int64_t key = tnetlib::KeyForSequence(2);
    const std::int64_t checksum = tnetlib::EncryptBody(cipher, 5, key);

    Check(std::memcmp(cipher, plain, 5) != 0, "5-byte tail: cipher differs");
    const bool ok = tnetlib::DecryptBody(cipher, 5, key, checksum);
    Check(ok, "5-byte tail: decrypt succeeds");
    Check(std::memcmp(cipher, plain, 5) == 0, "5-byte tail: plaintext restored");
}

void TestBodyRoundTrip_MixedSize()
{
    std::printf("[body round-trip — mixed (11 bytes = 1 chunk + 3 tail)]\n");
    std::byte plain[11];
    for (std::size_t i = 0; i < 11; ++i) plain[i] = static_cast<std::byte>(0xA0 + i);
    std::byte cipher[11];
    std::memcpy(cipher, plain, 11);

    const std::int64_t key = tnetlib::KeyForSequence(3);
    const std::int64_t checksum = tnetlib::EncryptBody(cipher, 11, key);

    const bool ok = tnetlib::DecryptBody(cipher, 11, key, checksum);
    Check(ok, "decrypt succeeds");
    Check(std::memcmp(cipher, plain, 11) == 0, "plaintext restored");
}

void TestBodyEmpty()
{
    std::printf("[empty body]\n");
    const std::int64_t key = tnetlib::KeyForSequence(0);
    const std::int64_t checksum = tnetlib::EncryptBody(nullptr, 0, key);
    Check(checksum == 0, "empty body produces zero checksum");
    Check(tnetlib::DecryptBody(nullptr, 0, key, 0), "empty body decrypt with 0 checksum ok");
    Check(!tnetlib::DecryptBody(nullptr, 0, key, 1), "empty body with wrong checksum rejected");
}

void TestBodyTamperedRejected()
{
    std::printf("[tampered body rejected by checksum]\n");
    std::byte plain[24];
    for (std::size_t i = 0; i < 24; ++i) plain[i] = static_cast<std::byte>(i);
    std::byte cipher[24];
    std::memcpy(cipher, plain, 24);

    const std::int64_t key = tnetlib::KeyForSequence(4);
    const std::int64_t checksum = tnetlib::EncryptBody(cipher, 24, key);

    // Flip one bit somewhere in the ciphertext.
    cipher[7] = cipher[7] ^ static_cast<std::byte>(0x01);

    const bool ok = tnetlib::DecryptBody(cipher, 24, key, checksum);
    Check(!ok, "DecryptBody returns false on tamper");
}

void TestHeaderRoundTrip()
{
    std::printf("[header round-trip]\n");
    tnetlib::PacketHeader h;
    h.wSize      = 64;
    h.wId        = 0x1988; // CS_LOGIN_REQ for fun
    h.dwNumber   = 42;
    h.llChecksum = 0x1234567890ABCDEFLL;

    tnetlib::PacketHeader before = h;
    const std::int64_t key = tnetlib::KeyForSequence(42);

    tnetlib::EncryptHeader(&h, key);
    Check(h.wSize == before.wSize, "wSize stays plaintext (framing requirement)");
    Check(h.wId != before.wId, "wId changes after encrypt");

    tnetlib::DecryptHeader(&h, key);
    Check(h.wSize == before.wSize, "wSize unchanged");
    Check(h.wId == before.wId, "wId restored");
    Check(h.dwNumber == before.dwNumber, "dwNumber restored");
    Check(h.llChecksum == before.llChecksum, "llChecksum restored");
}

void TestFullPacketRoundTrip()
{
    std::printf("[full packet (header + body) round-trip]\n");
    // Simulate a 32-byte packet: 16-byte header + 16-byte body.
    std::byte packet[32] = {};
    auto* hdr = reinterpret_cast<tnetlib::PacketHeader*>(packet);
    hdr->wSize    = 32;
    hdr->wId      = 0x4321;
    hdr->dwNumber = 7;
    // (llChecksum filled in by EncryptBody below)

    std::byte* body = packet + tnetlib::kPacketHeaderSize;
    for (std::size_t i = 0; i < 16; ++i) body[i] = static_cast<std::byte>('A' + i);

    std::byte plain_body[16];
    std::memcpy(plain_body, body, 16);

    const std::int64_t key = tnetlib::KeyForSequence(7);

    // Send path: encrypt body → store checksum → encrypt header
    hdr->llChecksum = tnetlib::EncryptBody(body, 16, key);
    tnetlib::EncryptHeader(hdr, key);

    // Recv path: decrypt header → check seq → decrypt body
    tnetlib::DecryptHeader(hdr, key);
    Check(hdr->dwNumber == 7, "dwNumber matches expected sequence post-decrypt");
    const bool ok = tnetlib::DecryptBody(body, 16, key, hdr->llChecksum);
    Check(ok, "DecryptBody succeeds with the stored checksum");
    Check(std::memcmp(body, plain_body, 16) == 0, "body matches original plaintext");
}

void TestFullPacketTamperedHeaderRejected()
{
    std::printf("[tampered header rejected via body-checksum mismatch]\n");
    std::byte packet[32] = {};
    auto* hdr = reinterpret_cast<tnetlib::PacketHeader*>(packet);
    hdr->wSize    = 32;
    hdr->wId      = 0xBEEF;
    hdr->dwNumber = 5;

    std::byte* body = packet + tnetlib::kPacketHeaderSize;
    for (std::size_t i = 0; i < 16; ++i) body[i] = static_cast<std::byte>(0x20 + i);

    const std::int64_t key = tnetlib::KeyForSequence(5);
    hdr->llChecksum = tnetlib::EncryptBody(body, 16, key);
    tnetlib::EncryptHeader(hdr, key);

    // Adversary flips a wId bit in transit.
    packet[2] = packet[2] ^ static_cast<std::byte>(0x01);

    tnetlib::DecryptHeader(hdr, key);
    // wId came out wrong; subsequent body decrypt uses the same key
    // (key was already chosen at send time from the sequence number
    // both sides know), so body XOR still works — but the rolling
    // checksum was computed against a header whose llChecksum slot
    // got affected by the wId bit-flip. Header llChecksum is now
    // garbled, so DecryptBody comparison fails.
    const bool ok = tnetlib::DecryptBody(body, 16, key, hdr->llChecksum);
    Check(!ok, "tampered header is caught — body checksum mismatches");
}

} // namespace

int main()
{
    std::printf("=== tnetlib packet_codec tests ===\n");
    TestKeyTable();
    TestBodyRoundTrip_AlignedSize();
    TestBodyRoundTrip_TailBytes();
    TestBodyRoundTrip_MixedSize();
    TestBodyEmpty();
    TestBodyTamperedRejected();
    TestHeaderRoundTrip();
    TestFullPacketRoundTrip();
    TestFullPacketTamperedHeaderRejected();
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
