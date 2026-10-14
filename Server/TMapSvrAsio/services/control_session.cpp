#include "control_session.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <spdlog/spdlog.h>
#include <cstring>
#include <utility>

namespace tmapsvr {
namespace {
#pragma pack(push,1)
struct PlainHeader {std::uint16_t size,id;std::uint32_t number;std::int64_t checksum;};
#pragma pack(pop)
static_assert(sizeof(PlainHeader)==16);
constexpr std::size_t kMaxPacket=0xFFFF;
}
struct ControlSession::SendState {
    using Permit=boost::asio::experimental::channel<void(boost::system::error_code,int)>;
    explicit SendState(boost::asio::any_io_executor executor):permit(executor,1){permit.try_send(boost::system::error_code{},0);}
    Permit permit;
    std::size_t frames{},bytes{};
};
ControlSession::ControlSession(boost::asio::ip::tcp::socket socket):m_socket(std::move(socket)),
    m_send(std::make_shared<SendState>(m_socket.get_executor())) {
    boost::system::error_code ec;const auto ep=m_socket.remote_endpoint(ec);
    if(!ec&&ep.address().is_v4())m_remote=ep.address().to_v4().to_string();
}
bool ControlSession::IsOpen() const {return m_socket.is_open();}
void ControlSession::Close() {
    m_send->permit.close();boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both,ec);m_socket.close(ec);
}
boost::asio::awaitable<void> ControlSession::Run(Handler handler) {
    struct Cleanup {std::shared_ptr<ControlSession> session;~Cleanup(){session->Close();}} cleanup{shared_from_this()};
    while(IsOpen()) {
        PlainHeader header{};boost::system::error_code ec;
        co_await boost::asio::async_read(m_socket,boost::asio::buffer(&header,sizeof(header)),
            boost::asio::redirect_error(boost::asio::use_awaitable,ec));
        if(ec)co_return;
        // Source CPacket::Clear supplies zero reserved fields. The modern
        // Control endpoint explicitly accepts only that evidenced profile.
        if(header.size<sizeof(header)||header.size>=kMaxPacket||header.number||header.checksum) {
            spdlog::warn("Map Control rejected non-Plain16 frame");co_return;
        }
        std::vector<std::byte> body(header.size-sizeof(header));
        if(!body.empty())co_await boost::asio::async_read(m_socket,boost::asio::buffer(body),
            boost::asio::redirect_error(boost::asio::use_awaitable,ec));
        if(ec)co_return;
        co_await handler(header.id,std::move(body));
    }
}
boost::asio::awaitable<void> ControlSession::SendPacket(std::uint16_t id,std::vector<std::byte> body) {
    const auto self=shared_from_this();if(!IsOpen())co_return;
    if(body.size()>=kMaxPacket-sizeof(PlainHeader)) {spdlog::warn("Map Control output exceeds source frame cap");Close();co_return;}
    const auto bytes=sizeof(PlainHeader)+body.size();const auto state=m_send;
    if(state->frames>=256||bytes>4U*1024U*1024U-state->bytes) {spdlog::warn("Map Control write backlog exceeded");Close();co_return;}
    ++state->frames;state->bytes+=bytes;
    struct Reservation {std::shared_ptr<SendState> state;std::size_t bytes;~Reservation(){--state->frames;state->bytes-=bytes;}} reservation{state,bytes};
    boost::system::error_code ec;
    co_await state->permit.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable,ec));
    if(ec||!IsOpen())co_return;
    struct Release {std::shared_ptr<SendState> state;~Release(){state->permit.try_send(boost::system::error_code{},0);}} release{state};
    std::vector<std::byte> frame(bytes);PlainHeader header{static_cast<std::uint16_t>(bytes),id,0,0};
    std::memcpy(frame.data(),&header,sizeof(header));
    if(!body.empty())std::memcpy(frame.data()+sizeof(header),body.data(),body.size());
    co_await boost::asio::async_write(m_socket,boost::asio::buffer(frame),boost::asio::redirect_error(boost::asio::use_awaitable,ec));
    if(ec)Close();
}
}
