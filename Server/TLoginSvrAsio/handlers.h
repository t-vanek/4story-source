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

// CS_GROUPLIST_REQ → CS_GROUPLIST_ACK. Phase-3 stub: returns an empty
// world-group list (BYTE bCount = 0 + BYTE bCheckFilePoint = 0).
// Real implementation queries TGROUP table for active worlds.
//
// Wire layout from CSHandler.cpp::OnCS_GROUPLIST_REQ:
//   request body: (none — empty)
//   ack body:  BYTE bCount, BYTE bCheckFilePoint, [per group: STRING szNAME,
//              BYTE bGroupID, BYTE bType, BYTE bStatus, BYTE bCharCount]
boost::asio::awaitable<void> OnGroupListReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_CHANNELLIST_REQ(BYTE bGroupID) → CS_CHANNELLIST_ACK. Phase-3 stub:
// returns empty channel list. Real impl queries TCHANNEL.
boost::asio::awaitable<void> OnChannelListReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

// CS_CHARLIST_REQ(BYTE bGroupID) → CS_CHARLIST_ACK. Phase-3 stub:
// returns empty char list. Real impl queries TCHARTABLE + TITEMTABLE.
boost::asio::awaitable<void> OnCharListReq(
    tnetlib::AsioSession& session,
    std::span<const std::byte> body);

} // namespace tloginsvr::handlers
