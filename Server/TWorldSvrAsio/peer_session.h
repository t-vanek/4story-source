#pragma once

// PeerSession — wraps a WorldSession with the per-map-server state
// that handlers need to make routing decisions: the server's wID
// (set by RW_RELAYSVR_REQ at handshake time), the cluster's nation
// flag, and a slot for any per-peer state that arrives in W5+ (war
// country flags, channel list, etc.).
//
// Lifetime:
//   * Constructed once per accepted TCP connection by
//     WorldServer::HandleConnection, before the read loop spawns.
//   * Carried through every dispatch call as the first argument so
//     handlers can answer "which map server is this packet from?"
//     without going through PeerRegistry.
//   * Registered into PeerRegistry by OnRelaysvrReq once the wID
//     arrives; unregistered when the read loop returns (peer
//     disconnect) — guarantees the registry only ever holds
//     references to live wire sessions.
//
// Parallel to TControlSvrAsio's OperatorSession / PeerSession split
// — we ship one wrapper here because TWorldSvr only ever talks to
// peer daemons (no human-operator side).

#include "world_session.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace tworldsvr {

class PeerSession : public std::enable_shared_from_this<PeerSession>
{
public:
    explicit PeerSession(std::shared_ptr<WorldSession> wire)
        : m_wire(std::move(wire)) {}

    PeerSession(const PeerSession&) = delete;
    PeerSession& operator=(const PeerSession&) = delete;

    // Underlying framer — used by senders to write outbound packets
    // and by handlers that need the remote IP for logging.
    const std::shared_ptr<WorldSession>& Wire() const { return m_wire; }

    // wID is set by RW_RELAYSVR_REQ. 0 means "not yet registered".
    // PeerRegistry only indexes peers with non-zero wID; handlers
    // that fan out to "every map server" can therefore enumerate
    // the registry without seeing half-registered peers.
    std::uint16_t Wid() const { return m_wid.load(); }
    void SetWid(std::uint16_t wid) { m_wid.store(wid); }

    // Cluster-nation byte (TCONTRY_A / TCONTRY_B / TCONTRY_N) the
    // world server advertises back to peers in RW_RELAYSVR_ACK.
    // The legacy module reads it from CTWorldSvrModule::m_bNation
    // and broadcasts the same byte to every peer that registers.
    // Stored on the peer for symmetry; W5 castle-war handlers will
    // expand this with per-peer war-country flags.
    std::uint8_t Nation() const
    {
        std::lock_guard g(m_mtx);
        return m_nation;
    }
    void SetNation(std::uint8_t n)
    {
        std::lock_guard g(m_mtx);
        m_nation = n;
    }

private:
    std::shared_ptr<WorldSession> m_wire;
    std::atomic<std::uint16_t>    m_wid{0};
    mutable std::mutex            m_mtx;
    std::uint8_t                  m_nation = 0;
};

} // namespace tworldsvr
