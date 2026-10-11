#include "soci_service_inventory.h"
#include "native_control.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <unordered_map>
#include <algorithm>

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
    const bool native=native_control::Enabled(m_pool);
    std::unique_ptr<soci::transaction> snapshot_tx;
    if(native) {
        snapshot_tx=std::make_unique<soci::transaction>(sql);
        sql << "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY";
    }
    const auto text=[native](const soci::row& r,std::size_t col) {
        auto value=r.get<std::string>(col);return native?native_control::Bytes(value):value;
    };

    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            (native ? "SELECT \"bGroupID\",encode(name_bytes,'hex') FROM app_control.\"TGROUP\" ORDER BY \"bGroupID\"" :
            "SELECT \"bGroupID\", \"szName\" "
            "FROM \"TGROUP\" ORDER BY \"bGroupID\""));
        for (const auto& r : rs)
        {
            Group g{};
            g.id   = static_cast<std::uint8_t>(r.get<int>(0));
            g.name = text(r,1);
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
            (native ? "SELECT \"bMachineID\",encode(name_bytes,'hex'),\"bRouteID\" FROM app_control.\"TMACHINE\" ORDER BY \"bMachineID\"" :
            "SELECT \"bMachineID\", \"szName\", \"bRouteID\" "
            "FROM \"TMACHINE\" ORDER BY \"bMachineID\""));
        for (const auto& r : rs)
        {
            Machine m{};
            m.id       = static_cast<std::uint8_t>(r.get<int>(0));
            m.name     = text(r,1);
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
            (native ? "SELECT \"bMachineID\",encode(public_bytes,'hex'),encode(private_bytes,'hex') FROM app_control.\"TIPADDR\" WHERE \"bActive\"=1 ORDER BY \"bMachineID\",public_bytes" :
            "SELECT \"bMachineID\", \"szIPAddr\", \"szPriAddr\" "
            "FROM \"TIPADDR\" WHERE \"bActive\" = 1"));
        for (const auto& r : rs)
        {
            const auto mid = static_cast<std::uint8_t>(r.get<int>(0));
            auto it = machine_idx.find(mid);
            if (it == machine_idx.end()) continue;
            auto& m = machines[it->second];
            soci::indicator pub_ind = r.get_indicator(1);
            if (pub_ind != soci::i_null && !text(r,1).empty())
                m.public_addrs.push_back(text(r,1));
            soci::indicator pri_ind = r.get_indicator(2);
            if (pri_ind != soci::i_null && !text(r,2).empty())
                m.private_addrs.push_back(text(r,2));
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
            (native ? "SELECT \"bType\",encode(name_bytes,'hex') FROM app_control.\"TSVRTYPE\" WHERE \"bControl\"=1 ORDER BY \"bType\"" :
            "SELECT \"bType\", \"szName\" "
            "FROM \"TSVRTYPE\" WHERE \"bControl\" = 1 ORDER BY \"bType\""));
        for (const auto& r : rs)
        {
            ServerType t{};
            t.id     = static_cast<std::uint8_t>(r.get<int>(0));
            t.name   = text(r,1);
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
            (native ? "SELECT \"bGroupID\",\"bServerID\",\"bType\",\"bMachineID\",\"wPort\",encode(name_bytes,'hex') FROM app_control.\"TSERVER\" WHERE \"bType\"<>6 ORDER BY \"bGroupID\",\"bType\",\"bServerID\"" :
            "SELECT \"bGroupID\", \"bServerID\", \"bType\", "
            "       \"bMachineID\", \"wPort\", \"szName\" "
            "FROM \"TSERVER\" WHERE \"bType\" <> 6 "
            "ORDER BY \"bGroupID\", \"bType\", \"bServerID\""));
        for (const auto& r : rs)
        {
            ServiceInstance s{};
            s.group_id   = static_cast<std::uint8_t>(r.get<int>(0));
            s.server_id  = static_cast<std::uint8_t>(r.get<int>(1));
            s.type_id    = static_cast<std::uint8_t>(r.get<int>(2));
            s.machine_id = static_cast<std::uint8_t>(r.get<int>(3));
            s.port       = static_cast<std::uint16_t>(r.get<int>(4));
            s.name       = text(r,5);
            s.service_id = MakeServiceId(s.group_id, s.type_id, s.server_id);
            // CTControlSvrModule::LoadData explicitly discards rows whose
            // group/machine/controllable type was not loaded. Keep the original
            // records in PostgreSQL and make this runtime exclusion observable.
            if(std::none_of(groups.begin(),groups.end(),[&](const auto& g){return g.id==s.group_id;})||
               std::none_of(machines.begin(),machines.end(),[&](const auto& m){return m.id==s.machine_id;})||
               std::none_of(types.begin(),types.end(),[&](const auto& t){return t.id==s.type_id;})) {
                spdlog::warn("Control topology excludes service {:#x}: missing source group/machine/controllable type",s.service_id);
                continue;
            }
            services.push_back(std::move(s));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_service_inventory.TSERVER load: {}", ex.what());
        throw;
    }

    // Original CTBLNetwork is a separate topology responsibility.
    {
        const auto query=native ?
            "SELECT \"bMachineID\",encode(network_bytes,'hex') FROM app_control.\"TNETWORK\"" :
            "SELECT \"bMachineID\",\"szNetwork\" FROM \"TNETWORK\"";
        soci::rowset<soci::row> rows=(sql.prepare << query);
        for(const auto& r:rows) {
            const auto it=machine_idx.find(static_cast<std::uint8_t>(r.get<int>(0)));
            if(it!=machine_idx.end())machines[it->second].network=text(r,1);
        }
    }

    spdlog::info("soci_service_inventory: loaded {} groups, {} machines, "
                 "{} types, {} services",
        groups.size(), machines.size(), types.size(), services.size());

    if(snapshot_tx)snapshot_tx->commit();
    std::lock_guard<std::mutex> lk(m_mtx);
    m_groups   = std::move(groups);
    m_machines = std::move(machines);
    m_types    = std::move(types);
    m_services = std::move(services);
}

InventorySnapshot SociServiceInventory::Snapshot() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return {m_groups, m_machines, m_types, m_services};
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
