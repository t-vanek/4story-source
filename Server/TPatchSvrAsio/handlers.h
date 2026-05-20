#pragma once

// TPatchSvrAsio handlers — 9 CT_* messages ported from
// Server/TPatchSvr/Handler.cpp. Server-server only (no client wire);
// peers are TControlSvr pushing patch updates + game servers querying
// for new files.

#include "patch_session.h"
#include "services/patch_repository.h"

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <string>

namespace boost::asio { class thread_pool; }

namespace tpatchsvr {
class PatchServer;
}

namespace tpatchsvr::handlers {

struct ServerContext
{
    PatchRepository*  repo = nullptr;
    std::string       ftp_url;
    std::string       pre_ftp_url;
    std::string       login_host;   // dotted IPv4 advertised to client
    std::uint16_t     login_port = 0;
    std::size_t       session_count = 0;  // for SERVICEMONITOR reply

    // Back-reference for the SERVICEMONITOR handler so it can run
    // legacy's stale-client sweep on each heartbeat (P-6).
    PatchServer*      server = nullptr;

    // Optional worker pool for off-loop SOCI calls. When non-null
    // the txn-heavy MarkPreVersionComplete + reads run on a worker
    // thread via fourstory::db::CoOffload; otherwise they execute
    // synchronously on the io_context (legacy path).
    boost::asio::thread_pool* db_pool = nullptr;
};

// CT_SERVICEMONITOR_ACK(INT64 pad + DWORD tick) →
// CT_SERVICEMONITOR_REQ(DWORD tick, DWORD session, DWORD user, DWORD active)
boost::asio::awaitable<void> OnServiceMonitor(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body,
    const ServerContext& ctx);

// CT_SERVICEDATACLEAR_ACK — no-op
boost::asio::awaitable<void> OnServiceDataClear(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body);

// CT_PATCH_REQ(DWORD dwVersion) → CT_PATCH_ACK
boost::asio::awaitable<void> OnPatch(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body,
    const ServerContext& ctx);

// CT_NEWPATCH_REQ(DWORD dwVersion) → CT_NEWPATCH_ACK
boost::asio::awaitable<void> OnNewPatch(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body,
    const ServerContext& ctx);

// CT_CHANGEIF_REQ(BYTE bOption) → CT_NEWPATCH_ACK (interface files)
boost::asio::awaitable<void> OnChangeInterface(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body,
    const ServerContext& ctx);

// CT_PREPATCH_REQ(DWORD dwBetaVer) → CT_PREPATCH_ACK
boost::asio::awaitable<void> OnPrePatch(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body,
    const ServerContext& ctx);

// CT_PATCHSTART_REQ — close session (returns EC_SESSION_EXIT in legacy)
boost::asio::awaitable<void> OnPatchStart(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body);

// CT_CTRLSVR_REQ — heartbeat, no-op
boost::asio::awaitable<void> OnCtrlSvr(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body);

// CT_PREPATCHCOMPLETE_REQ(DWORD dwBetaVer) → mark beta complete, close
boost::asio::awaitable<void> OnPrePatchComplete(
    std::shared_ptr<PatchSession> session,
    std::vector<std::byte> body,
    const ServerContext& ctx);

} // namespace tpatchsvr::handlers
