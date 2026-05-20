#pragma once

// Boot-time schema validation entry point. Confirms the TCURRENTUSER
// columns SociMapSessionValidator binds against exist before the
// listener opens. Throws fourstory::db::SchemaError on mismatch.

namespace fourstory::db { class SessionPool; }

namespace tmapsvr::db {

void ValidateGlobalSchema(fourstory::db::SessionPool& pool);

} // namespace tmapsvr::db
