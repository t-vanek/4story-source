#pragma once

// UdpAuditLogger — wire-compatible audit log shim for the legacy
// TLogSvr UDP collector. Replicates the byte layout of
// Server/TLoginSvr/UdpSocket.cpp::SendToLogPacket: a fixed-size
// _UDPPACKET frame wrapping a _LOG_DATA_ struct, fire-and-forget over
// UDP to the configured collector endpoint.
//
// Decorator pattern: instances composed atop any other IAuditLogger
// emit the event to both — keeps the structured-log path (Seq, Loki,
// stdout) alive alongside back-compat with TLogSvr ingestion.
//
// Failure mode: UDP send errors are swallowed (datagram protocol +
// fire-and-forget audit). Logged at debug level so operators can spot
// a misconfigured endpoint without flooding stderr.

#include "audit_logger.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <memory>
#include <mutex>
#include <string>

namespace tloginsvr::services {

class UdpAuditLogger : public IAuditLogger
{
public:
    // `endpoint_host` + `endpoint_port` resolve to a UDP target. The
    // socket is opened lazily on first send so construction never
    // throws on DNS failure (collector down at startup is recoverable
    // — events fall back to the inner sink).
    //
    // `inner` is a downstream IAuditLogger that gets the same events
    // routed to it after the UDP send (decorator). May be null —
    // the UDP logger then runs alone.
    UdpAuditLogger(boost::asio::io_context& io,
                   std::string endpoint_host,
                   std::uint16_t endpoint_port,
                   std::unique_ptr<IAuditLogger> inner);

    void LogLogin(LoginOutcome outcome,
                  const std::string& user_id_str,
                  std::int32_t db_user_id,
                  const std::string& client_ip,
                  std::uint32_t session_key) override;

    void LogCharCreate(CreateCharOutcome outcome,
                       std::int32_t db_user_id,
                       std::uint8_t group_id,
                       const std::string& char_name,
                       std::int32_t db_char_id) override;

    void LogCharDelete(DeleteCharOutcome outcome,
                       std::int32_t db_user_id,
                       std::uint8_t group_id,
                       std::int32_t db_char_id) override;

    void LogGameStart(bool success,
                      std::int32_t db_user_id,
                      std::uint8_t group_id,
                      std::uint8_t channel,
                      std::int32_t db_char_id,
                      std::uint8_t map_server_id) override;

private:
    void SendBuffer(const void* data, std::size_t bytes);

    boost::asio::io_context&        m_io;
    std::string                     m_endpoint_host;
    std::uint16_t                   m_endpoint_port;
    std::unique_ptr<IAuditLogger>   m_inner;

    std::mutex                                 m_send_mtx;
    std::unique_ptr<boost::asio::ip::udp::socket> m_socket;
    boost::asio::ip::udp::endpoint             m_endpoint;
    bool                                       m_resolve_failed = false;
    std::uint32_t                              m_seq = 0;
};

} // namespace tloginsvr::services
