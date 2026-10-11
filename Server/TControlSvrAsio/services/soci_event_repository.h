#pragma once

// SOCI-backed IEventRepository — reads TEVENTCHART /
// TCASHSHOPITEMCHART and calls the TEventUpdate SP. Schema column
// list matches Server/TControlSvr/DBAccess.h CTBLEvent +
// CTBLCashShopItem + CSPEventUpdate.

#include "event_repository.h"
#include "fourstory/db/session_pool.h"
#include "native_control.h"
#include <atomic>

namespace tcontrolsvr {

class SociEventRepository final : public IEventRepository
{
public:
    explicit SociEventRepository(fourstory::db::SessionPool& pool,
        std::shared_ptr<native_control::Owner> owner = {});

    std::vector<EventInfo> LoadAll() override;
    std::vector<CashItem>  ListCashItems() override;
    std::uint8_t           Persist(const EventInfo& ev,
                                   std::uint8_t op,
                                   const std::string& value_blob) override;

private:
    std::shared_ptr<native_control::Owner> m_owner;
    std::atomic_bool m_commit_uncertain{false};
    fourstory::db::SessionPool& m_pool;
};

} // namespace tcontrolsvr
