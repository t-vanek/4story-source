#include "handlers.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<WorldSession> sess,
         std::uint16_t                 wId,
         std::vector<std::byte>        body,
         const HandlerContext&         /*ctx*/)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;
    using tnetlib::protocol::NameOf;

    const auto id   = ToMessageId(wId);
    const auto name = NameOf(id);

    if (name.empty())
    {
        spdlog::warn("world_dispatch[{}]: unknown wID=0x{:04X} body={} bytes "
                     "— dropped (W1 scaffold)",
            sess->RemoteIPv4(), wId, body.size());
    }
    else
    {
        spdlog::info("world_dispatch[{}]: wID=0x{:04X} ({}) body={} bytes "
                     "— dropped (W1 scaffold)",
            sess->RemoteIPv4(), wId, name, body.size());
    }
    co_return;
}

} // namespace tworldsvr::handlers
