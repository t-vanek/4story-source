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

boost::asio::awaitable<void>
SendCtCmGiftAck(std::shared_ptr<PeerSession>   peer,
                std::uint8_t                   result,
                std::uint32_t                  gm_id)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint8_t>(body, result);
    WritePOD<std::uint32_t>(body, gm_id);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_CMGIFT_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendCtCashItemSaleAck(std::shared_ptr<PeerSession>   peer,
                      std::uint32_t                  dw_index,
                      std::uint16_t                  value)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, dw_index);
    WritePOD<std::uint16_t>(body, value);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_CASHITEMSALE_ACK),
        std::move(body));
}

boost::asio::awaitable<void>
SendMwCmGiftReq(std::shared_ptr<PeerSession>   peer,
                std::uint32_t                  target_char_id,
                const CmGift&                  gift,
                std::uint32_t                  gm_id,
                std::uint8_t                   tool,
                std::uint16_t                  requested_gift_id,
                std::uint8_t                   result)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, target_char_id);
    WritePOD<std::uint8_t>(body, gift.gift_type);
    WritePOD<std::uint32_t>(body, gift.value);
    WritePOD<std::uint8_t>(body, gift.count);
    WriteString(body, gift.title);
    WriteString(body, gift.msg);
    WritePOD<std::uint32_t>(body, gm_id);
    WritePOD<std::uint8_t>(body, tool);
    WritePOD<std::uint16_t>(body, requested_gift_id);
    WritePOD<std::uint16_t>(body, gift.gift_id);
    WritePOD<std::uint8_t>(body, result);
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CMGIFT_REQ),
        std::move(body));
}

boost::asio::awaitable<void>
SendCtCmGiftListAck(std::shared_ptr<PeerSession>   peer,
                    std::uint32_t                  manager,
                    const std::vector<CmGift>&     gifts)
{
    using namespace wire;
    std::vector<std::byte> body;
    WritePOD<std::uint32_t>(body, manager);
    WritePOD<std::uint16_t>(body,
        static_cast<std::uint16_t>(gifts.size()));
    for (const auto& g : gifts)
    {
        WritePOD<std::uint16_t>(body, g.gift_id);
        WritePOD<std::uint8_t>(body, g.gift_type);
        WritePOD<std::uint32_t>(body, g.value);
        WritePOD<std::uint8_t>(body, g.count);
        WritePOD<std::uint8_t>(body, g.take_type);
        WritePOD<std::uint8_t>(body, g.max_take_count);
        WritePOD<std::uint8_t>(body, g.tool_only);
        WritePOD<std::uint16_t>(body, g.err_gift_id);
        WriteString(body, g.title);
        WriteString(body, g.msg);
    }
    co_await peer->Wire()->SendPacket(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_CMGIFTLIST_ACK),
        std::move(body));
}

} // namespace tworldsvr::senders
