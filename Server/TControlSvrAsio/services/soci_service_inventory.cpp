#include "soci_service_inventory.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <unordered_map>

namespace tcontrolsvr {

namespace {

// Match the legacy synthetic-id composition in
// CTControlSvrModule::FindService(DWORD): high 8 bits = groupID,
// mid 8 bits = type, low 8 bits = serverID. Keep the bit layout
// identical so a CT_NEWCONNECT_REQ from a legacy TController.exe
// (which still embeds the old dwID) still resolves correctly.
constexpr std::uint32_t MakeServiceId(std::uint8_t group_id,
                                      std::uint8_t type_id,
                                      std::uint8_t server_id)
{
    return (static_cast<std::uint32_t>(group_id)  << 16) |
           (static_cast<std::uint32_t>(type_id)   <<  8) |
            static_cast<std::uint32_t>(server_id);
}

} // namespace

SociServiceInventory::SociServiceInventory(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

void SociServiceInventory::Reload()
{
    std::vector<Group>           groups;
    std::vector<Machine>         machines;
    std::vector<ServerType>      types;
    std::vector<ServiceInstance> services;

    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bGroupID\", \"szName\" "
            "FROM \"TGROUP\" ORDER BY \"bGroupID\"");
        for (const auto& r : rs)
        {
            Group g{};
            g.id   = static_cast<std::uint8_t>(r.get<int>(0));
            g.name = r.get<std::string>(1);
            groups.push_back(std::move(g));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_service_inventory.TGROUP load: {}", ex.what());
        throw;
    }

    // Build a temp map of machine_id → idx so the TIPADDR loop below
    // can append addrs in O(1) without rescanning the vector.
    std::unordered_map<std::uint8_t, std::size_t> machine_idx;
    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bMachineID\", \"szName\", \"bRouteID\" "
            "FROM \"TMACHINE\" ORDER BY \"bMachineID\"");
        for (const auto& r : rs)
        {
            Machine m{};
            m.id       = static_cast<std::uint8_t>(r.get<int>(0));
            m.name     = r.get<std::string>(1);
            m.route_id = static_cast<std::uint8_t>(r.get<int>(2));
            machine_idx[m.id] = machines.size();
            machines.push_back(std::move(m));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_service_inventory.TMACHINE load: {}", ex.what());
        throw;
    }

    // TIPADDR provides the public + private addresses per machine.
    // bActive=1 keeps the legacy filter. The legacy query is
    // parameterized per machine ID; we load once for the cluster
    // and bucket in-memory.
    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bMachineID\", \"szIPAddr\", \"szPriAddr\" "
            "FROM \"TIPADDR\" WHERE \"bActive\" = 1");
        for (const auto& r : rs)
        {
            const auto mid = static_cast<std::uint8_t>(r.get<int>(0));
            auto it = machine_idx.find(mid);
            if (it == machine_idx.end()) continue;
            auto& m = machines[it->second];
            soci::indicator pub_ind = r.get_indicator(1);
            if (pub_ind != soci::i_null)
                m.public_addrs.push_back(r.get<std::string>(1));
            soci::indicator pri_ind = r.get_indicator(2);
            if (pri_ind != soci::i_null)
                m.private_addrs.push_back(r.get<std::string>(2));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_service_inventory.TIPADDR load: {}", ex.what());
        throw;
    }

    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bType\", \"szName\" "
            "FROM \"TSVRTYPE\" WHERE \"bControl\" = 1 ORDER BY \"bType\"");
        for (const auto& r : rs)
        {
            ServerType t{};
            t.id     = static_cast<std::uint8_t>(r.get<int>(0));
            t.name   = r.get<std::string>(1);
            types.push_back(std::move(t));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_service_inventory.TSVRTYPE load: {}", ex.what());
        throw;
    }

    try
    {
        // bType <> 6 mirrors the legacy filter (skip MSGSVR / disabled
        // dummy rows). ORDER BY keeps the snapshot deterministic for
        // tests + diff-based audit logs.
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"bGroupID\", \"bServerID\", \"bType\", "
            "       \"bMachineID\", \"wPort\", \"szName\" "
            "FROM \"TSERVER\" WHERE \"bType\" <> 6 "
            "ORDER BY \"bGroupID\", \"bType\", \"bServerID\"");
        for (const auto& r : rs)
        {
            ServiceInstance s{};
            s.group_id   = static_cast<std::uint8_t>(r.get<int>(0));
            s.server_id  = static_cast<std::uint8_t>(r.get<int>(1));
            s.type_id    = static_cast<std::uint8_t>(r.get<int>(2));
            s.machine_id = static_cast<std::uint8_t>(r.get<int>(3));
            s.port       = static_cast<std::uint16_t>(r.get<int>(4));
            s.name       = r.get<std::string>(5);
            s.service_id = MakeServiceId(s.group_id, s.type_id, s.server_id);
            services.push_back(std::move(s));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_service_inventory.TSERVER load: {}", ex.what());
        throw;
    }

    spdlog::info("soci_service_inventory: loaded {} groups, {} machines, "
                 "{} types, {} services",
        groups.size(), machines.size(), types.size(), services.size());

    std::lock_guard<std::mutex> lk(m_mtx);
    m_groups   = std::move(groups);
    m_machines = std::move(machines);
    m_types    = std::move(types);
    m_services = std::move(services);
}

const std::vector<Group>& SociServiceInventory::Groups() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_groups;
}
const std::vector<Machine>& SociServiceInventory::Machines() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_machines;
}
const std::vector<ServerType>& SociServiceInventory::Types() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_types;
}
const std::vector<ServiceInstance>& SociServiceInventory::Services() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_services;
}

} // namespace tcontrolsvr
