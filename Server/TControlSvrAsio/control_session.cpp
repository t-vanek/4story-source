#include "control_session.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <utility>

namespace tcontrolsvr {

namespace {
// Retained modern SS8 checksum, identical to TPatchSvrAsio's FoldChecksum:
// little-endian DWORDs running XOR; the unaligned tail is folded one
// byte at a time into the low byte of the accumulator. Source Plain16 skips it.
std::uint32_t FoldChecksum(const std::byte* p, std::size_t n)
{
    std::uint32_t acc = 0;
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4)
    {
        std::uint32_t w = 0;
        std::memcpy(&w, p + i, 4);
        acc ^= w;
    }
    for (; i < n; ++i)
        acc ^= static_cast<std::uint32_t>(p[i]);
    return acc;
}
} // namespace

std::uint32_t ComputeChecksum(const std::byte* body, std::size_t len)
{
    return FoldChecksum(body, len);
}

namespace {
// Extract identity material (CN + DNS-type SubjectAltNames) from an
// SSL session's peer certificate in one pass. Combining the two
// keeps a single SSL_get_peer_certificate / X509_free pair instead
// of two — relevant because the OpenSSL 3 variant explicitly
// up-refs the cert.
//
// Caller must have already driven async_handshake to completion.
struct PeerIdentity
{
    std::string              cn;
    std::vector<std::string> sans;
};

PeerIdentity ExtractPeerIdentity(SSL* ssl)
{
    PeerIdentity id;
    if (ssl == nullptr) return id;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    X509* cert = SSL_get1_peer_certificate(ssl);
#else
    X509* cert = SSL_get_peer_certificate(ssl);
#endif
    if (cert == nullptr) return id;

    // CN — legacy identity field, still populated by most CAs.
    X509_NAME* subj = X509_get_subject_name(cert);
    if (subj != nullptr)
    {
        char buf[256] = {0};
        const int len = X509_NAME_get_text_by_NID(subj, NID_commonName,
                                                   buf, sizeof(buf));
        if (len > 0)
            id.cn.assign(buf, static_cast<std::size_t>(len));
    }

    // SAN — RFC 5280 §4.2.1.6 modern identity. We only collect the
    // DNS-type entries; IP-type and URI-type are out of scope for
    // this project's identity model (peer_name is a string).
    auto* sans = static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
    if (sans != nullptr)
    {
        const int n = sk_GENERAL_NAME_num(sans);
        for (int i = 0; i < n; ++i)
        {
            const GENERAL_NAME* gn = sk_GENERAL_NAME_value(sans, i);
            if (gn == nullptr || gn->type != GEN_DNS) continue;
            ASN1_IA5STRING* dns = gn->d.dNSName;
            if (dns == nullptr) continue;
            const unsigned char* data = ASN1_STRING_get0_data(dns);
            const int len             = ASN1_STRING_length(dns);
            if (data == nullptr || len <= 0) continue;
            id.sans.emplace_back(reinterpret_cast<const char*>(data),
                                  static_cast<std::size_t>(len));
        }
        GENERAL_NAMES_free(sans);
    }

    X509_free(cert);
    return id;
}
} // namespace

struct ControlSession::SendState {
    using Permit=boost::asio::experimental::channel<void(boost::system::error_code,int)>;
    explicit SendState(boost::asio::any_io_executor executor):permit(executor,1) {
        permit.try_send(boost::system::error_code{},0);
    }
    Permit permit;
    std::size_t pending_frames{},pending_bytes{};
    static constexpr std::size_t max_frames=256,max_bytes=4*1024*1024;
};

ControlSession::ControlSession(PlainSocket sock,WireProfile profile)
    : m_socket(std::in_place_type<PlainSocket>, std::move(sock))
    , m_profile(profile)
    , m_send_state(std::make_shared<SendState>(UnderlyingTcp().get_executor()))
{
    boost::system::error_code ec;
    auto ep = UnderlyingTcp().remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
        m_remote_ipv4 = ep.address().to_v4().to_string();
}

ControlSession::ControlSession(TlsStream stream,WireProfile profile)
    : m_socket(std::in_place_type<TlsStream>, std::move(stream))
    , m_profile(profile)
    , m_send_state(std::make_shared<SendState>(UnderlyingTcp().get_executor()))
{
    boost::system::error_code ec;
    auto ep = UnderlyingTcp().remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
        m_remote_ipv4 = ep.address().to_v4().to_string();

    // Capture peer identity (CN + SAN entries) now — the ssl::stream
    // is fully handshaked by the time the caller hands it to this
    // ctor (ControlServer drives async_handshake then constructs us),
    // so SSL_get_peer_cert returns the presented cert.
    auto identity = ExtractPeerIdentity(
        std::get<TlsStream>(m_socket).native_handle());
    m_peer_cn   = std::move(identity.cn);
    m_peer_sans = std::move(identity.sans);
}

ControlSession::PlainSocket& ControlSession::UnderlyingTcp()
{
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
        return tls->next_layer();
    return std::get<PlainSocket>(m_socket);
}

const ControlSession::PlainSocket& ControlSession::UnderlyingTcp() const
{
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
        return tls->next_layer();
    return std::get<PlainSocket>(m_socket);
}

bool ControlSession::IsOpen() const
{
    return UnderlyingTcp().is_open();
}

void ControlSession::TouchRecv()
{
    m_last_recv_at.store(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

boost::asio::awaitable<void>
ControlSession::Run(PacketHandler on_packet)
{
    using namespace boost::asio;
    struct CloseOnExit {
        std::shared_ptr<ControlSession> owner;
        ~CloseOnExit(){owner->Close();}
    } close{shared_from_this()};

    // Dispatch a single async_read through whichever variant arm is
    // active. boost::asio::async_read accepts any AsyncReadStream;
    // the variant only exists so connect/close target the raw layer.
    auto read_into = [&](void* dst, std::size_t n,
                          boost::system::error_code& ec)
        -> boost::asio::awaitable<bool> {
        if (auto* tls = std::get_if<TlsStream>(&m_socket))
        {
            co_await async_read(*tls, buffer(dst, n),
                redirect_error(use_awaitable, ec));
        }
        else
        {
            co_await async_read(std::get<PlainSocket>(m_socket),
                buffer(dst, n),
                redirect_error(use_awaitable, ec));
        }
        co_return !ec;
    };

    while (UnderlyingTcp().is_open())
    {
        const auto header_size=m_profile==WireProfile::SourcePlain16?sizeof(SourcePacketHeader):sizeof(PacketHeader);
        std::vector<std::byte> header(header_size);
        boost::system::error_code ec;
        if (!co_await read_into(header.data(), header_size, ec)) break;
        std::uint16_t size=0,id=0;std::uint32_t checksum=0;
        if(m_profile==WireProfile::SourcePlain16) {
            SourcePacketHeader source{};std::memcpy(&source,header.data(),sizeof(source));
            // Source SESSION_SERVER ignores both fields. Copy() can retain a
            // client sequence, so these bytes cannot be validated as crypto.
            size=source.wSize;id=source.wID;
        } else {
            PacketHeader modern{};std::memcpy(&modern,header.data(),sizeof(modern));
            size=modern.wSize;id=modern.wID;checksum=modern.dwChkSum;
        }
        if (size < header_size || (m_profile==WireProfile::SourcePlain16&&size>=kMaxPacketSize))
        {
            spdlog::warn("control_session[{}]: framing error wSize={} — closing",
                m_remote_ipv4, size);
            break;
        }
        const std::size_t body_size = size - header_size;
        std::vector<std::byte> body(body_size);
        if (body_size > 0)
        {
            if (!co_await read_into(body.data(), body_size, ec)) break;
        }
        const std::uint32_t expected = m_profile==WireProfile::ModernSs8
            ?ComputeChecksum(body.data(), body_size):0;
        if (m_profile==WireProfile::ModernSs8&&expected != checksum)
        {
            spdlog::warn("control_session[{}]: checksum mismatch wID=0x{:04X} "
                         "got=0x{:08X} expected=0x{:08X} — closing",
                m_remote_ipv4, id, checksum, expected);
            break;
        }
        TouchRecv();
        DecodedPacket pkt{id, std::move(body)};
        co_await on_packet(shared_from_this(), std::move(pkt));
    }
}

boost::asio::awaitable<void>
ControlSession::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    const auto self=shared_from_this();
    if (!IsOpen()) co_return;
    const auto header_size=m_profile==WireProfile::SourcePlain16?sizeof(SourcePacketHeader):sizeof(PacketHeader);
    if (body.size() > kMaxPacketSize-header_size||
        (m_profile==WireProfile::SourcePlain16&&body.size()>=kMaxPacketSize-header_size))
    {
        spdlog::error("control_session[{}]: outbound body too big ({}); closing",
            m_remote_ipv4, body.size());
        Close();
        co_return;
    }
    const std::size_t total=header_size+body.size();
    const auto state=m_send_state;
    if(state->pending_frames>=SendState::max_frames||total>SendState::max_bytes-state->pending_bytes) {
        spdlog::warn("control_session[{}]: outbound backlog exceeded; closing",m_remote_ipv4);
        Close();co_return;
    }
    ++state->pending_frames;state->pending_bytes+=total;
    struct Reservation {
        std::shared_ptr<SendState> state;std::size_t bytes;
        ~Reservation(){--state->pending_frames;state->pending_bytes-=bytes;}
    } reservation{state,total};
    boost::system::error_code ec;
    co_await state->permit.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable,ec));
    if(ec||!IsOpen())co_return;
    struct Release {
        std::shared_ptr<SendState> state;
        ~Release(){state->permit.try_send(boost::system::error_code{},0);}
    } release{state};
    // The frame is owned by this suspended invocation. Serializing only the
    // callbacks would not protect either a shared buffer or async_write itself.
    std::vector<std::byte> frame(total);
    if(m_profile==WireProfile::SourcePlain16) {
        SourcePacketHeader header{};header.wSize=static_cast<std::uint16_t>(total);header.wID=wId;
        std::memcpy(frame.data(),&header,sizeof(header));
    } else {
        PacketHeader header{};header.wSize=static_cast<std::uint16_t>(total);header.wID=wId;
        header.dwChkSum=ComputeChecksum(body.data(),body.size());
        std::memcpy(frame.data(),&header,sizeof(header));
    }
    if (!body.empty())
        std::memcpy(frame.data() + header_size,
                    body.data(), body.size());
    // Dispatch the write through the active variant arm.
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
    {
        co_await boost::asio::async_write(*tls,
            boost::asio::buffer(frame.data(), total),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    else
    {
        co_await boost::asio::async_write(
            std::get<PlainSocket>(m_socket),
            boost::asio::buffer(frame.data(), total),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    if (ec) {
        spdlog::debug("control_session[{}]: send wID=0x{:04X} failed: {}",
            m_remote_ipv4, wId, ec.message());
        Close();
    }
}

void ControlSession::Close()
{
    m_send_state->permit.close();
    boost::system::error_code ec;
    auto& sock = UnderlyingTcp();
    sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    sock.close(ec);
}

} // namespace tcontrolsvr
