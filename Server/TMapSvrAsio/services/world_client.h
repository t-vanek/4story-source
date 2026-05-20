#pragma once

// IWorldClient — abstract outbound connection to TWorldSvr.
//
// Normal cluster sequence (DM_LOADCHAR happens BEFORE CS_CONNECT_REQ):
//
//   WorldSvr → MapSvr: DM_LOADCHAR_REQ  (MapSvr loads char from DB)
//   MapSvr → WorldSvr: DM_LOADCHAR_ACK  (full char record)
//   [char snapshot stored in pre-load cache]
//   Client  → MapSvr: CS_CONNECT_REQ
//   MapSvr → WorldSvr: MW_ADDCHAR_ACK   (client IP/port/uid)
//   [snapshot taken from cache → CS_CHARINFO_ACK sent on CONREADY]
//
// Race sequence (CS_CONNECT_REQ before DM_LOADCHAR):
//   Client  → MapSvr: CS_CONNECT_REQ
//   MapSvr: RegisterPendingSession(char_id, sess, state_weak)
//   MapSvr → WorldSvr: MW_ADDCHAR_ACK
//   WorldSvr → MapSvr: DM_LOADCHAR_REQ → ACK
//   WorldSvr → MapSvr: MW_CONRESULT_REQ (connection approval)
//   AsioWorldClient: routes CS_CHARINFO_ACK to pending client session
//
// Source: Server/TMapSvr/SSSender.cpp — SendMW_ADDCHAR_ACK
//         Server/TMapSvr/SSHandler.cpp — OnDM_LOADCHAR_REQ
//         Server/TMapSvr/SSHandler.cpp:1332 — OnMW_CONRESULT_REQ

#include "asio_session.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
namespace tmapsvr::legacy { struct CharSnapshot; }
namespace tmapsvr         { struct MapSessionState; }

namespace tmapsvr {

class IWorldClient
{
public:
    virtual ~IWorldClient() = default;

    // ── Sending to WorldSvr ──────────────────────────────────────────

    // Send MW_ADDCHAR_ACK — notifies WorldSvr that a client connected.
    // client_ip_raw: IPv4 in host byte order.
    // Wire: dwCharID, dwKEY, dwIPAddr, wPort, dwUserID
    // Source: SSSender.cpp:SendMW_ADDCHAR_ACK
    virtual void SendMwAddCharAck(
        std::uint32_t char_id,
        std::uint32_t dw_key,
        std::uint32_t client_ip_raw,
        std::uint16_t client_port,
        std::uint32_t user_id) = 0;

    // Send DM_LOADCHAR_ACK in response to DM_LOADCHAR_REQ.
    // snap == nullptr → CN_NOCHAR.
    virtual void SendDmLoadCharAck(
        std::uint32_t               char_id,
        std::uint32_t               dw_key,
        const legacy::CharSnapshot* snap) = 0;

    // ── Pending session registry (race path) ─────────────────────────

    // Register a client session waiting for MW_CONRESULT_REQ so
    // CS_CHARINFO_ACK can be routed back. Called when pre-load cache
    // misses on CS_CONNECT_REQ (race: client connected before
    // DM_LOADCHAR round-trip completed).
    virtual void RegisterPendingSession(
        std::uint32_t                         char_id,
        std::shared_ptr<tnetlib::AsioSession> sess,
        std::weak_ptr<MapSessionState>        state_weak) = 0;

    // Remove pending entry on session close (prevents routing to
    // expired weak_ptr).
    virtual void CancelPendingSession(std::uint32_t char_id) = 0;

    // ── Pre-loaded snapshot cache (normal path) ───────────────────────

    // Store a snapshot after DM_LOADCHAR_REQ is handled (before the
    // client connects in the normal sequence).
    virtual void StorePreloadedChar(legacy::CharSnapshot snap) = 0;

    // Take the pre-loaded snapshot for char_id. Validates user_id +
    // dw_key. Returns nullopt if not cached or credentials mismatch.
    // Removes the entry from the cache on success.
    virtual std::optional<legacy::CharSnapshot>
        TakePreloadedChar(std::uint32_t char_id,
                          std::uint32_t user_id,
                          std::uint32_t dw_key) = 0;

    // Returns true if the underlying TCP connection to WorldSvr is live.
    virtual bool IsConnected() const = 0;
};

// ---------------------------------------------------------------------------
// FakeWorldClient — in-memory stub for unit tests.
// ---------------------------------------------------------------------------
//
// Header-only: no CharSnapshot / MapSessionState definition needed in
// the methods below (only pointers / weak_ptr / nullopt used).
// FakeWorldClient does NOT send CS_CHARINFO_ACK on SimulateConResult
// because the full sess+state wiring isn't set up in pure-unit tests;
// tests verify snapshot delivery instead.

class FakeWorldClient : public IWorldClient
{
public:
    struct AddCharAckCall { std::uint32_t char_id, dw_key, client_ip_raw, user_id; std::uint16_t client_port; };
    struct LoadCharAckCall { std::uint32_t char_id, dw_key; bool success; };
    struct PendingEntry    { std::shared_ptr<tnetlib::AsioSession> sess; std::weak_ptr<MapSessionState> state_weak; };

    const std::vector<AddCharAckCall>&  AddCharAckCalls()  const { return m_add_char; }
    const std::vector<LoadCharAckCall>& LoadCharAckCalls() const { return m_load_char; }
    bool HasPendingSession(std::uint32_t char_id)          const { return m_pending.count(char_id) > 0; }
    bool HasPreloaded(std::uint32_t char_id)               const { return m_preloaded.count(char_id) > 0; }

    void SendMwAddCharAck(std::uint32_t char_id, std::uint32_t dw_key,
                          std::uint32_t ip, std::uint16_t port,
                          std::uint32_t user_id) override
    {
        m_add_char.push_back({ char_id, dw_key, ip, user_id, port });
    }

    void SendDmLoadCharAck(std::uint32_t char_id, std::uint32_t dw_key,
                           const legacy::CharSnapshot* snap) override
    {
        m_load_char.push_back({ char_id, dw_key, snap != nullptr });
    }

    void RegisterPendingSession(std::uint32_t char_id,
                                std::shared_ptr<tnetlib::AsioSession> sess,
                                std::weak_ptr<MapSessionState> state_weak) override
    {
        m_pending[char_id] = { std::move(sess), state_weak };
    }

    void CancelPendingSession(std::uint32_t char_id) override
    {
        m_pending.erase(char_id);
    }

    void StorePreloadedChar(legacy::CharSnapshot snap) override;

    std::optional<legacy::CharSnapshot>
        TakePreloadedChar(std::uint32_t char_id,
                          std::uint32_t user_id,
                          std::uint32_t dw_key) override;

    bool IsConnected() const override { return m_connected; }
    void SetConnected(bool v)          { m_connected = v; }

    // Test helper: simulate MW_CONRESULT_REQ delivering snapshot to
    // a registered pending session.
    void SimulateConResult(std::uint32_t char_id,
                           legacy::CharSnapshot snap);

private:
    std::vector<AddCharAckCall>                               m_add_char;
    std::vector<LoadCharAckCall>                              m_load_char;
    std::unordered_map<std::uint32_t, PendingEntry>           m_pending;
    std::unordered_map<std::uint32_t, legacy::CharSnapshot>   m_preloaded;
    bool                                                       m_connected = true;
};

} // namespace tmapsvr
