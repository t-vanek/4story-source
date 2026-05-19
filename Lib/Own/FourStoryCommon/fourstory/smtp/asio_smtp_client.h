#pragma once

// AsioSmtpClient — production-ready ISmtpClient backed by a plain
// (non-TLS) SMTP TCP conversation driven by Boost.Asio's blocking API.
//
// Capabilities:
//   * EHLO with fallback to HELO
//   * Optional AUTH LOGIN (base64 username + password). Activated when
//     the server advertises AUTH and the caller provided credentials.
//   * MAIL FROM / RCPT TO / DATA with CRLF dot-stuffing
//   * QUIT on success and on every error path so the relay frees the
//     connection promptly
//
// Out of scope (intentionally):
//   * STARTTLS / SMTPS — the legacy login server's 2FA mail is a
//     low-volume one-way notification. Operators that need encryption
//     on the wire run a local Postfix submission relay on 127.0.0.1
//     that fronts to their real provider. The "real provider" leg
//     does TLS; the loopback hop doesn't need it. This avoids a hard
//     OpenSSL dependency at the wire layer.
//   * Connection pooling — each Send opens, sends one message, closes.
//     Fine for security-code traffic (one mail per new-device login);
//     higher-volume use cases should layer a queue in front.
//
// Thread safety: the Send method is synchronous + reentrant; multiple
// threads can call Send concurrently, each gets its own TCP connection.

#include "fourstory/smtp/smtp_client.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace fourstory::smtp {

struct AsioSmtpConfig
{
    // Relay hostname or dotted IP. Resolved on each Send. Required.
    std::string  host;

    // Submission / SMTP port. 25 is the historical default, 587 the
    // modern submission port. Required.
    std::uint16_t port = 25;

    // The MAIL FROM envelope sender. Should be a real, monitored
    // mailbox — bounce notifications land here.
    std::string  from_address;

    // The "From:" header. Defaults to `from_address` if empty.
    std::string  from_display;

    // AUTH LOGIN credentials. Both empty → AUTH is skipped (relay must
    // accept anonymous submission from the server IP). Both non-empty →
    // AUTH LOGIN is offered if the relay advertises it; mandatory if
    // the relay refuses without auth.
    std::string  username;
    std::string  password;

    // Use EHLO first; fall back to HELO if the relay rejects EHLO with
    // a 5xx. Default true — every relay built in the last 25 years
    // speaks ESMTP, but the fallback is cheap insurance.
    bool         prefer_ehlo = true;

    // Per-step timeout. Applied to connect + each request/response
    // exchange. Default 15s — generous for a healthy relay, hostile to
    // a dead one (caller's auth flow shouldn't hang on a flaky network).
    std::chrono::seconds io_timeout{ 15 };
};

class AsioSmtpClient : public ISmtpClient
{
public:
    explicit AsioSmtpClient(AsioSmtpConfig config);

    bool Send(const std::string& to_address,
              const std::string& subject,
              const std::string& body) override;

private:
    AsioSmtpConfig m_config;
};

} // namespace fourstory::smtp
