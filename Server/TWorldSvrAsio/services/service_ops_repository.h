#pragma once

// IServiceOpsRepository — DB write surface for the W6-38 service /
// control-plane handlers. Two legacy stored procedures:
//
//   THelpMessage(bID, timeStart, timeEnd, szMessage)   — persist the
//     operator help-message line CT_HELPMESSAGE_REQ broadcasts
//     (legacy OnDM_HELPMESSAGE_REQ, SSHandler.cpp:13149; the DM
//     round-trip is repository-collapsed).
//   TClearMapCurrentUser(bGroupID, bServerID, bSvrType) — zero the
//     departing map's TCURRENTUSER rows during the SM_DELSESSION
//     teardown (legacy OnDM_CLEARMAPCURRENTUSER_REQ,
//     SSHandler.cpp:9956; also repository-collapsed).

#include <cstdint>
#include <string>

namespace tworldsvr {

class IServiceOpsRepository
{
public:
    virtual ~IServiceOpsRepository() = default;

    // EXEC THelpMessage. start/end are Unix seconds; impls convert to
    // the DB timestamp type (legacy __TIMETODB uses local time).
    virtual bool SaveHelpMessage(std::uint8_t       id,
                                 std::int64_t       start_unix,
                                 std::int64_t       end_unix,
                                 const std::string& message) = 0;

    // EXEC TClearMapCurrentUser for one departing server.
    virtual bool ClearMapCurrentUser(std::uint8_t group_id,
                                     std::uint8_t server_id,
                                     std::uint8_t svr_type) = 0;
};

} // namespace tworldsvr
