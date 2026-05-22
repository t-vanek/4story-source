#pragma once

// SweepExpiredWanted — periodic prune coroutine. Replaces the
// legacy `CheckEventExpired` SM_EVENTEXPIRED_ACK fan-out
// (TWorldSvr.cpp:5280) which scanned `m_vExpired` against
// `m_timeCurrent` once per BatchThread tick. The legacy module
// notified the TIMER service so it could fire SM_EVENTEXPIRED_REQ
// across the cluster; our SOCI-direct port doesn't have a
// separate TIMER service, so the sweep just prunes locally and
// persists the deletions.
//
// Wire this into `fourstory::ops::RegistryRefresher` via
// `AddCoroutineHook`; the refresher will co_await this every
// tick.

#include "services/guild_repository.h"
#include "services/guild_wanted_registry.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/thread_pool.hpp>

namespace tworldsvr {

// One sweep pass:
//   1. Sample now (std::time(nullptr)).
//   2. registry.PruneExpired(now) → list of removed guild_ids.
//   3. For each removed id, queue repo->DeleteWanted via
//      CoOffloadVoidIf so the slow SOCI call doesn't stall the
//      io_context.
//   4. Log a one-line summary (`pruned: N`).
//
// `repo` may be null — in that case the DB persistence step is
// skipped (the in-memory prune still happens). `db_pool` may
// also be null — falls through to inline execution (legacy
// parity with the W3a-4d CoOffloadVoidIf rule).
boost::asio::awaitable<void>
SweepExpiredWanted(GuildWantedRegistry&     reg,
                   IGuildRepository*        repo,
                   boost::asio::thread_pool* db_pool);

} // namespace tworldsvr
