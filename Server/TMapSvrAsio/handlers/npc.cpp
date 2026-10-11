#include "handlers.h"
#include "MessageId.h"

namespace tmapsvr {
boost::asio::awaitable<void> OnNpcTalkReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    // OnCS_NPCTALK_REQ updates running talk terms before NPCTALK_ACK. The
    // repository applies CanTalk and commits these updates before publication.
    co_await OnNativeQuestReq(std::move(client),
        static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_NPCTALK_REQ),std::move(body),ctx);
}
}
