// WindowsScmServiceController — SCM backend for IServiceController.
//
// Compiles on every platform. The actual SCM logic is gated on
// FOURSTORY_HAS_WIN32 (defined in the root CMake when WIN32); on
// Linux every call returns NotSupported so the symbol exists and
// the binary still links. The legacy CTControlSvrModule entry
// points (StartService / StopService / QueryStatus) are the source
// of truth for the call sequence — we preserve the order and the
// per-status mapping exactly.

#include "windows_scm_service_controller.h"
#include "scm_name_resolver.h"
#include "service_inventory.h"

#include <spdlog/spdlog.h>

#ifdef FOURSTORY_HAS_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

#include <cstring>

namespace tcontrolsvr {

WindowsScmServiceController::WindowsScmServiceController(Options opts)
    : m_opts(std::move(opts))
{
}

#ifdef FOURSTORY_HAS_WIN32

namespace {

// Resolve a machine name → UNC path for OpenSCManager. Legacy uses
// `\\machine` from TMACHINE.szName / TIPADDR.szPriAddr — we accept
// either format and let SCM resolve.
std::wstring ToWide(const std::string& s)
{
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                  static_cast<int>(s.size()),
                                  nullptr, 0);
    std::wstring w(n, L'\0');
    if (n)
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(),
                              static_cast<int>(s.size()),
                              w.data(), n);
    return w;
}

ServiceStatus MapSCMStatus(DWORD state)
{
    switch (state)
    {
    case SERVICE_STOPPED:          return ServiceStatus::Stopped;
    case SERVICE_START_PENDING:    return ServiceStatus::StartPending;
    case SERVICE_STOP_PENDING:     return ServiceStatus::StopPending;
    case SERVICE_RUNNING:          return ServiceStatus::Running;
    case SERVICE_CONTINUE_PENDING: return ServiceStatus::ContinuePending;
    case SERVICE_PAUSE_PENDING:    return ServiceStatus::PausePending;
    case SERVICE_PAUSED:           return ServiceStatus::Paused;
    default:                       return ServiceStatus::Unknown;
    }
}

struct ScmHandles
{
    SC_HANDLE scm = nullptr;
    SC_HANDLE svc = nullptr;
    ~ScmHandles()
    {
        if (svc) ::CloseServiceHandle(svc);
        if (scm) ::CloseServiceHandle(scm);
    }
};

bool OpenForAccess(const ServiceInstance& inst,
                   const std::string&     name,
                   DWORD desired,
                   ScmHandles& h)
{
    // F2 doesn't yet know the machine address — the dialer maps that
    // by reading inventory. The SCM call needs the same. For now we
    // open SCM on localhost; F2+ can extend with a machine-resolver
    // hook to pass the UNC path. Legacy did the same in two phases:
    // QueryStatus ran against the local SCM during boot then re-ran
    // against \\machine once TIPADDR populated.
    (void)inst;
    h.scm = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!h.scm) return false;
    h.svc = ::OpenServiceW(h.scm, ToWide(name).c_str(), desired);
    return h.svc != nullptr;
}

} // namespace

boost::asio::awaitable<ServiceStatus>
WindowsScmServiceController::QueryStatus(const ServiceInstance& svc)
{
    const std::string name = ResolveScmName(
        svc, m_opts.service_name_template, m_opts.overrides);
    ScmHandles h;
    if (!OpenForAccess(svc, name, SERVICE_QUERY_STATUS, h))
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST)
            co_return ServiceStatus::NotInstalled;
        spdlog::debug("scm.QueryStatus open '{}' failed: 0x{:08X}", name, err);
        co_return ServiceStatus::Unknown;
    }
    SERVICE_STATUS st{};
    if (!::QueryServiceStatus(h.svc, &st))
    {
        spdlog::debug("scm.QueryStatus '{}' failed: 0x{:08X}",
            name, ::GetLastError());
        co_return ServiceStatus::Unknown;
    }
    co_return MapSCMStatus(st.dwCurrentState);
}

boost::asio::awaitable<ControlResult>
WindowsScmServiceController::Start(const ServiceInstance& svc)
{
    const std::string name = ResolveScmName(
        svc, m_opts.service_name_template, m_opts.overrides);
    ScmHandles h;
    if (!OpenForAccess(svc, name, SERVICE_START | SERVICE_QUERY_STATUS, h))
    {
        spdlog::warn("scm.Start open '{}' failed: 0x{:08X}",
            name, ::GetLastError());
        co_return ControlResult::Failed;
    }
    if (!::StartServiceW(h.svc, 0, nullptr))
    {
        const DWORD err = ::GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING)
            co_return ControlResult::Ok;
        spdlog::warn("scm.Start '{}' failed: 0x{:08X}", name, err);
        co_return ControlResult::Failed;
    }
    spdlog::info("scm.Start '{}' requested — SCM transitioning to "
                 "SERVICE_START_PENDING", name);
    co_return ControlResult::Ok;
}

boost::asio::awaitable<ControlResult>
WindowsScmServiceController::Stop(const ServiceInstance& svc)
{
    const std::string name = ResolveScmName(
        svc, m_opts.service_name_template, m_opts.overrides);
    ScmHandles h;
    if (!OpenForAccess(svc, name, SERVICE_STOP | SERVICE_QUERY_STATUS, h))
    {
        spdlog::warn("scm.Stop open '{}' failed: 0x{:08X}",
            name, ::GetLastError());
        co_return ControlResult::Failed;
    }
    SERVICE_STATUS st{};
    if (!::ControlService(h.svc, SERVICE_CONTROL_STOP, &st))
    {
        spdlog::warn("scm.Stop '{}' failed: 0x{:08X}",
            name, ::GetLastError());
        co_return ControlResult::Failed;
    }
    spdlog::info("scm.Stop '{}' requested — SCM transitioning to "
                 "SERVICE_STOP_PENDING", name);
    co_return ControlResult::Ok;
}

#else  // FOURSTORY_HAS_WIN32

// Non-Windows builds: every call surfaces NotSupported / Unknown so
// the symbol exists and main can decide between the SCM controller
// and the disabled one without per-platform compile gates.

boost::asio::awaitable<ServiceStatus>
WindowsScmServiceController::QueryStatus(const ServiceInstance& /*svc*/)
{
    co_return ServiceStatus::Unknown;
}

boost::asio::awaitable<ControlResult>
WindowsScmServiceController::Start(const ServiceInstance& /*svc*/)
{
    co_return ControlResult::NotSupported;
}

boost::asio::awaitable<ControlResult>
WindowsScmServiceController::Stop(const ServiceInstance& /*svc*/)
{
    co_return ControlResult::NotSupported;
}

#endif // FOURSTORY_HAS_WIN32

} // namespace tcontrolsvr
