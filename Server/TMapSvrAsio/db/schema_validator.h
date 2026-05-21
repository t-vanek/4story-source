#pragma once

// Map-server schema validation. Boot-time fail-fast against TUSER —
// confirms TCURRENTUSER has the columns the F4 handshake reads
// (dwUserID, dwKEY, bGroupID, bChannel, szLoginIP, bLocked) before
// the listener accepts traffic. Mirrors the equivalent entry points
// on TLoginSvrAsio (`tloginsvr::db::ValidateUserSchema`) and
// TPatchSvrAsio (`tpatchsvr::db::ValidateGlobalSchema`).
//
// Additional tables (TCHARTABLE for F5 char load, TITEMTABLE for F9
// items, quest charts for F12, etc.) get validated by their owning
// phases — we don't pre-check them here so the F2 binary can boot
// against a DB that only has the session table deployed.

namespace fourstory::db { class SessionPool; }

namespace tmapsvr::db {

// Throws fourstory::db::SchemaError when a required column is missing.
void ValidateUserSchema(fourstory::db::SessionPool& pool);

// TCHARTABLE column check — the columns the F8 player service SELECTs
// when handling DM_LOADCHAR_REQ. Later phases (items / skills /
// quests / …) will add their tables to a sibling validator each.
void ValidateCharSchema(fourstory::db::SessionPool& pool);

// TINVENTABLE column check — F9 inventory section of
// DM_LOADCHAR_ACK. Independent of TCHARTABLE so an operator with a
// fresh server can ship without TINVENTABLE deployed and have the F8
// snapshot still return; the ack just carries an empty inventory.
void ValidateInventorySchema(fourstory::db::SessionPool& pool);

// TNPCCHART column check — F10 NPC chart loaded once at boot.
// Independent of the per-char tables so a deploy without TNPCCHART
// still answers CS_CONNECT_REQ / DM_LOADCHAR_REQ; only
// CS_NPCTALK_REQ degrades to "no NPCs in world".
void ValidateNpcSchema(fourstory::db::SessionPool& pool);

// TSKILLTABLE column check — F11 per-char learned skill list.
// Independent of the chart side; an operator can ship without
// TSKILLTABLE deployed and the F8 char load still returns with an
// empty skill section.
void ValidateSkillSchema(fourstory::db::SessionPool& pool);

// TQUESTTABLE + TQUESTTERMTABLE column check — F12 per-char quest
// progress. Two-table validator since the encoder reads both.
void ValidateQuestSchema(fourstory::db::SessionPool& pool);

} // namespace tmapsvr::db
