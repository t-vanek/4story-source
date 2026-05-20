#pragma once

// MapServer — accept loop + per-connection AsioSession lifecycle +
// dispatch. Mirrors the LoginServer shape from TLoginSvrAsio so an
// operator who knows one knows the other.
//
// F1 wires the structural skeleton — handshake handler only. F2+ adds
// the gameplay handlers (movement, combat, items, …) behind the same
// dispatch surface in `handlers.cpp`.

#include "asio_session.h"
#include "handlers.h"
#include "services/session_validator.h"
#include "services/player_service.h"
#include "services/world_client.h"
#include "services/session_registry.h"
#include "map_state.h"
#include "monster_state.h"
#include "level_chart.h"
#include "player_hp_registry.h"
#include "inventory_service.h"
#include "loot_registry.h"
#include "npc_service.h"
#include "party_service.h"

#include "fourstory/ops/rate_limiter.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace tmapsvr {

struct MapServerConfig
{
    std::uint16_t                       port            = 5815;
    std::vector<std::byte>              rc4_secret_key;       // empty = no RC4 (test peers)
    std::vector<std::uint16_t>          accepted_versions = { 0x2918 };
    IMapSessionValidator*               validator       = nullptr;  // non-owning
    fourstory::ops::LoginRateLimiter*   connect_rate    = nullptr;  // non-owning; null disables
    std::chrono::seconds                pre_auth_timeout{ 30 };     // 0 disables
    std::uint32_t                       max_connections = 0;        // 0 = uncapped

    // Wire-protocol shutdown signal. Invoked when a peer (typically
    // TControlSvr) sends SM_QUITSERVICE_REQ. main() wires this to
    // io.stop() so the SIGTERM and wire-shutdown paths converge.
    // Null = log + ignore (the test-fixture default).
    std::function<void()>               on_quit_request;

    // F2b standalone: char loader (no TWorldSvr). If non-null and
    // world_client is null, LoadChar is called at CS_CONNECT_REQ time.
    IPlayerService*                     player_service  = nullptr;

    // F2b cluster: outbound world-server client.
    // If non-null, MW_ADDCHAR_ACK is sent to TWorldSvr instead of
    // loading char locally. The round-trip completes asynchronously.
    IWorldClient*                       world_client    = nullptr;

    // F3: AOI cell grid (one per map channel).
    // Non-null enables EnterMap/LeaveMap/OnMove broadcasts.
    IMapState*                          map_state       = nullptr;

    // F3: session lookup for AOI broadcasts.
    // Non-null enables CS_ENTER_ACK / CS_MOVE_ACK sends to neighbours.
    ISessionRegistry*                   session_registry = nullptr;

    // F4: live monster registry.
    IMonsterRegistry*                   monster_registry = nullptr;

    // F4: level chart for HP/exp/damage formulas.
    ILevelChart*                        level_chart      = nullptr;

    // F4: spawn manager — timer-based monster lifecycle + roam AI.
    // If non-null, Start() is called by MapServer after the listener
    // is ready. Null = no monster spawning.
    class ISpawnManager*                spawn_manager    = nullptr;

    // F4 Part 3: server-side player HP/MP tracking for monster damage.
    IPlayerHpRegistry*                  player_hp        = nullptr;

    // F5: live item inventory service.
    IInventoryService*                  inventory_svc    = nullptr;

    // F5 Part 2: monster loot store.
    ILootRegistry*                      loot_registry    = nullptr;

    // F6: NPC service.
    INpcService*                        npc_svc          = nullptr;

    // F6: party service (standalone).
    IPartyService*                      party_svc        = nullptr;
};

class MapServer
{
public:
    MapServer(boost::asio::io_context& io, MapServerConfig cfg);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_listener.Port(); }

    // Live session count — used by the /healthz endpoint and by tests.
    std::uint32_t LiveSessions() const
    {
        return m_active_connections.load(std::memory_order_relaxed);
    }

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<tnetlib::AsioSession> sess);

    boost::asio::io_context&         m_io;
    tnetlib::AsioListener            m_listener;
    MapServerConfig                  m_cfg;
    HandlerContext                   m_ctx;        // populated from cfg
    std::atomic<std::uint32_t>       m_active_connections{ 0 };
};

} // namespace tmapsvr
