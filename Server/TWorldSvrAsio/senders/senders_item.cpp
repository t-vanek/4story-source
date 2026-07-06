#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwDealItemErrorReq(std::shared_ptr<PeerSession> peer,
                       const std::string&           target,
                       const std::string&           error_char,
                       std::uint8_t                 error)
{
    using namespace wire;
    std::vector<std::byte> body;
    WriteString(body, target);
    WriteString(body, error_char);
    WritePOD<std::uint8_t>(body, error);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_DEALITEMERROR_REQ),
        std::move(body));
}

namespace {

// Shared payload for the W6-36 pair — legacy SSSender.cpp:3083/3092
// copy the DM_ITEMSTATE_ACK packet verbatim and only swap the wire id.
std::vector<std::byte>
BuildItemStateBody(std::uint32_t                       id,
                   const std::vector<ItemStateChange>& items)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, id);
    WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(items.size()));
    for (const auto& it : items)
    {
        WritePOD<std::uint16_t>(body, it.item_id);
        WritePOD<std::uint8_t>(body, it.init_state);
    }
    return body;
}

} // namespace

boost::asio::awaitable<void>
SendMwItemStateReq(std::shared_ptr<PeerSession>        peer,
                   std::uint32_t                       id,
                   const std::vector<ItemStateChange>& items)
{
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ITEMSTATE_REQ),
        BuildItemStateBody(id, items));
}

boost::asio::awaitable<void>
SendCtItemStateAck(std::shared_ptr<PeerSession>        peer,
                   std::uint32_t                       id,
                   const std::vector<ItemStateChange>& items)
{
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_ITEMSTATE_ACK),
        BuildItemStateBody(id, items));
}

boost::asio::awaitable<void>
SendCtItemFindAck(std::shared_ptr<PeerSession>       peer,
                  std::uint32_t                      manager_id,
                  const std::vector<ItemFindRow>&    rows)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(rows.size()));
    WritePOD<std::uint32_t>(body, manager_id);
    for (const auto& r : rows)
    {
        WritePOD<std::uint16_t>(body, r.item_id);
        WritePOD<std::uint8_t>(body, r.init_state);
        WriteString(body, r.name);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_ITEMFIND_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwAddItemReq(std::shared_ptr<PeerSession>       peer,
                 const std::vector<std::byte>&      body)
{
    std::vector<std::byte> copy = body;
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ADDITEM_REQ),
        std::move(copy));
}

} // namespace tworldsvr::senders
