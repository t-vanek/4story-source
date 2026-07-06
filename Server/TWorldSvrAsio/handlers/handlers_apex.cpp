// W6-39 — APEX (Taiwan anti-addiction) stubs.
//
// All four legacy handlers (SSHandler.cpp:14538-14616) are wrapped in
// `#ifdef __TW_APEX`, and TWorldType.h:157 ships the define
// **commented out** — the reference binary compiles them as empty
// bodies that accept the packet and do nothing. The enabled variant
// relays opaque APEX SDK blobs between the external Apex daemon and
// the char's main map (SM_APEXDATA → MW_APEXDATA_REQ), force-closes
// over-limit users (SM_APEXKILLUSER → CloseChar), and calls
// ApexNotifyUserData / ApexNotifyUserReturn — a third-party
// anti-addiction service SDK (ApexProxy.h), which is out of scope for
// the emulator the same way the HShield / XTrap anti-cheat callouts
// are (see the root README).
//
// We port the *shipped* behavior: accept + log + drop, so a hybrid
// deployment with a TW-flavored peer doesn't spam "unhandled wID"
// warnings. If a TW deployment ever becomes a target, these four
// stubs are the hook points.

#include "handlers.h"

#include <spdlog/spdlog.h>

namespace tworldsvr::handlers {

namespace {

boost::asio::awaitable<void>
ApexStub(const char* name, std::shared_ptr<PeerSession>& peer,
         const std::vector<std::byte>& body)
{
    spdlog::info("{}[{}]: APEX packet dropped ({} bytes) — __TW_APEX "
                 "is disabled in the reference build; the Apex SDK "
                 "callout is out of scope",
        name, peer->Wire()->RemoteIPv4(), body.size());
    co_return;
}

} // namespace

boost::asio::awaitable<void>
OnSmApexDataReq(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        /*ctx*/)
{
    co_await ApexStub("OnSmApexDataReq", peer, body);
}

boost::asio::awaitable<void>
OnSmApexKillUserReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        /*ctx*/)
{
    co_await ApexStub("OnSmApexKillUserReq", peer, body);
}

boost::asio::awaitable<void>
OnMwApexDataAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        /*ctx*/)
{
    co_await ApexStub("OnMwApexDataAck", peer, body);
}

boost::asio::awaitable<void>
OnMwApexStartAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        /*ctx*/)
{
    co_await ApexStub("OnMwApexStartAck", peer, body);
}

} // namespace tworldsvr::handlers
