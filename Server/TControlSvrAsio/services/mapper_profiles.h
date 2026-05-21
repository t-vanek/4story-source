#pragma once

// MapperProfile bundle for TControlSvrAsio.
//
// Bridges the live registry types (RegistryEntry, RuntimeStatus,
// ServiceInstance) to compact DTOs used by the admin shell and
// audit log lines so call sites don't repeat the same field-by-
// field copy.
//
//   * RegistryEntry  → PeerStatusDto   (admin shell `peers` output)
//   * ServiceInstance + RuntimeStatus → PeerStatusDto (composite)

#include "peer_registry.h"
#include "service_inventory.h"

#include "fourstory/mapper/mapper.h"

#include <cstdint>
#include <string>

namespace tcontrolsvr {

// Compact peer-status row for the admin shell. Built from the live
// RegistryEntry plus the static ServiceInstance for the friendly name.
struct PeerStatusDto
{
    std::uint32_t service_id    = 0;
    std::uint8_t  group_id      = 0;
    std::uint8_t  type_id       = 0;
    std::uint8_t  server_id     = 0;
    std::string   reported_name;
    std::string   reported_addr;
    std::uint16_t reported_port = 0;
    std::string   version;
    std::uint32_t pid           = 0;
    std::uint32_t cur_users     = 0;
    std::uint32_t max_users     = 0;
    std::uint64_t lease_epoch   = 0;
    std::int64_t  start_unix    = 0;
};

class ControlMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "ControlMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // RegistryEntry → PeerStatusDto. The synthetic service_id is
        // composed as (group<<16)|(type<<8)|server — decompose it via
        // lambda transforms so the DTO carries the individual bytes.
        MapperConfig<RegistryEntry, PeerStatusDto>()
            .Set(&PeerStatusDto::service_id,    &RegistryEntry::service_id)
            .Set(&PeerStatusDto::reported_name, &RegistryEntry::reported_name)
            .Set(&PeerStatusDto::reported_addr, &RegistryEntry::reported_addr)
            .Set(&PeerStatusDto::reported_port, &RegistryEntry::reported_port)
            .Set(&PeerStatusDto::version,       &RegistryEntry::version)
            .Set(&PeerStatusDto::pid,           &RegistryEntry::pid)
            .Set(&PeerStatusDto::cur_users,     &RegistryEntry::cur_users)
            .Set(&PeerStatusDto::max_users,     &RegistryEntry::max_users)
            .Set(&PeerStatusDto::lease_epoch,   &RegistryEntry::lease_epoch)
            .Set(&PeerStatusDto::start_unix,    &RegistryEntry::start_unix)
            .Set(&PeerStatusDto::group_id,
                 [](const RegistryEntry& e) {
                     return static_cast<std::uint8_t>((e.service_id >> 16) & 0xFF);
                 })
            .Set(&PeerStatusDto::type_id,
                 [](const RegistryEntry& e) {
                     return static_cast<std::uint8_t>((e.service_id >>  8) & 0xFF);
                 })
            .Set(&PeerStatusDto::server_id,
                 [](const RegistryEntry& e) {
                     return static_cast<std::uint8_t>( e.service_id        & 0xFF);
                 });

        // ServiceInstance → PeerStatusDto (used when a peer hasn't yet
        // registered — admin shell falls back to the static inventory).
        MapperConfig<ServiceInstance, PeerStatusDto>()
            .Set(&PeerStatusDto::service_id,    &ServiceInstance::service_id)
            .Set(&PeerStatusDto::group_id,      &ServiceInstance::group_id)
            .Set(&PeerStatusDto::type_id,       &ServiceInstance::type_id)
            .Set(&PeerStatusDto::server_id,     &ServiceInstance::server_id)
            .Set(&PeerStatusDto::reported_name, &ServiceInstance::name)
            .Set(&PeerStatusDto::reported_port, &ServiceInstance::port);
    }
};

} // namespace tcontrolsvr
