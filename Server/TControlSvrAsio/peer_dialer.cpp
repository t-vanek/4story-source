#include "peer_dialer.h"

#include "MessageId.h"

#include "fourstory/security/hostname_match.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

namespace {

const Machine* FindMachine(const IServiceInventory& inv, std::uint8_t mid)
{
    for (const auto& m : inv.Machines())
        if (m.id == mid) return &m;
    return nullptr;
}

} // namespace

boost::asio::awaitable<PeerDialResult>
PeerDialer::Dial(const ServiceInstance& svc)
{
    PeerDialResult res{};
    const Machine* m = FindMachine(m_inventory, svc.machine_id);
    if (!m || m->private_addrs.empty())
    {
        res.failure_reason = "no private address for machine "
            + std::to_string(svc.machine_id);
        co_return res;
    }
    const std::string& host = m->private_addrs.front();

    using boost::asio::ip::tcp;
    tcp::resolver resolver(m_io);
    boost::system::error_code ec;
    auto endpoints = co_await resolver.async_resolve(
        host, std::to_string(svc.port),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        res.failure_reason = "resolve " + host + ":"
            + std::to_string(svc.port) + " — " + ec.message();
        co_return res;
    }

    tcp::socket sock(m_io);

    // Race async_connect against a steady_timer fired with the
    // configured timeout. Whichever wins cancels the other —
    // canceling the socket trips async_connect with operation_aborted
    // (we surface that as "timeout" so the operator GUI gets a clean
    // reason string instead of asio's internal code).
    boost::asio::steady_timer deadline(m_io);
    deadline.expires_after(m_timeout);
    bool timed_out = false;
    deadline.async_wait([&sock, &timed_out](auto err) {
        if (err) return;  // canceled by success path
        timed_out = true;
        boost::system::error_code ig;
        sock.close(ig);
    });

    co_await boost::asio::async_connect(sock, endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    deadline.cancel();

    if (ec || timed_out)
    {
        res.failure_reason = timed_out
            ? std::string("timeout after ")
                + std::to_string(m_timeout.count()) + "ms"
            : ec.message();
        spdlog::warn("peer_dialer: dial svc_id={:08x} ({}:{}) failed: {}",
            svc.service_id, host, svc.port, res.failure_reason);
        co_return res;
    }

    // Drive the client-side TLS handshake before publishing the
    // session. The remote ControlServer is configured for hybrid
    // detection (PR #46), so its accept loop will see our 0x16
    // ClientHello and route into the TLS code path.
    std::shared_ptr<ControlSession> wire;
    if (m_ssl_ctx != nullptr)
    {
        ControlSession::TlsStream stream(std::move(sock), *m_ssl_ctx);
        boost::system::error_code tls_ec;
        co_await stream.async_handshake(
            boost::asio::ssl::stream_base::client,
            boost::asio::redirect_error(
                boost::asio::use_awaitable, tls_ec));
        if (tls_ec)
        {
            res.failure_reason = "TLS handshake failed: " + tls_ec.message();
            spdlog::warn("peer_dialer: dial svc_id={:08x} ({}:{}) — {}",
                svc.service_id, host, svc.port, res.failure_reason);
            boost::system::error_code ig;
            stream.next_layer().close(ig);
            co_return res;
        }
        wire = std::make_shared<ControlSession>(std::move(stream));

        // Outbound identity enforcement. When the security gate is
        // wired (typical Phase A deployment), the cert's CN / SAN
        // entries must match the operator-configured peer_name for
        // the target (group, server). Mirror of the inbound check
        // in OnPeerRegisterReq — same trust map, same hostname-
        // match semantics (CN literal + SAN with RFC 6125 wildcard
        // expansion). Without an opinion in the trust map we just
        // log the captured CN and proceed; without a gate at all
        // we keep the pre-enforce behaviour.
        if (m_security != nullptr)
        {
            const auto expected = m_security->LookupPeerName(
                svc.group_id, svc.server_id);
            if (expected.has_value())
            {
                const auto& peer_cn   = wire->PeerCommonName();
                const auto& peer_sans = wire->PeerSubjectAltNames();
                const bool cn_matches =
                    fourstory::security::detail::EqualIgnoreCase(
                        peer_cn, *expected);
                bool san_matches = false;
                for (const auto& san : peer_sans)
                {
                    if (fourstory::security::HostnameMatch(san, *expected))
                    {
                        san_matches = true;
                        break;
                    }
                }
                if (!cn_matches && !san_matches)
                {
                    res.failure_reason =
                        "peer identity mismatch — cert CN='" + peer_cn +
                        "' SANs=" + std::to_string(peer_sans.size()) +
                        " expected='" + *expected + "'";
                    spdlog::warn("peer_dialer: dial svc_id={:08x} ({}:{}) — {}",
                        svc.service_id, host, svc.port, res.failure_reason);
                    boost::system::error_code ig;
                    wire->Close();
                    co_return res;
                }
            }
        }

        if (!wire->PeerCommonName().empty())
        {
            spdlog::info("peer_dialer: TLS peer CN='{}' on svc_id={:08x}",
                wire->PeerCommonName(), svc.service_id);
        }
    }
    else
    {
        wire = std::make_shared<ControlSession>(std::move(sock));
    }

    auto peer = std::make_shared<PeerSession>(wire, svc);
    m_registry.SetConnection(svc.service_id, peer);

    // Touch the runtime status so the timer code sees a fresh
    // last_recv_tick — otherwise the keep-alive loop would mark the
    // freshly-dialed peer offline immediately.
    if (auto* st = m_registry.Status(svc.service_id))
    {
        st->status         = ServiceStatus::Running;
        st->last_recv_tick = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    spdlog::info("peer_dialer: dialed svc_id={:08x} ({}:{}) — name='{}'",
        svc.service_id, host, svc.port, svc.name);
    res.session = std::move(peer);
    co_return res;
}

} // namespace tcontrolsvr
