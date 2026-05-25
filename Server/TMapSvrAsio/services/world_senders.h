#pragma once

// world_senders — body encoders for the MW_/RW_ packets the map sends up
// to TWorld. Kept as a declared-here / defined-in-.cpp unit (rather than
// inline) so the encoders compile against TMap's own wire_codec.h exactly
// once and stay callable from tests without bare-header include clashes
// when a test links both server cores.
//
// The byte layouts mirror the legacy CTMapSvrModule::SendMW_* senders
// (Server/TMapSvrAsio/legacy_src/SSSender.cpp) and must match the
// corresponding TWorld reader byte-for-byte.

#include "domain/character.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tmapsvr {

// MW_ENTERSVR_ACK body — 24 fields, mirrors SSSender.cpp:782. The char
// identity is taken straight from the loaded CharSnapshot; the
// session/transient fields (key, aid_country, channel, logout, save,
// result, title_id, rank_point, user_ip) are supplied by the caller
// because they don't live on the TCHARTABLE row. Parsed by TWorld's
// OnEnterSvrAck.
std::vector<std::byte> EncodeEnterSvrAck(const CharSnapshot& s,
                                         std::uint32_t key,
                                         std::uint8_t  aid_country,
                                         std::uint8_t  channel,
                                         std::uint8_t  logout,
                                         std::uint8_t  save,
                                         std::uint8_t  result,
                                         std::uint16_t title_id,
                                         std::uint32_t rank_point,
                                         std::uint32_t user_ip);

} // namespace tmapsvr
