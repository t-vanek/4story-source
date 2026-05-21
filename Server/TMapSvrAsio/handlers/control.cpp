// Control-protocol handlers — CT_* messages from TControlSvrAsio.
//
// All five are decode-stubs in this phase; the consolidation pass
// adds:
//   - peer-IP auth gate against the configured control server
//   - CT_ANNOUNCEMENT_ACK    → broadcast announce to all sessions
//                              via channel_presence
//   - CT_USERKICKOUT_ACK     → close target session via session_reg
//   - CT_SERVICEMONITOR_ACK  → periodic stale-client sweep
//   - CT_SERVICEDATACLEAR_ACK → flush in-memory caches
//   - CT_CTRLSVR_REQ         → handshake back to TControlSvrAsio

#include "handlers.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
OnCtAnnouncementAck(std::shared_ptr<tnetlib::AsioSession> sess,
                    std::vector<std::byte>                body,
                    const HandlerContext&                 ctx)
{
    (void)sess; (void)ctx;
    spdlog::info("CT_ANNOUNCEMENT_ACK body={} bytes — F17 stub "
                 "(channel broadcast lands with consolidation)",
        body.size());
    co_return;
}

boost::asio::awaitable<void>
OnCtUserKickoutAck(std::shared_ptr<tnetlib::AsioSession> sess,
                   std::vector<std::byte>                body,
                   const HandlerContext&                 ctx)
{
    (void)sess; (void)ctx;
    spdlog::info("CT_USERKICKOUT_ACK body={} bytes — F17 stub",
        body.size());
    co_return;
}

boost::asio::awaitable<void>
OnCtServiceMonitorAck(std::shared_ptr<tnetlib::AsioSession> sess,
                      std::vector<std::byte>                body,
                      const HandlerContext&                 ctx)
{
    (void)sess; (void)ctx;
    spdlog::info("CT_SERVICEMONITOR_ACK body={} bytes — F17 stub",
        body.size());
    co_return;
}

boost::asio::awaitable<void>
OnCtServiceDataClearAck(std::shared_ptr<tnetlib::AsioSession> sess,
                        std::vector<std::byte>                body,
                        const HandlerContext&                 ctx)
{
    (void)sess; (void)ctx;
    spdlog::info("CT_SERVICEDATACLEAR_ACK body={} bytes — F17 stub",
        body.size());
    co_return;
}

boost::asio::awaitable<void>
OnCtCtrlSvrReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    (void)sess; (void)ctx;
    spdlog::info("CT_CTRLSVR_REQ body={} bytes — F17 stub "
                 "(handshake-back to TControlSvrAsio lands with "
                 "consolidation)",
        body.size());
    co_return;
}

} // namespace tmapsvr
