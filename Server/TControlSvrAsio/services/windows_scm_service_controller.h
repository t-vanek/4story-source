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

#include <string>

namespace tcontrolsvr {

class WindowsScmServiceController final : public IServiceController
{
public:
    // `service_name_template` controls how a ServiceInstance is
    // mapped to its SCM display name. The token `{type}-{group}-{id}`
    // is replaced per-instance — e.g. a MapSvr in world 1 with
    // server id 3 becomes "MapSvr-1-3" by default, matching the
    // legacy installer's `m_szServiceName` convention.
    explicit WindowsScmServiceController(
        std::string service_name_template = "{type_name}-{group}-{server}");

    boost::asio::awaitable<ServiceStatus>
        QueryStatus(const ServiceInstance& svc) override;

    boost::asio::awaitable<ControlResult>
        Start(const ServiceInstance& svc) override;

    boost::asio::awaitable<ControlResult>
        Stop(const ServiceInstance& svc) override;

private:
    std::string m_template;
};

} // namespace tcontrolsvr
