#pragma once

// systemd-backed IServiceController. The Linux counterpart of
// WindowsScmServiceController — same surface (Start/Stop/QueryStatus
// returning ServiceStatus/ControlResult), driven through systemctl.
//
// Why shell-out instead of D-Bus: systemctl is universally available
// on every systemd-managed distro, parses the same way across
// versions, and doesn't pull in libdbus-1 / sd-bus / GLib. The trade
// is that we exec a process per call (~5ms typical). For TControl's
// expected call rate (operator-driven, plus a 30s reconciliation
// tick) that's well below noise.
//
// The unit name is resolved the same way Windows SCM resolves its
// service display name — see scm_name_resolver.h. A MapSvr in group
// 1 / server 3 becomes "MapSvr-1-3.service" under the default
// template; the .service suffix is appended automatically. Operators
// can override per-service via [cluster.scm.overrides] in TOML.
//
// Permissions: systemctl needs root (system scope) or the caller's
// uid (user scope, --user). Deployments that don't want to grant
// root should ship .service files into ~/.config/systemd/user/ and
// set [cluster.scm] systemd_user = true.

#include "service_controller.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace boost::asio { class thread_pool; }

namespace tcontrolsvr {

// Result of executing `systemctl ...`. Captured for both the real
// runner (via popen) and the test runner stub.
struct SystemctlResult
{
    int          exit_code = -1;   // -1 = fork/exec failed
    std::string  stdout_text;
};

// Default runner — execs the configured systemctl binary with the
// given argv and captures stdout. Exposed in the header so tests
// can stub it.
SystemctlResult RunSystemctlDefault(
    const std::string& systemctl_path,
    const std::vector<std::string>& argv);

class SystemdServiceController final : public IServiceController
{
public:
    using SystemctlRunner = std::function<SystemctlResult(
        const std::vector<std::string>& /*argv (without binary)*/)>;

    struct Options
    {
        std::string service_name_template = "{type_name}-{group}-{server}";

        // Per-service overrides for deploys where the systemd unit
        // name doesn't follow the template.
        std::unordered_map<std::uint32_t, std::string> overrides;

        // false → system scope (`systemctl start ...`).
        // true  → user scope  (`systemctl --user start ...`).
        bool user_scope = false;

        // Absolute path or PATH lookup name; default "systemctl".
        std::string systemctl_path = "systemctl";

        // Worker pool for offloading the blocking popen call. Without
        // it every Start/Stop/QueryStatus blocks the io_context for
        // ~5ms. Wire it from the same pool that handles SOCI work.
        boost::asio::thread_pool* worker_pool = nullptr;

        // Test seam: when set, replaces RunSystemctlDefault. The
        // production factory never sets this.
        SystemctlRunner runner;
    };

    explicit SystemdServiceController(Options opts);

    boost::asio::awaitable<ServiceStatus>
        QueryStatus(const ServiceInstance& svc) override;

    boost::asio::awaitable<ControlResult>
        Start(const ServiceInstance& svc) override;

    boost::asio::awaitable<ControlResult>
        Stop(const ServiceInstance& svc) override;

private:
    SystemctlResult Run(const std::vector<std::string>& argv) const;
    std::string     ResolveUnit(const ServiceInstance& svc) const;

    Options m_opts;
};

} // namespace tcontrolsvr
