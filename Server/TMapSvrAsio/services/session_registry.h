#pragma once

// ISessionRegistry — maps char_id → live AsioSession for AOI broadcasts.
//
// When player A moves into player B's AOI, MapSvr needs to send
// CS_ENTER_ACK to B's session and CS_MOVE_ACK to all shared-AOI
// neighbours. The registry provides O(1) session lookup by char_id.
//
// Lifetime contract: Register is called from OnConnectReq (cluster)
// or OnConReadyReq (standalone) when the player enters the world.
// Unregister is called from HandleConnection on session close (RAII
// guard ensures cleanup even on exception).

#include "asio_session.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace tmapsvr {

class ISessionRegistry
{
public:
    virtual ~ISessionRegistry() = default;

    virtual void Register(std::uint32_t char_id,
                          std::shared_ptr<tnetlib::AsioSession> sess) = 0;

    virtual void Unregister(std::uint32_t char_id) = 0;

    // Returns nullptr if char_id not registered.
    virtual std::shared_ptr<tnetlib::AsioSession>
        Get(std::uint32_t char_id) const = 0;

    virtual std::size_t Size() const = 0;
};

// Production implementation — in-memory, single-threaded (same io_context).
class LocalSessionRegistry : public ISessionRegistry
{
public:
    void Register(std::uint32_t char_id,
                  std::shared_ptr<tnetlib::AsioSession> sess) override
    {
        m_sessions[char_id] = std::move(sess);
    }

    void Unregister(std::uint32_t char_id) override
    {
        m_sessions.erase(char_id);
    }

    std::shared_ptr<tnetlib::AsioSession>
    Get(std::uint32_t char_id) const override
    {
        auto it = m_sessions.find(char_id);
        return it != m_sessions.end() ? it->second : nullptr;
    }

    std::size_t Size() const override { return m_sessions.size(); }

private:
    std::unordered_map<std::uint32_t,
        std::shared_ptr<tnetlib::AsioSession>> m_sessions;
};

// Test stub — records Register/Unregister calls; injects fake sessions.
class FakeSessionRegistry : public ISessionRegistry
{
public:
    void Register(std::uint32_t char_id,
                  std::shared_ptr<tnetlib::AsioSession> sess) override
    {
        m_sessions[char_id] = std::move(sess);
    }

    void Unregister(std::uint32_t char_id) override
    {
        m_sessions.erase(char_id);
    }

    std::shared_ptr<tnetlib::AsioSession>
    Get(std::uint32_t char_id) const override
    {
        auto it = m_sessions.find(char_id);
        return it != m_sessions.end() ? it->second : nullptr;
    }

    std::size_t Size() const override { return m_sessions.size(); }

    bool Has(std::uint32_t char_id) const
    {
        return m_sessions.count(char_id) > 0;
    }

private:
    std::unordered_map<std::uint32_t,
        std::shared_ptr<tnetlib::AsioSession>> m_sessions;
};

} // namespace tmapsvr
