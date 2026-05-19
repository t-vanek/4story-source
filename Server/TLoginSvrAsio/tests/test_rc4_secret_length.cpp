// Regression for round-6 critical bug: default RC4 secret length
// must include the trailing NUL.
//
// Background: legacy Session.cpp:88-93 calls EncryptBuffer with
// `g_strSecretKey.size() + 1` — i.e. it hashes the secret bytes
// PLUS the NUL terminator. The shipped client mirrors this. MD5 of
// a 31-byte input produces a different digest than MD5 of those
// same 31 bytes plus a 00 byte, which means the derived RC4 key
// schedules diverge and every byte of every packet decrypts to
// garbage.
//
// LoadConfig("") synthesizes the default secret from
// kDefaultLegacySecret. This test verifies the in-memory secret
// length matches the literal's sizeof — i.e. the array's trailing
// NUL is present in the bytes the AsioSession will feed to
// RC4MD5Transform.

#include "../config.h"

#include <cstdio>
#include <cstring>

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

// Bytes of the legacy g_strSecretKey, including the embedded
// non-ASCII (\x92 / \x94) bytes and the trailing NUL — same
// literal as config.cpp::kDefaultLegacySecret.
constexpr unsigned char kExpectedSecret[] =
    "A5$$8AFS13A1::-11#!..'\x92" "19716AC&\x94" "/D1;;1#";
constexpr std::size_t   kExpectedLen = sizeof(kExpectedSecret);

void TestDefaultLengthIncludesTrailingNul()
{
    std::printf("[rc4_secret_length — default secret keeps the NUL]\n");

    const auto cfg = tloginsvr::LoadConfig("");
    Check(!cfg.server.rc4_secret_key.empty(), "rc4_secret_key not empty");
    Check(cfg.server.rc4_secret_key.size() == kExpectedLen,
        "secret length matches sizeof(literal) (NUL byte included)");

    // The last byte must be 0x00 — that's the trailing NUL legacy
    // Session.cpp explicitly relies on (CRITICAL warning comment
    // at line 18).
    if (!cfg.server.rc4_secret_key.empty())
    {
        Check(cfg.server.rc4_secret_key.back() == std::byte{0},
            "last byte is the trailing NUL");
    }

    // The full byte sequence must match the literal verbatim.
    bool match = (cfg.server.rc4_secret_key.size() == kExpectedLen);
    if (match)
    {
        match = std::memcmp(cfg.server.rc4_secret_key.data(),
                            kExpectedSecret, kExpectedLen) == 0;
    }
    Check(match, "secret bytes match the legacy literal");
}

} // namespace

int main()
{
    TestDefaultLengthIncludesTrailingNul();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
