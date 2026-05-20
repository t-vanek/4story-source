#pragma once

#include "service_inventory.h"

#include <utility>

namespace tcontrolsvr {

class FakeServiceInventory final : public IServiceInventory
{
public:
    void AddGroup(Group g)                 { m_groups.push_back(std::move(g)); }
    void AddMachine(Machine m)             { m_machines.push_back(std::move(m)); }
    void AddType(ServerType t)             { m_types.push_back(std::move(t)); }
    void AddService(ServiceInstance s)     { m_services.push_back(std::move(s)); }

    const std::vector<Group>&            Groups()   const override { return m_groups; }
    const std::vector<Machine>&          Machines() const override { return m_machines; }
    const std::vector<ServerType>&       Types()    const override { return m_types; }
    const std::vector<ServiceInstance>&  Services() const override { return m_services; }

private:
    std::vector<Group>            m_groups;
    std::vector<Machine>          m_machines;
    std::vector<ServerType>       m_types;
    std::vector<ServiceInstance>  m_services;
};

} // namespace tcontrolsvr
