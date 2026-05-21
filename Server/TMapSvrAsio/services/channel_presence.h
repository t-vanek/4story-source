#pragma once

// Per-channel presence map — char_id → (channel, position,
// AsioSession). Populated when CS_CONNECT_REQ clears (so the
// channel comes from the wire-claimed bChannel) and updated on each
// CS_MOVE_REQ. The CS_MOVE_REQ handler iterates ForEachInChannel
// to broadcast a CS_MOVE_ACK to every other client on the same map
// channel — F7's flat-list AOI (real grid lands with F8/F9 as the
// load grows).
//
// Modeled on the legacy CTChannel::m_listPLAYER (TChannel.h:14) plus
// the CS_MOVE_REQ broadcast path in legacy CSHandler.cpp:613.
//
// Thread-safety: a mutex guards reads and writes for the same reason
// session_registry has one — SOCI worker pools (F8+) may touch the
// map off the io_context thread.

#include "asio_session.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

struct Position
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct ChannelPresenceEntry
{
    std::uint32_t                          char_id  = 0;
    std::uint8_t                           channel  = 0;
    std::uint16_t                          map_id   = 0;
    Position                               pos;
    std::weak_ptr<tnetlib::AsioSession>    session;
};

class IChannelPresence
{
public:
    virtual ~IChannelPresence() = default;

    virtual void Bind(std::uint32_t char_id,
                      std::uint8_t channel,
                      std::shared_ptr<tnetlib::AsioSession> session) = 0;

    virtual void Unbind(std::uint32_t char_id) = 0;

    // Remove any entry whose weak_ptr locks to `session`. Used by the
    // per-connection teardown to clean up after dropped sockets.
    virtual std::size_t UnbindIfMatches(const tnetlib::AsioSession* session) = 0;

    virtual void UpdatePosition(std::uint32_t char_id,
                                std::uint16_t map_id,
                                Position pos) = 0;

    // Snapshot of the entry for `char_id`, or nullopt when absent.
    // Returned by value so the caller doesn't hold a lock or pointer
    // into the map across co_awaits.
    virtual std::optional<ChannelPresenceEntry>
        FindEntry(std::uint32_t char_id) const = 0;

    using Visitor = std::function<void(const ChannelPresenceEntry&,
                                       std::shared_ptr<tnetlib::AsioSession>)>;

    // Walk every char on the given channel and invoke `visit` with its
    // entry + locked session. Entries whose weak_ptr has expired are
    // skipped (they'll get pruned on the next Unbind / UnbindIfMatches).
    // The optional `skip_char_id` lets a broadcast omit the sender,
    // which is the legacy convention for CS_MOVE_ACK.
    virtual void ForEachInChannel(std::uint8_t channel,
                                  std::uint32_t skip_char_id,
                                  const Visitor& visit) const = 0;

    virtual std::size_t Size() const = 0;
};

class InMemoryChannelPresence final : public IChannelPresence
{
public:
    void Bind(std::uint32_t char_id,
              std::uint8_t channel,
              std::shared_ptr<tnetlib::AsioSession> session) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        auto& e   = m_rows[char_id];
        e.char_id = char_id;
        e.channel = channel;
        e.session = std::move(session);
    }

    void Unbind(std::uint32_t char_id) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows.erase(char_id);
    }

    std::size_t UnbindIfMatches(const tnetlib::AsioSession* session) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::size_t removed = 0;
        for (auto it = m_rows.begin(); it != m_rows.end();)
        {
            auto sp = it->second.session.lock();
            if (!sp || sp.get() == session)
            {
                it = m_rows.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        return removed;
    }

    void UpdatePosition(std::uint32_t char_id,
                        std::uint16_t map_id,
                        Position pos) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return;
        it->second.map_id = map_id;
        it->second.pos    = pos;
    }

    std::optional<ChannelPresenceEntry>
        FindEntry(std::uint32_t char_id) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    void ForEachInChannel(std::uint8_t channel,
                          std::uint32_t skip_char_id,
                          const Visitor& visit) const override
    {
        // Snapshot under the lock then visit outside — visitors may
        // call SendPacket which yields back to the executor, and we
        // don't want to hold the registry mutex across co_awaits.
        std::vector<std::pair<ChannelPresenceEntry,
                              std::shared_ptr<tnetlib::AsioSession>>> snap;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            snap.reserve(m_rows.size());
            for (const auto& [cid, e] : m_rows)
            {
                if (e.channel != channel) continue;
                if (e.char_id == skip_char_id) continue;
                auto sp = e.session.lock();
                if (!sp) continue;
                snap.emplace_back(e, std::move(sp));
            }
        }
        for (auto& [entry, sp] : snap)
            visit(entry, sp);
    }

    std::size_t Size() const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_rows.size();
    }

private:
    mutable std::mutex                                       m_mtx;
    std::unordered_map<std::uint32_t, ChannelPresenceEntry>  m_rows;
};

} // namespace tmapsvr
