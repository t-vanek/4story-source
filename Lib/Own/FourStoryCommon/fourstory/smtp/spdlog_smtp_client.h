#pragma once

// Unconfigured-relay fallback: logs a fixed warning without message contents
// and returns false. It must not make a login challenge appear delivered.

#include "fourstory/smtp/smtp_client.h"

namespace fourstory::smtp {

class SpdlogSmtpClient : public ISmtpClient
{
public:
    bool Send(const std::string& to_address,
              const std::string& subject,
              const std::string& body) override;
};

} // namespace fourstory::smtp
