#pragma once

// SOCI-backed IUserProtectedService — calls TUserProtectedAdd on
// the configured DB pool. The legacy SP is part of TGLOBAL_RAGEZONE
// and accepts the parameter set documented in user_protected_service.h.

#include "user_protected_service.h"
#include "fourstory/db/session_pool.h"
#include "native_control.h"
#include <atomic>

namespace tcontrolsvr {

class SociUserProtectedService final : public IUserProtectedService
{
public:
    explicit SociUserProtectedService(fourstory::db::SessionPool& pool,
        std::shared_ptr<native_control::Owner> owner = {});

    std::uint8_t AddBan(const std::string& user_id,
                        std::uint32_t duration_days,
                        const std::string& reason,
                        std::uint8_t permanent,
                        const std::string& operator_id) override;

private:
    std::shared_ptr<native_control::Owner> m_owner;
    std::atomic_bool m_commit_uncertain{false};
    fourstory::db::SessionPool& m_pool;
};

} // namespace tcontrolsvr
