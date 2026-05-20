#pragma once

// Server-type enumeration shared by the admin-forwarder handlers
// (handlers_admin.cpp). Values match the legacy
// SVRGRP_* enum used across Server/TControlSvr/Handler.cpp and the
// rest of the cluster. The numeric IDs land in TSVRTYPE.bType and
// TSERVER.bType rows in the production DB; if a deployment uses
// alternative numbering, operators must align the DB rows with
// these constants.
//
// The legacy NetCode.h that originally defined these is not in the
// shared TProtocol library — these are documented here based on the
// in-code uses we have visibility into:
//   - SVRGRP_MAPSVR = 4: confirmed via Server/TLoginSvrAsio/services/
//     soci_map_server_locator.h (SELECT … WHERE bType = 4)
//   - SVRGRP_WORLDSVR = 5: inferred from the WorldSvr-stop cascade in
//     Handler.cpp:146 and the WORLDSVR-typed forwarders
//   - SVRGRP_RLYSVR = 7: relay server, used by Announcement / CharMsg
//   - SVRGRP_MSGSVR = 6: excluded by the TSERVER query (bType <> 6)
//   - SVRGRP_LOGSVR / LOGINSVR / PATCHSVR: assigned based on legacy
//     installer ordering.

#include <cstdint>

namespace tcontrolsvr {

namespace svr_type {

constexpr std::uint8_t kNull     = 0;
constexpr std::uint8_t kLoginSvr = 1;
constexpr std::uint8_t kLogSvr   = 2;
constexpr std::uint8_t kPatchSvr = 3;
constexpr std::uint8_t kMapSvr   = 4;
constexpr std::uint8_t kWorldSvr = 5;
constexpr std::uint8_t kMsgSvr   = 6;   // excluded from inventory load
constexpr std::uint8_t kRlySvr   = 7;
constexpr std::uint8_t kCtlSvr   = 8;
constexpr std::uint8_t kBrSvr    = 9;   // Battle Royale
constexpr std::uint8_t kBoWSvr   = 10;  // Bow / training server

} // namespace svr_type

} // namespace tcontrolsvr
