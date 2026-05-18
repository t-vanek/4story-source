#include "spdlog_smtp_client.h"

#include <spdlog/spdlog.h>

namespace tloginsvr::services {

bool SpdlogSmtpClient::Send(const std::string& to_address,
                            const std::string& subject,
                            const std::string& body)
{
    // Strip CR/LF from logged body so multi-line emails don't
    // fragment the log record. The body still gets emitted in one
    // line; operators can grep + reconstruct if they need it.
    std::string flat = body;
    for (auto& c : flat) if (c == '\r' || c == '\n') c = ' ';
    spdlog::info("smtp.Send to='{}' subject='{}' body='{}'",
        to_address, subject, flat);
    return true;
}

} // namespace tloginsvr::services
