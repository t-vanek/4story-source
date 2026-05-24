#include "senders/senders.h"

#include "MessageId.h"

namespace tworldsvr::senders {

namespace {
boost::asio::awaitable<void>
Forward(std::shared_ptr<PeerSession> peer, tnetlib::protocol::MessageId id,
        const std::vector<std::byte>& body)
{
    std::vector<std::byte> copy(body);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(id), std::move(copy));
}
} // namespace

boost::asio::awaitable<void>
SendMwCreateRecallMonReq(std::shared_ptr<PeerSession>  peer,
                         const std::vector<std::byte>& body)
{
    co_await Forward(std::move(peer),
        tnetlib::protocol::MessageId::MW_CREATERECALLMON_REQ, body);
}

boost::asio::awaitable<void>
SendMwRecallMonDataReq(std::shared_ptr<PeerSession>  peer,
                       const std::vector<std::byte>& body)
{
    co_await Forward(std::move(peer),
        tnetlib::protocol::MessageId::MW_RECALLMONDATA_REQ, body);
}

boost::asio::awaitable<void>
SendMwRecallMonDelReq(std::shared_ptr<PeerSession>  peer,
                      const std::vector<std::byte>& body)
{
    co_await Forward(std::move(peer),
        tnetlib::protocol::MessageId::MW_RECALLMONDEL_REQ, body);
}

boost::asio::awaitable<void>
SendMwCreateSpolecnikMonReq(std::shared_ptr<PeerSession>  peer,
                            const std::vector<std::byte>& body)
{
    co_await Forward(std::move(peer),
        tnetlib::protocol::MessageId::MW_CREATESPOLECNIKMON_REQ, body);
}

boost::asio::awaitable<void>
SendMwSpolecnikMonDelReq(std::shared_ptr<PeerSession>  peer,
                         const std::vector<std::byte>& body)
{
    co_await Forward(std::move(peer),
        tnetlib::protocol::MessageId::MW_SPOLECNIKMONDEL_REQ, body);
}

} // namespace tworldsvr::senders
