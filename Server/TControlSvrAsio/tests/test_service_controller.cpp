// Cross-platform service-controller coverage:
//
//   1. scm_name_resolver — template rendering + override lookup
//   2. service_controller_factory — backend selection for all four
//      values of `backend`, including the platform-default auto path
//   3. SystemdServiceController — drives Start/Stop/QueryStatus via
//      an injected SystemctlRunner stub, asserts on argv passed to
//      systemctl and on the controller's mapping of exit codes /
//      `is-active` output back into ControlResult / ServiceStatus
//
// The real Win32 SCM + real popen paths can't be unit-tested without
// the actual platform; this covers everything the controller layer
// computes BEFORE handing off to the OS.

#include "services/service_controller.h"
#include "services/service_controller_factory.h"
#include "services/scm_name_resolver.h"
#include "services/service_inventory.h"
#include "services/systemd_service_controller.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace asio = boost::asio;

using tcontrolsvr::ControlResult;
using tcontrolsvr::IServiceController;
using tcontrolsvr::MakeServiceController;
using tcontrolsvr::RenderScmName;
using tcontrolsvr::ResolveScmName;
using tcontrolsvr::ServiceControllerFactoryConfig;
using tcontrolsvr::ServiceInstance;
using tcontrolsvr::ServiceStatus;
using tcontrolsvr::SystemctlResult;
using tcontrolsvr::SystemdServiceController;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

ServiceInstance MakeService(std::uint32_t sid, std::uint8_t group_id,
                            std::uint8_t type_id, std::string name,
                            std::uint8_t server_id = 0)
{
    ServiceInstance s{};
    s.service_id = sid;
    s.group_id   = group_id;
    s.type_id    = type_id;
    s.server_id  = server_id ? server_id
                             : static_cast<std::uint8_t>(sid & 0xFF);
    s.machine_id = 42;
    s.name       = std::move(name);
    return s;
}

template <class T>
T RunOne(asio::io_context& io, asio::awaitable<T> aw)
{
    auto fut = asio::co_spawn(io, std::move(aw), asio::use_future);
    io.restart();
    io.run();
    return fut.get();
}

// ---------------------------------------------------------------------------

void TestNameResolverRendersAllPlaceholders()
{
    std::printf("[resolver — every placeholder substitutes]\n");
    auto svc = MakeService(0x010301, 1, 4, "MapSvr", 3);
    Check(RenderScmName("{type_name}-{group}-{server}", svc) == "MapSvr-1-3",
        "default template");
    Check(RenderScmName("svc.{type}.g{group}.s{server}.m{machine}", svc)
            == "svc.4.g1.s3.m42",
        "all five tokens replaced");
    Check(RenderScmName("plain", svc) == "plain",
        "template without placeholders is verbatim");
    Check(RenderScmName("{nonsense}", svc).empty(),
        "unknown placeholder is silently dropped");
    Check(RenderScmName("a{group}b{server}c", svc) == "a1b3c",
        "surrounding literals preserved");
}

void TestResolveScmNamePrefersOverride()
{
    std::printf("[resolver — override beats template]\n");
    auto svc = MakeService(0x010301, 1, 4, "MapSvr", 3);
    std::unordered_map<std::uint32_t, std::string> overrides;
    overrides[0x010301] = "4Story_Map_World1";

    Check(ResolveScmName(svc, "{type_name}-{group}-{server}", overrides)
            == "4Story_Map_World1",
        "override applied for matching sid");

    auto other = MakeService(0x010302, 1, 4, "MapSvr", 4);
    Check(ResolveScmName(other, "{type_name}-{group}-{server}", overrides)
            == "MapSvr-1-4",
        "non-matching sid falls back to template");

    overrides[0x010301] = "";
    Check(ResolveScmName(svc, "{type_name}-{group}-{server}", overrides)
            == "MapSvr-1-3",
        "empty override is treated as no override");
}

// ---------------------------------------------------------------------------

void TestFactoryDisabledExplicit()
{
    std::printf("[factory — backend='disabled' returns disabled controller]\n");
    ServiceControllerFactoryConfig cfg;
    cfg.backend = "disabled";
    auto c = MakeServiceController(cfg);
    Check(c != nullptr, "factory returns a controller");

    // Disabled = QueryStatus returns Unknown, Start/Stop return
    // NotSupported. Drive each through a small io_context.
    asio::io_context io;
    ServiceInstance svc{};
    Check(RunOne(io, c->QueryStatus(svc)) == ServiceStatus::Unknown,
        "disabled.QueryStatus → Unknown");
    Check(RunOne(io, c->Start(svc)) == ControlResult::NotSupported,
        "disabled.Start → NotSupported");
    Check(RunOne(io, c->Stop(svc)) == ControlResult::NotSupported,
        "disabled.Stop → NotSupported");
}

void TestFactoryAutoPicksPlatformDefault()
{
    std::printf("[factory — backend='auto' returns a non-null controller]\n");
    ServiceControllerFactoryConfig cfg;
    cfg.backend = "auto";
    auto c = MakeServiceController(cfg);
    Check(c != nullptr,
        "auto returns *something* on every platform "
        "(disabled fallback as worst case)");
}

void TestFactoryEmptyBackendIsAuto()
{
    std::printf("[factory — empty backend treated as auto]\n");
    ServiceControllerFactoryConfig cfg;
    cfg.backend = "";
    auto c = MakeServiceController(cfg);
    Check(c != nullptr, "empty backend defaults to auto, never null");
}

void TestFactoryWrongPlatformFallsBackWithWarning()
{
    std::printf("[factory — explicit backend on wrong platform = disabled]\n");
    ServiceControllerFactoryConfig cfg;
#ifdef _WIN32
    cfg.backend = "systemd";
#else
    cfg.backend = "windows";
#endif
    auto c = MakeServiceController(cfg);
    Check(c != nullptr, "wrong-platform backend doesn't throw");

    // The fallback IS the disabled controller, so Start should
    // surface NotSupported (the disabled marker).
    asio::io_context io;
    ServiceInstance svc{};
    Check(RunOne(io, c->Start(svc)) == ControlResult::NotSupported,
        "wrong-platform fallback behaves like disabled");
}

void TestFactoryUnknownBackendThrows()
{
    std::printf("[factory — unknown backend value throws clear error]\n");
    ServiceControllerFactoryConfig cfg;
    cfg.backend = "nonsense";
    bool threw = false;
    std::string what;
    try { (void)MakeServiceController(cfg); }
    catch (const std::exception& ex) { threw = true; what = ex.what(); }
    Check(threw, "unknown backend throws");
    Check(what.find("nonsense") != std::string::npos,
        "exception names the offending backend value");
}

// ---------------------------------------------------------------------------

void TestSystemdQueryStatusActive()
{
    std::printf("[systemd — is-active=active maps to Running]\n");
    SystemdServiceController::Options o;
    std::vector<std::string> captured;
    o.runner = [&](const std::vector<std::string>& argv) {
        captured = argv;
        return SystemctlResult{0, "active\n"};
    };
    SystemdServiceController c(std::move(o));
    auto svc = MakeService(0x010301, 1, 4, "MapSvr", 3);

    asio::io_context io;
    const auto st = RunOne(io, c.QueryStatus(svc));
    Check(st == ServiceStatus::Running, "'active' → Running");
    Check(captured.size() == 2 &&
          captured[0] == "is-active" &&
          captured[1] == "MapSvr-1-3.service",
        "argv = ['is-active', '<unit>.service']");
}

void TestSystemdQueryStatusInactiveAndFailed()
{
    std::printf("[systemd — is-active maps inactive/failed to Stopped]\n");
    SystemdServiceController::Options o;
    o.runner = [](const std::vector<std::string>&) {
        return SystemctlResult{3, "inactive\n"};
    };
    SystemdServiceController c1(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    Check(RunOne(io, c1.QueryStatus(svc)) == ServiceStatus::Stopped,
        "'inactive' → Stopped");

    SystemdServiceController::Options o2;
    o2.runner = [](const std::vector<std::string>&) {
        return SystemctlResult{3, "failed\n"};
    };
    SystemdServiceController c2(std::move(o2));
    Check(RunOne(io, c2.QueryStatus(svc)) == ServiceStatus::Stopped,
        "'failed' → Stopped");
}

void TestSystemdQueryStatusActivatingDeactivating()
{
    std::printf("[systemd — activating/deactivating map to pending]\n");
    SystemdServiceController::Options o;
    o.runner = [](const std::vector<std::string>&) {
        return SystemctlResult{0, "activating\n"};
    };
    SystemdServiceController c(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    Check(RunOne(io, c.QueryStatus(svc)) == ServiceStatus::StartPending,
        "'activating' → StartPending");

    SystemdServiceController::Options o2;
    o2.runner = [](const std::vector<std::string>&) {
        return SystemctlResult{0, "deactivating\n"};
    };
    SystemdServiceController c2(std::move(o2));
    Check(RunOne(io, c2.QueryStatus(svc)) == ServiceStatus::StopPending,
        "'deactivating' → StopPending");
}

void TestSystemdQueryStatusNotInstalled()
{
    std::printf("[systemd — exit_code=4 → NotInstalled]\n");
    SystemdServiceController::Options o;
    o.runner = [](const std::vector<std::string>&) {
        return SystemctlResult{4, "Unit foo.service could not be found.\n"};
    };
    SystemdServiceController c(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    Check(RunOne(io, c.QueryStatus(svc)) == ServiceStatus::NotInstalled,
        "exit 4 → NotInstalled");
}

void TestSystemdStartMapsExitCode()
{
    std::printf("[systemd — Start: exit 0 → Ok, non-zero → Failed]\n");
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    {
        SystemdServiceController::Options o;
        std::vector<std::string> captured;
        o.runner = [&](const std::vector<std::string>& argv) {
            captured = argv;
            return SystemctlResult{0, ""};
        };
        SystemdServiceController c(std::move(o));
        Check(RunOne(io, c.Start(svc)) == ControlResult::Ok,
            "exit 0 → Ok");
        Check(captured[0] == "start" && captured[1] == "X-1-1.service",
            "Start uses 'start' verb + unit name");
    }
    {
        SystemdServiceController::Options o;
        o.runner = [](const std::vector<std::string>&) {
            return SystemctlResult{1, "Failed to start X-1-1.service\n"};
        };
        SystemdServiceController c(std::move(o));
        Check(RunOne(io, c.Start(svc)) == ControlResult::Failed,
            "non-zero exit → Failed");
    }
}

void TestSystemdUserScopePrependsFlag()
{
    std::printf("[systemd — user_scope=true prepends '--user']\n");
    SystemdServiceController::Options o;
    o.user_scope = true;
    std::vector<std::string> captured;
    o.runner = [&](const std::vector<std::string>& argv) {
        captured = argv;
        return SystemctlResult{0, ""};
    };
    SystemdServiceController c(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    (void)RunOne(io, c.Stop(svc));
    Check(captured.size() == 3 &&
          captured[0] == "--user" &&
          captured[1] == "stop" &&
          captured[2] == "X-1-1.service",
        "argv = ['--user', 'stop', '<unit>.service']");
}

void TestSystemdOverrideTakesPriority()
{
    std::printf("[systemd — override beats template (already-suffixed accepted)]\n");
    SystemdServiceController::Options o;
    o.overrides[1] = "custom.service";  // already has .service
    std::vector<std::string> captured;
    o.runner = [&](const std::vector<std::string>& argv) {
        captured = argv;
        return SystemctlResult{0, ""};
    };
    SystemdServiceController c(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    (void)RunOne(io, c.Start(svc));
    Check(captured.back() == "custom.service",
        "override applied without double-suffixing");
}

void TestSystemdAppendsServiceSuffixWhenMissing()
{
    std::printf("[systemd — bare override gets .service appended]\n");
    SystemdServiceController::Options o;
    o.overrides[1] = "bareunit";  // no suffix
    std::vector<std::string> captured;
    o.runner = [&](const std::vector<std::string>& argv) {
        captured = argv;
        return SystemctlResult{0, ""};
    };
    SystemdServiceController c(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    (void)RunOne(io, c.Start(svc));
    Check(captured.back() == "bareunit.service",
        "bare override is suffixed");
}

void TestSystemdRunnerErrorIsUnknown()
{
    std::printf("[systemd — runner error → ServiceStatus::Unknown]\n");
    SystemdServiceController::Options o;
    o.runner = [](const std::vector<std::string>&) {
        return SystemctlResult{-1, "popen failed: no such file"};
    };
    SystemdServiceController c(std::move(o));
    asio::io_context io;
    auto svc = MakeService(1, 1, 1, "X");
    Check(RunOne(io, c.QueryStatus(svc)) == ServiceStatus::Unknown,
        "runner -1 surfaces as Unknown");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio service-controller test ===\n");
    try
    {
        TestNameResolverRendersAllPlaceholders();
        TestResolveScmNamePrefersOverride();
        TestFactoryDisabledExplicit();
        TestFactoryAutoPicksPlatformDefault();
        TestFactoryEmptyBackendIsAuto();
        TestFactoryWrongPlatformFallsBackWithWarning();
        TestFactoryUnknownBackendThrows();
        TestSystemdQueryStatusActive();
        TestSystemdQueryStatusInactiveAndFailed();
        TestSystemdQueryStatusActivatingDeactivating();
        TestSystemdQueryStatusNotInstalled();
        TestSystemdStartMapsExitCode();
        TestSystemdUserScopePrependsFlag();
        TestSystemdOverrideTakesPriority();
        TestSystemdAppendsServiceSuffixWhenMissing();
        TestSystemdRunnerErrorIsUnknown();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
