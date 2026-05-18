#pragma once

// ISmtpClient — outbound mail abstraction. Legacy
// `Server/TLoginSvr/CSmtp.cpp` linked jwsmtp directly into the
// process; the modern shape pushes the transport behind an interface
// so:
//   * tests + dev mode get a log-only implementation (the security
//     code lands in stderr, no SMTP server needed)
//   * production deploys can pin a real SMTP client implementation
//     (Postfix/SES/SendGrid) without touching handler code
//   * the rate-limiting / queueing concerns live in the impl, not in
//     the auth flow
//
// The interface is synchronous on purpose — the auth handler decides
// whether to ship the mail in-line or hand it off to a worker thread.

#include <string>

namespace fourstory::smtp {

class ISmtpClient
{
public:
    virtual ~ISmtpClient() = default;

    // Send `body` to `to_address` with the given subject. Returns true
    // when the message was at least handed to the underlying
    // transport without an immediate error. False = delivery refused
    // synchronously (DNS / SMTP rejection).
    virtual bool Send(const std::string& to_address,
                      const std::string& subject,
                      const std::string& body) = 0;
};

} // namespace fourstory::smtp
