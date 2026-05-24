#pragma once

// CtrlSvrSlot — single-cell store for the control-server peer
// reference, set by `OnCtCtrlsvrReq` when a connecting peer
// announces itself via `CT_CTRLSVR_REQ` (legacy SSHandler.cpp:207
// stores it on `m_pCtrlSvr`). Handlers that route a reply back to
// the operator console call `Get()` to obtain the live session.
//
// Held as a weak_ptr so a disconnected ctrl-svr session doesn't
// linger forever — the PeerRegistry owns the strong reference and
// drops it on the read-loop exit; our `Get()` returns nullptr once
// that happens, matching the legacy "ctrl-svr offline, drop the
// reply" branches.

#include "../peer_session.h"

#include <memory>
#include <shared_mutex>

namespace tworldsvr {

class CtrlSvrSlot
{
public:
    CtrlSvrSlot() = default;
    CtrlSvrSlot(const CtrlSvrSlot&) = delete;
    CtrlSvrSlot& operator=(const CtrlSvrSlot&) = delete;

    // Register the peer as the cluster's ctrl-svr. Late re-registration
    // (a fresh ctrl-svr connect after the previous one died) replaces
    // the slot — same shape as legacy `m_pCtrlSvr = pSERVER;` with no
    // duplicate-detection.
    void Set(std::shared_ptr<PeerSession> peer);

    // Returns the live ctrl-svr session, or nullptr if the slot is
    // empty or the previously-registered peer has disconnected.
    std::shared_ptr<PeerSession> Get() const;

    // Drop the slot. Optional — callers can rely on the weak_ptr
    // expiring naturally; provided for tests + ops triggers.
    void Clear();

private:
    mutable std::shared_mutex     m_lock;
    std::weak_ptr<PeerSession>    m_peer;
};

} // namespace tworldsvr
