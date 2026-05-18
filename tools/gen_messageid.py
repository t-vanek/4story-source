#!/usr/bin/env python3
"""Generate Lib/Own/TProtocol/include/MessageId.h
   + Lib/Own/TNetLib/TNetLib/MessageId.cpp
   from _rewrite/docs/packet-ids.csv.

Re-run after editing the CSV (which is itself extracted from the
legacy CSProtocol.h / MWProtocol.h / etc. via the PowerShell script
in _rewrite/docs/extract-packet-ids.ps1).

Usage:
    python3 tools/gen_messageid.py

Writes both files in place. No external dependencies — only stdlib.
"""

import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, '_rewrite', 'docs', 'packet-ids.csv')
HEADER_PATH = os.path.join(ROOT, 'Lib', 'Own', 'TProtocol', 'include', 'MessageId.h')
SOURCE_PATH = os.path.join(ROOT, 'Lib', 'Own', 'TNetLib', 'TNetLib', 'MessageId.cpp')


def load_rows():
    with open(CSV_PATH, encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        return list(reader)


def gen_header(rows):
    groups = {}
    for r in rows:
        groups.setdefault(r['Base'], []).append(r)

    out = ['''#pragma once

// AUTO-GENERATED — do NOT hand-edit. Regenerate via:
//     python3 tools/gen_messageid.py
//
// Source of truth: _rewrite/docs/packet-ids.csv (1542 IDs extracted
// from the legacy CSProtocol.h / MWProtocol.h / CTProtocol.h /
// DMProtocol.h / SSProtocol.h / ProtocolBase.h).
//
// Modernization context: the legacy protocol headers declare message
// IDs as `#define` macros, which means every dispatcher switch
// statement and every packet construction loses type safety. New
// AsioSession-based handlers should use this strongly-typed enum
// class instead. The numeric values are byte-identical to the macros,
// so legacy and modern code interoperate freely.
//
//     // legacy switch
//     case CS_LOGIN_REQ: ...
//
//     // modern switch
//     case tnetlib::protocol::MessageId::CS_LOGIN_REQ: ...
//
//     // bridging at handler entry
//     auto id = tnetlib::protocol::ToMessageId(packet.GetID());
//
// Legacy ID collisions: the original macros have a handful of cases
// where two different names map to the same hex value (e.g. on
// 0x9208, both MW_ADDBRTEAMS_ACK and MW_BRTEAMMATEDEL_REQ exist).
// Both enumerators are preserved here (legal in C++; two names for
// one value), but `NameOf` returns whichever name occurred first in
// the CSV — see MessageId.cpp for the canonical list.

#include <cstdint>
#include <string_view>

namespace tnetlib::protocol {

enum class MessageId : std::uint16_t {''']

    for base in sorted(groups.keys()):
        out.append(f'\n    // === {base} ===')
        seen = set()
        for r in sorted(groups[base], key=lambda x: int(x['IdDec'])):
            name = r['Name']
            if name in seen:
                continue
            seen.add(name)
            out.append(f"    {name} = {r['Id']},  // {r['File']}:{r['Line']}")

    out.append('''
};

// Cast helpers for round-tripping with the legacy 16-bit raw IDs.
constexpr MessageId ToMessageId(std::uint16_t raw) noexcept
{
    return static_cast<MessageId>(raw);
}

constexpr std::uint16_t ToUint16(MessageId id) noexcept
{
    return static_cast<std::uint16_t>(id);
}

// Diagnostic name lookup. Returns empty view if the ID isn't one we
// know about. Defined out-of-line in MessageId.cpp to keep this
// header free of the 1500-row table.
std::string_view NameOf(MessageId id) noexcept;

} // namespace tnetlib::protocol
''')
    return '\n'.join(out)


def gen_source(rows):
    out = ['''// AUTO-GENERATED alongside MessageId.h — do NOT hand-edit.
// Regenerate via: python3 tools/gen_messageid.py
//
// Defines the diagnostic name-lookup table for MessageId. Lives
// out-of-line so the 1500-row switch doesn't end up in every
// translation unit that includes the header.
//
// Legacy duplicates: the original protocol macros include a handful
// of IDs where two different message names map to the same hex value.
// Those collisions are preserved in the enum declaration (both
// enumerators exist with the same value — legal in C++), but the
// switch below can only return one name per value, so we return
// whichever name appeared first in packet-ids.csv. Each skip is
// marked with a "// collision" comment.

#include "MessageId.h"

namespace tnetlib::protocol {

std::string_view NameOf(MessageId id) noexcept
{
    switch (id)
    {''']

    seen_values = set()
    seen_names = set()
    for r in sorted(rows, key=lambda x: int(x['IdDec'])):
        name = r['Name']
        value = int(r['IdDec'])
        if name in seen_names:
            continue
        seen_names.add(name)
        if value in seen_values:
            out.append(f"        // collision (same value as earlier): {name} = {r['Id']}")
            continue
        seen_values.add(value)
        out.append(f"        case MessageId::{name}: return \"{name}\";")

    out.append('''        default: break;
    }
    return {};
}

} // namespace tnetlib::protocol
''')
    return '\n'.join(out)


def main():
    rows = load_rows()
    print(f"Loaded {len(rows)} rows from {CSV_PATH}", file=sys.stderr)
    with open(HEADER_PATH, 'w') as f:
        f.write(gen_header(rows))
    print(f"Wrote {HEADER_PATH}", file=sys.stderr)
    with open(SOURCE_PATH, 'w') as f:
        f.write(gen_source(rows))
    print(f"Wrote {SOURCE_PATH}", file=sys.stderr)


if __name__ == '__main__':
    main()
