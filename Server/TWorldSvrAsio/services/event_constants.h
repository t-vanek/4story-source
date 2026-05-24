#pragma once

// Event-type enum values from NetCode.h:2427 (`enum EVENT_TYPE`).
// Only the ones we route on are surfaced here; the full table is in
// the legacy header (EVENT_EXPADD=1 .. EVENT_GIFTTIME=15, EVENT_COUNT=16).

#include <cstdint>

namespace tworldsvr::event_type {

// EVENT_LOTTERY (NetCode.h:2442) — randomized item raffle. Legacy
// runs `LotteryItem` on the world server (random char pick + in-game
// mail via SendPost + MW_EVENTMSGLOTTERY_REQ fan-out). Deferred —
// the helper depends on Mail/post infrastructure we haven't ported.
constexpr std::uint8_t kLottery   = 14;

// EVENT_GIFTTIME (NetCode.h:2443) — timed gift drop, narrowed by
// level range (HIBYTE/LOBYTE of m_wValue). Legacy runs `GiftTime`
// on the world server, same SendPost dependency as kLottery.
constexpr std::uint8_t kGiftTime  = 15;

// EVENT_COUNT (NetCode.h:2444) — one past the highest event id.
// Inbound CT_EVENTUPDATE_REQ with event_id > kCount is dropped
// (legacy SSHandler.cpp:276 `if(bEventID > EVENT_COUNT) return`).
constexpr std::uint8_t kCount     = 16;

} // namespace tworldsvr::event_type
