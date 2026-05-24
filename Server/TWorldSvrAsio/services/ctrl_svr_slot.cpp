#include "ctrl_svr_slot.h"

#include <mutex>

namespace tworldsvr {

void CtrlSvrSlot::Set(std::shared_ptr<PeerSession> peer)
{
    std::unique_lock lk(m_lock);
    m_peer = peer;
}

std::shared_ptr<PeerSession> CtrlSvrSlot::Get() const
{
    std::shared_lock lk(m_lock);
    return m_peer.lock();
}

void CtrlSvrSlot::Clear()
{
    std::unique_lock lk(m_lock);
    m_peer.reset();
}

} // namespace tworldsvr
