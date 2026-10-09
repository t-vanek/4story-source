#include "service_controller.h"

namespace tcontrolsvr {

const char* ServiceStatusName(ServiceStatus s)
{
    switch (s)
    {
        case ServiceStatus::Stopped:          return "stopped";
        case ServiceStatus::StartPending:     return "start-pending";
        case ServiceStatus::StopPending:      return "stop-pending";
        case ServiceStatus::Running:          return "running";
        case ServiceStatus::ContinuePending:  return "continue-pending";
        case ServiceStatus::PausePending:     return "pause-pending";
        case ServiceStatus::Paused:           return "paused";
        case ServiceStatus::NotInstalled:     return "not-installed";
        case ServiceStatus::Unknown:
        default:                              return "unknown";
    }
}

} // namespace tcontrolsvr
