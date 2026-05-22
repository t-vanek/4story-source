#pragma once

// Internal-only logging adapter shared between AsioSession and
// TlsAsioSession. The public API is AsioSession::SetErrorLogger;
// both transports route their non-fatal protocol diagnostics
// through the single sink set by that call so operators see one
// stream regardless of which transport hit the failure.
//
// Not exported through TNetLib.h — this header is only included by
// the two session .cpp files. Behaviour identical to a printf-style
// helper: format into a 256-byte stack buffer, emit if a sink is
// registered, no-op otherwise.

#include "asio_session.h"

#include <atomic>
#include <cstdarg>

namespace tnetlib::detail {

// Defined in asio_session.cpp; declared here so tls_asio_session.cpp
// can route through the same sink.
extern std::atomic<AsioSession::ErrorLogger> g_proto_logger;

void LogProto(const char* fmt, ...) noexcept;

} // namespace tnetlib::detail
