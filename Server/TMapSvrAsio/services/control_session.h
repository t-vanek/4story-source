#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tmapsvr {
// Original SESSION_SERVER / !m_bUseCrypt: raw sixteen-byte CPacket,
// no RC4, XOR, sequence or checksum transform. This is a distinct role from
// tnetlib::AsioSession's encrypted game-client protocol.
class ControlSession final : public std::enable_shared_from_this<ControlSession> {
public:
    using Handler=std::function<boost::asio::awaitable<void>(std::uint16_t,std::vector<std::byte>)>;
    explicit ControlSession(boost::asio::ip::tcp::socket);
    boost::asio::awaitable<void> Run(Handler);
    // Serialized complete writes, bounded to256 frames /4MiB. No reconnect
    // replay or application-delivery acknowledgment. Single Map I/O thread.
    boost::asio::awaitable<void> SendPacket(std::uint16_t,std::vector<std::byte>);
    void Close();
    bool IsOpen() const;
    const std::string& RemoteIPv4() const {return m_remote;}
private:
    struct SendState;
    boost::asio::ip::tcp::socket m_socket;
    std::shared_ptr<SendState> m_send;
    std::string m_remote;
};
}
