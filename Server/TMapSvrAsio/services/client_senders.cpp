#include "services/client_senders.h"

#include "wire_codec.h"

namespace tmapsvr {

std::vector<std::byte> EncodeAddConnectAck(
    const std::vector<ConnectRoute>& routes)
{
    std::vector<std::byte> b;
    b.reserve(1 + routes.size() * 7);
    wire::WritePOD<std::uint8_t>(b, static_cast<std::uint8_t>(routes.size()));
    for (const auto& r : routes)
    {
        wire::WritePOD<std::uint32_t>(b, r.ip_addr);
        wire::WritePOD<std::uint16_t>(b, r.port);
        wire::WritePOD<std::uint8_t> (b, r.server_id);
    }
    return b;
}

} // namespace tmapsvr
