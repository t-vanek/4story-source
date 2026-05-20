// FakeWorldClient out-of-line implementations.
//
// Separated from world_client.h because StorePreloadedChar / TakePreloadedChar
// / SimulateConResult need the full CharSnapshot definition, and
// SimulateConResult needs MapSessionState (from handlers.h).
//
// This file is compiled into tmapsvr_asio_core because test binaries
// link that library and need FakeWorldClient in both tests and benchmarks.

#include "services/world_client.h"
#include "handlers.h"           // defines MapSessionState
#include "legacy_port/types_layer3.h"

#include <spdlog/spdlog.h>

namespace tmapsvr {

void FakeWorldClient::StorePreloadedChar(legacy::CharSnapshot snap)
{
    const auto id    = snap.char_id;
    m_preloaded[id]  = std::move(snap);
}

std::optional<legacy::CharSnapshot>
FakeWorldClient::TakePreloadedChar(std::uint32_t char_id,
                                   std::uint32_t user_id,
                                   std::uint32_t dw_key)
{
    auto it = m_preloaded.find(char_id);
    if (it == m_preloaded.end()) return std::nullopt;

    const auto& s = it->second;
    if (s.user_id != user_id || s.dw_key != dw_key)
    {
        spdlog::debug("FakeWorldClient::TakePreloadedChar: "
                      "char_id={} credentials mismatch", char_id);
        return std::nullopt;
    }

    auto snap = std::move(it->second);
    m_preloaded.erase(it);
    return snap;
}

void FakeWorldClient::SimulateConResult(std::uint32_t          char_id,
                                        legacy::CharSnapshot   snap)
{
    auto it = m_pending.find(char_id);
    if (it == m_pending.end())
    {
        spdlog::debug("FakeWorldClient::SimulateConResult: "
                      "no pending entry for char_id={}", char_id);
        return;
    }

    PendingEntry entry = std::move(it->second);
    m_pending.erase(it);

    auto state = entry.state_weak.lock();
    if (!state)
    {
        spdlog::debug("FakeWorldClient::SimulateConResult: "
                      "session state expired for char_id={}", char_id);
        return;
    }

    state->snapshot = std::move(snap);
    spdlog::debug("FakeWorldClient::SimulateConResult: "
                  "snapshot delivered to session for char_id={}", char_id);
}

} // namespace tmapsvr
