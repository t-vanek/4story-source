#pragma once

#include "alerter.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

class SpdlogAlerter final : public IAlerter
{
public:
    void Notify(std::uint8_t svr_type,
                std::uint32_t svr_id,
                std::uint8_t svr_status) override
    {
        spdlog::warn("alerter: svr_type={} svr_id={:08x} status={} "
                     "— SMS path disabled (using spdlog default)",
            svr_type, svr_id, svr_status);
    }
};

} // namespace tcontrolsvr
