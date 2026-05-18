#pragma once

// SpdlogSmtpClient — log-only ISmtpClient. Writes the would-be email
// to spdlog instead of delivering it over SMTP. Suitable for:
//   * dev environments without a mail relay
//   * production deploys that ship the security-code flow via
//     out-of-band channels (push notifications, SMS) and just want
//     an audit trail of when codes were generated
//   * tests
//
// Real SMTP deployment plugs a different ISmtpClient impl in main —
// the interface stays the same.

#include "smtp_client.h"

namespace tloginsvr::services {

class SpdlogSmtpClient : public ISmtpClient
{
public:
    bool Send(const std::string& to_address,
              const std::string& subject,
              const std::string& body) override;
};

} // namespace tloginsvr::services
