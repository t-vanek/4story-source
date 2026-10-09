#include "fourstory/smtp/spdlog_smtp_client.h"
#include <spdlog/spdlog.h>

namespace fourstory::smtp {
bool SpdlogSmtpClient::Send(const std::string&, const std::string&, const std::string&)
{
    // A missing relay cannot deliver a challenge. Never log its secret body.
    spdlog::warn("smtp: message not delivered; no relay configured");
    return false;
}
} // namespace fourstory::smtp
