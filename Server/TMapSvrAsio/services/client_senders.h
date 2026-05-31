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
#include "domain/inventory.h"
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

// CS_MONMOVE_ACK body (single monster) — WORD count=1, then DWORD mon id,
// BYTE type (OT_MON), pos, pitch, dir, mouse/key dir, action. Mirrors
// legacy SendCS_MONMOVE_ACK (CSSender.cpp:1113), which is a count-prefixed
// list; the AI roam tick sends one monster per packet. pitch / mouse /
// key dir ship 0 (the roam doesn't drive them).
std::vector<std::byte> EncodeMonMoveAck(
    std::uint32_t mon_id, float x, float y, float z,
    std::uint16_t dir, std::uint8_t action);

// CS_ACTION_ACK body — an object performed an action / attack animation
// (BYTE result + DWORD obj id + BYTE obj type + BYTE action id + DWORD
// act id + DWORD ani id + WORD skill id). Mirrors legacy
// CTPlayer::SendCS_ACTION_ACK (CSSender.cpp:1186). This is the *animation*
// half of an attack — the legacy OnCS_ACTION_REQ broadcasts it to everyone
// in view and computes no damage; the damage lands separately in
// CS_DEFEND_REQ. `result` is the SKILL_* validation code (0 = success).
std::vector<std::byte> EncodeActionAck(
    std::uint8_t result, std::uint32_t obj_id, std::uint8_t obj_type,
    std::uint8_t action_id, std::uint32_t act_id, std::uint32_t ani_id,
    std::uint16_t skill_id);

// CS_DIE_ACK body — an object died (DWORD id + BYTE obj type). Mirrors
// legacy CTPlayer::SendCS_DIE_ACK (CSSender.cpp:1392), broadcast to
// everyone in view from CTObjBase::OnDie so the death animation plays.
std::vector<std::byte> EncodeDieAck(
    std::uint32_t id, std::uint8_t obj_type);

// CS_REVIVAL_ACK body — a player revived at a position (DWORD char id +
// FLOAT x/y/z). Mirrors legacy CTPlayer::SendCS_REVIVAL_ACK
// (CSSender.cpp:1404). Broadcast to everyone in view so the corpse stands
// back up at the revival point.
std::vector<std::byte> EncodeRevivalAck(
    std::uint32_t char_id, float x, float y, float z);

// CS_MONEY_ACK body — the player's purse changed (DWORD gold + silver +
// cooper). Mirrors legacy CTPlayer::SendCS_MONEY_ACK (CSSender.cpp:3183).
// Sent to the owner only (private state).
std::vector<std::byte> EncodeMoneyAck(
    std::uint32_t gold, std::uint32_t silver, std::uint32_t cooper);

// One item descriptor — the faithful port of CTItem::WrapPacketClient
// (TItem.cpp:502). Emitted inside CS_MONITEMLIST_ACK / CS_GETITEM_ACK /
// CS_ADDITEM_ACK. Layout (non-cash item):
//   [add_item_id] BYTE slot, WORD itemID, BYTE level, BYTE gem,
//   WORD mogg, WORD companion, BYTE count, DWORD duraMax, DWORD duraCur,
//   BYTE refineMax, BYTE refineCur, BYTE gLevel, INT64 endTime,
//   BYTE gradeEffect, BYTE eld, BYTE wrap, WORD color, WORD customTex,
//   BYTE regGuild, BYTE magicCount, magicCount×(BYTE id, WORD value).
// `bRegGuild` is set when the item is guild-bound to `viewer_char_id`
// (legacy IEV_GUILD == dwCharID check). Magic options are deferred → 0.
std::vector<std::byte> EncodeItemDescriptor(
    const ItemInstance& it, std::uint32_t viewer_char_id, bool add_item_id);

// CS_MONITEMLIST_ACK body — the loot window for a monster corpse. Mirrors
// legacy CTPlayer::SendCS_MONITEMLIST_ACK (CSSender.cpp:2991): BYTE ret,
// BYTE update, DWORD monID, DWORD gold/silver/cooper (the corpse purse
// split from its cooper total), then — only on ret == 0 (MIL_SUCCESS) —
// BYTE count + one item descriptor (add_item_id = true) per corpse item.
std::vector<std::byte> EncodeMonItemListAck(
    std::uint8_t ret, std::uint8_t update, std::uint32_t mon_id,
    std::uint32_t gold, std::uint32_t silver, std::uint32_t cooper,
    const std::vector<ItemInstance>& items, std::uint32_t viewer_char_id);

// CS_MONITEMTAKE_ACK body — BYTE result (MONITEMTAKE_RESULT: 0 = success,
// 1 = full inven, 2 = not found, …). Mirrors legacy
// CTPlayer::SendCS_MONITEMTAKE_ACK (CSSender.cpp:2982).
std::vector<std::byte> EncodeMonItemTakeAck(std::uint8_t result);

// CS_QUESTUPDATE_ACK body — one quest term advanced (DWORD quest id + DWORD
// term id + BYTE term type + BYTE count + BYTE status). 11 bytes. Mirrors
// legacy CTPlayer::SendCS_QUESTUPDATE_ACK (CSSender.cpp:1825). Sent to the
// owner as a hunt/collect objective progresses; `status` is QTS_RUN until
// the goal is met, then QTS_SUCCESS.
std::vector<std::byte> EncodeQuestUpdateAck(
    std::uint32_t quest_id, std::uint32_t term_id, std::uint8_t type,
    std::uint8_t count, std::uint8_t status);

// CS_QUESTCOMPLETE_ACK body — a quest turn-in resolved (BYTE result + DWORD
// quest id + DWORD term id + BYTE term type + DWORD drop id). 14 bytes.
// Mirrors legacy CTPlayer::SendCS_QUESTCOMPLETE_ACK (CSSender.cpp:1843).
// `result` is QR_SUCCESS on completion, QR_TERM (with the unmet term id /
// type) when objectives remain, or QR_DROP on abandon.
std::vector<std::byte> EncodeQuestCompleteAck(
    std::uint8_t result, std::uint32_t quest_id, std::uint32_t term_id,
    std::uint8_t type, std::uint32_t drop_id);

} // namespace tmapsvr
