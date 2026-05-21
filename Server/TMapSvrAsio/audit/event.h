#pragma once

// Audit event catalog — typed POD events emitted by handlers and
// services for operational visibility. Each event has:
//   - a stable EventKind tag (wire-level discriminator)
//   - a fixed binary layout (no strings, no variable-length fields
//     in this initial set — keeps the UDP send / decode trivial)
//   - millisecond wall-clock timestamp + monotonically increasing
//     correlation id assigned at the entry point
//
// Why fixed POD: the audit transport is UDP fire-and-forget; small
// packets that don't fragment ship reliably even under load. JSON
// or text serialization is a luxury we don't need at this layer —
// the spdlog mirror gives human-readable text, the UDP send gives
// machine-aggregatable structured data.
//
// New event types: declare a struct here, bump kind enum, add a
// dedicated EmitXxx on IAuditLog. Don't reuse kinds.

#include <cstdint>

namespace tmapsvr::audit {

enum class EventKind : std::uint16_t
{
    Unknown        = 0,
    LoginAttempt   = 1,   // CS_CONNECT_REQ outcome
    CharLoad       = 2,   // DM_LOADCHAR_REQ outcome
    HandlerInvoke  = 3,   // every dispatch call (success or fail)
    HandlerError   = 4,   // dispatch call threw or returned with error
};

const char* EventKindName(EventKind k);

// Wire-level header — every audit event packet starts here.
//   kind  : EventKind tag
//   size  : total bytes of this event including header
//   ts_ms : wall-clock timestamp, ms since epoch
//   corr  : monotonic correlation id (assigned at handler entry)
struct EventHeader
{
    std::uint64_t  ts_ms = 0;   // wall-clock ms since epoch — first to avoid padding
    std::uint32_t  corr  = 0;   // monotonic correlation id
    std::uint16_t  kind  = 0;   // EventKind tag
    std::uint16_t  size  = 0;   // total bytes of this event including header
};
static_assert(sizeof(EventHeader) == 16,
              "EventHeader layout drift would break decoders");

// CS_CONNECT_REQ outcome — handshake result + identifying claims.
struct LoginAttemptEvent
{
    EventHeader    hdr;
    std::uint32_t  user_id    = 0;
    std::uint32_t  key        = 0;
    std::uint32_t  char_id    = 0;
    std::uint8_t   channel    = 0;
    std::uint8_t   result     = 0;   // ConnectResult value (0 = OK)
    std::uint16_t  version    = 0;
};

// DM_LOADCHAR_REQ outcome — char load result + latency.
struct CharLoadEvent
{
    EventHeader    hdr;
    std::uint32_t  char_id    = 0;
    std::uint32_t  key        = 0;
    std::uint32_t  user_id    = 0;
    std::uint32_t  latency_us = 0;
    std::uint8_t   result     = 0;   // CN_* style: 0 OK, 3 INTERNAL, 6 NOCHAR
};

// Every handler dispatch — useful for per-message-id traffic stats
// and for spotting handlers that suddenly start failing.
struct HandlerInvokeEvent
{
    EventHeader    hdr;
    std::uint16_t  wId        = 0;
    std::uint16_t  _pad       = 0;
    std::uint32_t  body_size  = 0;
    std::uint32_t  latency_us = 0;
    std::uint8_t   ok         = 1;   // 1 = handler returned normally, 0 = exception
};

} // namespace tmapsvr::audit
