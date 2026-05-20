#pragma once

// IAlerter — fires the legacy SendSvrStatusSMS path:
//
//   { CALL OPTool_SMSEmergency(?, ?, ?) }
//   INPUT bSvrType, dwSvrID, bSvrStatus
//
// Triggered from the peer-keepalive watchdog when a daemon has
// gone offline (legacy `SendSvrStatusSMS` is called on the
// 60s-timeout branch in OnCT_TIMER_REQ).
//
// The spdlog implementation is the safe default — operators see
// the alert on the audit channel without paging anyone. Production
// deploys swap in SociAlerter to fire SMS via the SP.

#include <cstdint>

namespace tcontrolsvr {

class IAlerter
{
public:
    virtual ~IAlerter() = default;

    virtual void Notify(std::uint8_t svr_type,
                        std::uint32_t svr_id,
                        std::uint8_t svr_status) = 0;
};

} // namespace tcontrolsvr
