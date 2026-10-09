#pragma once

// Lobby discovery plus authenticated character-to-Map handoff.
// StartAuthorized binds the authenticated session to its selected endpoint.
// Lookup is retained for legacy backends without a native handoff contract.

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

// Original TSTART_RESULT wire codes (NetCode.h).
enum class StartStatus : std::uint8_t { Success=0, NoServer=1, NoGroup=2, Internal=3 };
struct StartRequest {
    std::int32_t user_id=0;
    std::uint32_t session_key=0;
    std::uint8_t group_id=0, channel=0;
    std::int32_t char_id=0;
};
struct StartResponse {
    StartStatus status=StartStatus::NoServer;
    std::optional<MapEndpoint> endpoint;
};

class IMapServerLocator
{
public:
    virtual ~IMapServerLocator() = default;

    // Compatibility lookup has no session key. Native PostgreSQL refuses it.
    virtual std::optional<MapEndpoint> Lookup(
        std::int32_t  user_id,
        std::uint8_t  group_id,
        std::uint8_t  channel,
        std::int32_t  char_id) = 0;

    // Native PostgreSQL commits an authenticated handoff here. The default
    // retains the legacy backend/test locator interface.
    virtual StartResponse StartAuthorized(const StartRequest& request) {
        auto ep=Lookup(request.user_id,request.group_id,request.channel,request.char_id);
        return {ep ? StartStatus::Success : StartStatus::NoServer, ep};
    }

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
