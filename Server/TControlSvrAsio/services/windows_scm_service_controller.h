#pragma once

// Windows Service Control Manager backed IServiceController. Wraps
// the legacy CTControlSvrModule::StartService / StopService /
// QueryStatus calls in a coroutine-friendly interface.
//
// Legacy entry points open SCM on a remote machine via UNC path
// `\\machine\` and call ::CreateService / ::StartService /
// ::ControlService / ::QueryServiceStatus. The modern version
// preserves that contract but plumbs the result back through an
// awaitable so the dispatch loop can move on while the SCM call
// blocks (which it can — these calls are synchronous in the Win32
// API and historically take several seconds on cold machines).
//
// Linux builds compile this header but link against a stub
// implementation that returns NotSupported — the same surface as
// DisabledServiceController. The IFDEF gate keeps the
// SCM-specific includes out of the Linux translation unit.

#include "service_controller.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace tcontrolsvr {

class WindowsScmServiceController final : public IServiceController
{
public:
    struct Options
    {
        // Template grammar: {type_name}-{group}-{server}-{type}-{machine}
        // — see scm_name_resolver.h. Default mirrors the legacy
        // installer's `m_szServiceName` convention.
        std::string service_name_template = "{type_name}-{group}-{server}";

        // Per-service overrides for deploys where the SCM service
        // name doesn't follow the template (legacy hand-installed
        // services, etc.). Keyed by ServiceInstance::service_id.
        std::unordered_map<std::uint32_t, std::string> overrides;
    };

    explicit WindowsScmServiceController(Options opts = {});

    boost::asio::awaitable<ServiceStatus>
        QueryStatus(const ServiceInstance& svc) override;

    boost::asio::awaitable<ControlResult>
        Start(const ServiceInstance& svc) override;

    boost::asio::awaitable<ControlResult>
        Stop(const ServiceInstance& svc) override;

private:
    Options m_opts;
};

} // namespace tcontrolsvr
