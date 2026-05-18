#pragma once

// ISessionTerminator — runs the "user X is no longer connected to
// this Login server" cleanup. Called from:
//   * HandleConnection coroutine after RunPackets returns (the
//     session's TCP socket closed for whatever reason).
//   * Future explicit CS_TERMINATE_REQ handler (real impl, replaces
//     the Phase-3 stub that just closes the socket).
//
// Legacy server (TLogout SP) does:
//   * DELETE FROM TCURRENTUSER WHERE dwUserID = @user_id
//   * UPDATE TLOG SET timeLOGOUT = NOW() WHERE dwKEY = @session_key
//
// The modern interface keeps that two-step shape via the session_key
// parameter — so the same call can clean both the live-session
// directory and the audit log without requiring two service calls.
//
// Important hand-off case (legacy CSHandler.cpp:1428): after a
// successful CS_START_REQ, the client's about to reconnect to a
// Map server with the same dwKEY. The TCURRENTUSER row must NOT be
// deleted in that case — Map validates the key against the row.
// The terminator interface exposes a `mode` so callers can express
// "this is a Map-handoff close, leave the row" vs "real cleanup".

#include <cstdint>

namespace tloginsvr::services {

enum class TerminationReason : std::uint8_t
{
    // Default: clean up everything.
    Disconnect,
    // CS_TERMINATE_REQ from the client — explicit clean logout.
    // Identical to Disconnect for our purposes; logged differently
    // so ops can distinguish "user clicked quit" from "TCP died".
    ClientRequest,
    // Session is being handed off to a Map server (CS_START_REQ
    // succeeded). Leave TCURRENTUSER intact so Map can validate
    // the handoff dwKEY; just stamp the audit log timestamp.
    MapHandoff,
};

class ISessionTerminator
{
public:
    virtual ~ISessionTerminator() = default;

    // Called on session close. `user_id` and `session_key` come from
    // the AsioSession's auth-success state — both 0 if the session
    // never authenticated (terminator should no-op in that case).
    virtual void Terminate(std::int32_t user_id,
                           std::uint32_t session_key,
                           TerminationReason reason) = 0;
};

} // namespace tloginsvr::services
