#include "senders/senders.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwPostRecvReq(std::shared_ptr<PeerSession>  peer,
                  const std::vector<std::byte>& body)
{
    std::vector<std::byte> copy(body);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_POSTRECV_REQ),
        std::move(copy));
}

} // namespace tworldsvr::senders
