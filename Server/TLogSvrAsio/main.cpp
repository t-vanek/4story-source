#include "config.h"
#include "db/schema_validator.h"
#include "log_server.h"
#include "services/log_sink.h"

#include "fourstory/db/session_pool.h"
#include "fourstory/ops/health_endpoint.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

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
        "tlogsvr_asio — modernized 4Story audit log collector\n"
        "Usage: tlogsvr_asio [--config FILE]\n");
}

// Periodic visibility into queue depth + drop counters. Without
// this loop the server only emits cumulative totals at shutdown, so
// a runtime queue saturation surfaces too late to act on.
boost::asio::awaitable<void>
BackpressureMonitor(boost::asio::io_context&           io,
                    const tlogsvr::LogServer&          server,
                    const tlogsvr::SociLogSink*        sink,
                    std::chrono::seconds               interval,
                    bool                               always_log)
{
    boost::asio::steady_timer timer(io);
    std::uint64_t prev_drops_format = 0;
    std::uint64_t prev_drops_queue  = sink ? sink->DroppedQueueFull() : 0;

    while (true)
    {
        timer.expires_after(interval);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) co_return; // executor stopping

        const auto cur_drops_format = server.DropsBadFormat();
        const auto cur_drops_queue  = sink ? sink->DroppedQueueFull() : 0;
        const auto delta_format     = cur_drops_format - prev_drops_format;
        const auto delta_queue      = cur_drops_queue  - prev_drops_queue;
        const auto queue_depth      = sink ? sink->QueueDepth() : 0;

        if (delta_format > 0 || delta_queue > 0)
        {
            spdlog::warn(
                "backpressure: drops_bad_format=+{} drops_queue_full=+{} "
                "queue_depth={} (totals format={} queue={})",
                delta_format, delta_queue, queue_depth,
                cur_drops_format, cur_drops_queue);
        }
        else if (always_log)
        {
            spdlog::warn(
                "backpressure: nominal (drops_bad_format=0 "
                "drops_queue_full=0 queue_depth={})",
                queue_depth);
        }
        else
        {
            spdlog::info(
                "backpressure: nominal (queue_depth={} totals format={} "
                "queue={})",
                queue_depth, cur_drops_format, cur_drops_queue);
        }

        prev_drops_format = cur_drops_format;
        prev_drops_queue  = cur_drops_queue;
    }
}
} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tlogsvr.toml";
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
        auto cfg = tlogsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        std::unique_ptr<fourstory::db::SessionPool> pool;
        std::unique_ptr<boost::asio::thread_pool>   db_pool;
        std::unique_ptr<tlogsvr::ILogSink>           sink;
        tlogsvr::SociLogSink*                        soci_sink = nullptr;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error("database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            // Fail-fast on missing LT_* columns before we bind the UDP
            // socket (F5 in SQL_AUDIT). Datagrams that arrive after a
            // misconfigured target_table would otherwise silently drop
            // at INSERT time with no visibility.
            tlogsvr::db::ValidateAuditSchema(*pool, cfg.target_table);
            tlogsvr::SociLogSink::Options sink_opts;
            sink_opts.max_retry_queue  = cfg.retry.max_queue;
            sink_opts.drain_interval   = cfg.retry.drain_interval;
            sink_opts.drain_batch_size = cfg.retry.drain_batch_size;
            auto owning = std::make_unique<tlogsvr::SociLogSink>(
                *pool, cfg.target_table, sink_opts);
            soci_sink = owning.get();
            sink = std::move(owning);
            soci_sink->StartDrainLoop(io);

            // Single-thread worker pool so INSERTs run off the
            // io_context (which is also handling UDP receive).
            // Single thread preserves FIFO ordering between
            // datagrams — see SociLogSink::SetWorkerPool. Disabled
            // when worker_threads=0 (legacy in-line behavior).
            if (cfg.database.worker_threads > 0)
            {
                db_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
                soci_sink->SetWorkerPool(db_pool.get());
                spdlog::info("log_sink: worker pool = {} thread(s)",
                    cfg.database.worker_threads);
            }
            spdlog::info("log_sink: SOCI → {}.{} (retry_queue={} drain={}s)",
                cfg.database.backend, cfg.target_table,
                cfg.retry.max_queue,
                static_cast<long long>(cfg.retry.drain_interval.count()));
        }
        else
        {
            sink = std::make_unique<tlogsvr::StdoutLogSink>();
            spdlog::warn("log_sink: stdout (no [database] configured)");
        }

        tlogsvr::LogServerConfig srv_cfg{
            .bind_address = cfg.bind_address,
            .port         = cfg.port,
            .sink         = sink.get(),
        };
        tlogsvr::LogServer server(io, srv_cfg);
        spdlog::info("log server listening UDP {}:{}",
            cfg.bind_address, server.Port());
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // Optional /healthz endpoint on a separate port. Matches the
        // wiring in TLoginSvrAsio + TPatchSvrAsio — silently warn if
        // the port is in use rather than aborting the UDP listener.
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

        if (cfg.backpressure.sample_interval.count() > 0)
        {
            spdlog::info("backpressure monitor: every {}s (always_log={})",
                static_cast<long long>(cfg.backpressure.sample_interval.count()),
                cfg.backpressure.always_log);
            boost::asio::co_spawn(io,
                BackpressureMonitor(io, server, soci_sink,
                    cfg.backpressure.sample_interval,
                    cfg.backpressure.always_log),
                boost::asio::detached);
        }

        io.run();

        // Wait for any in-flight pool work (SOCI INSERTs) to finish
        // so a record posted just before io.stop() doesn't disappear.
        if (db_pool)
        {
            db_pool->stop();
            db_pool->join();
        }

        if (soci_sink != nullptr)
        {
            spdlog::info(
                "totals: received={} drops_bad_format={} "
                "inserted={} enqueued={} drained={} dropped_queue_full={} "
                "queue_depth_at_shutdown={}",
                server.PacketsReceived(), server.DropsBadFormat(),
                soci_sink->Inserts(), soci_sink->EnqueuedOnError(),
                soci_sink->DrainedAfterRetry(),
                soci_sink->DroppedQueueFull(),
                soci_sink->QueueDepth());
        }
        else
        {
            spdlog::info("totals: received={} drops_bad_format={}",
                server.PacketsReceived(), server.DropsBadFormat());
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
