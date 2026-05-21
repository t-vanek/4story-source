#include "audit_log.h"

#include "services/log_peer.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <span>

namespace tmapsvr::audit {

namespace {

std::uint64_t NowMillis()
{
    using clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now().time_since_epoch()).count();
}

// Cast a POD event to a span of bytes for the UDP send. Each event
// struct above is a fixed-layout POD; reinterpret_cast is the
// honest tool here. Receivers reconstruct via memcpy in the same
// order.
template <class Event>
std::span<const std::byte> AsBytes(const Event& ev)
{
    static_assert(std::is_trivially_copyable_v<Event>,
                  "audit events must be trivially copyable");
    return { reinterpret_cast<const std::byte*>(&ev), sizeof(Event) };
}

} // namespace

const char* EventKindName(EventKind k)
{
    switch (k) {
        case EventKind::Unknown:       return "unknown";
        case EventKind::LoginAttempt:  return "login_attempt";
        case EventKind::CharLoad:      return "char_load";
        case EventKind::HandlerInvoke: return "handler_invoke";
        case EventKind::HandlerError:  return "handler_error";
    }
    return "?";
}

AuditLog::AuditLog(ILogPeer* peer)
    : m_peer(peer)
{
}

std::uint32_t AuditLog::NextCorrelation()
{
    // Wraparound is intentional — we don't need globally unique
    // ids, just locally distinct within a short window.
    return m_next_corr++;
}

void AuditLog::Emit(const LoginAttemptEvent& src)
{
    // Local copy so we can stamp the header without const-cast.
    LoginAttemptEvent ev = src;
    ev.hdr.kind  = static_cast<std::uint16_t>(EventKind::LoginAttempt);
    ev.hdr.size  = sizeof(ev);
    ev.hdr.ts_ms = NowMillis();

    spdlog::info("[audit] login uid={} key={} char={} ch={} ver={} result={} corr={}",
        ev.user_id, ev.key, ev.char_id, ev.channel, ev.version, ev.result,
        ev.hdr.corr);

    if (m_peer && m_peer->Enabled())
        m_peer->Send(AsBytes(ev));
}

void AuditLog::Emit(const CharLoadEvent& src)
{
    CharLoadEvent ev = src;
    ev.hdr.kind  = static_cast<std::uint16_t>(EventKind::CharLoad);
    ev.hdr.size  = sizeof(ev);
    ev.hdr.ts_ms = NowMillis();

    spdlog::info("[audit] char_load char={} key={} user={} latency_us={} result={} corr={}",
        ev.char_id, ev.key, ev.user_id, ev.latency_us, ev.result, ev.hdr.corr);

    if (m_peer && m_peer->Enabled())
        m_peer->Send(AsBytes(ev));
}

void AuditLog::Emit(const HandlerInvokeEvent& src)
{
    HandlerInvokeEvent ev = src;
    ev.hdr.kind  = static_cast<std::uint16_t>(
        ev.ok ? EventKind::HandlerInvoke : EventKind::HandlerError);
    ev.hdr.size  = sizeof(ev);
    ev.hdr.ts_ms = NowMillis();

    // HandlerInvoke is high-frequency — mirror at debug level so
    // production spdlog isn't drowned. Errors go to warn so they
    // surface in the default level.
    if (ev.ok)
    {
        spdlog::debug("[audit] handler wId=0x{:04X} body={} latency_us={} corr={}",
            ev.wId, ev.body_size, ev.latency_us, ev.hdr.corr);
    }
    else
    {
        spdlog::warn("[audit] handler wId=0x{:04X} body={} latency_us={} "
                     "FAILED corr={}",
            ev.wId, ev.body_size, ev.latency_us, ev.hdr.corr);
    }

    if (m_peer && m_peer->Enabled())
        m_peer->Send(AsBytes(ev));
}

} // namespace tmapsvr::audit
