#pragma once

// SOCI-backed IEventRepository — reads TEVENTCHART /
// TCASHSHOPITEMCHART and calls the TEventUpdate SP. Schema column
// list matches Server/TControlSvr/DBAccess.h CTBLEvent +
// CTBLCashShopItem + CSPEventUpdate.

#include "event_repository.h"
#include "fourstory/db/session_pool.h"

namespace tcontrolsvr {

class SociEventRepository final : public IEventRepository
{
public:
    explicit SociEventRepository(fourstory::db::SessionPool& pool);

    std::vector<EventInfo> LoadAll() override;
    std::vector<CashItem>  ListCashItems() override;
    std::uint8_t           Persist(const EventInfo& ev,
                                   std::uint8_t op,
                                   const std::string& value_blob) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tcontrolsvr
