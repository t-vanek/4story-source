#pragma once

// Authority gate — mirror of legacy `CTManager::CheckAuthority(BYTE
// bClass)` (Server/TControlSvr/TManager.cpp). Each admin handler
// states the minimum OperatorRole required; non-matching operators
// receive `CT_AUTHORITY_ACK` and the handler short-circuits.
//
// Legacy contract:
//   - The MANAGER_CLASS enum is hierarchical only at the boundary
//     case (authority 1 = MANAGER_ALL = full access). The other
//     numeric values are *categories* rather than ranks: a level-2
//     operator can do GM_LEVEL_2 actions but cannot do SERVICE
//     (which is category 4). The legacy CheckAuthority returns
//     TRUE iff (bAuthority == 1) || (bAuthority == bClass).
//   - On failure the manager receives CT_AUTHORITY_ACK and the
//     handler returns EC_NOERROR (silent reject).
//
// We preserve the same logic + the same wire response. Audit
// logging on denial lives in the caller (so it can include the
// requested action and target).

#include "../operator_session.h"

namespace tcontrolsvr {

// Check the operator's role against `required`. Returns true on
// pass. Authority 1 (MANAGER_ALL / OperatorRole::All) always passes.
inline bool HasAuthority(const OperatorSession& op, OperatorRole required)
{
    const auto raw = op.AuthorityRaw();
    if (raw == 0) return false;
    if (raw == static_cast<std::uint8_t>(OperatorRole::All)) return true;
    return raw == static_cast<std::uint8_t>(required);
}

} // namespace tcontrolsvr
