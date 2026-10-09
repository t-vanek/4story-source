#include "service_controller_factory.h"

#include "disabled_service_controller.h"
#include "systemd_service_controller.h"
#include "windows_scm_service_controller.h"

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace tcontrolsvr {

namespace {

#if defined(_WIN32)
constexpr const char* kPlatformDefault = "windows";
#elif defined(__linux__)
constexpr const char* kPlatformDefault = "systemd";
#else
// macOS, BSDs, illumos, etc. — no built-in service-manager backend.
// Operators can still force "systemd" or "windows" explicitly; the
// factory will warn + fall back to disabled.
constexpr const char* kPlatformDefault = "disabled";
#endif

std::unique_ptr<IServiceController>
MakeWindows(const ServiceControllerFactoryConfig& cfg)
{
#if defined(_WIN32)
    WindowsScmServiceController::Options o;
    o.service_name_template = cfg.service_name_template;
    o.overrides             = cfg.overrides;
    return std::make_unique<WindowsScmServiceController>(std::move(o));
#else
    (void)cfg;
    spdlog::warn("scm: backend='windows' requested on non-Windows build "
                 "— falling back to disabled controller");
    return std::make_unique<DisabledServiceController>();
#endif
}

std::unique_ptr<IServiceController>
MakeSystemd(const ServiceControllerFactoryConfig& cfg)
{
#if defined(__linux__)
    SystemdServiceController::Options o;
    o.service_name_template = cfg.service_name_template;
    o.overrides             = cfg.overrides;
    o.user_scope            = cfg.systemd_user_scope;
    o.systemctl_path        = cfg.systemctl_path;
    o.worker_pool           = cfg.worker_pool;
    return std::make_unique<SystemdServiceController>(std::move(o));
#else
    (void)cfg;
    spdlog::warn("scm: backend='systemd' requested on non-Unix build "
                 "— falling back to disabled controller");
    return std::make_unique<DisabledServiceController>();
#endif
}

} // namespace

std::unique_ptr<IServiceController>
MakeServiceController(const ServiceControllerFactoryConfig& cfg)
{
    std::string backend = cfg.backend;
    if (backend == "auto" || backend.empty())
        backend = kPlatformDefault;

    if (backend == "disabled")
    {
        spdlog::info("scm: backend=disabled (cluster start/stop are no-ops)");
        return std::make_unique<DisabledServiceController>();
    }
    if (backend == "windows")
    {
        spdlog::info("scm: backend=windows (template='{}', overrides={})",
            cfg.service_name_template, cfg.overrides.size());
        return MakeWindows(cfg);
    }
    if (backend == "systemd")
    {
        spdlog::info("scm: backend=systemd (template='{}', overrides={}, "
                     "user_scope={}, systemctl='{}')",
            cfg.service_name_template, cfg.overrides.size(),
            cfg.systemd_user_scope, cfg.systemctl_path);
        return MakeSystemd(cfg);
    }
    throw std::runtime_error(
        "scm.backend: unknown value '" + cfg.backend +
        "' (expected auto|windows|systemd|disabled)");
}

} // namespace tcontrolsvr
