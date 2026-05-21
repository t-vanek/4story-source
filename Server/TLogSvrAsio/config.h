#pragma once

#include "fourstory/cluster/peer_client.h"

#include <spdlog/common.h>

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <string>

namespace tlogsvr {

struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size = 4;
    // Worker threads for off-loop SOCI INSERTs. Single thread
    // preserves FIFO ordering between UDP datagrams; multi-thread
    // pools work too (retry queue + drain semantics still hold)
    // but may reorder INSERTs. 0 = legacy in-line behavior
    // (INSERT runs on the UDP receive coroutine, blocks the io
    // context on DB latency).
    std::size_t  worker_threads = 1;
};

struct RetryConfig
{
    // Mirrors the legacy bounded IO pool (`MAX_IO_CONTEXT = 1000` in
    // Server/TLogSvr/StdAfx.h). New datagrams that would push the
    // queue past this cap are dropped (counter-bumped). 0 disables
    // queueing — failed records are dropped on first INSERT error.
    std::size_t                max_queue           = 1000;

    // Drain coroutine tick. Legacy `WorkTickProc` reconnects every
    // 30 s; this matches that cadence. Set smaller for tests or for
    // faster recovery on flaky DBs.
    std::chrono::seconds       drain_interval{30};

    // Per-tick cap so one drain pass can't monopolize the io_context
    // when the backlog is large.
    std::size_t                drain_batch_size    = 64;
};

struct AppConfig
{
    // UDP listening port. Legacy default was 2000 (UdpSocket peer port
    // in TLoginSvr); keep it for back-compat.
    std::uint16_t  port = 2000;
    std::string    bind_address = "0.0.0.0";

    // Insert target. Default matches schema/tlog-audit.sql. Override
    // to a different table when you partition or move logs to a
    // dedicated audit DB.
    std::string    target_table = "TLOG_AUDIT";

    // TGLOBAL DB. Production deploys typically point this at a
    // dedicated audit database (separate from the live game DB) so
    // log retention policies don't touch player data.
    DbConfig       database;

    // In-RAM retry buffer parameters — see RetryConfig. Mirrors the
    // legacy `m_listReadCompleted` requeue semantics so transient
    // DB outages don't drop audit records.
    RetryConfig    retry;

    // Optional /healthz HTTP endpoint port (k8s liveness/readiness +
    // LB health checks). 0 disables. Matches the [health] block in
    // tloginsvr / tpatchsvr config.
    std::uint16_t  health_port = 0;

    // Cluster self-registration; empty control_host disables.
    // Service type byte is fixed at 2 (svr_type::kLogSvr).
    fourstory::cluster::ClusterConfig cluster;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tlogsvr
