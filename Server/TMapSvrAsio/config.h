#pragma once

// TMapSvrAsio TOML configuration. Same shape as TPatchSvrAsio /
// TLoginSvrAsio — TOML sections [server], [crypto], [mode], [database],
// [world], [health], [log], plus map-server-specific [mode] (run-time
// PvE / Bow / BR switch, replacing legacy BR/BOW_COMPILE_MODE compile
// flags) and [world] (the TWorldSvr peer this map talks to, wired up
// starting in phase F5).

#include "map_server.h"

#include "fourstory/cluster/peer_client.h"

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>

namespace tmapsvr {

// Run-time game-mode selector. Replaces legacy BR_COMPILE_MODE /
// BOW_COMPILE_MODE preprocessor gates: now a single binary serves any
// mode, picked at boot from [mode] type.
enum class Mode : std::uint8_t {
    PvE = 0,
    Bow = 1,
    Br  = 2,
};

const char* ModeName(Mode m);

// SOCI DB connection. The map server reads TCURRENTUSER (session
// validation against the token TLoginSvrAsio wrote at login) and
// TCHARTABLE (char load/save) — for F2 we only validate the first;
// char-table validation lands with the F5 player service.
struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size = 4;
    // Worker threads for off-loop SOCI calls. 0 disables (in-line on
    // the io_context thread, fine for low-load dev runs).
    std::size_t  worker_threads = 4;
};

// TWorldSvr peer address. Populated for forward compatibility — the
// outbound link is implemented in F5 (services/world_client.cpp).
struct WorldPeerConfig
{
    std::string    host = "127.0.0.1";
    std::uint16_t  port = 0;     // 0 = unconfigured, world peer disabled
};

// TLogSvrAsio audit-shim address — UDP fire-and-forget sink for
// structured events. Empty host or port=0 disables the peer;
// events stay in the local spdlog file. T4 observability will start
// emitting actual events through this peer.
struct AuditPeerConfig
{
    std::string    host;          // empty → disabled
    std::uint16_t  port = 0;
};

// T5 rate limiter — token bucket per session.
//   burst:        max consecutive messages without throttling
//   refill_per_s: tokens regenerated per second
// Both zero ⇒ limiter disabled (every TryAcquire passes).
struct RateLimitConfig
{
    std::uint32_t  burst         = 0;   // disabled by default
    std::uint32_t  refill_per_s  = 0;
};

// T5 graceful shutdown — on SIGINT/SIGTERM the accept loop stops
// immediately, then we sleep for this many milliseconds to let
// in-flight handlers + outbound peer sends drain before io.stop().
struct ShutdownConfig
{
    std::uint32_t  drain_ms = 2000;
};

// T6 admin TCP shell. Bind defaults to localhost. Secret is
// optional; bind+secret is the production posture.
struct AdminConfig
{
    std::string    bind   = "127.0.0.1";
    std::uint16_t  port   = 0;       // 0 = disabled
    std::string    secret;            // empty = no auth (bind protects)
};

struct AppConfig
{
    // Listener configuration consumed by MapServer. Holds port,
    // max_connections, and the RC4 key shared with the legacy client.
    MapServerConfig server;

    // Game mode (replaces legacy compile-time flags).
    Mode           mode = Mode::PvE;

    // TUSER DB (TCURRENTUSER for the F4 handshake; TCHARTABLE for F5+).
    DbConfig       database;

    // TWorldSvr peer (F5+).
    WorldPeerConfig world;

    // TLogSvrAsio audit UDP shim (T3+).
    AuditPeerConfig audit;

    // T5 rate limiter (per-session token bucket).
    RateLimitConfig rate_limit;

    // T5 graceful shutdown timing.
    ShutdownConfig  shutdown;

    // T6 admin TCP shell.
    AdminConfig     admin;

    // Health endpoint port. 0 disables.
    std::uint16_t  health_port = 8916;

    // T6 metrics endpoint port (Prometheus text format). 0 disables.
    std::uint16_t  metrics_port = 8917;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tmapsvr
