#pragma once

// Login-specific schema validation entry points. Delegates to the
// shared fourstory::db::CheckColumns helper with the TGLOBAL +
// TGAME column lists this server's SOCI services consult.

namespace fourstory::db { class SessionPool; }

namespace tloginsvr::db {

// Validate the TGLOBAL schema — accounts, sessions, server registry,
// 2FA (TUSEREMAIL/TUSERTRUSTEDIP). Throws fourstory::db::SchemaError
// on mismatch.
void ValidateGlobalSchema(fourstory::db::SessionPool& pool);

// Validate the TGAME (per-world) schema — chars, items, guilds,
// BR/BOW shard tables. Throws fourstory::db::SchemaError on mismatch.
void ValidateWorldSchema(fourstory::db::SessionPool& pool);

} // namespace tloginsvr::db
