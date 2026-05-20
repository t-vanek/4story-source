// Per-nation character-name validator implementation.
//
// One CheckXxx() predicate per locale — each accepts a string_view and
// returns true iff the bytes are legal in that locale's character set.
// The dispatch in IsValidCharName routes by Nation enum, with US
// (ASCII-only) as the safe fallback.
//
// Double-byte locales (Taiwan / Japan / Korea) use the shared
// CheckDoubleByte helper plus a pair of lead/trail predicates per
// locale (Big5, Shift-JIS, EUC-KR ranges) — matches the legacy
// per-byte checks in CSHandler.cpp:1010-1066's CheckCharName.
//
// Legacy parity: Server/TLoginSvr/CSHandler.cpp::CheckCharName.

#include "charname_validator.h"

#include <cstddef>

namespace tloginsvr::services {

namespace {

constexpr std::size_t kMinLen = 3;
constexpr std::size_t kMaxLen = 16;

bool IsAsciiAlnum(unsigned char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9');
}

// Generic walker for double-byte locales. Iterates the buffer; ASCII
// alphanumeric is always allowed. When a high-bit byte appears the
// caller's lead/trail predicates decide whether the next two bytes
// form a valid pair. Anything else fails.
template <typename LeadPred, typename TrailPred>
bool CheckDoubleByte(std::string_view s, LeadPred is_lead, TrailPred is_trail)
{
    for (std::size_t i = 0; i < s.size(); )
    {
        const auto c = static_cast<unsigned char>(s[i]);
        if (IsAsciiAlnum(c))
        {
            ++i;
            continue;
        }
        if (!is_lead(c)) return false;
        if (i + 1 >= s.size()) return false;
        const auto t = static_cast<unsigned char>(s[i + 1]);
        if (!is_trail(t)) return false;
        i += 2;
    }
    return true;
}

bool CheckUS(std::string_view s)
{
    for (char c : s)
        if (!IsAsciiAlnum(static_cast<unsigned char>(c))) return false;
    return true;
}

bool CheckGermany(std::string_view s)
{
    for (char c : s)
    {
        const auto u = static_cast<unsigned char>(c);
        if (IsAsciiAlnum(u)) continue;
        // Latin-1 ä ö ü ß (lowercase) + Ä Ö Ü (uppercase). ß has no
        // uppercase counterpart in the legacy ANSI codepage.
        const bool german_diacritic =
            u == 0xE4 || u == 0xF6 || u == 0xFC || u == 0xDF
         || u == 0xC4 || u == 0xD6 || u == 0xDC;
        if (!german_diacritic) return false;
    }
    return true;
}

bool CheckTaiwan(std::string_view s)
{
    // Big5: lead 0x81-0xFE, trail 0x40-0x7E or 0xA1-0xFE.
    return CheckDoubleByte(s,
        [](unsigned char c) { return c >= 0x81 && c <= 0xFE; },
        [](unsigned char c) {
            return (c >= 0x40 && c <= 0x7E) || (c >= 0xA1 && c <= 0xFE);
        });
}

bool CheckJapan(std::string_view s)
{
    // Shift-JIS: lead 0x81-0x9F or 0xE0-0xFC, trail 0x40-0x7E or 0x80-0xFC.
    return CheckDoubleByte(s,
        [](unsigned char c) {
            return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
        },
        [](unsigned char c) {
            return (c >= 0x40 && c <= 0x7E) || (c >= 0x80 && c <= 0xFC);
        });
}

bool CheckKorea(std::string_view s)
{
    // EUC-KR / CP949: lead 0x81-0xFE, trail 0x41-0xFE.
    return CheckDoubleByte(s,
        [](unsigned char c) { return c >= 0x81 && c <= 0xFE; },
        [](unsigned char c) { return c >= 0x41 && c <= 0xFE; });
}

bool CheckRussia(std::string_view s)
{
    // CP1251: Cyrillic alphabet is 0xC0-0xFF (single-byte). Lowercase
    // Russian "Ё" is 0xB8 and uppercase is 0xA8 — include them.
    for (char c : s)
    {
        const auto u = static_cast<unsigned char>(c);
        if (IsAsciiAlnum(u)) continue;
        if (u == 0xA8 || u == 0xB8) continue;
        if (u < 0xC0) return false;
        // 0xC0..0xFF is one block, no need for upper bound check
    }
    return true;
}

} // namespace

bool IsValidCharName(std::string_view name, Nation nation)
{
    if (name.size() < kMinLen || name.size() > kMaxLen) return false;

    switch (nation)
    {
    case Nation::US:      return CheckUS(name);
    case Nation::Germany: return CheckGermany(name);
    case Nation::Taiwan:  return CheckTaiwan(name);
    case Nation::Japan:   return CheckJapan(name);
    case Nation::Korea:   return CheckKorea(name);
    case Nation::Russia:  return CheckRussia(name);
    }
    return CheckUS(name);  // unknown nation → safest fallback
}

} // namespace tloginsvr::services
