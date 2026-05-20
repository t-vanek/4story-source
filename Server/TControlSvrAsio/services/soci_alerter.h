#pragma once

// SOCI-backed IAlerter — fires the legacy OPTool_SMSEmergency SP.
// Production deploys with a paging integration link this in;
// dev/staging stays on SpdlogAlerter.

#include "alerter.h"
#include "fourstory/db/session_pool.h"

namespace tcontrolsvr {

class SociAlerter final : public IAlerter
{
public:
    explicit SociAlerter(fourstory::db::SessionPool& pool);

    void Notify(std::uint8_t svr_type,
                std::uint32_t svr_id,
                std::uint8_t svr_status) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tcontrolsvr
