#pragma once

// Nation-aware character name validator. Mirrors the per-locale rules
// from legacy CSHandler.cpp::CheckCharName (1010-1066 in the shipped
// build) so a JP/KR/TW/RU client can submit a name in its own script
// without the modernized server rejecting it.
//
// Length is 3..16 bytes regardless of locale (legacy MAX_NAME / 2 lower
// bound). The byte-range tables below are byte-level filters — the
// legacy server walks raw bytes too because the wire never carries an
// encoding annotation. The rules deliberately reject NUL, control, and
// punctuation across all locales.

#include "nation.h"

#include <string_view>

namespace tloginsvr::services {

// Returns true if `name` is a syntactically valid character name for
// the given deployment nation. Byte-level rules:
//   * US      — ASCII a-z A-Z 0-9 only
//   * Germany — ASCII alnum + the four Latin-1 codepoints ä ö ü ß
//               (single-byte 0xE4 0xF6 0xFC 0xDF). Won't accept UTF-8
//               multi-byte forms; legacy clients send Latin-1.
//   * Taiwan  — ASCII alnum + Big5 (lead byte 0x81-0xFE,
//               trail byte 0x40-0x7E or 0xA1-0xFE)
//   * Japan   — ASCII alnum + Shift-JIS (lead 0x81-0x9F or 0xE0-0xFC,
//               trail 0x40-0x7E or 0x80-0xFC)
//   * Korea   — ASCII alnum + EUC-KR / CP949 (lead 0x81-0xFE,
//               trail 0x41-0xFE)
//   * Russia  — ASCII alnum + CP1251 Cyrillic block (0xC0-0xFF
//               single-byte)
//
// The double-byte locales accept legitimate Big5 / SJIS / CP949 pairs
// but also pass through "ASCII letters mixed with multi-byte chars" —
// matches the legacy server which doesn't tokenize.
bool IsValidCharName(std::string_view name, Nation nation);

} // namespace tloginsvr::services
