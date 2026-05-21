#pragma once

// Helpers shared between handler implementation files. Anything used by
// more than one handler .cpp lives here; helpers used by exactly one
// handler stay in that file's anonymous namespace.

#include "asio_session.h"
#include "handlers.h"
#include "services/session_registry.h"

#include <cstdint>
#include <memory>

namespace tmapsvr::handlers_detail {

// Look up the sender's char id for log lines. Returns 0 when the
// session hasn't bound a char yet (the request arrived before
// CS_CONNECT_REQ cleared, or the handler is being called on a
// pre-auth connection). Used by chat / party / bow handlers; the
// real auth gate that should reject pre-auth requests lands in the
// observability consolidation pass.
inline std::uint32_t SenderCharId(
    const std::shared_ptr<tnetlib::AsioSession>& sess,
    const HandlerContext& ctx)
{
    if (!ctx.session_reg) return 0;
    const auto found = ctx.session_reg->FindCharIdBySession(sess.get());
    return found ? *found : 0;
}

} // namespace tmapsvr::handlers_detail
