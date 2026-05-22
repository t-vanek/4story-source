#pragma once

// Handler dispatch for TWorldSvrAsio.
//
// W1 only logs + drops every inbound packet — no handler is wired
// yet. The scaffold exists so W2+ can land per-family files
// (handlers_char.cpp, handlers_guild.cpp, …) without touching this
// header's shape.
//
// HandlerContext mirrors the TControlSvrAsio pattern: non-owning
// pointers handed to every handler so test fakes can substitute
// individual services without rebuilding the whole context. W1
// holds only the io_context — every subsequent phase adds fields.

#include "../world_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace tworldsvr {

struct HandlerContext
{
    boost::asio::io_context*  io      = nullptr;

    // Worker pool for synchronous SOCI calls. Wired in W2 with the
    // [database] section. nullptr in W1 — the dispatch stub doesn't
    // touch the DB.
    boost::asio::thread_pool* db_pool = nullptr;

    // W2 fields land here:
    //   ICharRepository*   char_repo  = nullptr;
    //   CharRegistry*      chars      = nullptr;
    //
    // W3 fields:
    //   IGuildRepository*  guild_repo = nullptr;
    //   GuildRegistry*     guilds     = nullptr;
    //   PartyRegistry*     parties    = nullptr;
    //   CorpsRegistry*     corps      = nullptr;
    //
    // … see Server/TWorldSvrAsio/README.md for the full phasing.
};

namespace handlers {

// Top-level dispatch. W1 logs the packet ID + body size + (when
// recognised) the human-readable MessageId name from tnetlib, then
// drops the packet. Connection stays open so the legacy framer can
// continue sending packets — a single unknown ID must NOT tear down
// the peer link.
boost::asio::awaitable<void> Dispatch(
    std::shared_ptr<WorldSession> sess,
    std::uint16_t                 wId,
    std::vector<std::byte>        body,
    const HandlerContext&         ctx);

} // namespace handlers
} // namespace tworldsvr
