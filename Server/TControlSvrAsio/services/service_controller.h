#pragma once

// IServiceController — abstracts the Win32 Service Control Manager
// calls that legacy CTControlSvrModule::StartService / StopService /
// QueryStatus open against `pPriAddr.front()` (= \\machine). Three
// production impls are planned:
//
//   - DisabledServiceController (F1, default-on) — returns
//     NotSupported for Start/Stop and Unknown for QueryStatus. Safe
//     default; the operator GUI shows "unknown" status and the
//     start/stop buttons no-op. F1 wires only this implementation.
//
//   - WindowsScmServiceController (F2, opt-in) — replicates the
//     legacy OpenSCManager/StartService/ControlService/
//     QueryServiceStatus chain. Stays Win32-only.
//
//   - SystemdServiceController (post-F6, opt-in) — Linux production:
//     systemctl over ssh, or org.freedesktop.systemd1 over D-Bus.
//
// Status enum maps the SCM SERVICE_STATUS::dwCurrentState values
// onto a platform-neutral surface. The wire response shape
// (CT_SERVICESTAT_ACK) carries the raw DWORD, so the legacy
// constants are kept in sync intentionally.

#include <boost/asio/awaitable.hpp>

#include <cstdint>

namespace tcontrolsvr {

struct ServiceInstance;  // forward — service_inventory.h

enum class ServiceStatus : std::uint32_t
{
    Unknown        = 0,
    Stopped        = 1,   // SERVICE_STOPPED
    StartPending   = 2,   // SERVICE_START_PENDING
    StopPending    = 3,   // SERVICE_STOP_PENDING
    Running        = 4,   // SERVICE_RUNNING
    ContinuePending = 5,  // SERVICE_CONTINUE_PENDING
    PausePending   = 6,   // SERVICE_PAUSE_PENDING
    Paused         = 7,   // SERVICE_PAUSED
    NotInstalled   = 0xFF
};

enum class ControlResult
{
    Ok,
    Failed,
    NotSupported,    // default impl returns this — operator GUI shows it as a no-op
};

// Human-readable name for a ServiceStatus value. Used by the admin
// shell + the registry-event formatter; promoted here so both
// callers share the same enum→string mapping and can't drift.
const char* ServiceStatusName(ServiceStatus s);

class IServiceController
{
public:
    virtual ~IServiceController() = default;

    virtual boost::asio::awaitable<ServiceStatus>
        QueryStatus(const ServiceInstance& svc) = 0;

    virtual boost::asio::awaitable<ControlResult>
        Start(const ServiceInstance& svc) = 0;

    virtual boost::asio::awaitable<ControlResult>
        Stop(const ServiceInstance& svc) = 0;
};

} // namespace tcontrolsvr
