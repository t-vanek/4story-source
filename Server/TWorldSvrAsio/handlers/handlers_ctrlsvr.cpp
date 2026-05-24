#include "handlers.h"

#include <spdlog/spdlog.h>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnCtCtrlsvrReq(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.ctrl_svr)
    {
        spdlog::warn("OnCtCtrlsvrReq[{}]: ctrl_svr slot not wired",
            ip);
        co_return;
    }

    if (!body.empty())
        spdlog::debug("OnCtCtrlsvrReq[{}]: ignored unexpected body "
                      "({} bytes)", ip, body.size());

    ctx.ctrl_svr->Set(peer);
    spdlog::info("OnCtCtrlsvrReq[{}]: registered as cluster ctrl-svr "
                 "(wID={})", ip, peer->Wid());
    co_return;
}

} // namespace tworldsvr::handlers
