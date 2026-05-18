#pragma once

#include <boost/asio/awaitable.hpp>

#include "asio_session.h"

#include <cstddef>
#include <span>
#include <vector>

namespace tloginsvr::handlers {

// CS_LOGIN_REQ → CS_LOGIN_ACK. Phase-3 stub: validates wire format,
// returns a hardcoded success response with placeholder fields. Real
// authentication, DB lookup, session registry, and duplicate-kick
// logic are deferred to follow-up commits.
//
// Wire layout from CSHandler.cpp::OnCS_LOGIN_REQ:
//   WORD   wVersion
//   STRING Zombie3
//   STRING strPasswd
//   STRING Zombie1
//   STRING Zombie2
//   STRING strUserID
//   INT64  dlCheck
//   INT64  llChecksum_recv
//   [BYTE  bChanneling  — JP nation only]
//
// This stub only reads wVersion and replies. Everything else is
// ignored. That's enough to demonstrate the codec + dispatch
// pipeline end-to-end without needing the auth DB plumbed in.
boost::asio::awaitable<void> OnLoginReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

} // namespace tloginsvr::handlers
