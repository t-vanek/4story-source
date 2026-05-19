// Unit coverage for the nation-aware character name validator added
// in the parity pass with legacy CSHandler.cpp::CheckCharName.

#include "services/charname_validator.h"
#include "nation.h"

#include <cassert>
#include <cstdio>
#include <string>

using tloginsvr::Nation;
using tloginsvr::services::IsValidCharName;

namespace {

int failures = 0;

#define EXPECT(cond, msg) do {                                  \
    if (!(cond)) {                                              \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\n",              \
            (msg), __FILE__, __LINE__);                         \
        ++failures;                                             \
    }                                                           \
} while (0)

void TestLengthBounds()
{
    EXPECT(!IsValidCharName("ab",            Nation::US), "len<3 rejected");
    EXPECT( IsValidCharName("abc",           Nation::US), "len=3 accepted");
    EXPECT( IsValidCharName("Aaaaaaaaaaaaaaaa", Nation::US), "len=16 accepted");
    EXPECT(!IsValidCharName("Aaaaaaaaaaaaaaaab",Nation::US), "len=17 rejected");
}

void TestUS()
{
    EXPECT( IsValidCharName("Alice42",  Nation::US), "ascii alnum");
    EXPECT(!IsValidCharName("Alice 42", Nation::US), "space rejected");
    EXPECT(!IsValidCharName("Alice-1",  Nation::US), "punct rejected");
    EXPECT(!IsValidCharName(std::string("\xC3\xA4kki"), Nation::US),
        "utf-8 multibyte rejected on US");
}

void TestGermany()
{
    // Latin-1 single-byte umlauts. Build as raw bytes so we don't
    // accidentally smuggle UTF-8 in via the source encoding.
    const char umlauts[] = { 'M', char(0xE4), 'd', 'c', 'h', 'e', 'n', '\0' };
    EXPECT( IsValidCharName(umlauts,    Nation::Germany), "Latin-1 ä accepted");
    EXPECT(!IsValidCharName(umlauts,    Nation::US),      "Latin-1 ä rejected on US");
    const char eszett[] = { 'S', 't', 'r', 'a', char(0xDF), 'e', '\0' };
    EXPECT( IsValidCharName(eszett,     Nation::Germany), "Latin-1 ß accepted");
    const char unknown_hi[] = { 'A', char(0xA9), 'b', '\0' };
    EXPECT(!IsValidCharName(unknown_hi, Nation::Germany), "non-umlaut high byte rejected");
}

void TestJapan()
{
    // Shift-JIS for ねこ (cat) — 82 CB 82 B1. Both bytes in 0x40-0xFC
    // trail range; lead bytes in 0x81-0x9F.
    const char neko[] = { char(0x82), char(0xCB), char(0x82), char(0xB1), 'a', '\0' };
    EXPECT( IsValidCharName(neko, Nation::Japan),  "SJIS ねこa accepted");
    EXPECT(!IsValidCharName(neko, Nation::US),     "SJIS rejected on US");
    // Dangling lead byte (no trail) → reject.
    const char dangling[] = { 'A', char(0x82), '\0' };
    EXPECT(!IsValidCharName(dangling, Nation::Japan), "dangling SJIS lead rejected");
}

void TestKorea()
{
    // EUC-KR/CP949 for 가 = B0 A1. Both bytes in valid lead+trail.
    const char ka[] = { char(0xB0), char(0xA1), 'X', 'Y', '\0' };
    EXPECT( IsValidCharName(ka, Nation::Korea), "EUC-KR 가XY accepted");
    EXPECT(!IsValidCharName(ka, Nation::Japan), "EUC-KR rejected on JP (B0 isn't SJIS lead)");
}

void TestTaiwan()
{
    // Big5 for 中 = A4 A4. Lead 0x81-0xFE, trail 0xA1-0xFE.
    const char zhong[] = { char(0xA4), char(0xA4), 'a', '1', '\0' };
    EXPECT( IsValidCharName(zhong, Nation::Taiwan), "Big5 中a1 accepted");
    // Trail in 0x40-0x7E range — also valid in Big5.
    const char low_trail[] = { char(0xA4), char(0x40), 'b', '\0' };
    EXPECT( IsValidCharName(low_trail, Nation::Taiwan), "Big5 low-trail accepted");
    const char bad_trail[] = { char(0xA4), char(0x7F), 'b', '\0' };
    EXPECT(!IsValidCharName(bad_trail, Nation::Taiwan), "Big5 trail in DEL gap rejected");
}

void TestRussia()
{
    // CP1251 cyrillic А (0xC0) Л (0xCB) И (0xC8) С (0xD1) А (0xC0).
    const char alisa[] = { char(0xC0), char(0xCB), char(0xC8),
                           char(0xD1), char(0xC0), '\0' };
    EXPECT( IsValidCharName(alisa, Nation::Russia), "CP1251 АЛИСА accepted");
    // 0xA0 isn't in the Cyrillic block — only Ё (0xA8/0xB8) is allowed
    // below 0xC0; anything else high-bit should reject.
    const char weird[] = { 'X', char(0xA0), 'Y', '\0' };
    EXPECT(!IsValidCharName(weird, Nation::Russia), "non-Cyrillic high byte rejected");
    const char yo[] = { 'X', char(0xA8), 'Y', '\0' };
    EXPECT( IsValidCharName(yo, Nation::Russia), "Cyrillic Ё (0xA8) accepted");
}

} // namespace

int main()
{
    TestLengthBounds();
    TestUS();
    TestGermany();
    TestJapan();
    TestKorea();
    TestTaiwan();
    TestRussia();

    if (failures == 0)
    {
        std::printf("charname_validator: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "charname_validator: %d check(s) failed\n", failures);
    return 1;
}
