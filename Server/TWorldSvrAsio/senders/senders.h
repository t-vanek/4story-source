#pragma once

// Outbound packet builders — the W3a-2 batch covers the senders
// that the W3a-1 and W3a-2 handlers need to reply with. Modelled
// on Server/TControlSvrAsio/senders.h: stateless free functions
// that take a PeerSession and the wire fields, build the body
// through wire::WritePOD / WriteString, and co_await
// PeerSession::Wire()->SendPacket(wID, body).
//
// Each phase appends here:
//   W3a-2 — SendRwRelaysvrAck, SendMwGuildEstablishReq.
//   W3a-3 — guild info / member-list / disorg / leave / kickout /
//           duty / fame / contribution / point-reward / pvp /
//           cabinet / article senders (~25 total).
//   W3b   — party + corps senders.
//   W4    — friend / chat / soulmate senders.
//   …
//
// The legacy TWorldSvr/SSSender.cpp is 4046 LOC across 196 sender
// functions; the family-file split lives under senders/senders_*.cpp.

#include "../peer_session.h"

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tworldsvr::senders {

// --- RW family ------------------------------------------------------

// RW_RELAYSVR_ACK — reply to RW_RELAYSVR_REQ. Tells the registering
// peer the cluster's nation flag, the current operator list, and
// the per-server motd table. The latter two are read-only snapshots
// the peer caches for the lifetime of its connection.
//
// Wire layout (RWSender.cpp:1):
//   BYTE bNation
//   WORD operator_count
//   DWORD operators[operator_count]
//   WORD msg_count
//   { STRING key, STRING value } × msg_count
boost::asio::awaitable<void> SendRwRelaysvrAck(
    std::shared_ptr<PeerSession>      peer,
    std::uint8_t                       nation,
    const std::vector<std::uint32_t>&  operators,
    const std::map<std::string, std::string>& svr_msgs);

// --- MW family (guild) ----------------------------------------------

// MW_GUILDESTABLISH_REQ — sent back to the originating map server
// after world ingests a DM_GUILDLOAD_ACK (or after a real guild-
// create flow lands in W3a-3). The map server forwards the result
// down to the client.
//
// Wire layout (SSSender.cpp:843):
//   DWORD dwCharID
//   DWORD dwKey
//   BYTE  bRet              -- GUILD_SUCCESS / failure codes
//   DWORD dwGuildID
//   STRING strName
//   BYTE  bEstablish        -- 1 = freshly created, 0 = re-confirm
boost::asio::awaitable<void> SendMwGuildEstablishReq(
    std::shared_ptr<PeerSession> peer,
    std::uint32_t                char_id,
    std::uint32_t                key,
    std::uint8_t                 result,
    std::uint32_t                guild_id,
    const std::string&           name,
    std::uint8_t                 establish);

// Result codes for SendMwGuildEstablishReq.bRet. Mirrors the
// legacy GUILD_* constants from NetCode.h. Only the values
// referenced by W3a-2 handlers are duplicated here; W3a-3 will
// pull the full enum once the matching handler ports demand them.
constexpr std::uint8_t kGuildSuccess = 0;

} // namespace tworldsvr::senders
