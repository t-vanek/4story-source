#include "senders/senders.h"
#include "wire_codec.h"

#include "MessageId.h"

namespace tworldsvr::senders {

boost::asio::awaitable<void>
SendMwCashItemSaleReq(std::shared_ptr<PeerSession>      peer,
                      std::uint32_t                     dw_index,
                      std::uint16_t                     value,
                      const std::vector<TCashItemSale>& items)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, dw_index);
    WritePOD<std::uint16_t>(body, value);
    WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(items.size()));
    for (const auto& it : items)
    {
        WritePOD<std::uint16_t>(body, it.id);
        WritePOD<std::uint8_t>(body, it.sale_value);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASHITEMSALE_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCashShopStopReq(std::shared_ptr<PeerSession>   peer,
                      std::uint8_t                   type,
                      std::uint8_t                   send_player)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, type);
    WritePOD<std::uint8_t>(body, send_player);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CASHSHOPSTOP_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCmGiftResultReq(std::shared_ptr<PeerSession>   peer,
                      std::uint8_t                   result,
                      std::uint32_t                  gm_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, gm_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CMGIFTRESULT_REQ),
        std::move(body));
}

} // namespace tworldsvr::senders
