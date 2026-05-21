#pragma once

// Picks an IServiceController implementation based on the operator's
// configured backend choice plus the build platform. Centralizes the
// "which controller do we instantiate" decision so main.cpp stays
// short and the test suite can re-run the same logic in isolation.
//
// Backend selection rules:
//
//   backend = "auto"     →  Windows builds: WindowsScmServiceController
//                           Linux  builds: SystemdServiceController
//                           Other  builds: DisabledServiceController
//   backend = "windows"  →  WindowsScmServiceController on Windows;
//                           DisabledServiceController + warning log
//                           on other platforms.
//   backend = "systemd"  →  SystemdServiceController on Linux/Unix;
//                           DisabledServiceController + warning log
//                           on other platforms.
//   backend = "disabled" →  DisabledServiceController (default-safe).
//   anything else        →  throws std::runtime_error so the operator
//                           sees the typo at startup, not silently
//                           later when a `cluster start` no-ops.

#include "service_controller.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace boost::asio { class thread_pool; }

namespace tcontrolsvr {

struct ServiceControllerFactoryConfig
{
    std::string  backend = "auto";

    // Used by both Windows + systemd backends — keeps the unit-name
    // grammar consistent across platforms. See scm_name_resolver.h.
    std::string  service_name_template = "{type_name}-{group}-{server}";

    // Per-service overrides applied BEFORE template rendering.
    // Operators set these via the [cluster.scm.overrides] TOML block.
    std::unordered_map<std::uint32_t, std::string> overrides;

    // systemd-specific knob — see SystemdServiceController::Options.
    bool         systemd_user_scope = false;
    std::string  systemctl_path    = "systemctl";

    // Optional worker pool for offloading the blocking
    // SCM/systemctl calls. nullptr = run inline on io_context. The
    // production wiring shares the same pool that handles SOCI.
    boost::asio::thread_pool* worker_pool = nullptr;
};

std::unique_ptr<IServiceController>
MakeServiceController(const ServiceControllerFactoryConfig& cfg);

} // namespace tcontrolsvr
