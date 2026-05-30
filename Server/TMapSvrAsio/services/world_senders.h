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

// MW_ADDCHAR_ACK body — 18 bytes, mirrors SSSender.cpp:237. Sent right
// after a CS_CONNECT_REQ clears so TWorld knows which map owns the char
// and can route DM_/MW_ traffic to it. Parsed by TWorld's OnAddCharAck.
std::vector<std::byte> EncodeAddCharAck(std::uint32_t char_id,
                                        std::uint32_t key,
                                        std::uint32_t ip_addr,
                                        std::uint16_t port,
                                        std::uint32_t user_id);

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

// RW_ENTERCHAR_REQ body — dwCharID + szName. The relay/map asks TWorld
// to resolve a char by name (and verify the id) before opening an entry;
// TWorld answers RW_ENTERCHAR_ACK with the cluster-side guild/party/etc.
// state. Parsed by TWorld's OnEnterCharReq (RWHandler.cpp:28).
std::vector<std::byte> EncodeEnterCharReq(std::uint32_t      char_id,
                                          const std::string& name);

// MW_ENTERCHAR_ACK body — 8 bytes (dwCharID + dwKEY), mirrors legacy
// SSSender.cpp:730. The map's "this connection is ready" reply to the
// World→Map MW_ENTERCHAR_REQ entry composite; TWorld's OnEnterCharAck
// reads exactly these two fields to flip the con's `ready` flag and
// drive its CheckMainCon reconcile.
std::vector<std::byte> EncodeEnterCharAck(std::uint32_t char_id,
                                          std::uint32_t key);

} // namespace tmapsvr
