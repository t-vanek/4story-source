#pragma once

// SOCI-backed map session validator. Reads TCURRENTUSER by
// (dwUserID, dwKEY) to confirm the session token TLoginSvrAsio
// wrote at login is still current. Schema is validated at boot by
// tmapsvr::db::ValidateUserSchema.

#include "session_validator.h"

#include <cstdint>
#include <optional>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociMapSessionValidator final : public IMapSessionValidator
{
public:
    explicit SociMapSessionValidator(fourstory::db::SessionPool& pool);

    std::optional<MapSessionInfo>
        LookupSession(std::uint32_t user_id, std::uint32_t key) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tmapsvr
