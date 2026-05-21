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

} // namespace tmapsvr::db
