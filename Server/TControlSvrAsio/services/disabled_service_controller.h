#pragma once

// DisabledServiceController — default-on IServiceController for
// deployments that don't grant TControlSvr remote-daemon control.
// Wire-level CT_SERVICECONTROL_REQ still resolves (returns
// ACK_FAILED) and CT_SERVICESTAT_ACK reports Unknown; operators see
// the buttons but they no-op rather than firing the legacy SCM
// calls. The Windows-SCM and systemd impls live elsewhere and can
// be swapped in without changing the handler chain.

#include "service_controller.h"

namespace tcontrolsvr {

class DisabledServiceController final : public IServiceController
{
public:
    boost::asio::awaitable<ServiceStatus>
        QueryStatus(const ServiceInstance& /*svc*/) override
    {
        co_return ServiceStatus::Unknown;
    }

    boost::asio::awaitable<ControlResult>
        Start(const ServiceInstance& /*svc*/) override
    {
        co_return ControlResult::NotSupported;
    }

    boost::asio::awaitable<ControlResult>
        Stop(const ServiceInstance& /*svc*/) override
    {
        co_return ControlResult::NotSupported;
    }
};

} // namespace tcontrolsvr
