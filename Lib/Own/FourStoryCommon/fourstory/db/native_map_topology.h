#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace soci { class session; }
namespace fourstory::db {
// CTBLServerChart and CTBLChannelChart, not an inferred channel list.
// Logical keys are the original DWORD MAKELONG(map, WORD(logical channel)).
struct NativeMapTopology {
    std::string manifest;
    std::uint8_t source_group{};
    std::vector<std::uint8_t> physical_channels;
    std::map<std::uint32_t,std::uint8_t> logical_channels;
    std::string Json() const;
};
// Caller owns the PostgreSQL transaction/snapshot. No defaults, writes or
// hidden SQL Server connection. Empty proved query results remain empty.
NativeMapTopology ReadNativeMapTopology(soci::session&,std::uint8_t world,
    std::uint8_t server,const std::string& expected_manifest={});
}
