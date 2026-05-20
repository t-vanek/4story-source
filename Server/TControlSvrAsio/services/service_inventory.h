#pragma once

// IServiceInventory — read-only snapshot of the cluster topology
// loaded from TMACHINE / TGROUP / TSVRTYPE / TSERVER / TIPADDR
// (TGLOBAL_RAGEZONE) at startup. The handler set for the first four
// post-login acks (CT_GROUPLIST_ACK, CT_MACHINELIST_ACK,
// CT_SVRTYPELIST_ACK, CT_SERVICESTAT_ACK) reads off this snapshot.
//
// F1 ships the interface + an in-memory Fake implementation so the
// post-login handler chain has data to emit. The SOCI implementation
// against the real DB lands in F2 along with the peer dialer.

#include <cstdint>
#include <string>
#include <vector>

namespace tcontrolsvr {

struct Group
{
    std::uint8_t  id   = 0;       // bGroupID
    std::string   name;           // szName
};

struct Machine
{
    std::uint8_t              id   = 0;   // bMachineID
    std::string               name;       // szName
    std::uint8_t              route_id = 0;
    std::vector<std::string>  public_addrs;   // szIPAddr — broadcast to clients
    std::vector<std::string>  private_addrs;  // szPriAddr — used for peer dial
    std::string               network;        // PDH counter — F1 informational
};

struct ServerType
{
    std::uint8_t  id     = 0;    // bType — SVRGRP_LOGINSVR / MAPSVR / ...
    std::uint8_t  global = 0;
    std::string   name;
};

// One row of TSERVER joined with its FK targets. Legacy m_mapTSVRTEMP.
// Status / live counters are runtime state and live on PeerSession
// in F2 — they are not part of the inventory snapshot.
struct ServiceInstance
{
    std::uint32_t  service_id  = 0;   // synthetic dwID = (group<<16) | (type<<8) | server
    std::uint8_t   group_id    = 0;
    std::uint8_t   type_id     = 0;
    std::uint8_t   server_id   = 0;
    std::uint8_t   machine_id  = 0;
    std::uint16_t  port        = 0;
    std::string    name;              // human-facing service name
    std::string    bin_path;
    std::string    bin_name;
};

class IServiceInventory
{
public:
    virtual ~IServiceInventory() = default;

    virtual const std::vector<Group>&            Groups() const = 0;
    virtual const std::vector<Machine>&          Machines() const = 0;
    virtual const std::vector<ServerType>&       Types() const = 0;
    virtual const std::vector<ServiceInstance>&  Services() const = 0;
};

} // namespace tcontrolsvr
