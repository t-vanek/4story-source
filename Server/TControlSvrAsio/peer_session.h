#pragma once

// PeerSession — wraps a ControlSession when the remote endpoint is a
// peer daemon (LoginSvr / MapSvr / WorldSvr / PatchSvr / LogSvr / ...)
// rather than a GM operator. Legacy CTServer holds the equivalent
// state. Two arrival paths:
//
//   - Inbound: peers connect to TControlSvr after handshake. F2 will
//     flip an OperatorSession to a PeerSession when the first
//     CT_CTRLSVR_REQ / CT_SERVICEMONITOR_ACK arrives.
//   - Outbound: TControlSvr dials each registered service on demand
//     via CT_NEWCONNECT_REQ. F2 implements the dialer.
//
// F1 keeps the type around so the handler chain can compile against
// the planned shape and peer-aware tests can stand it up directly.

#include "control_session.h"
#include "services/service_inventory.h"

#include <cstdint>
#include <memory>

namespace tcontrolsvr {

class PeerSession : public std::enable_shared_from_this<PeerSession>
{
public:
    PeerSession(std::shared_ptr<ControlSession> sess,
                ServiceInstance svc)
        : m_sess(std::move(sess))
        , m_svc(std::move(svc))
    {}

    const std::shared_ptr<ControlSession>& Wire() const { return m_sess; }
    const ServiceInstance& Service() const              { return m_svc; }

    std::uint32_t ServiceId() const { return m_svc.service_id; }

private:
    std::shared_ptr<ControlSession>  m_sess;
    ServiceInstance                  m_svc;
};

} // namespace tcontrolsvr
