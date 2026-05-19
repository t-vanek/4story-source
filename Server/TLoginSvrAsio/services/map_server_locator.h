#pragma once

// IMapServerLocator — resolves "where do I send this character?" for
// CS_START_REQ.
//
// Legacy server queries TSERVER + TIPADDR (joined on bMachineID) for
// rows where bGroupID matches the requested world AND bType == 4
// (SVRGRP_MAPSVR). Returns the first active row, or the special BR
// (Battle Royale) shard ID 50 for characters registered in
// TBRPLAYERTABLE — see CSHandler.cpp:1387-1398.
//
// Phase A.5 keeps the BR override out of the interface — that's a
// gameplay concern that the eventual SociMapServerLocator handles
// internally via a separate JOIN. The interface here just gives the
// "regular" lookup; BR routing is a service-impl detail.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tloginsvr::services {

struct MapEndpoint
{
    // IPv4 octets in network byte order — so the wire bytes match
    // what the legacy server sends (inet_addr() return value, sent
    // as DWORD straight into the packet buffer on a little-endian
    // host, which happens to lay down the bytes in network order).
    std::array<std::uint8_t, 4> ipv4 = {0, 0, 0, 0};
    std::uint16_t               port = 0;
    std::uint8_t                server_id = 0;
};

// Wire-level status code (legacy TSTATUS_* in NetCode.h:940-946).
//   0 = SLEEP   — server marked offline
//   1 = NORMAL  — accepting logins
//   2 = BUSY    — accepting, but >bWusy current users
//   3 = FULL    — capped, refuses new logins
enum class GroupStatus : std::uint8_t
{
    Sleep  = 0,
    Normal = 1,
    Busy   = 2,
    Full   = 3,
};

// One row of CS_GROUPLIST_ACK. Wire-format per CSHandler.cpp:519-538:
//   szName, bGroupID, bType, bStatus, bCount
//
// `has_char` is the wire byte the legacy server fills with
// `COUNT(DISTINCT TALLCHARTABLE.dwCharID) WHERE dwUserID=:u AND bDelete=0`
// — i.e. the count of non-deleted characters the user has in this
// world. The shipped client (TNetHandler.cpp:351) parses it as
// `m_bCharCnt` BYTE and uses it to decorate the group entry in the
// lobby UI. Capped at 255 to fit the wire byte; the lobby UI doesn't
// distinguish above ~3 anyway. A previous version of this struct
// mislabeled the byte as a flags/visibility nibble, which made the
// legacy client always show every group as un-decorated.
//
// `max_user` (TGROUP.dwMaxUser) drives the per-group cap override:
// when the user has no character in the group AND the live count hits
// max_user, status is forced to Full so the client refuses to enroll
// (legacy CSHandler.cpp:525-534).
struct GroupInfo
{
    std::string   name;
    std::uint8_t  group_id  = 0;
    std::uint8_t  type      = 0;   // legacy TGROUP.bType (server "kind")
    GroupStatus   status    = GroupStatus::Sleep;
    std::uint8_t  has_char  = 0;   // wire bCount — 1 if user owns a non-deleted char in this group
    std::uint32_t max_user  = 0;   // legacy TGROUP.dwMaxUser — soft enrollment cap for new users
    std::uint32_t current_count = 0; // live TCURRENTUSER count — used by the cap override
};

// One row of CS_CHANNELLIST_ACK. Wire-format per CSHandler.cpp:574-578:
//   szName, bChannel, bStatus
struct ChannelInfo
{
    std::string  name;
    std::uint8_t channel = 0;
    GroupStatus  status  = GroupStatus::Sleep;
};

class IMapServerLocator
{
public:
    virtual ~IMapServerLocator() = default;

    // Resolve an endpoint for (`user_id`, `group_id`, `channel`, `char_id`).
    // The tuple matches the legacy CS_START_REQ wire format + the
    // session-level user_id needed by the BR/BOW override checks.
    //
    // Behavior reference (legacy CSHandler.cpp:1312-1430):
    //   1. Find the "natural" server_id for the character (channel/map
    //      based). Current impl uses "first map server in group" — a
    //      future SociMapServerLocator revision can plug in the
    //      character-specific routing once we wire CSPFindServerID's
    //      logic against the schema's map placement columns.
    //   2. If (user_id, char_id) ∈ TBOWPLAYERTABLE → override
    //      server_id to BOW_SERVER_ID (30).
    //   3. If (user_id, char_id) ∈ TBRPLAYERTABLE → override
    //      server_id AND channel to BR_SERVER_ID (50).
    //
    // Returns nullopt if no Map server is available for the chosen
    // server_id (covers SR_NOSERVER / SR_NOGROUP at the wire level).
    virtual std::optional<MapEndpoint> Lookup(
        std::int32_t  user_id,
        std::uint8_t  group_id,
        std::uint8_t  channel,
        std::int32_t  char_id) = 0;

    // CS_GROUPLIST_REQ — return the list of game-groups visible to
    // this user. Status is computed from the live current-user count
    // against the group's bBusy/wFull thresholds (legacy join with
    // CTBLUserCount at CSHandler.cpp:524).
    virtual std::vector<GroupInfo> ListGroups(std::int32_t user_id) = 0;

    // CS_CHANNELLIST_REQ — channels within a group. Same status
    // computation as ListGroups (per-channel counts).
    virtual std::vector<ChannelInfo> ListChannels(std::uint8_t group_id) = 0;
};

} // namespace tloginsvr::services
