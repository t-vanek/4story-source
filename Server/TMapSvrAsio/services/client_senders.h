#pragma once

// client_senders — body encoders for the CS_ packets the map sends down
// to the *client* as a result of inbound World→Map cluster traffic
// (MW_ADDCONNECT / MW_CONRESULT / MW_CLOSECHAR / MW_ROUTELIST). Kept as a
// declared-here / defined-in-.cpp unit (like world_senders) so the
// encoders compile against TMap's wire_codec.h once and stay unit-
// testable without a live client socket — AsioSession::SendPacket isn't
// virtual and needs a real connection, so the byte layout is what the
// tests pin down (the handler glue that finds the session and sends is
// thin).
//
// Byte layouts mirror the legacy CTPlayer::SendCS_* / inline CS_ACK
// builders in Server/TMapSvrAsio/legacy_src/CSSender.cpp + SSHandler.cpp.

#include "domain/character.h"
#include "domain/monster.h"
#include "domain/position.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr {

// One entry of the cross-server connect/route list: where a peer map
// server lives. Shared by CS_ADDCONNECT_ACK (MW_ADDCONNECT_REQ relay)
// and, later, CS_ROUTE_ACK.
struct ConnectRoute
{
    std::uint32_t ip_addr   = 0;
    std::uint16_t port      = 0;
    std::uint8_t  server_id = 0;
};

// CS_ADDCONNECT_ACK body — BYTE count + count × (DWORD ip, WORD port,
// BYTE server_id). Mirrors the inline builder in legacy
// SSHandler.cpp:6797 (OnMW_ADDCONNECT_REQ → pPlayer->Say). The map
// relays the peer-server list TWorld handed it down to the client so the
// client can open its cross-server connections.
std::vector<std::byte> EncodeAddConnectAck(
    const std::vector<ConnectRoute>& routes);

// CS_CONNECT_ACK body — BYTE result + BYTE count + count × BYTE
// server_id. Mirrors legacy CTPlayer::SendCS_CONNECT_ACK
// (CSSender.cpp:78). The authoritative connect result the map forwards
// to the client once TWorld settles it via MW_CONRESULT_REQ (result 0 =
// CN_SUCCESS). The transport-only handshake in session.cpp sends the
// same shape with an empty server list.
std::vector<std::byte> EncodeConnectAck(
    std::uint8_t result, const std::vector<std::uint8_t>& server_ids);

// CS_CHARINFO_ACK body — the player's own full char sheet, sent on
// CS_CONREADY_REQ once the client's scene has loaded. Mirrors legacy
// CTPlayer::SendCS_CHARINFO_ACK (CSSender.cpp:121): the ~50 identity /
// money / position / stat scalars, then five list sections (inventory,
// skills, maintain-skills, hotkeys, item cooldowns), then PvP points, a
// server-clock string, and medals.
//
// This bounded port emits the full wire structure so the client parses
// cleanly, but with the lists empty (count = 0) and the fields that are
// World-sourced cluster state (guild / party / corps / tactics / title /
// rank), secure-code, or gameplay-derived (prev/next-level exp, max
// HP/MP, skill-kind points, PvP, medals) shipped as 0 / "" until their
// owning services land — the same staging the legacy DM_LOADCHAR_ACK
// sub-sections and the .NET rewrite's CHARINFO_ACK used. `time_str` is
// the "AM/PM HH:MM" server clock the caller formats (kept a parameter so
// the encoder stays pure / testable). max HP/MP default to current
// HP/MP (full bars) pending the stat formula.
std::vector<std::byte> EncodeCharInfoAck(
    const CharSnapshot& s, const std::string& time_str);

// CS_ENTER_ACK body — another character becoming visible to the client
// (the enter-map AOI exchange on CS_CONREADY, and later every time
// someone walks into view). Mirrors legacy CTPlayer::SendCS_ENTER_ACK
// (CSSender.cpp:395): ~50 identity / appearance / position / state
// scalars, then a maintain-skill list, an equipped-item list, and the
// `new_member` flag.
//
// Appearance + level + region come from the visible char's snapshot;
// `pos` is its live ChannelPresence position (which moves diverge from
// the snapshot spawn point). `color` is the legacy TNCOLOR faction tint
// (CanFight) — a PvP/gameplay value defaulted to 0/friendly here.
// `new_member` is 1 when announcing a just-entered char to the people
// already in view, 0 when listing the existing crowd to the newcomer.
// World-sourced cluster fields (title/guild/fame/tactics/party/corps/
// rank/castle), store, riding, and the two lists ship 0 / "" / empty
// until their owning services land — the same staging CS_CHARINFO_ACK
// uses. max HP/MP default to current HP/MP.
std::vector<std::byte> EncodeEnterAck(
    const CharSnapshot& s, const Position& pos,
    std::uint8_t color, std::uint8_t new_member);

// CS_ADDMON_ACK body — a monster becoming visible to the client (the
// monster half of the enter-map AOI flood, and later every spawn /
// chase into view). Mirrors legacy CTPlayer::SendCS_ADDMON_ACK
// (CSSender.cpp:913): instance id + template id + level + hp/mp +
// position + facing/action state + new_member + country + faction tint
// + region, then a maintain-skill list.
//
// `level` is looked up from the MonsterTemplate by the caller; `country`
// from the spawn point; `color` is the legacy TNCOLOR (PvP / faction)
// defaulted to 0/hostile pending the combat layer; `new_member` is 1
// when announcing a fresh spawn, 0 when listing the standing crowd to a
// joining client. max HP/MP default to current HP / 0, and the
// facing / action / region / maintain-skill fields ship 0 / empty until
// the AI + combat layer drives them. The wire structure is complete so
// the client parses cleanly.
std::vector<std::byte> EncodeAddMonAck(
    const MonsterInstance& m, std::uint8_t level, std::uint8_t country,
    std::uint8_t color, std::uint8_t new_member);

// CS_HPMP_ACK body — an object's HP/MP changed (DWORD id + maxHP + HP +
// maxMP + MP). Mirrors legacy SendCS_HPMP_ACK (CSSender.cpp:1315; the
// bType/bLevel args drive the party relay, not this packet). Broadcast
// to everyone who can see the object so health bars update.
std::vector<std::byte> EncodeHpMpAck(
    std::uint32_t id, std::uint32_t max_hp, std::uint32_t hp,
    std::uint32_t max_mp, std::uint32_t mp);

// CS_DELMON_ACK body — a monster left view (DWORD mon id + BYTE
// exit_map: 1 = walked off the map, 0 = died). Mirrors legacy
// SendCS_DELMON_ACK (CSSender.cpp:1011).
std::vector<std::byte> EncodeDelMonAck(
    std::uint32_t mon_id, std::uint8_t exit_map);

// CS_EXP_ACK body — the player's EXP changed (DWORD exp + prev-level
// threshold + next-level threshold + soul-lot exp). Mirrors legacy
// SendCS_EXP_ACK (CSSender.cpp:1375). The level thresholds need the
// level chart (not modelled yet) and ship 0 until it lands.
std::vector<std::byte> EncodeExpAck(
    std::uint32_t exp, std::uint32_t prev_level_exp,
    std::uint32_t next_level_exp, std::uint32_t soul_lot_exp);

} // namespace tmapsvr
