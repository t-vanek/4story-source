#pragma once

// PeerTlsContext — translates SecurityConfig.peer_tls_* fields into a
// configured boost::asio::ssl::context, ready to hand to TlsAsioSession.
//
// One builder is shared between every peer-client (outbound) and
// peer-listener (inbound) so the cert/key/CA paths and TLS-version
// policy stay consistent across a single process. Each side calls
// either BuildServerContext() or BuildClientContext() depending on
// who initiated the TCP connection (acceptor → server, dialer →
// client).
//
// Lifetime: the returned ssl::context outlives every session built
// from it — the contained SSL_CTX must stay alive while at least one
// asio::ssl::stream references it. Typical caller pattern is to hold
// the context as a member of the cluster wiring object.
//
// Errors: any cert / key / CA load failure surfaces as a
// std::runtime_error with the offending file path in the message.
// SecurityConfig::Validate() is the right place to catch
// configuration mistakes before construction; this helper assumes
// it's been called and only re-checks the unconditional invariants
// (paths non-empty, version string recognized).

#include "fourstory/security/security_config.h"

#include <boost/asio/ssl/context.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace fourstory::security {

class PeerTlsContextBuilder
{
public:
    // Build the SSL_CTX shared between every peer session this
    // process initiates / accepts. Throws if peer_tls_enabled is
    // false (caller is expected to guard).
    static boost::asio::ssl::context
    BuildServerContext(const SecurityConfig& cfg)
    {
        return Build(cfg, /*server_side=*/true);
    }

    static boost::asio::ssl::context
    BuildClientContext(const SecurityConfig& cfg)
    {
        return Build(cfg, /*server_side=*/false);
    }

private:
    static boost::asio::ssl::context
    Build(const SecurityConfig& cfg, bool server_side)
    {
        if (!cfg.peer_tls_enabled)
            throw std::runtime_error(
                "PeerTlsContextBuilder: peer_tls_enabled is false");
        if (cfg.peer_tls_ca_cert.empty())
            throw std::runtime_error(
                "PeerTlsContextBuilder: peer_tls_ca_cert is empty");
        if (cfg.peer_tls_peer_cert.empty())
            throw std::runtime_error(
                "PeerTlsContextBuilder: peer_tls_peer_cert is empty");
        if (cfg.peer_tls_peer_key.empty())
            throw std::runtime_error(
                "PeerTlsContextBuilder: peer_tls_peer_key is empty");

        namespace ssl = boost::asio::ssl;

        // Method selection — fix one direction (server/client) and
        // also pin a minimum version. Asio's _server / _client method
        // families are version-agnostic; the SSL_CTX_set_min_proto_version
        // call below tightens the floor.
        ssl::context ctx(server_side
                            ? ssl::context::tls_server
                            : ssl::context::tls_client);

        // Common hardening — refuse legacy SSL versions outright.
        // tls_server / tls_client already disable SSLv2/SSLv3 in
        // recent Boost, but be explicit. single_dh_use is harmless
        // on the client side where there's no DH key to reuse.
        ctx.set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3 |
            ssl::context::single_dh_use);

        // Min version pin. SSL_CTX_set_min_proto_version takes a
        // version macro; the strings in SecurityConfig are the
        // operator-facing form. Defaults to 1.3 in SecurityConfig.
        const int min_ver = (cfg.peer_tls_min_version == "1.2")
            ? TLS1_2_VERSION : TLS1_3_VERSION;
        SSL_CTX_set_min_proto_version(ctx.native_handle(), min_ver);

        // Identity material. Loaded as files (not in-memory blobs)
        // because that's the operator-facing surface — config points
        // at on-disk PEM files maintained by the cert-rotation tool.
        try
        {
            ctx.use_certificate_chain_file(cfg.peer_tls_peer_cert);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(
                "PeerTlsContextBuilder: load peer cert '" +
                cfg.peer_tls_peer_cert + "' failed — " + ex.what());
        }
        try
        {
            ctx.use_private_key_file(cfg.peer_tls_peer_key,
                                      ssl::context::pem);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(
                "PeerTlsContextBuilder: load peer key '" +
                cfg.peer_tls_peer_key + "' failed — " + ex.what());
        }

        // Trust anchor. Verify the other side's cert against it.
        try
        {
            ctx.load_verify_file(cfg.peer_tls_ca_cert);
        }
        catch (const std::exception& ex)
        {
            throw std::runtime_error(
                "PeerTlsContextBuilder: load CA cert '" +
                cfg.peer_tls_ca_cert + "' failed — " + ex.what());
        }

        // Mutual TLS. Both sides demand a peer cert; verification
        // chains to the CA loaded above. fail_if_no_peer_cert is
        // server-only — clients always send their cert when
        // configured, and adding the flag on a client_context is a
        // no-op but Asio's API insists on consistency.
        auto mode = ssl::verify_peer;
        if (server_side)
            mode |= ssl::verify_fail_if_no_peer_cert;
        ctx.set_verify_mode(mode);

        return ctx;
    }
};

} // namespace fourstory::security
