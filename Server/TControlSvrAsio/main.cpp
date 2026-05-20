// Entry point for the modernized TControlSvrAsio binary. F1 brought
// up the scaffold + operator login; F2 layers on the SOCI service
// inventory + auth, peer dialer, peer registry, monitoring timers,
// and the schema validator.

#include "config.h"
#include "control_server.h"
#include "peer_dialer.h"
#include "db/schema_validator.h"
#include "services/chat_ban_repository.h"
#include "services/disabled_service_controller.h"
#include "services/fake_operator_auth_service.h"
#include "services/fake_service_inventory.h"
#include "services/fake_user_protected_service.h"
#include "services/operator_auth_service.h"
#include "services/peer_registry.h"
#include "services/service_inventory.h"
#include "services/soci_operator_auth_service.h"
#include "services/soci_service_inventory.h"
#include "services/soci_user_protected_service.h"
#include "services/spdlog_admin_audit_logger.h"
#include "services/user_protected_service.h"

#include "fourstory/db/session_pool.h"
#include "fourstory/ops/admin_shell.h"
#include "fourstory/ops/health_endpoint.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace {
void Usage()
{
    std::printf(
        "tcontrolsvr_asio — modernized 4Story control / orchestration server\n"
        "Usage: tcontrolsvr_asio [--config FILE]\n");
}

// Pick the controller backend. F2 ships only the disabled default
// on Linux; the Windows SCM impl is gated behind FOURSTORY_HAS_WIN32
// (see services/windows_scm_service_controller.h once added).
std::unique_ptr<tcontrolsvr::IServiceController> MakeServiceController()
{
    return std::make_unique<tcontrolsvr::DisabledServiceController>();
}

} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tcontrolsvr.toml";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h") == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        auto cfg = tcontrolsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // --- Auth + inventory --------------------------------------
        //
        // Prefer the SOCI-backed services when a [database] section
        // is present. Fall back to the in-memory fakes otherwise (F1
        // bring-up path).
        std::unique_ptr<fourstory::db::SessionPool>           pool;
        std::unique_ptr<tcontrolsvr::IOperatorAuthService>    auth;
        std::unique_ptr<tcontrolsvr::IServiceInventory>       inventory_ptr;
        std::unique_ptr<tcontrolsvr::IUserProtectedService>   user_ban;

        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string set but database.backend empty");
            const auto backend =
                fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);

            // Fail-fast on missing inventory tables before we accept
            // operator traffic. Optional tables (TEVENTCHART etc.)
            // log warnings; required tables throw.
            tcontrolsvr::db::ValidateGlobalSchema(*pool);

            auto soci_inv =
                std::make_unique<tcontrolsvr::SociServiceInventory>(*pool);
            soci_inv->Reload();
            inventory_ptr = std::move(soci_inv);
            auth = std::make_unique<tcontrolsvr::SociOperatorAuthService>(*pool);
            user_ban =
                std::make_unique<tcontrolsvr::SociUserProtectedService>(*pool);
            spdlog::info("auth + inventory: SOCI ({}) ready",
                fourstory::db::BackendName(backend));
        }
        else
        {
            spdlog::info("no [database] — running with FakeAuth + FakeInventory");
            auto fake_auth =
                std::make_unique<tcontrolsvr::FakeOperatorAuthService>();
            for (const auto& op : cfg.fake_operators)
                fake_auth->AddOperator(op.id, op.password, op.authority);
            auth = std::move(fake_auth);

            auto fake_inv =
                std::make_unique<tcontrolsvr::FakeServiceInventory>();
            for (const auto& g : cfg.fake_inventory.groups)
                fake_inv->AddGroup({g.id, g.name});
            for (const auto& m : cfg.fake_inventory.machines)
                fake_inv->AddMachine({m.id, m.name, 0, {}, {}, ""});
            for (const auto& t : cfg.fake_inventory.types)
                fake_inv->AddType({t.id, 0, t.name});
            inventory_ptr = std::move(fake_inv);
            user_ban = std::make_unique<tcontrolsvr::FakeUserProtectedService>();
        }

        // --- Admin audit + chat-ban registry -----------------------
        tcontrolsvr::SpdlogAdminAuditLogger audit;
        tcontrolsvr::ChatBanRepository      chat_bans;

        // --- Peer infra --------------------------------------------
        tcontrolsvr::PeerRegistry peers(*inventory_ptr);
        auto controller = MakeServiceController();
        tcontrolsvr::PeerDialer dialer(io, peers, *inventory_ptr);

        // --- Server ------------------------------------------------
        tcontrolsvr::ControlServerConfig svr_cfg{};
        svr_cfg.port       = cfg.port;
        svr_cfg.auth       = auth.get();
        svr_cfg.inventory  = inventory_ptr.get();
        svr_cfg.controller = controller.get();
        svr_cfg.dialer     = &dialer;
        svr_cfg.peers      = &peers;
        svr_cfg.audit      = &audit;
        svr_cfg.user_ban   = user_ban.get();
        svr_cfg.chat_bans  = &chat_bans;
        svr_cfg.auto_start = cfg.auto_start;
        tcontrolsvr::ControlServer server(io, svr_cfg);
        spdlog::info("control server listening on 0.0.0.0:{}", server.Port());
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // 1Hz peer-keepalive watchdog (legacy TimerThread).
        boost::asio::co_spawn(io,
            server.PeerKeepaliveLoop(),
            boost::asio::detached);

        std::unique_ptr<fourstory::ops::HealthEndpoint> health;
        if (cfg.health_port != 0)
        {
            try
            {
                health = std::make_unique<fourstory::ops::HealthEndpoint>(
                    io, cfg.health_port);
                spdlog::info("health endpoint listening on 0.0.0.0:{}",
                    health->Port());
                boost::asio::co_spawn(io, health->Run(),
                    boost::asio::detached);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("health endpoint failed to bind on port {}: {}",
                    cfg.health_port, ex.what());
            }
        }

        std::shared_ptr<fourstory::ops::AdminShell> admin;
        if (cfg.admin_port != 0)
        {
            try
            {
                admin = std::make_shared<fourstory::ops::AdminShell>(
                    io, cfg.admin_bind, cfg.admin_port,
                    [&server] { return server.LiveOperators(); },
                    std::chrono::steady_clock::now());
                spdlog::info("admin shell listening on {}:{}",
                    cfg.admin_bind, admin->Port());
                boost::asio::co_spawn(io, admin->Run(),
                    boost::asio::detached);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("admin shell failed to bind on {}:{}: {}",
                    cfg.admin_bind, cfg.admin_port, ex.what());
            }
        }

        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
