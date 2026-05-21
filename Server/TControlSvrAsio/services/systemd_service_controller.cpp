#include "systemd_service_controller.h"

#include "scm_name_resolver.h"
#include "service_inventory.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <utility>

#ifdef __unix__
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tcontrolsvr {

namespace {

// Shell-quote a single argv element. Conservative: always wrap in
// single quotes and escape embedded single quotes. Avoids the
// classic "find -name '*.txt'" foot-gun where dash gets eaten by
// the outer shell's word-splitting.
std::string ShellQuote(const std::string& s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'') out += "'\\''";
        else           out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

// Trim trailing whitespace + newline. systemctl is-active prints the
// state followed by a newline; we want the state only.
std::string TrimRight(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
                          s.back() == ' '  || s.back() == '\t'))
        s.pop_back();
    return s;
}

ServiceStatus MapSystemctlActive(const std::string& state)
{
    // systemctl is-active values: active, inactive, failed,
    // activating, deactivating, reloading, unknown.
    if (state == "active")        return ServiceStatus::Running;
    if (state == "inactive")      return ServiceStatus::Stopped;
    if (state == "failed")        return ServiceStatus::Stopped;
    if (state == "activating")    return ServiceStatus::StartPending;
    if (state == "deactivating")  return ServiceStatus::StopPending;
    if (state == "reloading")     return ServiceStatus::Running;
    return ServiceStatus::Unknown;
}

} // namespace

#ifdef __unix__

SystemctlResult RunSystemctlDefault(
    const std::string& systemctl_path,
    const std::vector<std::string>& argv)
{
    // Build the full command line via shell-quoted argv. popen()
    // exec's `/bin/sh -c <cmd>` so we MUST quote every element
    // ourselves — otherwise a unit name with a hyphen would fool the
    // shell tokenizer.
    std::ostringstream cmd;
    cmd << ShellQuote(systemctl_path);
    for (const auto& a : argv) cmd << ' ' << ShellQuote(a);
    // Merge stderr into stdout so the captured text includes failure
    // diagnostics. systemctl writes "Failed to start X" to stderr.
    cmd << " 2>&1";

    FILE* pipe = ::popen(cmd.str().c_str(), "r");
    if (!pipe) return {-1, "popen failed: " + std::string(::strerror(errno))};

    std::string out;
    std::array<char, 256> buf;
    while (true)
    {
        const auto n = std::fread(buf.data(), 1, buf.size(), pipe);
        if (n > 0) out.append(buf.data(), n);
        if (n < buf.size()) break;
    }
    const int rc = ::pclose(pipe);
    // pclose returns the wait4 status; pull the exit code out.
    int exit_code = -1;
    if (WIFEXITED(rc))      exit_code = WEXITSTATUS(rc);
    else if (WIFSIGNALED(rc)) exit_code = 128 + WTERMSIG(rc);
    return {exit_code, std::move(out)};
}

#else

// On non-Unix targets the default runner is a stub returning a
// "would have run" sentinel so the surface still compiles. The
// production factory short-circuits before constructing
// SystemdServiceController on non-Linux platforms, but keeping the
// symbol around lets the unit tests link uniformly.
SystemctlResult RunSystemctlDefault(
    const std::string& /*systemctl_path*/,
    const std::vector<std::string>& /*argv*/)
{
    return {-1, "systemctl unsupported on this platform"};
}

#endif

SystemdServiceController::SystemdServiceController(Options opts)
    : m_opts(std::move(opts))
{
    if (!m_opts.runner)
    {
        // Bind the production runner once. Capture by value so the
        // copy doesn't reach back into m_opts.systemctl_path after a
        // move-from.
        const auto path = m_opts.systemctl_path;
        m_opts.runner = [path](const std::vector<std::string>& argv) {
            return RunSystemctlDefault(path, argv);
        };
    }
}

std::string
SystemdServiceController::ResolveUnit(const ServiceInstance& svc) const
{
    auto name = ResolveScmName(svc, m_opts.service_name_template,
                               m_opts.overrides);
    // systemd unit names canonically end in `.service`. Operators
    // can include the suffix in their override; we only append when
    // it's absent so legacy overrides don't double-suffix.
    if (name.size() < 8 ||
        name.compare(name.size() - 8, 8, ".service") != 0)
        name += ".service";
    return name;
}

SystemctlResult
SystemdServiceController::Run(const std::vector<std::string>& argv) const
{
    std::vector<std::string> full;
    full.reserve(argv.size() + 1);
    if (m_opts.user_scope) full.push_back("--user");
    for (const auto& a : argv) full.push_back(a);
    return m_opts.runner(full);
}

boost::asio::awaitable<ServiceStatus>
SystemdServiceController::QueryStatus(const ServiceInstance& svc)
{
    const auto unit = ResolveUnit(svc);
    // Offload the blocking popen onto the worker pool if wired;
    // otherwise run inline on the io_context (legacy behavior).
    const auto result = co_await fourstory::db::CoOffloadIf(
        m_opts.worker_pool,
        [this, unit] { return Run({"is-active", unit}); });

    // systemctl is-active exits 0 for active, 3 for inactive/failed,
    // 4 for "no such unit". We map both 0 and 3 by parsing stdout —
    // exit code 4 is "not installed".
    if (result.exit_code == 4)
        co_return ServiceStatus::NotInstalled;
    if (result.exit_code == -1)
    {
        spdlog::warn("systemctl is-active '{}': runner error: {}",
            unit, result.stdout_text);
        co_return ServiceStatus::Unknown;
    }
    co_return MapSystemctlActive(TrimRight(result.stdout_text));
}

boost::asio::awaitable<ControlResult>
SystemdServiceController::Start(const ServiceInstance& svc)
{
    const auto unit = ResolveUnit(svc);
    const auto result = co_await fourstory::db::CoOffloadIf(
        m_opts.worker_pool,
        [this, unit] { return Run({"start", unit}); });
    if (result.exit_code == 0)
    {
        spdlog::info("systemctl start '{}' ok", unit);
        co_return ControlResult::Ok;
    }
    spdlog::warn("systemctl start '{}' failed: rc={} {}",
        unit, result.exit_code, TrimRight(result.stdout_text));
    co_return ControlResult::Failed;
}

boost::asio::awaitable<ControlResult>
SystemdServiceController::Stop(const ServiceInstance& svc)
{
    const auto unit = ResolveUnit(svc);
    const auto result = co_await fourstory::db::CoOffloadIf(
        m_opts.worker_pool,
        [this, unit] { return Run({"stop", unit}); });
    if (result.exit_code == 0)
    {
        spdlog::info("systemctl stop '{}' ok", unit);
        co_return ControlResult::Ok;
    }
    spdlog::warn("systemctl stop '{}' failed: rc={} {}",
        unit, result.exit_code, TrimRight(result.stdout_text));
    co_return ControlResult::Failed;
}

} // namespace tcontrolsvr
